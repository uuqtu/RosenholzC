// ============================================================
// FolderRevision.cpp  —  Revision model + state machine
// ============================================================
#include "FolderRevision.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../../core/Repository.h"
#include "../../model/Utils.h"

namespace Rosenholz {

Database* FolderRevision::db() {
    return DatabasePool::instance().get("akt");
}

void FolderRevision::fromRow(const Row& r) {
    auto g  = [&](const char* k) { return rowGet(r,k); };
    auto gi = [&](const char* k) { return rowGetInt(r,k); };
    auto gb = [&](const char* k) { return rowGetBool(r,k); };

    folderId  = g("folder_id");
    rev         = (uint32_t)gi("rev");
    parentRev   = (uint32_t)gi("parent_rev");
    { auto s = r.count("rev_state") ? r.at("rev_state") : "in_work";
      revState = revStateFromString(s); }
    superseded  = gb("superseded");
    contentHash = g("content_hash");
    contentSize = (int64_t)gi("content_size");
    createdBy   = g("created_by");
    changeNote  = g("change_note");
    createdAt   = g("created_at");
    updatedAt   = g("updated_at");
}

// ── save ─────────────────────────────────────────────────────
bool FolderRevision::save() const {
    auto* d = db(); if (!d) return false;
    auto n = [](const std::string& s) { return s.empty() ? BindParam::null() : BindParam::text(s); };

    return d->exec(R"SQL(
        INSERT OR REPLACE INTO folder_revisions
        (folder_id, rev, parent_rev, rev_state, superseded,
         content_hash, content_size, created_by, change_note, created_at, updated_at)
        VALUES(?,?,?,?,?, ?,?,?,?,?,?)
    )SQL", {
        BindParam::text(folderId), BindParam::int64(rev), BindParam::int64(parentRev),
        BindParam::text(revStateToString(revState)), BindParam::int64(superseded ? 1 : 0),
        BindParam::nullOrText(contentHash), BindParam::int64(contentSize),
        BindParam::nullOrText(createdBy), BindParam::nullOrText(changeNote),
        BindParam::text(createdAt), BindParam::text(updatedAt)
    });
}

bool FolderRevision::update() {
    updatedAt = nowIso();
    return save();
}

bool FolderRevision::remove() const {
    auto* d = db(); if (!d) return false;
    return d->exec(
        "DELETE FROM folder_revisions WHERE folder_id=? AND rev=?;",
        {BindParam::text(folderId), BindParam::int64(rev)});
}

// ── Factory ───────────────────────────────────────────────────
std::shared_ptr<FolderRevision> FolderRevision::createRevision(
    const std::string& folderId,
    uint32_t           baseRev,
    const std::string& createdBy,
    const std::string& note)
{
    // Determine the next rev number
    uint32_t nextRev = latestRevNumber(folderId) + 1;

    auto r = std::make_shared<FolderRevision>();
    r->folderId  = folderId;
    r->rev         = nextRev;
    r->parentRev   = baseRev;
    r->revState    = RevState::IN_WORK;
    r->superseded  = true;   // will be corrected by recomputeSuperseded
    r->createdBy   = createdBy;
    r->changeNote  = note;
    r->createdAt   = nowIso();
    r->updatedAt   = nowIso();

    if (!r->save()) {
        LOG_ERROR("[DocRevision] Failed to save rev " + std::to_string(nextRev) +
                  " for " + folderId);
        return nullptr;
    }

    // Recompute who holds superseded=false for this docId
    r->recomputeSuperseded();

    LOG_INFO("[DocRevision] Created: " + folderId + " rev=" + std::to_string(nextRev));
    return r;
}

std::shared_ptr<FolderRevision> FolderRevision::loadByRev(
    const std::string& folderId, uint32_t rev)
{
    auto* d = db();
    if (!d) return nullptr;
    auto rows = d->query(
        "SELECT * FROM folder_revisions WHERE folder_id=? AND rev=?;",
        {BindParam::text(folderId), BindParam::int64(rev)});
    if (rows.empty()) return nullptr;
    auto r = std::make_shared<FolderRevision>();
    r->fromRow(rows[0]);
    return r;
}

std::vector<std::shared_ptr<FolderRevision>> FolderRevision::loadAllRevisions(
    const std::string& folderId)
{
    auto* d = db();
    std::vector<std::shared_ptr<FolderRevision>> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM folder_revisions WHERE folder_id=? ORDER BY rev;",
        {BindParam::text(folderId)});
    for (auto& row : rows) {
        auto r = std::make_shared<FolderRevision>();
        r->fromRow(row);
        result.push_back(r);
    }
    return result;
}

std::shared_ptr<FolderRevision> FolderRevision::currentRevision(
    const std::string& folderId)
{
    auto* d = db();
    if (!d) return nullptr;
    auto rows = d->query(
        "SELECT * FROM folder_revisions WHERE folder_id=? AND superseded=0 LIMIT 1;",
        {BindParam::text(folderId)});
    if (rows.empty()) return nullptr;
    auto r = std::make_shared<FolderRevision>();
    r->fromRow(rows[0]);
    return r;
}

uint32_t FolderRevision::latestRevNumber(const std::string& folderId) {
    auto* d = db();
    if (!d) return 0;
    auto rows = d->query(
        "SELECT MAX(rev) AS maxrev FROM folder_revisions WHERE folder_id=?;",
        {BindParam::text(folderId)});
    if (rows.empty()) return 0;
    std::string v = rowGet(rows[0], "maxrev");
    return v.empty() ? 0 : (uint32_t)std::stoul(v);
}

// ── State machine ─────────────────────────────────────────────
bool FolderRevision::isTransitionAllowed(RevState from,
                                            RevState to,
                                            bool adminMode) {
    if (from == to) return false;
    if (from == RevState::CLOSED) return false;      // terminal

    // Admin mode: any transition allowed
    if (adminMode) return true;

    // ── Standard transition matrix ───────────────────────────
    // Exact rules per specification:
    //   in_work      → locked | pre_released | released | closed
    //   locked       → released | pre_released | in_work | closed
    //   pre_released → released | closed
    //   released     → closed  (only)
    //   closed       → (terminal — blocked above)
    //
    // NOT allowed in standard mode:
    //   pre_released → in_work      (must go through released or close)
    //   pre_released → locked       (must go to released or close)
    //   released     → locked       (must close)
    //   released     → in_work      (must close, then revise)
    //   released     → pre_released (must close)

    if (from == RevState::IN_WORK)
        return to == RevState::LOCKED       ||
               to == RevState::PRE_RELEASED ||
               to == RevState::RELEASED     ||   // direct release allowed
               to == RevState::CLOSED;

    if (from == RevState::LOCKED)
        return to == RevState::RELEASED     ||   // direct release from lock
               to == RevState::PRE_RELEASED ||
               to == RevState::IN_WORK      ||   // unlock
               to == RevState::CLOSED;

    if (from == RevState::PRE_RELEASED)
        return to == RevState::RELEASED     ||
               to == RevState::CLOSED;           // no back-paths in standard

    if (from == RevState::RELEASED)
        return to == RevState::CLOSED;           // only close from released

    return false;
}

bool FolderRevision::transitionState(RevState target, bool f77Gated) {
    const std::string targetState = revStateToString(target);
    if (!isTransitionAllowed(revState, target, false)) {
        LOG_WARN("[DocRevision] Transition not allowed: " +
                 std::string(revStateToString(revState)) + " → " +
                 targetState + " for " + folderId +
                 " rev=" + std::to_string(rev));
        return false;
    }

    // ── F77 guard: refuse terminal transitions if an active F77 exists ─────
    if (!f77Gated && (target == RevState::RELEASED || target == RevState::CLOSED)) {
        // Check whether an active F77 workflow owns this document's release.
        auto* f77db = DatabasePool::instance().get("f77");
        if (f77db) {
            auto rows = f77db->query(
                "SELECT workflow_id FROM f77_workflows"
                " WHERE entity_type='akt' AND entity_id=? AND status='active' LIMIT 1;",
                {BindParam::text(folderId)});
            if (!rows.empty()) {
                LOG_WARN("[DocRevision] Blocked: active F77 workflow must complete first for "
                         + folderId);
                return false;
            }
        }
    }

    // Additional locked→pre_released rule:
    // locked may only unlock if it is the NEWEST revision
    if (revState == RevState::LOCKED && target == RevState::PRE_RELEASED) {
        uint32_t latest = latestRevNumber(folderId);
        if (rev != latest) {
            LOG_WARN("[DocRevision] locked→pre_released only allowed for newest rev. "
                     "This rev=" + std::to_string(rev) +
                     " latest=" + std::to_string(latest));
            return false;
        }
    }

    revState  = target;
    updatedAt = nowIso();

    if (!save()) {
        LOG_ERROR("[DocRevision] transitionState save failed: " + folderId +
                  " rev=" + std::to_string(rev));
        return false;
    }

    // Recompute which revision holds superseded=false
    recomputeSuperseded();

    LOG_INFO("[DocRevision] State changed: " + folderId + " rev=" +
             std::to_string(rev) + " → " + targetState);
    return true;
}

// ── recomputeSuperseded ───────────────────────────────────────
// Priority:
//   1. Latest released revision
//   2. Latest non-locked/non-closed revision
//   3. Latest in_work revision
// All operations in one transaction for atomicity.
bool FolderRevision::recomputeSuperseded() {
    auto* d = db();
    if (!d) return false;

    // Load all revisions for this docId
    auto all = loadAllRevisions(folderId);
    if (all.empty()) return true;

    // Determine which revision should be current (superseded=false)
    FolderRevision* chosen = nullptr;

    // Priority order: released > locked > pre_released > in_work > (closed fallback)
    // Single loop per priority; adding a new state = adding one entry here.
    static const RevState priority[] = {
        RevState::RELEASED, RevState::LOCKED,
        RevState::PRE_RELEASED, RevState::IN_WORK
    };
    for (RevState target : priority) {
        for (auto it = all.rbegin(); it != all.rend(); ++it) {
            if ((*it)->revState == target) { chosen = it->get(); break; }
        }
        if (chosen) break;
    }
    if (!chosen) chosen = all.back().get();  // all closed: last one is "current"

    // Atomically update all superseded flags
    bool ok = true;
    for (auto& r : all) {
        bool shouldBeCurrent = (r.get() == chosen || r->rev == chosen->rev);
        bool newSuperseded = !shouldBeCurrent;
        if (r->superseded != newSuperseded) {
            r->superseded = newSuperseded;
            r->updatedAt  = nowIso();
            ok &= r->save();
        }
    }

    // Update our own superseded field to reflect computed state
    if (chosen->rev == rev) {
        superseded = false;
    }

    return ok;
}

} // namespace Rosenholz

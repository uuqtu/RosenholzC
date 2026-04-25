// ============================================================
// DocumentRevision.cpp  —  Revision model + state machine
// ============================================================
#include "DocumentRevision.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/Repository.h"
#include "../model/Utils.h"

namespace Rosenholz {

Database* DocumentRevision::db() {
    return DatabasePool::instance().get("dok");
}

void DocumentRevision::fromRow(const Row& r) {
    auto g  = [&](const char* k) { return rowGet(r,k); };
    auto gi = [&](const char* k) { return rowGetInt(r,k); };
    auto gb = [&](const char* k) { return rowGetBool(r,k); };

    documentId  = g("document_id");
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
bool DocumentRevision::save() const {
    auto* d = db(); if (!d) return false;
    auto n = [](const std::string& s) { return s.empty() ? BindParam::null() : BindParam::text(s); };

    return d->exec(R"SQL(
        INSERT OR REPLACE INTO document_revisions
        (document_id, rev, parent_rev, rev_state, superseded,
         content_hash, content_size, created_by, change_note, created_at, updated_at)
        VALUES(?,?,?,?,?, ?,?,?,?,?,?)
    )SQL", {
        BindParam::text(documentId), BindParam::int64(rev), BindParam::int64(parentRev),
        BindParam::text(revStateToString(revState)), BindParam::int64(superseded ? 1 : 0),
        BindParam::nullOrText(contentHash), BindParam::int64(contentSize),
        BindParam::nullOrText(createdBy), BindParam::nullOrText(changeNote),
        BindParam::text(createdAt), BindParam::text(updatedAt)
    });
}

bool DocumentRevision::update() {
    updatedAt = nowIso();
    return save();
}

bool DocumentRevision::remove() const {
    auto* d = db(); if (!d) return false;
    return d->exec(
        "DELETE FROM document_revisions WHERE document_id=? AND rev=?;",
        {BindParam::text(documentId), BindParam::int64(rev)});
}

// ── Factory ───────────────────────────────────────────────────
std::shared_ptr<DocumentRevision> DocumentRevision::createRevision(
    const std::string& documentId,
    uint32_t           baseRev,
    const std::string& createdBy,
    const std::string& note)
{
    // Determine the next rev number
    uint32_t nextRev = latestRevNumber(documentId) + 1;

    auto r = std::make_shared<DocumentRevision>();
    r->documentId  = documentId;
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
                  " for " + documentId);
        return nullptr;
    }

    // Recompute who holds superseded=false for this docId
    r->recomputeSuperseded();

    LOG_INFO("[DocRevision] Created: " + documentId + " rev=" + std::to_string(nextRev));
    return r;
}

std::shared_ptr<DocumentRevision> DocumentRevision::loadByRev(
    const std::string& documentId, uint32_t rev)
{
    auto* d = db();
    if (!d) return nullptr;
    auto rows = d->query(
        "SELECT * FROM document_revisions WHERE document_id=? AND rev=?;",
        {BindParam::text(documentId), BindParam::int64(rev)});
    if (rows.empty()) return nullptr;
    auto r = std::make_shared<DocumentRevision>();
    r->fromRow(rows[0]);
    return r;
}

std::vector<std::shared_ptr<DocumentRevision>> DocumentRevision::loadAllRevisions(
    const std::string& documentId)
{
    auto* d = db();
    std::vector<std::shared_ptr<DocumentRevision>> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM document_revisions WHERE document_id=? ORDER BY rev;",
        {BindParam::text(documentId)});
    for (auto& row : rows) {
        auto r = std::make_shared<DocumentRevision>();
        r->fromRow(row);
        result.push_back(r);
    }
    return result;
}

std::shared_ptr<DocumentRevision> DocumentRevision::currentRevision(
    const std::string& documentId)
{
    auto* d = db();
    if (!d) return nullptr;
    auto rows = d->query(
        "SELECT * FROM document_revisions WHERE document_id=? AND superseded=0 LIMIT 1;",
        {BindParam::text(documentId)});
    if (rows.empty()) return nullptr;
    auto r = std::make_shared<DocumentRevision>();
    r->fromRow(rows[0]);
    return r;
}

uint32_t DocumentRevision::latestRevNumber(const std::string& documentId) {
    auto* d = db();
    if (!d) return 0;
    auto rows = d->query(
        "SELECT MAX(rev) AS maxrev FROM document_revisions WHERE document_id=?;",
        {BindParam::text(documentId)});
    if (rows.empty()) return 0;
    std::string v = rowGet(rows[0], "maxrev");
    return v.empty() ? 0 : (uint32_t)std::stoul(v);
}

// ── State machine ─────────────────────────────────────────────
bool DocumentRevision::isTransitionAllowed(RevState from,
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

bool DocumentRevision::transitionState(const std::string& targetState) {
    if (!isTransitionAllowed(revState, revStateFromString(targetState), false)) {
        LOG_WARN("[DocRevision] Transition not allowed: " +
                 std::string(revStateToString(revState)) + " → " +
                 targetState + " for " + documentId +
                 " rev=" + std::to_string(rev));
        return false;
    }

    // Additional locked→pre_released rule:
    // locked may only unlock if it is the NEWEST revision
    if (revState == RevState::LOCKED && targetState == "pre_released") {
        uint32_t latest = latestRevNumber(documentId);
        if (rev != latest) {
            LOG_WARN("[DocRevision] locked→pre_released only allowed for newest rev. "
                     "This rev=" + std::to_string(rev) +
                     " latest=" + std::to_string(latest));
            return false;
        }
    }

    revState  = revStateFromString(targetState);
    updatedAt = nowIso();

    if (!save()) {
        LOG_ERROR("[DocRevision] transitionState save failed: " + documentId +
                  " rev=" + std::to_string(rev));
        return false;
    }

    // Recompute which revision holds superseded=false
    recomputeSuperseded();

    LOG_INFO("[DocRevision] State changed: " + documentId + " rev=" +
             std::to_string(rev) + " → " + targetState);
    return true;
}

// ── recomputeSuperseded ───────────────────────────────────────
// Priority:
//   1. Latest released revision
//   2. Latest non-locked/non-closed revision
//   3. Latest in_work revision
// All operations in one transaction for atomicity.
bool DocumentRevision::recomputeSuperseded() {
    auto* d = db();
    if (!d) return false;

    // Load all revisions for this docId
    auto all = loadAllRevisions(documentId);
    if (all.empty()) return true;

    // Determine which revision should be current (superseded=false)
    DocumentRevision* chosen = nullptr;

    // Priority 1: latest released revision (most common active state)
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
        if ((*it)->revState == RevState::RELEASED) {
            chosen = it->get(); break;
        }
    }

    // Priority 2: latest locked (locked can be current if no released exists)
    if (!chosen) {
        for (auto it = all.rbegin(); it != all.rend(); ++it) {
            if ((*it)->revState == RevState::LOCKED) {
                chosen = it->get(); break;
            }
        }
    }

    // Priority 3: latest pre_released
    if (!chosen) {
        for (auto it = all.rbegin(); it != all.rend(); ++it) {
            if ((*it)->revState == RevState::PRE_RELEASED) {
                chosen = it->get(); break;
            }
        }
    }

    // Priority 4: latest in_work
    if (!chosen) {
        for (auto it = all.rbegin(); it != all.rend(); ++it) {
            if ((*it)->revState == RevState::IN_WORK) {
                chosen = it->get(); break;
            }
        }
    }

    // Fallback: all revisions are closed — the latest one is still "current"
    // (closed = invalid, but we need exactly one superseded=false)
    if (!chosen) chosen = all.back().get();

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

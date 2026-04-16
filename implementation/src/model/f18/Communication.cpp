// ============================================================
// Communication.cpp  —  Communication entity (replaces Meeting)
// ============================================================
#include "Communication.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../../core/Repository.h"
#include "../Utils.h"

namespace Rosenholz {

Database* Communication::db() {
    return DatabasePool::instance().get("f18");
}

void Communication::fromRow(const Row& r) {
    auto g  = [&](const char* k) { return rowGet(r,k); };
    auto gi = [&](const char* k) { return rowGetInt(r,k); };

    commId        = g("comm_id");
    ownerId       = g("owner_id");
    ownerType     = g("owner_type");
    commType      = g("comm_type");
    title         = g("title");
    agenda        = g("agenda");
    scheduledDate = g("scheduled_date");
    actualDate    = g("actual_date");
    durationMins  = gi("duration_mins");
    channel       = g("channel");
    location      = g("location");
    organiserId   = g("organiser_id");
    participants  = g("participants");
    decisions     = g("decisions");
    actions       = g("actions");
    notes         = g("notes");
    status        = g("status");
    createdAt     = g("created_at");
    updatedAt     = g("updated_at");
}

bool Communication::save() const {
    auto* d = db(); if (!d) return false;
    auto t = [](const std::string& s) { return BindParam::text(s); };
    auto n = [](const std::string& s) { return s.empty() ? BindParam::null() : BindParam::text(s); };
    auto i = [](int v) { return BindParam::int64(v); };

    return d->exec(R"SQL(
        INSERT OR REPLACE INTO communications
        (comm_id, owner_id, owner_type, comm_type, title, agenda,
         scheduled_date, actual_date, duration_mins, channel, location, organiser_id,
         participants, decisions, actions, notes, status, created_at, updated_at)
        VALUES(?,?,?,?,?,?, ?,?,?,?,?,?, ?,?,?,?,?,?,?)
    )SQL", {
        t(commId), t(ownerId), t(ownerType), t(commType), t(title), n(agenda),
        n(scheduledDate), n(actualDate), i(durationMins), n(channel), n(location), n(organiserId),
        t(participants.empty()?"[]":participants),
        n(decisions), t(actions.empty()?"[]":actions), n(notes),
        t(status), t(createdAt), t(updatedAt)
    });
}

bool Communication::update() { updatedAt = nowIso(); return save(); }

bool Communication::remove() const {
    auto* d = db(); if (!d) return false;
    return d->exec("DELETE FROM communications WHERE comm_id=?;",
                   {BindParam::text(commId)});
}

bool Communication::load(const std::string& id) {
    auto* d = db(); if (!d) return false;
    auto rows = d->query("SELECT * FROM communications WHERE comm_id=?;",
                         {BindParam::text(id)});
    if (rows.empty()) return false;
    fromRow(rows[0]);
    return true;
}

std::shared_ptr<Communication> Communication::create(
    const std::string& ownerId,
    const std::string& ownerType,
    const std::string& title,
    const std::string& commType)
{
    auto c = std::make_shared<Communication>();
    c->commId    = genId("COM");
    c->ownerId   = ownerId;
    c->ownerType = ownerType;
    c->commType  = commType.empty() ? "meeting" : commType;
    c->title     = title;
    c->status    = "scheduled";
    c->createdAt = nowIso();
    c->updatedAt = nowIso();
    if (!c->save()) {
        LOG_ERROR("[Communication] Failed to save: " + title);
        return nullptr;
    }
    return c;
}

std::shared_ptr<Communication> Communication::loadById(const std::string& id) {
    auto c = std::make_shared<Communication>();
    if (!c->load(id)) return nullptr;
    return c;
}

std::vector<std::shared_ptr<Communication>> Communication::loadForOwner(
    const std::string& ownerId, const std::string& ownerType)
{
    auto* d = db();
    std::vector<std::shared_ptr<Communication>> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM communications WHERE owner_id=? AND owner_type=? ORDER BY scheduled_date DESC;",
        {BindParam::text(ownerId), BindParam::text(ownerType)});
    for (auto& r : rows) {
        auto c = std::make_shared<Communication>(); c->fromRow(r); result.push_back(c);
    }
    return result;
}

bool Communication::complete(const std::string& decs, const std::string& acts) {
    decisions  = decs;
    actions    = acts;
    status     = "completed";
    actualDate = nowIso().substr(0,10);
    return update();
}

std::vector<std::shared_ptr<Communication>> Communication::loadRecent(int n) {
    auto* d = db();
    std::vector<std::shared_ptr<Communication>> result;
    if (!d) return result;
    for (auto& r : d->query(
            "SELECT * FROM communications ORDER BY created_at DESC LIMIT ?;",
            {BindParam::int64(n)})) {
        auto c = std::make_shared<Communication>(); c->fromRow(r); result.push_back(c);
    }
    return result;
}

} // namespace Rosenholz

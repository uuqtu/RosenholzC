// ============================================================
// Trackable.cpp  —  ise-cobra trackable item implementation
// ============================================================

#include "Trackable.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>
#include <stdexcept>

using json = nlohmann::json;

namespace Rosenholz {

// ── Status helpers ───────────────────────────────────────────
std::string trackableStatusToString(TrackableStatus s) {
    switch (s) {
        case TrackableStatus::Planned:  return "planned";
        case TrackableStatus::Focused:  return "focused";
        case TrackableStatus::Due:      return "due";
        case TrackableStatus::Archived: return "archived";
    }
    return "planned";
}

TrackableStatus trackableStatusFromString(const std::string& s) {
    if (s == "focused")  return TrackableStatus::Focused;
    if (s == "due")      return TrackableStatus::Due;
    if (s == "archived") return TrackableStatus::Archived;
    return TrackableStatus::Planned;
}

// ── Note ─────────────────────────────────────────────────────
json Note::toJson() const {
    return {
        {"noteId",    noteId},
        {"authorId",  authorId},
        {"content",   content},
        {"noteType",  noteType},
        {"createdAt", createdAt},
        {"updatedAt", updatedAt},
        {"isPinned",  isPinned},
        {"isPrivate", isPrivate}
    };
}

Note Note::fromJson(const json& j) {
    Note n;
    n.noteId    = j.value("noteId",    "");
    n.authorId  = j.value("authorId",  "");
    n.content   = j.value("content",   "");
    n.noteType  = j.value("noteType",  "general");
    n.createdAt = j.value("createdAt", "");
    n.updatedAt = j.value("updatedAt", "");
    n.isPinned  = j.value("isPinned",  false);
    n.isPrivate = j.value("isPrivate", false);
    return n;
}

Note Note::create(const std::string& authorId, const std::string& text, const std::string& type) {
    Note n;
    n.noteId    = "note_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    n.authorId  = authorId;
    n.noteType  = type;
    n.createdAt = nowIso();  // forward decl — defined below
    n.updatedAt = n.createdAt;
    // Store as JSON object
    json jc;
    jc["text"]        = text;
    jc["format"]      = "plain";
    jc["attachments"] = json::array();
    n.content = jc.dump();
    return n;
}

// ── Reminder ─────────────────────────────────────────────────
bool Reminder::isDue() const {
    if (isDismissed) return false;
    return !nextTrigger.empty() && nextTrigger <= nowIso();
}

void Reminder::advance() {
    if (!isRecurring) { isDismissed = true; return; }
    // Simple next trigger calc — production: use proper calendar lib
    lastTriggered = nextTrigger;
    // For now just add a fixed offset; real impl would parse recurrenceRule
    nextTrigger = ""; // TODO: add recurrence engine
}

json Reminder::toJson() const {
    return {
        {"reminderId",      reminderId},
        {"trackableId",     trackableId},
        {"title",           title},
        {"message",         message},
        {"remindAt",        remindAt},
        {"isRecurring",     isRecurring},
        {"recurrenceRule",  recurrenceRule},
        {"isDismissed",     isDismissed},
        {"channel",         channel}
    };
}

Reminder Reminder::fromJson(const json& j) {
    Reminder r;
    r.reminderId     = j.value("reminderId",     "");
    r.trackableId    = j.value("trackableId",    "");
    r.title          = j.value("title",          "");
    r.message        = j.value("message",        "");
    r.remindAt       = j.value("remindAt",       "");
    r.isRecurring    = j.value("isRecurring",    false);
    r.recurrenceRule = j.value("recurrenceRule", "");
    r.isDismissed    = j.value("isDismissed",    false);
    r.channel        = j.value("channel",        "console");
    return r;
}

// ── TrackableItem ─────────────────────────────────────────────


// ── Lifecycle ────────────────────────────────────────────────
void TrackableItem::plan(const std::string& date) {
    status      = TrackableStatus::Planned;
    plannedDate = date;
    updatedAt   = nowIso();
    LOG_INFO("TrackableItem planned: " + trackableId + " for " + date);
}

void TrackableItem::focus(const std::string& date) {
    status    = TrackableStatus::Focused;
    focusDate = date.empty() ? nowIso() : date;
    updatedAt = nowIso();
    LOG_INFO("TrackableItem focused: " + trackableId);
}

void TrackableItem::markDue() {
    status    = TrackableStatus::Due;
    updatedAt = nowIso();
    LOG_WARN("TrackableItem marked due: " + trackableId + " title=" + title);
}

void TrackableItem::archive(const std::string& date) {
    status       = TrackableStatus::Archived;
    archivedDate = date.empty() ? nowIso() : date;
    updatedAt    = nowIso();
    LOG_INFO("TrackableItem archived: " + trackableId);
}

// ── Notes ────────────────────────────────────────────────────
Note& TrackableItem::addNote(const std::string& authorId, const std::string& text, const std::string& type) {
    notes.push_back(Note::create(authorId, text, type));
    updatedAt = nowIso();
    LOG_DEBUG("Note added to trackable: " + trackableId);
    return notes.back();
}

void TrackableItem::removeNote(const std::string& noteId) {
    notes.erase(std::remove_if(notes.begin(), notes.end(),
        [&](const Note& n){ return n.noteId == noteId; }), notes.end());
    updatedAt = nowIso();
}

// ── Reminders ────────────────────────────────────────────────
Reminder& TrackableItem::addReminder(const std::string& title_, const std::string& remindAt, const std::string& assigneeId_) {
    Reminder r;
    r.reminderId  = "rem_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    r.trackableId = trackableId;
    r.title       = title_;
    r.remindAt    = remindAt;
    r.nextTrigger = remindAt;
    r.assigneeId  = assigneeId_;
    r.channel     = "console";
    reminders.push_back(r);
    LOG_DEBUG("Reminder added: " + title_ + " at " + remindAt);
    return reminders.back();
}

void TrackableItem::dismissReminder(const std::string& reminderId) {
    for (auto& r : reminders)
        if (r.reminderId == reminderId) { r.isDismissed = true; r.dismissedAt = nowIso(); }
}

std::vector<Reminder> TrackableItem::dueReminders() const {
    std::vector<Reminder> result;
    for (const auto& r : reminders)
        if (r.isDue()) result.push_back(r);
    return result;
}

// ── Notes JSON serialisation ──────────────────────────────────
std::string TrackableItem::notesToJsonString() const {
    json arr = json::array();
    for (const auto& n : notes) arr.push_back(n.toJson());
    return arr.dump();
}

void TrackableItem::notesFromJsonString(const std::string& jsonStr) {
    notes.clear();
    if (jsonStr.empty()) return;
    try {
        auto arr = json::parse(jsonStr);
        for (auto& j : arr) notes.push_back(Note::fromJson(j));
    } catch (...) {
        LOG_WARN("Failed to parse notes JSON for trackable: " + trackableId);
    }
}

// ── Persistence ──────────────────────────────────────────────
std::shared_ptr<TrackableItem> TrackableItem::create(
    const std::string& entityType,
    const std::string& entityId,
    const std::string& title_,
    const std::string& createdBy_)
{
    auto item = std::make_shared<TrackableItem>();
    item->trackableId = genId("VBF");
    item->entityType  = entityType;
    item->entityId    = entityId;
    item->title       = title_;
    item->createdBy   = createdBy_;
    item->createdAt   = nowIso();
    item->updatedAt   = item->createdAt;
    item->status      = TrackableStatus::Planned;

    LOG_INFO("TrackableItem created: " + item->trackableId + " for " + entityType + "/" + entityId);
    return item;
}

bool TrackableItem::save() const {
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) { LOG_ERROR("Trackable::save — tracking DB not available"); return false; }

    std::string notesStr = notesToJsonString();
    std::string statusStr = trackableStatusToString(status);

    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO trackable_items
        (trackable_id, entity_type, entity_id, parent_trackable_id,
         title, description, status, priority,
         focus_date, due_date, archived_date, planned_date,
         assignee_id, created_by, created_at, updated_at, notes)
        VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(trackableId),
        BindParam::text(entityType),
        BindParam::text(entityId),
        ton(parentTrackableId),
        BindParam::text(title),
        ton(description),
        BindParam::text(statusStr),
        ton(priority),
        ton(focusDate),
        ton(dueDate),
        ton(archivedDate),
        ton(plannedDate),
        ton(assigneeId),
        ton(createdBy),
        BindParam::text(createdAt),
        BindParam::text(updatedAt),
        BindParam::text(notesStr)
    });

    // Save reminders
    for (const auto& r : reminders) {
        db->exec(R"(
            INSERT OR REPLACE INTO reminders
            (reminder_id, trackable_id, entity_type, entity_id, assignee_id,
             title, message, remind_at, is_recurring, recurrence_rule,
             last_triggered, next_trigger, is_dismissed, dismissed_at, channel)
            VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
        )", {
            BindParam::text(r.reminderId), BindParam::text(r.trackableId),
            BindParam::text(entityType),   BindParam::text(entityId),
            ton(r.assigneeId), BindParam::text(r.title),
            ton(r.message),    BindParam::text(r.remindAt),
            BindParam::int64(r.isRecurring ? 1 : 0),
            ton(r.recurrenceRule),
            ton(r.lastTriggered), ton(r.nextTrigger),
            BindParam::int64(r.isDismissed ? 1 : 0), ton(r.dismissedAt),
            BindParam::text(r.channel)
        });
    }

    LOG_DEBUG("TrackableItem saved: " + trackableId);
    return ok;
}

bool TrackableItem::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return false;

    auto rows = db->query("SELECT * FROM trackable_items WHERE trackable_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("TrackableItem not found: " + id); return false; }

    auto& r       = rows[0];
    trackableId   = r["trackable_id"];
    entityType    = r["entity_type"];
    entityId      = r["entity_id"];
    parentTrackableId = r["parent_trackable_id"];
    title         = r["title"];
    description   = r["description"];
    status        = trackableStatusFromString(r["status"]);
    priority      = r["priority"];
    focusDate     = r["focus_date"];
    dueDate       = r["due_date"];
    archivedDate  = r["archived_date"];
    plannedDate   = r["planned_date"];
    assigneeId    = r["assignee_id"];
    createdBy     = r["created_by"];
    createdAt     = r["created_at"];
    updatedAt     = r["updated_at"];
    notesFromJsonString(r["notes"]);

    // Load reminders
    reminders.clear();
    auto rems = db->query("SELECT * FROM reminders WHERE trackable_id=?;",
                          {BindParam::text(id)});
    for (auto& rm : rems) {
        Reminder rem;
        rem.reminderId     = rm["reminder_id"];
        rem.trackableId    = rm["trackable_id"];
        rem.assigneeId     = rm["assignee_id"];
        rem.title          = rm["title"];
        rem.message        = rm["message"];
        rem.remindAt       = rm["remind_at"];
        rem.isRecurring    = rm["is_recurring"] == "1";
        rem.recurrenceRule = rm["recurrence_rule"];
        rem.lastTriggered  = rm["last_triggered"];
        rem.nextTrigger    = rm["next_trigger"];
        rem.isDismissed    = rm["is_dismissed"] == "1";
        rem.dismissedAt    = rm["dismissed_at"];
        rem.channel        = rm["channel"];
        reminders.push_back(rem);
    }
    return true;
}

bool TrackableItem::remove() {
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return false;
    db->exec("DELETE FROM reminders WHERE trackable_id=?;", {BindParam::text(trackableId)});
    return db->exec("DELETE FROM trackable_items WHERE trackable_id=?;", {BindParam::text(trackableId)});
}

std::vector<std::shared_ptr<TrackableItem>> TrackableItem::loadForEntity(
    const std::string& entityType_, const std::string& entityId_)
{
    std::vector<std::shared_ptr<TrackableItem>> result;
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return result;

    auto rows = db->query(
        "SELECT trackable_id FROM trackable_items WHERE entity_type=? AND entity_id=? ORDER BY created_at;",
        {BindParam::text(entityType_), BindParam::text(entityId_)});

    for (auto& r : rows) {
        auto item = std::make_shared<TrackableItem>();
        if (item->load(r["trackable_id"]))
            result.push_back(item);
    }
    LOG_DEBUG("Loaded " + std::to_string(result.size()) + " trackables for " + entityType_ + "/" + entityId_);
    return result;
}

} // namespace Rosenholz

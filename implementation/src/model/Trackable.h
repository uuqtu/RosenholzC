#pragma once
#include "Utils.h"
// ============================================================
// Trackable.h  —  Base class for all trackable items
//
// Inspired by the ise-cobra Krankentransport model with states:
//   planned  → item exists, not yet actively worked
//   focused  → currently being actively worked (focus date set)
//   due      → deadline is imminent or reached
//   archived → completed / done
//
// Any entity can attach Trackable items to itself.
// Trackable items can have child Trackable items (recursive).
// Notes are stored as JSON arrays in the database.
// ============================================================

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace RH {

// ── TrackableStatus ──────────────────────────────────────────
enum class TrackableStatus {
    Planned,   ///< Scheduled, not yet being worked
    Focused,   ///< Currently being actively worked (ise-cobra: focused)
    Due,       ///< Deadline reached or imminent
    Archived   ///< Completed / done (ise-cobra: archived)
};

std::string trackableStatusToString(TrackableStatus s);
TrackableStatus trackableStatusFromString(const std::string& s);

// ── Note ─────────────────────────────────────────────────────
struct Note {
    std::string noteId;
    std::string authorId;
    std::string content;     ///< JSON: {text, format, attachments[]}
    std::string noteType;    ///< general|decision|action|reminder
    std::string createdAt;
    std::string updatedAt;
    bool        isPinned   { false };
    bool        isPrivate  { false };

    nlohmann::json toJson() const;
    static Note fromJson(const nlohmann::json& j);
    static Note create(const std::string& authorId,
                       const std::string& text,
                       const std::string& type = "general");
};

// ── Reminder ─────────────────────────────────────────────────
struct Reminder {
    std::string reminderId;
    std::string trackableId;
    std::string entityType;
    std::string entityId;
    std::string assigneeId;
    std::string title;
    std::string message;
    std::string remindAt;      ///< ISO 8601 datetime
    bool        isRecurring  { false };
    std::string recurrenceRule; ///< DAILY|WEEKLY|MONTHLY
    std::string lastTriggered;
    std::string nextTrigger;
    bool        isDismissed  { false };
    std::string dismissedAt;
    std::string channel;       ///< console|email|qt-notification

    bool isDue() const;        ///< true if nextTrigger <= now
    void advance();            ///< Compute next trigger from recurrenceRule

    nlohmann::json toJson() const;
    static Reminder fromJson(const nlohmann::json& j);
};

// ── TrackableItem ─────────────────────────────────────────────
class TrackableItem {
public:
    // ── Identity ───────────────────────────────────────────
    std::string trackableId;
    std::string entityType;       ///< What kind of entity owns this
    std::string entityId;         ///< ID of the owning entity
    std::string parentTrackableId; ///< For child tasks (recursive)
    std::string title;
    std::string description;

    // ── ise-cobra state model ──────────────────────────────
    TrackableStatus status     { TrackableStatus::Planned };
    std::string     priority;
    std::string     plannedDate;   ///< When it was planned to start
    std::string     focusDate;     ///< Date to actively work on it (ise-cobra: focused)
    std::string     dueDate;       ///< Hard deadline (ise-cobra: due)
    std::string     archivedDate;  ///< Completion date (ise-cobra: archived)
    std::string     assigneeId;
    std::string     createdBy;
    std::string     createdAt;
    std::string     updatedAt;

    // ── Notes (stored as JSON array in DB) ─────────────────
    std::vector<Note>     notes;
    std::vector<Reminder> reminders;

    // ── Child items ────────────────────────────────────────
    std::vector<std::shared_ptr<TrackableItem>> children;

    // ── Lifecycle ──────────────────────────────────────────
    void plan(const std::string& plannedDate);
    void focus(const std::string& focusDate = "");   ///< Move to focused
    void markDue();
    void archive(const std::string& archivedDate = "");

    // ── Notes management ───────────────────────────────────
    Note& addNote(const std::string& authorId, const std::string& text,
                  const std::string& type = "general");
    void  removeNote(const std::string& noteId);

    // ── Reminders management ───────────────────────────────
    Reminder& addReminder(const std::string& title, const std::string& remindAt,
                          const std::string& assigneeId = "");
    void       dismissReminder(const std::string& reminderId);
    std::vector<Reminder> dueReminders() const;

    // ── Persistence ────────────────────────────────────────
    bool save() const;
    bool load(const std::string& id);
    bool remove();
    static std::shared_ptr<TrackableItem> create(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& title,
        const std::string& createdBy = "");

    /// Load all trackables for a given entity
    static std::vector<std::shared_ptr<TrackableItem>> loadForEntity(
        const std::string& entityType, const std::string& entityId);

    /// Serialize notes vector to JSON string (for DB storage)
    std::string notesToJsonString() const;
    void notesFromJsonString(const std::string& jsonStr);


};

} // namespace RH

#pragma once
// ============================================================
// Communication.h  —  Communication entity (replaces Meeting)
//
// A Communication is a message, call, meeting, email, or report
// that can be attached to:
//   - F16 Projekt      (ownerType = "project")
//   - F22 Aufgabe      (ownerType = "task")
//   - F18OperationStep  (ownerType = "f18step")
//
// The ownerType + ownerId pair identifies the parent.
// ============================================================

#include "../Utils.h"
#include <string>
#include <vector>
#include <memory>
#include "../../core/Database.h"

namespace Rosenholz {

class Communication {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string commId;             // XV/COM/nnnn/yyyy
    std::string ownerId;            // → F16.projectId | F22.taskId | F18OperationStep.stepId
    std::string ownerType;          // "project" | "task" | "f18step"

    // ── Content ───────────────────────────────────────────────
    std::string commType    { "meeting" };  // message|call|meeting|email|report
    std::string title;
    std::string agenda;
    std::string scheduledDate;
    std::string actualDate;
    int         durationMins    { 0 };
    std::string channel;                   // teams|zoom|phone|email|in-person
    std::string location;
    std::string organiserId;               // → Person

    // ── Outcome ───────────────────────────────────────────────
    std::string participants;   // JSON array of {personId, role}
    std::string decisions;
    std::string actions;        // JSON array of action items
    std::string notes;
    CommStatus  status  { CommStatus::SCHEDULED }; ///< lifecycle

    // ── Audit ─────────────────────────────────────────────────
    std::string createdAt;
    std::string updatedAt;

    // ── CRUD ──────────────────────────────────────────────────
    bool save()   const;
    bool update();
    bool remove() const;
    bool load(const std::string& id);

    // ── Factory ───────────────────────────────────────────────
    // ------------------------------
    // create
    //
    // Parameters:
    //   ownerId   : ID of F16, F22, or F18OperationStep
    //   ownerType : "project" | "task" | "f18step"
    //   title     : display name
    //   commType  : message|call|meeting|email|report
    // ------------------------------
    static std::shared_ptr<Communication> create(
        const std::string& ownerId,
        const std::string& ownerType,
        const std::string& title,
        const std::string& commType = "meeting");

    static std::shared_ptr<Communication> loadById(const std::string& id);

    // ------------------------------
    // loadForOwner
    // Load all communications for a given owner (project, task, or step).
    // ------------------------------
    static std::vector<std::shared_ptr<Communication>> loadForOwner(
        const std::string& ownerId,
        const std::string& ownerType);

    // ------------------------------
    // complete
    // Record decisions and actions after the communication.
    // Sets status = "completed" and actualDate = now.
    // ------------------------------
    bool complete(const std::string& decisions, const std::string& actions);

    static std::vector<std::shared_ptr<Communication>> loadRecent(int n = 20);

    // ── Notiz template ───────────────────────────────────────────
    /// Returns a pre-filled note template string for the given commType.
    /// Used only in KOM creation flow — nowhere else.
    static std::string notizTemplate(CommType type);

private:
    void fromRow(const Row& r);
    static Database* db();
};

} // namespace Rosenholz

#pragma once
// ============================================================
// F18OperationStep.h  —  Step inside an F18Operation
//
// Replaces WorkflowAction entirely.
// Every F18Operation has exactly:
//   - One Init step (isInitialize=true, autoApprove=true, sequenceOrder=0)
//   - Zero or more mid-steps (added via F18Operation::addStep)
//   - One End step (isFinal=true, sequenceOrder=9999)
//
// Steps carry the ise-cobra tracking state (planned/focused/due/archived).
// Steps can own Communications and Documents.
// ============================================================

#include "../Utils.h"
#include <string>
#include <vector>
#include <memory>
#include "../../core/Database.h"

namespace Rosenholz {

class F18OperationStep {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string stepId;             // XV/WFS/nnnn/yyyy
    std::string vorgangId;          // → F18Operation (parent)
    std::string tplStepId;          // → template step (optional)

    // ── Content ───────────────────────────────────────────────
    std::string title;
    std::string description;
    std::string stepType    { "task" };   // approval|review|task|notification
    int         sequenceOrder       { 0 };
    // Always sequential — F18 Operation Steps are strictly ordered
    std::string predecessorStepIds;              // comma-separated stepIds

    // ── Bookend flags ─────────────────────────────────────────
    bool        isInitialize        { false };
    bool        isFinal             { false };

    // ── Assignment ────────────────────────────────────────────
    std::string assignedTo;
    std::string requiredRole;
    std::string dueDate;
    std::string startedDate;
    std::string completedDate;
    int         slaHours            { 0 };
    bool        slaBreached         { false };

    // ── Status & result ───────────────────────────────────────
    std::string status  { "pending" };
    // pending|in_progress|approved|rejected|skipped|cancelled
    bool        autoApprove         { false };
    bool        requiresComment     { false };
    bool        requiresDocument    { false };
    std::string decision;
    std::string decisionBy;
    std::string decisionDate;
    std::string comment;

    // ── ise-cobra tracking state ──────────────────────────────
    // Tracking — auto-computed from dates (not manually set)
    // planned: focusDate not yet reached
    // focused: focusDate passed, dueDate not yet
    // due:     dueDate passed, step not done
    // in_work: explicitly marked as being worked on
    // archived:step completed/rejected/skipped
    std::string trackingStatus  { "planned" };
    std::string focusDate;    ///< Must be before dueDate
    std::string inWorkSince;  ///< Set when marked in_work
    std::string priority        { "medium" };  // low|medium|high|critical
    std::string assignedToGroup;
    std::string progressNote;
    int         percentComplete { 0 };

    // ── Audit ─────────────────────────────────────────────────
    std::string notes   { "{}" };
    std::string createdAt;
    std::string updatedAt;

    // ── State predicates ──────────────────────────────────────
    bool isComplete() const {
        return status == "approved" || status == "rejected" || status == "skipped";
    }

    // ------------------------------
    // canStart
    // Returns true if all predecessor steps are complete.
    // Always true if predecessorStepIds is empty.
    // ------------------------------
    /// Auto-compute and persist trackingStatus from current dates and state.
    /// Called automatically on save and on date changes.
    void computeTrackingStatus();

    bool canStart(const std::vector<F18OperationStep>& allSteps) const;

    // ── CRUD ──────────────────────────────────────────────────
    bool save()   const;
    bool remove() const;

    // ── Factory ───────────────────────────────────────────────
    static std::shared_ptr<F18OperationStep> loadById(const std::string& id);
    static std::vector<F18OperationStep>     loadForVorgang(const std::string& vorgangId);

private:
    void fromRow(const Row& r);
    static Database* db();
};

} // namespace Rosenholz

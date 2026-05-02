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
    std::string operationId;          // → F18Operation (parent)
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
    // isFree: free steps have no predecessor dependencies.
    // canStart() always returns true for free steps regardless of
    // other steps' states, and any status transition is always allowed.
    bool        isFree              { false };

    // ── Assignment ────────────────────────────────────────────
    std::string assignedTo;
    std::string requiredRole;
    std::string dueDate;
    std::string startedDate;
    std::string completedDate;
    int         slaHours            { 0 };
    bool        slaBreached         { false };

    // ── Status & result ───────────────────────────────────────
    F18StepStatus status { F18StepStatus::PENDING };
    // Lifecycle: PENDING → IN_PROGRESS → WAITING → BLOCKED → DONE / SKIPPED
    // Terminal states: DONE, SKIPPED
    // WAITING: blocked on external input (not predecessor steps)
    // BLOCKED: blocked by predecessor steps or hard constraint
    bool        autoApprove         { false };
    bool        requiresComment     { false };
    bool        requiresDocument    { false };
    // Outcome fields: populated when step transitions to done or skipped
    std::string decision;        // why done/skipped
    std::string decisionBy;      // person who set the terminal state
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
    std::string startDatePlanned; ///< planned start (for tracking)
    std::string endDatePlanned;   ///< planned end (= planned due)
    std::string focusDate;        ///< auto-computed midpoint
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
    /// Returns display symbol — UI layer maps to ASCII/icon.
    F18StepSymbol displaySymbol() const;
    bool isComplete() const {
        return status == F18StepStatus::DONE || status == F18StepStatus::SKIPPED;
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
    bool update() const;  ///< save with updatedAt=now
    bool complete();      ///< Sets status=done and completedDate=now
    bool remove() const;

    // ── Factory ───────────────────────────────────────────────
    static std::shared_ptr<F18OperationStep> loadById(const std::string& id);
    static std::vector<F18OperationStep>     loadForVorgang(const std::string& operationId);

private:
    void fromRow(const Row& r);
    static Database* db();
};

} // namespace Rosenholz

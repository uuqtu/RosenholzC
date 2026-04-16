#pragma once
// ============================================================
// F18WorkflowStep.h  —  Step inside an F18Workflow
//
// Replaces WorkflowAction entirely.
// Every F18Workflow has exactly:
//   - One Init step (isInitialize=true, autoApprove=true, sequenceOrder=0)
//   - Zero or more mid-steps (added via F18Workflow::addStep)
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

class F18WorkflowStep {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string stepId;             // XV/WFS/nnnn/yyyy
    std::string vorgangId;          // → F18Workflow (parent)
    std::string tplStepId;          // → template step (optional)

    // ── Content ───────────────────────────────────────────────
    std::string title;
    std::string description;
    std::string stepType    { "task" };   // approval|review|task|notification
    int         sequenceOrder       { 0 };
    std::string executionType { "sequential" };  // sequential|parallel|free
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
    std::string trackingStatus  { "planned" }; // planned|focused|due|archived
    std::string plannedDate;
    std::string focusDate;
    std::string archivedDate;
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
    bool canStart(const std::vector<F18WorkflowStep>& allSteps) const;

    // ── CRUD ──────────────────────────────────────────────────
    bool save()   const;
    bool remove() const;

    // ── Factory ───────────────────────────────────────────────
    static std::shared_ptr<F18WorkflowStep> loadById(const std::string& id);
    static std::vector<F18WorkflowStep>     loadForVorgang(const std::string& vorgangId);

private:
    void fromRow(const Row& r);
    static Database* db();
};

} // namespace Rosenholz

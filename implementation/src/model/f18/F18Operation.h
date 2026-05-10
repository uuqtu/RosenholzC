#pragma once

// ============================================================
// F18Operation.h  —  Unified F18 Operation entity
//
// An F18 Operation (formerly F18Workflow) represents a DDR-style
// operational vorgang (Incident, Risk, Measure, etc.).
// NOT to be confused with F77 Freigabe-Workflows (WorkflowInstance).
//
// F18Operation is the single concrete class that represents all
// types of workflow-based entities in Rosenholz PM.
// The `operationType` field selects the semantic role and determines
// which type-specific fields are populated.
//
// Hierarchy:
//   F16 (Projekt) ──0..*──► F18Operation
//   F22 (Aufgabe) ──0..*──► F18Operation
//   F18Operation   ──1..*──► F24
//
// ChangeObject is the only F18 that references another F18
// (parentVorgangId → a ChangeRequest F18).
//
// Cardinality rules per F22:
//   assumptionConstraint : 0..1
//   lessonsLearned       : 0..1
//   decisionLog          : 0..1
//   all others           : 0..*
// ============================================================

#include "../Utils.h"
#include "../../core/OperationResult.h"
#include <string>
#include <vector>
#include <memory>
#include "../../core/Database.h"
#include "../../core/RegNumber.h"

namespace Rosenholz {

// ------------------------------
// F18OperationType
// All valid values for the operationType discriminator field.
// ------------------------------
namespace F18OperationType {
    constexpr const char* GENERIC              = "generic";
    constexpr const char* INCIDENT             = "incident";
    constexpr const char* RISK                 = "risk";
    constexpr const char* MEASURE              = "measure";
    constexpr const char* QUALITY_GATE         = "qualityGate";
    constexpr const char* ASSUMPTION_CONSTRAINT= "assumptionConstraint";
    constexpr const char* COMMUNICATION_PLAN   = "communicationPlan";
    constexpr const char* LESSONS_LEARNED      = "lessonsLearned";
    constexpr const char* DECISION_LOG         = "decisionLog";
    constexpr const char* CHANGE_REQUEST       = "changeRequest";
    constexpr const char* CHANGE_OBJECT        = "changeObject";
    constexpr const char* F77_STEP             = "f77_step";  ///< F77 workflow step
}

class F24; // forward

class F18Operation {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string operationId;              // XV/F18/nnnn/yyyy
    std::string operationType;            // see F18OperationType::*
    std::string taskId;                 // → F22 (required)
    std::string parentVorgangId;        // → F18 ChangeRequest (CO only)

    // ── Common fields (all types) ─────────────────────────────
    std::string title;
    std::string description;
    EntityStatus status         { EntityStatus::IN_WORK }; ///< lifecycle via F77 onlyWFI End step)
    std::string releaseWorkflowId;               // WFI ID of the controlling Main Workflow
    bool        wfLocked { false };               // true while F77W is ACTIVE — mutations blocked
    std::string ownerId;                      // → Person
    std::string priority        { "medium" }; // low|medium|high|critical
    std::string createdAt;
    std::string updatedAt;

    // ── Type-specific: incident ────────────────────────────────
    std::string incidentType;           // technical|process|security|quality
    std::string severity;               // low|medium|high|critical
    std::string occurredDate;
    std::string resolvedDate;
    std::string rootCause;
    std::string immediateAction;
    std::string resolution;
    double      costImpact          { 0.0 };
    int         scheduleImpactDays  { 0 };
    std::string scopeImpact;
    std::string qualityImpact;

    // ── Type-specific: risk ───────────────────────────────────
    std::string riskLevel       { "medium" }; // low|medium|high|critical
    int         probabilityScore    { 0 };    // 1-5
    int         impactScoreTime     { 0 };
    int         impactScoreCost     { 0 };
    int         impactScoreQuality  { 0 };
    int         impactScoreScope    { 0 };
    int         overallRiskScore    { 0 };
    std::string responseStrategy;   // avoid|mitigate|transfer|accept
    std::string contingencyPlan;
    std::string triggerCondition;
    std::string residualRiskLevel;
    double      costReserve         { 0.0 };
    int         scheduleReserveDays { 0 };

    // ── Type-specific: measure ────────────────────────────────
    std::string measureCategory;    // corrective|preventive|detective
    std::string plannedDate;
    std::string actualDate;
    std::string effectiveness;
    std::string verificationMethod;
    std::string verifiedDate;
    std::string verifiedBy;

    // ── Type-specific: qualityGate ────────────────────────────
    std::string phase;
    std::string criteria;
    std::string acceptanceCriteria;
    std::string findings;
    std::string gateResult;         // passed|failed|conditional|pending
    std::string gateDecision;       // proceed|hold|stop

    // ── Type-specific: assumptionConstraint ───────────────────
    std::string acType;             // assumption|constraint
    std::string validatedDate;
    std::string validatedBy;
    std::string impact;

    // ── Type-specific: communicationPlan ─────────────────────
    std::string audience;
    std::string frequency;
    std::string channel;
    std::string responsible;

    // ── Type-specific: lessonsLearned ─────────────────────────
    std::string lessonType;         // positive|negative|observation
    std::string recommendation;
    std::string applicablePhases;

    // ── Type-specific: decisionLog ────────────────────────────
    std::string decisionType;       // architectural|process|resource|scope
    std::string rationale;
    std::string decisionDate;
    std::string decisionBy;
    std::string alternativesConsidered;

    // ── Type-specific: changeRequest ─────────────────────────
    std::string changeType;         // general|scope|budget|schedule|quality
    std::string justification;
    std::string crImpact;
    std::string raisedDate;
    std::string crDecisionDate;
    std::string crDecisionRationale;
    int         crScheduleImpactDays { 0 };

    // ── Type-specific: changeObject ──────────────────────────
    // parentVorgangId (see above) points to the ChangeRequest F18
    std::string executedBy;
    std::string executionDate;

    // ── Common tail ───────────────────────────────────────────
    std::string notes           { "{}" };   // JSON array of progress notes
    std::string links;

    // ── Lazy-loaded children ──────────────────────────────────
    std::vector<F24> steps;  ///< Lazy-loaded, value type for iteration


    // ── State predicates ──────────────────────────────────────
    bool isReleased()    const { return status == EntityStatus::RELEASED; }
    bool isLocked()      const { return status == EntityStatus::LOCKED; }
    bool isClosed()      const { return status == EntityStatus::CLOSED; }
    bool isEditable()    const { return status == EntityStatus::IN_WORK; }
    bool canEdit()    const { return status == EntityStatus::IN_WORK && !isWorkflowComplete(); }
    /// True when the controlling F77 workflow has status="completed".
    bool isWorkflowComplete() const;

    // ── CRUD ──────────────────────────────────────────────────
    OperationResult save()   const;
    void ensureReleaseWorkflow();  ///< Called after first save to create Main WFI
    OperationResult update();
    OperationResult remove() const;
    bool load(const std::string& id);

    // ── Factory ───────────────────────────────────────────────
    // ------------------------------
    // create
    //
    // Parameters:
    //   taskId  : owning F22 (required)
    //   title      : display name
    //   type       : one of F18OperationType::*
    //   taskId     : owning F22 (optional)
    //
    // Returns: saved F18Operation with generated operationId
    // ------------------------------
    static int count();
    static std::shared_ptr<F18Operation> create(
        const std::string& taskId,
        const std::string& title,
        const std::string& type = F18OperationType::GENERIC);

    static std::shared_ptr<F18Operation> loadById(const std::string& id);

    // ------------------------------
    // loadForProject / loadForTask
    // Load all F18 Workflows for a given parent entity.
    // Optional type filter.
    // ------------------------------

    static std::vector<std::shared_ptr<F18Operation>> loadForTask(
        const std::string& taskId,
        const std::string& type = "");

    // ------------------------------
    // loadRecent — last n created across all types
    // ------------------------------
    static std::vector<std::shared_ptr<F18Operation>> loadRecent(int n = 20);

    // ── Step management ───────────────────────────────────────
    // ------------------------------
    // addStep — add an F24 to this workflow.
    // Always inserts between Init and End bookends.
    // ------------------------------
    /// Default: appends new step before the End step.
    // addStep: append a regular step (connected in the linear chain) or a
    // free step (isFree=true, no predecessor dependencies, always startable).
    /// Engine tick: auto-approve Init, auto-approve End when all mid-steps done.
    /// Returns true if any state changed.
    bool tick();

    std::shared_ptr<F24> addStep(
        const std::string& title,
        const std::string& stepType        = "task",
        const std::string& assigneeId      = "",
        bool               isFree          = false,
        const std::string& startDatePlanned = "",
        const std::string& endDatePlanned   = "");

    /// Custom: inserts between predecessorStepId and its successor.
    /// The old successor then points to the new step.
    std::shared_ptr<F24> insertAfter(
        const std::string& predecessorStepId,
        const std::string& title,
        const std::string& stepType   = "task",
        const std::string& assigneeId = "");

    // ------------------------------
    // loadSteps — populate the steps vector from DB.
    // ------------------------------
    void loadSteps();

    // ── Score helpers ─────────────────────────────────────────
    void recalcRiskScore();   ///< Sets overallRiskScore + riskLevel from impact scores

    // ── Note management (stored as JSON in notes field) ───────
    OperationResult addNote(const std::string& authorId,
                 const std::string& text,
                 const std::string& noteType = "general");

    std::string mfsSchluesselText() const;
private:
    void fromRow(const Row& r);
    static Database* db();
};

} // namespace Rosenholz

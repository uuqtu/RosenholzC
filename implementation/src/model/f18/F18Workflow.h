#pragma once
// ============================================================
// F18Workflow.h  —  Unified Workflow-Vorgang entity
//
// F18Workflow is the single concrete class that represents all
// types of workflow-based entities in Rosenholz PM.
// The `vorgangType` field selects the semantic role and determines
// which type-specific fields are populated.
//
// Hierarchy:
//   F16 (Projekt) ──0..*──► F18Workflow
//   F22 (Aufgabe) ──0..*──► F18Workflow
//   F18Workflow   ──1..*──► F18WorkflowStep
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
#include <string>
#include <vector>
#include <memory>
#include "../../core/Database.h"
#include "../../core/RegNumber.h"

namespace Rosenholz {

// ------------------------------
// F18VorgangType
// All valid values for the vorgangType discriminator field.
// ------------------------------
namespace F18VorgangType {
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
}

class F18WorkflowStep; // forward

class F18Workflow {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string vorgangId;              // XV/F18/nnnn/yyyy
    std::string vorgangType;            // see F18VorgangType::*
    std::string projectId;              // → F16 (0..1)
    std::string taskId;                 // → F22 (0..1)
    std::string parentVorgangId;        // → F18 ChangeRequest (CO only)

    // ── Common fields (all types) ─────────────────────────────
    std::string title;
    std::string description;
    std::string status          { "draft" };  // draft|active|completed|archived
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
    std::vector<F18WorkflowStep> steps;  ///< Lazy-loaded, value type for iteration

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
    //   projectId  : owning F16 (may be empty if taskId set)
    //   title      : display name
    //   type       : one of F18VorgangType::*
    //   taskId     : owning F22 (optional)
    //
    // Returns: saved F18Workflow with generated vorgangId
    // ------------------------------
    static std::shared_ptr<F18Workflow> create(
        const std::string& projectId,
        const std::string& title,
        const std::string& type       = F18VorgangType::GENERIC,
        const std::string& taskId     = "");

    static std::shared_ptr<F18Workflow> loadById(const std::string& id);

    // ------------------------------
    // loadForProject / loadForTask
    // Load all F18 Workflows for a given parent entity.
    // Optional type filter.
    // ------------------------------
    static std::vector<std::shared_ptr<F18Workflow>> loadForProject(
        const std::string& projectId,
        const std::string& type = "");

    static std::vector<std::shared_ptr<F18Workflow>> loadForTask(
        const std::string& taskId,
        const std::string& type = "");

    // ------------------------------
    // loadRecent — last n created across all types
    // ------------------------------
    static std::vector<std::shared_ptr<F18Workflow>> loadRecent(int n = 20);

    // ── Step management ───────────────────────────────────────
    // ------------------------------
    // addStep — add an F18WorkflowStep to this workflow.
    // Always inserts between Init and End bookends.
    // ------------------------------
    std::shared_ptr<F18WorkflowStep> addStep(
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
    bool addNote(const std::string& authorId,
                 const std::string& text,
                 const std::string& noteType = "general");

private:
    void fromRow(const Row& r);
    static Database* db();
};

} // namespace Rosenholz

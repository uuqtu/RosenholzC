#pragma once
#include "../core/OperationResult.h"
// ============================================================
// F77Workflow.h — F77 Freigabe-Workflow Engine
//
// Two object groups:
//
//   DECLARATIVE (admin-time, never changed during execution):
//     F77_WorkflowTemplate      — named template with target state
//     F77_WorkflowTemplateStep  — step definition in a template
//
//   RUNTIME (created from template snapshot on start):
//     F77_Workflow              — running instance; template changes don't affect it
//     F77_WorkflowStep          — running step; each backed by one F18_Operation
//
// Step execution model:
//   - Init step: auto-approved immediately on workflow start
//   - Mid steps: sequential or parallel; each is an F18_Operation of type "f77_step"
//   - Wait conditions: a step can require a separate F18_Operation (any type) to be
//     completed before the step itself can start
//   - End step: auto-approved by engine when all mid-steps are done
//   - On End approval: entity transitions to template's target_state
//
// F77 applies to: F16 (project), F22 (task), F18 (operation), DOK (document)
// ============================================================
#include "../core/Database.h"
#include "../model/f18/F18Operation.h"
#include <string>
#include <vector>
#include <memory>

namespace Rosenholz {

// ── F77_WorkflowTemplateStep ──────────────────────────────────
// SystemAction — what a system step (isSystem=true) does when auto-approved.
// Replaces the fragile `title == "Create DB Objects"` magic string.
// Adding a new system action: add an enum value + a case in executeSystemStep().
enum class SystemAction {
    NONE,               // Not a system step (default)
    COMMIT_DB_OBJECTS,      // Commit all uncommitted DocumentObjects to LMDB archive
    SCAN_UNREGISTERED_FILES, // Scan entity MFS folder; spawn F77_Task per loose file
};

struct F77_WorkflowTemplateStep {
    // Mirror of F77_WorkflowStep.systemAction — set when defining the template.
    std::string tplStepId;
    std::string templateId;
    std::string title;
    std::string description;
    int         sequenceOrder      { 0 };
    bool        isInitialize       { false };
    bool        isFinal            { false };
    std::string executionMode      { "sequential" }; // sequential|parallel
    std::string predecessorTplStepIds;  // comma-sep tpl_step_ids

    // Wait condition: before this step starts, a separate F18_Operation
    // of the given type must be completed.
    std::string waitConditionF18Type;   // e.g. "measure","qualityGate" or ""
    std::string waitConditionTitle;     // title for the auto-spawned F18 Operation

    std::string requiredRole;
    int         slaHours           { 0 };
    bool        autoApprove        { false };
    bool        requiresComment    { false };
    bool        requiresDocument   { false };
    bool        isSystem           { false }; ///< Auto-added step, cannot be edited/deleted
    SystemAction systemAction     { SystemAction::NONE }; ///< what this system step does

    std::string createdAt;
    std::string updatedAt;

    OperationResult save()   const;
    OperationResult remove() const;
    void fromRow(const Row& r);
};

// ── F77_WorkflowTemplate ─────────────────────────────────────
struct F77_WorkflowTemplate {
    std::string templateId;
    std::string name;
    std::string version         { "1.0" };
    std::string description;
    std::string entityTypes;    // comma-sep: "f16,f22,f18,dok"
    std::string targetState     { "released" }; // in_work|pre_released|released|locked|closed
    std::string status          { "active" };   // active|inactive
    std::string createdBy;
    std::string createdAt;
    std::string updatedAt;

    std::vector<F77_WorkflowTemplateStep> steps;

    OperationResult save()   const;
    OperationResult remove() const;
    bool loadSteps();
    void fromRow(const Row& r);

    /// Add a step to this template (admin-time only).
    F77_WorkflowTemplateStep addTemplateStep(
        const std::string& title,
        const std::string& executionMode = "sequential",
        bool isInit  = false,
        bool isFinal = false);

    static std::shared_ptr<F77_WorkflowTemplate> create(
        const std::string& name,
        const std::string& targetState = "released",
        const std::string& entityTypes = "f16,f22,f18,dok");
    static std::shared_ptr<F77_WorkflowTemplate> loadById(const std::string& id);
    static std::vector<std::shared_ptr<F77_WorkflowTemplate>> loadAll();
    static std::vector<std::shared_ptr<F77_WorkflowTemplate>> loadForEntityType(
        const std::string& entityType);

private:
    static Database* db();
};

// ── F77_WorkflowOperation ──────────────────────────────────────
// A single operation within a running F77_Workflow. (Was: F77_WorkflowStep)
struct F77_WorkflowOperation {
    std::string stepId;
    std::string workflowId;
    std::string tplStepId;      // soft ref; snapshot source (template may have changed)
    std::string title;
    int         sequenceOrder   { 0 };
    bool        isInitialize    { false };
    bool        isFinal         { false };
    std::string executionMode   { "sequential" };
    std::vector<std::string> predecessors; // step IDs that must complete before this
    // Serialisation helper (SQL storage): comma-joined IDs
    std::string predecessorsToString() const;
    static std::vector<std::string> predecessorsFromString(const std::string& csv);

    // The F18_Operation that executes this step (NULL for Init/End)
    std::string f18OperationId;

    // Optional wait-condition F18_Operation that must complete before this step starts
    std::string waitF18OperationId;
    std::string waitConditionF18Type;

    std::string status          { "pending" }; // pending|in_progress|approved|rejected|skipped|cancelled
    bool        autoApprove     { false };
    bool        requiresComment { false };
    bool        requiresDocument{ false };
    bool        isSystem        { false }; ///< System-managed, cannot be skipped/deleted
    SystemAction systemAction   { SystemAction::NONE }; ///< what this step does when auto-executed
    std::string completedDate;
    std::string createdAt;
    std::string updatedAt;

    /// True if status is a terminal state.
    bool isComplete() const {
        return status == "approved" || status == "rejected" || status == "skipped";
    }

    /// Sync status from the linked F18_Operation.
    void syncFromF18();

    /// Check if all predecessor steps are complete.
    bool canStart(const std::vector<F77_WorkflowOperation>& allSteps) const;

    OperationResult save()   const;
    OperationResult remove() const;
    void fromRow(const Row& r);

    static std::shared_ptr<F77_WorkflowOperation> loadById(const std::string& id);
    static std::vector<F77_WorkflowOperation>     loadForWorkflow(const std::string& workflowId);

private:
    static Database* db();
};

/// Backward-compat alias so existing code using F77_WorkflowStep still compiles
using F77_WorkflowStep = F77_WorkflowOperation;

// ── F77_Workflow ──────────────────────────────────────────────
struct F77_Workflow {
    std::string workflowId;
    std::string templateId;     // soft ref only; template may have changed
    std::string templateName;   // snapshot of name at start
    std::string entityType;     // f16|f22|f18|dok
    std::string entityId;
    std::string targetState     { "released" }; // snapshot from template
    std::string status          { "active" };   // active|completed|cancelled|locked
    std::string initiatedBy;
    std::string initiatedDate;
    std::string completedDate;
    std::string notes           { "{}" };
    std::string createdAt;
    std::string updatedAt;

    std::vector<F77_WorkflowOperation> steps;

    OperationResult save()   const;
    OperationResult update() const;
    OperationResult remove() const;
    bool loadSteps();
    void fromRow(const Row& r);

    bool isComplete() const;
    std::vector<F77_WorkflowOperation*> readySteps();

    static std::shared_ptr<F77_Workflow> create(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& templateName = "Standard-Freigabe",
        const std::string& targetState  = "released",
        const std::string& initiatedBy  = "system");

    static std::shared_ptr<F77_Workflow> loadById(const std::string& id);
    static std::vector<std::shared_ptr<F77_Workflow>> loadForEntity(
        const std::string& entityType, const std::string& entityId);
    static std::vector<std::shared_ptr<F77_Workflow>> loadActive();

private:
    static Database* db();
};

// ── F77_Engine ────────────────────────────────────────────────
// Stateless coordinator. All methods are static.
class F77_Engine {
public:
    /// Start a F77_Workflow from a named template.
    /// Snapshots the template; template changes won't affect the running workflow.
    static std::shared_ptr<F77_Workflow> startFromTemplate(
        const std::string& templateId,
        const std::string& entityType,
        const std::string& entityId,
        const std::string& initiatedBy = "system");

    /// Start a minimal F77_Workflow (Init → "Freigabe vorbereiten" → End).
    /// Used when no template is configured for the entity type.
    static std::shared_ptr<F77_Workflow> startDefault(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& targetState  = "released",
        const std::string& initiatedBy  = "system");

    /// Engine tick: auto-approve Init/autoApprove steps, sync F18 status,
    /// spawn wait-condition F18 Operations, auto-approve End when all done.
    static bool tick(F77_Workflow& wf);

    /// Fire (approve/reject/skip) a specific step of a running workflow.
    /// Validates prerequisites (canStart, wait condition done), then
    /// sets status on the linked F18_Operation.
    static bool fireStep(
        F77_Workflow& wf,
        const std::string& stepId,
        const std::string& decision,    // approved|rejected|skipped
        const std::string& actorId,
        const std::string& comment = "");

    /// Check if all blocking sub-workflows for an entity are done.
    // ── Workflow attachment (engine-owned, no raw SQL in CLI) ─────
    /// Persist a workflow ID back to the entity (after startDefault/startFromTemplate).
    static void attachWorkflow(const std::string& entityType,
                                const std::string& entityId,
                                const std::string& workflowId);
    /// Remove the workflow ID from the entity (after cancel/complete).
    static void detachWorkflow(const std::string& entityType,
                                const std::string& entityId);
    // Cancel a running workflow: sets status=cancelled, detaches from entity.
    static void cancelWorkflow(F77_Workflow& wf);

    static bool canRelease(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& releaseWorkflowId,
        int& blockerCount);

    /// Lock all open F77 workflows on an entity (except the release WFI).
    static int lockAll(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& releaseWorkflowId,
        bool confirmLock);

    /// Add a manual F77_WorkflowOperation to a running workflow.
    /// Creates a F77_Task (not an F18) as the actionable work item.
    /// The operation blocks End until the F77_Task is closed.
    /// Returns the new stepId, or empty string on failure.
    static std::string addManualOperation(
        F77_Workflow&      wf,
        const std::string& title,
        const std::string& description = "",
        const std::string& assignedTo  = "");

    /// Validate whether a step can be fired — dry-run, no state change.
    /// Returns a human-readable status string starting with "OK:" or "BLOCKED:".
    static std::string validateStep(
        const F77_Workflow& wf,
        const std::string& stepId);

    /// Apply the target state to the entity (called automatically by tick on End).
    static bool applyTargetState(const F77_Workflow& wf);

    /// Seed built-in templates (idempotent).
    static void seedDefaultTemplates();

private:
    static Database* db();
    static void spawnWaitConditionF18(
        F77_WorkflowStep& step,
        const std::string& projectId);
    static bool checkAndComplete(F77_Workflow& wf);
};


struct Version {
    static constexpr int  major       = 4;
    static constexpr int  minor       = 0;
    static constexpr int  patch       = 0;
    static constexpr char tag[]       = "";
    static constexpr char buildDate[] = __DATE__;

    static std::string toString() {
        std::string v = std::to_string(major) + "." +
                        std::to_string(minor) + "." +
                        std::to_string(patch);
        if (tag[0] != '\0') v += std::string("-") + tag;
        return v;
    }
    static std::string full() {
        return "Rosenholz PM v" + toString() + " (built " + buildDate + ")";
    }
};

} // namespace Rosenholz

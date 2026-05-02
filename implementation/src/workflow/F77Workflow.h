#pragma once
#include "../core/OperationResult.h"
// ============================================================
// F77Workflow.h — F77 Freigabe-Workflow Engine
//
// Two object groups:
//
//   DECLARATIVE (admin-time, never changed during execution):
//     F77W_Template      — named template with target state
//     F77W_TemplateStep  — step definition in a template
//
//   RUNTIME (created from template snapshot on start):
//     F77W              — running instance; template changes don't affect it
//     F77W_Operation          — running step; each backed by one F18_Operation
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

// ── F77W_TemplateStep ──────────────────────────────────
// SystemAction — what a system step (isSystem=true) does when auto-approved.
// Replaces the fragile `title == "Create DB Objects"` magic string.
// Adding a new system action: add an enum value + a case in executeSystemStep().
enum class SystemAction {
    NONE,               // Not a system step (default)
    COMMIT_DB_OBJECTS,      // Commit all uncommitted FolderObjects to LMDB archive
    SCAN_UNREGISTERED_FILES, // Scan entity MFS folder; spawn F77Task per loose file
};

// ── WorkflowStatus / StepStatus ──────────────────────────────
enum class WorkflowStatus { ACTIVE = 0, COMPLETED, LOCKED, CANCELLED };
std::string    toString(WorkflowStatus s);
WorkflowStatus workflowStatusFrom(const std::string& s);

enum class StepStatus {
    PENDING = 0, IN_PROGRESS, APPROVED, REJECTED, SKIPPED, CANCELLED };
std::string toString(StepStatus s);
StepStatus  stepStatusFrom(const std::string& s);

// ── Display symbols — returned by model, rendered by UI layer ────────────
// CLI maps to ASCII strings; Qt maps to icons/colors. Both in one place.
enum class StepSymbol {
    PENDING,      ///< step not yet startable or not started
    IN_PROGRESS,  ///< step is being worked
    APPROVED,     ///< step completed successfully
    REJECTED,     ///< step was rejected
    SKIPPED,      ///< step was skipped
    LOCKED,       ///< workflow is locked
};

enum class WorkflowSymbol {
    ACTIVE,
    COMPLETED,
    LOCKED,
    CANCELLED,
};

// Canonical step symbol from StepStatus — single conversion point.
StepSymbol     stepSymbol(StepStatus s);
WorkflowSymbol workflowSymbol(WorkflowStatus s);



struct F77W_TemplateStep {
    // Mirror of F77W_Operation.systemAction — set when defining the template.
    std::string tplStepId;
    std::string templateId;
    std::string title;
    std::string description;
    int         sequenceOrder      { 0 };
    bool        isInitialize       { false };
    bool        isFinal            { false };
    std::string executionMode      { "sequential" }; // sequential|parallel
    std::string predecessorTplStepIds;  // comma-sep tpl_step_ids

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

// ── F77W_Template ─────────────────────────────────────
struct F77W_Template {
    std::string templateId;
    std::string name;
    std::string version         { "1.0" };
    std::string description;
    std::string entityTypes;    // comma-sep: "f22,f18,akt"
    EntityStatus   targetState  { EntityStatus::RELEASED }; ///< target after workflow completes
    TemplateStatus status       { TemplateStatus::ACTIVE };  ///< active|inactive
    std::string createdBy;
    std::string createdAt;
    std::string updatedAt;

    std::vector<F77W_TemplateStep> steps;

    OperationResult save()   const;
    OperationResult remove() const;
    OperationResult deactivate(); ///< Sets status=inactive
    bool loadSteps();
    void fromRow(const Row& r);

    /// Add a step to this template (admin-time only).
    F77W_TemplateStep addTemplateStep(
        const std::string& title,
        const std::string& executionMode = "sequential",
        bool isInit  = false,
        bool isFinal = false);

    static std::shared_ptr<F77W_Template> create(
        const std::string& name,
        EntityStatus targetState = EntityStatus::RELEASED,
        const std::string& entityTypes = "f22,f18,akt");
    static std::shared_ptr<F77W_Template> loadById(const std::string& id);
    static std::vector<std::shared_ptr<F77W_Template>> loadAll();
    static std::vector<std::shared_ptr<F77W_Template>> loadForEntityType(
        const std::string& entityType);

private:
    static Database* db();
};

// ── F77W_Operation ──────────────────────────────────────
// A single operation within a running F77W. (Was: F77W_Operation)
struct F77W_Operation {
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

    StepStatus  status          { StepStatus::PENDING }; ///< step lifecycle
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
        return status == StepStatus::APPROVED
            || status == StepStatus::REJECTED
            || status == StepStatus::SKIPPED;
    }

    /// Check if all predecessor steps are complete.
    bool canStart(const std::vector<F77W_Operation>& allSteps) const;

    OperationResult save()   const;
    OperationResult remove() const;
    void fromRow(const Row& r);

    static std::shared_ptr<F77W_Operation> loadById(const std::string& id);
    static std::vector<F77W_Operation>     loadForWorkflow(const std::string& workflowId);

    StepSymbol     stepSymbol()     const; ///< canonical symbol — UI maps to string/icon

private:
    static Database* db();
};

/// Backward-compat alias so existing code using F77W_Operation still compiles
using F77W_Operation = F77W_Operation;

// ── F77W ──────────────────────────────────────────────
struct F77W {
    std::string workflowId;
    std::string templateId;     // soft ref only; template may have changed
    std::string templateName;   // snapshot of name at start
    std::string entityType;     // f22|f18|akt
    std::string entityId;
    EntityStatus   targetState  { EntityStatus::RELEASED }; ///< target state applied on completion
    WorkflowStatus status       { WorkflowStatus::ACTIVE };  ///< workflow lifecycle
    std::string initiatedBy;
    std::string initiatedDate;
    std::string completedDate;
    std::string notes           { "{}" };
    std::string createdAt;
    std::string updatedAt;

    std::vector<F77W_Operation> steps;

    OperationResult save()   const;
    OperationResult update() const;
    OperationResult remove() const;
    bool loadSteps();
    void fromRow(const Row& r);

    bool isComplete() const { return status == WorkflowStatus::COMPLETED; }
    std::vector<F77W_Operation*> readySteps();

    static std::shared_ptr<F77W> create(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& templateName = "Standard-Freigabe",
        EntityStatus targetState = EntityStatus::RELEASED,
        const std::string& initiatedBy  = "system");

    static std::shared_ptr<F77W> loadById(const std::string& id);
    WorkflowSymbol workflowSymbol() const; ///< canonical symbol — UI maps to string/icon
    static std::vector<std::shared_ptr<F77W>> loadForEntity(
        const std::string& entityType, const std::string& entityId);
    static std::vector<std::shared_ptr<F77W>> loadActive();
    static std::vector<std::shared_ptr<F77W>> loadAll(int limit = 100);

private:
    static Database* db();
};

// ── F77Engine ────────────────────────────────────────────────
// Stateless coordinator. All methods are static.
class F77Engine {
public:
    /// Start a F77W from a named template.
    /// Snapshots the template; template changes won't affect the running workflow.
    static std::shared_ptr<F77W> startFromTemplate(
        const std::string& templateId,
        const std::string& entityType,
        const std::string& entityId,
        const std::string& initiatedBy = "system");

    /// Start a minimal F77W (Init → "Freigabe vorbereiten" → End).
    /// Used when no template is configured for the entity type.
    static std::shared_ptr<F77W> startDefault(
        const std::string& entityType,
        const std::string& entityId,
        EntityStatus targetState = EntityStatus::RELEASED,
        const std::string& initiatedBy  = "system");

    /// Engine tick: auto-approve Init/autoApprove steps, sync F18 status,
    /// spawn wait-condition F18 Operations, auto-approve End when all done.
    static bool tick(F77W& wf);

    /// Fire (approve/reject/skip) a specific step of a running workflow.
    /// Validates prerequisites (canStart, wait condition done), then
    /// sets status on the linked F18_Operation.
    static bool fireStep(
        F77W& wf,
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
    static void cancelWorkflow(F77W& wf);

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


// ── OperationSpec ─────────────────────────────────────────────────────────
// Declarative description of one F77W_Operation in the default chain.
// Adding a new system step = adding one OperationSpec to the entity's list.
struct OperationSpec {
    std::string  title;
    bool         isSystem    { false };
    SystemAction action      { SystemAction::NONE };
    bool         autoApprove { true };
    // If empty: chains from previous step; explicit overrides possible in builder
    std::string  predecessorHint {};  ///< optional: explicit predecessor step ID
};

/// Return the default operation chain for a given entity type.
/// This is the single place to add/remove/reorder system steps per entity.
static std::vector<OperationSpec> defaultOperations(const std::string& entityType);

    /// Add a manual F77W_Operation to a running workflow.
    /// Creates a F77Task (not an F18) as the actionable work item.
    /// The operation blocks End until the F77Task is closed.
    /// Returns the new stepId, or empty string on failure.
    /// Scan an entity's MFS folder for unregistered files.
    /// Each entity type knows its own scan logic. F77 does not know internals.
    static std::vector<std::pair<std::string,std::string>>
        scanLooseFiles(const std::string& entityType, const std::string& entityId);

    static std::string addManualOperation(
        F77W&      wf,
        const std::string& title,
        const std::string& description = "",
        const std::string& assignedTo  = "");

    /// Validate whether a step can be fired — dry-run, no state change.
    /// Returns a human-readable status string starting with "OK:" or "BLOCKED:".
    static std::string validateStep(
        const F77W& wf,
        const std::string& stepId);

    /// Apply the target state to the entity (called automatically by tick on End).
    static bool applyTargetState(const F77W& wf);

    /// Seed built-in templates (idempotent).
    static void seedDefaultTemplates();

private:
    static Database* db();
    static void spawnWaitConditionF18(
        F77W_Operation& step,
        const std::string& projectId);
    static bool checkAndComplete(F77W& wf);
};


struct Version {
    static constexpr int  major       = 8;
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

inline std::ostream& operator<<(std::ostream& os, WorkflowStatus s) {
    return os << toString(s);
}
inline std::ostream& operator<<(std::ostream& os, StepStatus s) {
    return os << toString(s);
}
inline std::ostream& operator<<(std::ostream& os, WorkflowSymbol s) {
    return os << static_cast<int>(s);
}
} // namespace Rosenholz

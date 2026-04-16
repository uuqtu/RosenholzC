#pragma once
// ============================================================
// WorkflowEngine.h  —  Central workflow coordinator
//
// The WorkflowEngine is a stateless service that drives
// WorkflowInstance objects through their lifecycle.
//
// Design decisions:
//   - All methods are static (no instance needed)
//   - Supports three execution types:
//       sequential : actions fire in order-of-sequence
//       parallel   : all actions can be fired in any order
//       free       : no predecessor constraints enforced
//   - Every instance auto-starts with an "Init"
//     action (auto-approved on first tick)
//   - ise-cobra tracking state lives on WorkflowAction so
//     progress is visible alongside approval state
//   - Documents attach at instance or action level via
//     the entity_documents table in documents.db
// ============================================================
// WorkflowEngine.h  —  Workflow Engine
//
// Design:
//   A WorkflowInstance attaches to ANY entity (project/task/doc/...)
//   and contains WorkflowActions (steps).
//
//   Execution types:
//     sequential  — each action has a defined predecessor; must complete
//                   before next can start
//     parallel    — actions can run simultaneously; instance completes
//                   when ALL are approved
//     free        — any action can be executed in any order; instance
//                   completes when all are done
//
//   Every instance auto-starts with an 'initialize' action that is
//   automatically approved on the first engine tick (tick()).
//
//   Actions (steps) carry their own status, due date, assignee,
//   and can have documents attached via documents.db entity_documents.
//
//   The engine is Trackable-aware: each action writes to
//   ise-cobra tracking state is embedded directly in WorkflowAction.
// ============================================================
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/Repository.h"
#include "../model/Document.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace Rosenholz {

// ─────────────────────────────────────────────────────────────
// WorkflowAction  (a single step / gate inside an instance)
// ─────────────────────────────────────────────────────────────
struct WorkflowAction {
    std::string actionId;
    std::string instanceId;
    std::string tplActionId;          // soft ref to template action, may be ""
    std::string title;
    std::string description;
    int         sequenceOrder   { 0 };
    std::string executionType   { "sequential" }; // sequential|parallel|free
    std::string predecessorActionIds; // comma-sep actionIds

    // Status
    std::string status          { "pending" };
    // pending|in_progress|approved|rejected|skipped|cancelled
    bool        isInitialize    { false };
    bool        isFinal         { false };

    // Assignment
    std::string assignedTo;
    std::string requiredRole;
    std::string dueDate;
    std::string startedDate;
    std::string completedDate;
    int         slaHours        { 0 };
    bool        slaBreached     { false };

    // Result
    std::string decision;
    std::string decisionBy;
    std::string decisionDate;
    std::string comment;

    // Flags
    bool requiresComment              { false };
    bool requiresDocument             { false };
    bool autoApprove                  { false };
    bool requiresDecisionLogEntry     { false };
    bool requiresLessonLearnedEntry   { false };

    // ── ise-cobra tracking state ────────────────────────────────
    // Status lifecycle: pending → in_progress → approved/rejected/skipped
    // Extended tracking: planned → focused → due → archived
    std::string trackingStatus  { "planned" };  // planned|focused|due|archived
    std::string plannedDate;     ///< When this action is planned to start
    std::string focusDate;       ///< Date actively worked on (ise-cobra: focused)
    std::string archivedDate;    ///< Completion/archive date
    std::string priority        { "medium" }; // low|medium|high|critical
    std::string assignedToGroup; ///< Team or group ID (complement to assignedTo person)
    std::string progressNote;    ///< Free-text progress update
    int         percentComplete { 0 };

    std::string notes       { "{}" };
    std::string createdAt;
    std::string updatedAt;

    bool save()   const;
    bool remove() const;
    void fromRow(const Row& r);

    bool isComplete() const {
        return status == "approved" || status == "rejected" || status == "skipped";
    }
    bool canStart(const std::vector<WorkflowAction>& allActions) const;
};

// ─────────────────────────────────────────────────────────────
// WorkflowTemplate  (reusable definition)
// ─────────────────────────────────────────────────────────────
struct WorkflowTemplateAction {
    std::string tplActionId;
    std::string templateId;
    std::string title;
    std::string description;
    int         sequenceOrder { 0 };
    std::string executionType { "sequential" };
    std::string predecessorIds;
    std::string requiredRole;
    int         slaHours      { 0 };
    bool        autoApprove   { false };
    bool        isInitialize  { false };
    bool        isFinal       { false };
    bool        requiresDecisionLogEntry  { false };
    bool        requiresLessonLearnedEntry { false };
    bool        requiresComment { false };
    std::string notes;

    bool save()   const;
    void fromRow(const Row& r);
};

struct WorkflowTemplate {
    std::string templateId;
    std::string name;
    std::string version       { "1.0" };
    std::string description;
    std::string entityTypes;  // "project,task,document"
    std::string executionType { "sequential" };
    bool        slaEnforced   { false };
    int         defaultSlaHours { 0 };
    std::string status        { "active" };
    std::string createdBy;
    std::string createdAt;
    std::string updatedAt;

    std::vector<WorkflowTemplateAction> templateActions;

    bool save()   const;
    bool remove() const;
    bool loadTemplateActions();
    void fromRow(const Row& r);

    static std::shared_ptr<WorkflowTemplate> create(
        const std::string& name,
        const std::string& execType = "sequential");
    static std::shared_ptr<WorkflowTemplate> loadById(const std::string& id);
    static std::vector<std::shared_ptr<WorkflowTemplate>> loadAll();
    static std::vector<std::shared_ptr<WorkflowTemplate>> loadForEntityType(
        const std::string& entityType);
};

// ─────────────────────────────────────────────────────────────
// WorkflowInstance
// ─────────────────────────────────────────────────────────────
struct WorkflowParticipant {
    std::string participantId;
    std::string instanceId;
    std::string personId;
    std::string role;
    bool        active         { true };
    std::string delegationFrom;
    std::string addedAt;

    bool save()   const;
    void fromRow(const Row& r);
};

struct WorkflowInstance {
    std::string instanceId;
    std::string templateId;   // "" for ad-hoc
    std::string name;
    std::string entityType;
    std::string entityId;
    std::string executionType { "sequential" };
    std::string status        { "active" };
    std::string initiatedBy;
    std::string initiatedDate;
    std::string dueDate;
    std::string completedDate;
    int         slaHours      { 0 };
    bool        slaBreached   { false };
    std::string slaBreachDate;
    std::string escalatedTo;
    std::string escalatedDate;
    std::string priority      { "medium" };
    std::string outcome;
    std::string notes         { "{}" };
    std::string createdAt;
    std::string updatedAt;

    std::vector<WorkflowAction>      actions;
    std::vector<WorkflowParticipant> participants;

    bool save()   const;
    bool update() const;
    bool remove() const;
    void fromRow(const Row& r);

    bool loadActions();
    bool loadParticipants();

    WorkflowAction* findAction(const std::string& actionId);

    /// Add a progress note to this instance (stored as JSON array in notes field).
    bool addNote(const std::string& authorId, const std::string& text,
                 const std::string& noteType = "general");
    /// Load all notes as a JSON string.
    std::string getNotes() const { return notes; }
    std::vector<WorkflowAction*> readyActions();   // actions whose predecessors are done
    bool isComplete() const;

    static std::shared_ptr<WorkflowInstance> create(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& name,
        const std::string& execType = "sequential");
    static std::shared_ptr<WorkflowInstance> loadById(const std::string& id);
    static std::vector<std::shared_ptr<WorkflowInstance>> loadForEntity(
        const std::string& entityType, const std::string& entityId);
    static std::vector<std::shared_ptr<WorkflowInstance>> loadActive();
    static std::vector<std::shared_ptr<WorkflowInstance>> loadBreached();
};

// ─────────────────────────────────────────────────────────────
// WorkflowEngine  (the stateless coordinator)
// ─────────────────────────────────────────────────────────────
class WorkflowEngine {
public:
    /// Start a new instance from a template.
    /// Automatically creates the 'initialize' action and ticks.
    // ------------------------------
    // Create and start a new WorkflowInstance from a named template.
    //
    // Parameters:
    //   templateId  : ID of an existing WorkflowTemplate (WFD)
    //   entityType  : owning entity type (e.g. "project", "task", "risk")
    //   entityId    : owning entity ID
    //   initiatedBy : Person-ID of the initiator (may be empty)
    //
    // Behavior:
    //   - Loads the template and all its TemplateActions
    //   - Instantiates each action in order, resolving predecessor chains
    //   - Auto-creates an "Init" action (sequenceOrder=0)
    //   - Saves instance + all actions atomically
    //   - Returns nullptr on failure (template not found, DB error)
    // ------------------------------
    // ------------------------------
    // createFinalizeAction (internal helper)
    // Creates the mandatory "End" bookend action for a new instance.
    // Called automatically by startAdHoc() and startFromTemplate().
    // ------------------------------
    static std::shared_ptr<WorkflowAction> createFinalizeAction(
        const std::string& instanceId,
        const std::string& predecessors = "");

    static std::shared_ptr<WorkflowInstance> startFromTemplate(
        const std::string& templateId,
        const std::string& entityType,
        const std::string& entityId,
        const std::string& name,
        const std::string& initiatedBy = "");

    /// Start an ad-hoc instance (no template).
    // ------------------------------
    // Create and start a new ad-hoc WorkflowInstance (no template).
    //
    // Parameters:
    //   entityType    : owning entity type
    //   entityId      : owning entity ID
    //   name          : human-readable instance name
    //   executionType : "sequential" | "parallel" | "free"
    //   initiatedBy   : Person-ID of the initiator (may be empty)
    //
    // Behavior:
    //   - Creates a bare instance with no template reference
    //   - Adds the mandatory "Init" action
    //   - Caller adds further actions via addAction()
    // ------------------------------
    static std::shared_ptr<WorkflowInstance> startAdHoc(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& name,
        const std::string& execType = "sequential",
        const std::string& initiatedBy = "");

    /// Add an action (step) to an existing instance.
    static std::shared_ptr<WorkflowAction> addAction(
        WorkflowInstance& inst,
        const std::string& title,
        const std::string& execType      = "sequential",
        int                seqOrder      = 0,
        const std::string& predecessors  = "",
        const std::string& assignedTo    = "",
        const std::string& dueDate       = "",
        int                slaHours      = 0);

    /// Approve / reject / skip an action.
    // ------------------------------
    // Execute (fire) one action step — the core state transition.
    //
    // Parameters:
    //   inst      : the workflow instance (must be active)
    //   actionId  : ID of the action to fire
    //   decision  : "approved" | "rejected" | "skipped"
    //   actor     : Person-ID performing the action
    //   comment   : optional free-text comment
    //
    // Behavior:
    //   - Validates that the action can start (canStart() check)
    //   - Sets status, decisionBy, decisionDate, completedDate
    //   - For "approved" final actions: checks if all others are done
    //     and marks the instance as "completed"
    //   - Wrapped in a BEGIN/COMMIT transaction; rolls back on error
    //   - Logs the transition to the application logger
    //
    // Returns:
    //   true on success, false if action cannot start or DB error
    // ------------------------------
    static bool fireAction(
        WorkflowInstance& inst,
        const std::string& actionId,
        const std::string& decision,     // "approved"|"rejected"|"skipped"
        const std::string& actorId,
        const std::string& comment       = "");

    /// Engine tick: auto-approve 'initialize' actions, check SLA, complete instances.
    /// Call periodically or after each user interaction.
    // ------------------------------
    // Advance engine state without user input.
    //
    // Parameters:
    //   inst : the workflow instance to tick
    //
    // Behavior:
    //   - Auto-approves the "Init" action if still pending
    //   - Checks all actions for SLA breach and marks slaBreached=true
    //   - Does NOT fire non-auto actions (user must call fireAction)
    //   - Safe to call repeatedly; idempotent for completed instances
    // ------------------------------
    static bool tick(WorkflowInstance& inst);

    /// Escalate an instance.
    // ------------------------------
    // Escalate a workflow instance to a named person.
    //
    // Parameters:
    //   inst     : the active workflow instance
    //   targetId : Person-ID to escalate to
    //   reason   : free-text escalation reason (stored in notes)
    //
    // Behavior:
    //   - Sets escalatedTo and escalatedDate on the instance
    //   - Appends a note entry via addNote()
    // ------------------------------
    static bool escalate(WorkflowInstance& inst, const std::string& escalateTo,
                         const std::string& reason = "");

    /// Add a participant to an instance.
    // ------------------------------
    // Add a participant to a workflow instance.
    //
    // Parameters:
    //   inst     : the target instance
    //   personId : Person-ID to add
    //   role     : "approver" | "reviewer" | "watcher" | "informed" | "delegate"
    //
    // Behavior:
    //   - Inserts into workflow_participants (unique per instance+person)
    //   - Use "approver" for people who can call fireAction()
    //   - Use "watcher" for read-only observers
    // ------------------------------
    static bool addParticipant(WorkflowInstance& inst,
                               const std::string& personId,
                               const std::string& role);

    /// Attach a document to a workflow instance (instance-level).
    // ------------------------------
    // Attach an existing Document to the workflow instance as a whole.
    //
    // Parameters:
    //   instanceId   : the target workflow instance
    //   documentId   : existing Document ID (XV/DOK/...)
    //   relationship : "attached" | "mandatory" | "reference"
    //   notes        : optional free-text note
    //
    // Behavior:
    //   - Inserts into entity_documents (entity_type = "workflow_instance")
    //   - Does not copy files; the document must already exist in MFS
    // ------------------------------
    static bool attachDocumentToInstance(const std::string& instanceId,
                                          const std::string& documentId,
                                          const std::string& relationship = "attached",
                                          const std::string& notes = "");

    /// Attach a document to a specific action step.
    // ------------------------------
    // Attach an existing Document to a specific action step.
    //
    // Parameters:
    //   actionId     : the target action
    //   documentId   : existing Document ID
    //   relationship : "attached" | "mandatory" | "reference"
    //
    // Behavior:
    //   - Inserts into entity_documents (entity_type = "workflow_action")
    //   - Use "mandatory" to signal that the document is required
    //     before the step can be approved
    // ------------------------------
    static bool attachDocumentToAction(const std::string& actionId,
                                       const std::string& documentId,
                                       const std::string& relationship = "attached");

    /// Load all documents attached to a workflow instance.
    // ------------------------------
    // Load all documents attached to a workflow instance.
    //
    // Includes:
    //   - Documents attached at the instance level
    //   - Documents attached to any action of the instance
    //
    // Returns:
    //   Combined list ordered by linked_at (oldest first).
    // ------------------------------
    static std::vector<std::shared_ptr<Document>> loadDocumentsForInstance(
        const std::string& instanceId);

    /// Load all documents attached to a specific action.
    // ------------------------------
    // Load all documents attached to a specific action step.
    //
    // Returns:
    //   Documents ordered by linked_at (oldest first).
    //   Empty vector if none found or DB unavailable.
    // ------------------------------
    static std::vector<std::shared_ptr<Document>> loadDocumentsForAction(
        const std::string& actionId);

    /// Create a decision-log entry as part of a final action step.
    // ------------------------------
    // Create a DecisionLog entry from a completed final action.
    //
    // Called automatically by fireAction() for actions where
    // requiresDecisionLogEntry == true.
    //
    // Parameters:
    //   actionId   : the completed action
    //   entityType : owning entity type (e.g. "project")
    //   entityId   : owning entity ID
    //   title      : decision title
    //   rationale  : decision reasoning
    //
    // Behavior:
    //   - Finds or creates the DL header for the entity
    //   - Appends a DLEntry with decision text and metadata
    // ------------------------------
    static bool createDecisionLogEntry(const std::string& actionId,
                                       const std::string& entityType,
                                       const std::string& entityId,
                                       const std::string& title,
                                       const std::string& rationale);

    /// Create a lessons-learned entry as part of an action step.
    static bool createLessonLearnedEntry(const std::string& actionId,
                                          const std::string& entityType,
                                          const std::string& entityId,
                                          const std::string& title,
                                          const std::string& description);

    /// Standard workflow templates (built-in)
    // ------------------------------
    // Seed the database with built-in workflow templates.
    //
    // Templates created (idempotent — skips if already present):
    //   "Standardgenehmigung"  : 4-step sequential approval chain
    //   "Projektabschluss"     : 4-step sequential closure with
    //                            LL + DL auto-creation on final step
    //
    // Called once at startup from main_cli.cpp::run().
    // Safe to call multiple times.
    // ------------------------------
    static void createStandardTemplates();

    /// Sync entity's workflow fields (current state etc.)
    static bool syncEntityWorkflowFields(const WorkflowInstance& inst);

    // ── Entity Lifecycle ─────────────────────────────────────────
    // ------------------------------
    // createMainWorkflow
    //
    // Creates and starts the controlling Main WorkflowInstance for an entity.
    // Called automatically by F16/F22/F18 create factories.
    // Sets entity.mainWorkflowId and entity.status = "in_work".
    //
    // Parameters:
    //   entityType  : "project" | "task" | "f18"
    //   entityId    : the entity's primary key
    //   entityTitle : used as the WFI name
    // Returns: the new WorkflowInstance (already saved)
    // ------------------------------
    static std::shared_ptr<WorkflowInstance> createMainWorkflow(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& entityTitle);

    // ------------------------------
    // canReleaseEntity
    //
    // Returns true if all WorkflowInstances attached to the entity
    // are either completed or locked (no pending/active/in_progress WFIs).
    // The Main WFI itself is excluded from the check.
    //
    // Parameters:
    //   entityType    : "project" | "task" | "f18"
    //   entityId      : the entity's primary key
    //   mainWfiId     : the Main WFI ID to exclude from the check
    //   blockerCount  : set to number of blocking WFIs (out param)
    // ------------------------------
    static bool canReleaseEntity(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& mainWfiId,
        int& blockerCount);

    // ------------------------------
    // lockAllOpenWorkflows
    //
    // Force-transitions all non-completed WFIs on an entity to "locked".
    // Requires explicit confirmation (confirmLock must be true).
    // Used as a precondition for releasing an entity whose sub-WFIs are still open.
    //
    // Parameters:
    //   entityType  : "project" | "task" | "f18"
    //   entityId    : the entity's primary key
    //   mainWfiId   : the Main WFI ID to skip
    //   confirmLock : must be true (caller must ask the user)
    // Returns: number of WFIs locked, or -1 on error
    // ------------------------------
    static int lockAllOpenWorkflows(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& mainWfiId,
        bool confirmLock);

    // ------------------------------
    // releaseEntity
    //
    // Sets an entity's status to "released" in the database.
    // Called automatically when the Main WFI's End step is fired.
    // Released entities: no new WFIs, no new child objects.
    //
    // Parameters:
    //   entityType : "project" | "task" | "f18"
    //   entityId   : the entity's primary key
    // ------------------------------
    static bool releaseEntity(
        const std::string& entityType,
        const std::string& entityId);

    // ------------------------------
    // searchInstances
    //
    // Filter workflow instances by multiple criteria.
    // Parameters:
    //   entityType   : entity type filter (empty = all)
    //   status       : "active"|"completed"|"cancelled"|"" = all
    //   nameContains : case-insensitive substring match on name
    //   slaOnly      : true = only SLA-breached instances
    // Returns:
    //   Matching instances, ordered newest first.
    // ------------------------------
    static std::vector<std::shared_ptr<WorkflowInstance>> searchInstances(
        const std::string& entityType   = "",
        const std::string& status       = "",
        const std::string& nameContains = "",
        bool slaOnly = false);

private:
    static std::shared_ptr<WorkflowAction> createInitializeAction(
        const std::string& instanceId);
    static bool checkAndCompleteInstance(WorkflowInstance& inst);
    static bool checkSLA(WorkflowAction& action);
    static std::string nowIso();
};

// ─────────────────────────────────────────────────────────────
// Version information
// ─────────────────────────────────────────────────────────────
struct Version {
    static constexpr int  major       = 2;
    static constexpr int  minor       = 0;
    static constexpr int  patch       = 0;
    static constexpr char tag[]       = "refactor";
    static constexpr char buildDate[] = __DATE__;

    static std::string toString() {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch) + "-" + tag;
    }

    static std::string full() {
        return "Rosenholz PM v" + toString() + " (built " + buildDate + ")";
    }
};

} // namespace Rosenholz

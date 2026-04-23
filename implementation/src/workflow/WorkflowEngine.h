#pragma once
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
//   trackable_items so progress is visible alongside other tracking.
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
    static std::shared_ptr<WorkflowInstance> startFromTemplate(
        const std::string& templateId,
        const std::string& entityType,
        const std::string& entityId,
        const std::string& name,
        const std::string& initiatedBy = "");

    /// Start an ad-hoc instance (no template).
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
    static bool fireAction(
        WorkflowInstance& inst,
        const std::string& actionId,
        const std::string& decision,     // "approved"|"rejected"|"skipped"
        const std::string& actorId,
        const std::string& comment       = "");

    /// Engine tick: auto-approve 'initialize' actions, check SLA, complete instances.
    /// Call periodically or after each user interaction.
    static bool tick(WorkflowInstance& inst);

    /// Escalate an instance.
    static bool escalate(WorkflowInstance& inst, const std::string& escalateTo,
                         const std::string& reason = "");

    /// Add a participant to an instance.
    static bool addParticipant(WorkflowInstance& inst,
                               const std::string& personId,
                               const std::string& role);

    /// Attach a document to a workflow instance (instance-level).
    static bool attachDocumentToInstance(const std::string& instanceId,
                                          const std::string& documentId,
                                          const std::string& relationship = "attached",
                                          const std::string& notes = "");

    /// Attach a document to a specific action step.
    static bool attachDocumentToAction(const std::string& actionId,
                                       const std::string& documentId,
                                       const std::string& relationship = "attached");

    /// Load all documents attached to a workflow instance.
    static std::vector<std::shared_ptr<Document>> loadDocumentsForInstance(
        const std::string& instanceId);

    /// Load all documents attached to a specific action.
    static std::vector<std::shared_ptr<Document>> loadDocumentsForAction(
        const std::string& actionId);

    /// Create a decision-log entry as part of a final action step.
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
    static void createStandardTemplates();

    /// Sync entity's workflow fields (current state etc.)
    static bool syncEntityWorkflowFields(const WorkflowInstance& inst);

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

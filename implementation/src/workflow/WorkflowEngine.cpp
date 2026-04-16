// ============================================================
// WorkflowEngine.cpp
// ============================================================
#include "WorkflowEngine.h"
#include "../model/f18/F18Workflow.h"
#include "../model/Document.h"
#include "../model/Utils.h"
#include <sstream>
#include <map>
#include <algorithm>

namespace Rosenholz {

static Database* wfDB() { return DatabasePool::instance().get("workflow"); }
static Database* docDB() { return DatabasePool::instance().get("documents"); }
static Database* repDB() { return DatabasePool::instance().get("f18"); }  // reporting.db replaced by f18.db

// ═════════════════════════════════════════════════════════════
// WorkflowAction
// ═════════════════════════════════════════════════════════════
void WorkflowAction::fromRow(const Row& r) {
    actionId            = rowGet(r,"action_id");
    instanceId          = rowGet(r,"instance_id");
    tplActionId         = rowGet(r,"tpl_action_id");
    title               = rowGet(r,"title");
    description         = rowGet(r,"description");
    sequenceOrder       = rowGetInt(r,"sequence_order");
    executionType       = rowGetOr(r,"execution_type","sequential");
    predecessorActionIds= rowGet(r,"predecessor_action_ids");
    status              = rowGetOr(r,"status","pending");
    isInitialize        = rowGetBool(r,"is_initialize");
    isFinal             = rowGetBool(r,"is_final");
    assignedTo          = rowGet(r,"assigned_to");
    requiredRole        = rowGet(r,"required_role");
    dueDate             = rowGet(r,"due_date");
    startedDate         = rowGet(r,"started_date");
    completedDate       = rowGet(r,"completed_date");
    slaHours            = rowGetInt(r,"sla_hours");
    slaBreached         = rowGetBool(r,"sla_breached");
    decision            = rowGet(r,"decision");
    decisionBy          = rowGet(r,"decision_by");
    decisionDate        = rowGet(r,"decision_date");
    comment             = rowGet(r,"comment");
    requiresComment     = rowGetBool(r,"requires_comment");
    requiresDocument    = rowGetBool(r,"requires_document");
    autoApprove         = rowGetBool(r,"auto_approve");
    requiresDecisionLogEntry   = rowGetBool(r,"requires_decision_log_entry");
    requiresLessonLearnedEntry = rowGetBool(r,"requires_lesson_learned_entry");
    notes               = rowGetOr(r,"notes","{}");
    createdAt           = rowGet(r,"created_at");
    updatedAt           = rowGet(r,"updated_at");
    trackingStatus      = rowGetOr(r,"tracking_status","planned");
    plannedDate         = rowGet(r,"planned_date");
    focusDate           = rowGet(r,"focus_date");
    archivedDate        = rowGet(r,"archived_date");
    priority            = rowGetOr(r,"priority","medium");
    assignedToGroup     = rowGet(r,"assigned_to_group");
    progressNote        = rowGet(r,"progress_note");
    percentComplete     = rowGetInt(r,"percent_complete");
}

// ============================================================
// WorkflowAction CRUD
// ============================================================

// ------------------------------
// Persist the action to workflow.db.
// Uses INSERT OR REPLACE (upsert) so this serves as both
// create and update.  All ise-cobra tracking fields included.
// ------------------------------
bool WorkflowAction::save() const {
    auto* db = wfDB(); if (!db) return false;
    return db->exec(R"SQL(
        INSERT OR REPLACE INTO workflow_actions
        (action_id,instance_id,tpl_action_id,title,description,
         sequence_order,execution_type,predecessor_action_ids,
         status,is_initialize,is_final,
         assigned_to,required_role,due_date,started_date,completed_date,
         sla_hours,sla_breached,decision,decision_by,decision_date,comment,
         requires_comment,requires_document,auto_approve,
         requires_decision_log_entry,requires_lesson_learned_entry,
         tracking_status,planned_date,focus_date,archived_date,
         priority,assigned_to_group,progress_note,percent_complete,
         notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(actionId), BindParam::text(instanceId), textOrNull(tplActionId),
        BindParam::text(title), textOrNull(description),
        BindParam::int64(sequenceOrder), BindParam::text(executionType),
        textOrNull(predecessorActionIds),
        BindParam::text(status),
        BindParam::int64(isInitialize?1:0), BindParam::int64(isFinal?1:0),
        textOrNull(assignedTo), textOrNull(requiredRole),
        textOrNull(dueDate), textOrNull(startedDate), textOrNull(completedDate),
        BindParam::int64(slaHours), BindParam::int64(slaBreached?1:0),
        textOrNull(decision), textOrNull(decisionBy), textOrNull(decisionDate),
        textOrNull(comment),
        BindParam::int64(requiresComment?1:0),
        BindParam::int64(requiresDocument?1:0),
        BindParam::int64(autoApprove?1:0),
        BindParam::int64(requiresDecisionLogEntry?1:0),
        BindParam::int64(requiresLessonLearnedEntry?1:0),
        BindParam::text(trackingStatus),
        textOrNull(plannedDate), textOrNull(focusDate), textOrNull(archivedDate),
        BindParam::text(priority.empty()?"medium":priority),
        textOrNull(assignedToGroup), textOrNull(progressNote),
        BindParam::int64(percentComplete),
        BindParam::text(notes.empty()?"{}":notes),
        BindParam::text(createdAt), BindParam::text(updatedAt)
    });
}

bool WorkflowAction::remove() const {
    auto* db = wfDB(); if (!db) return false;
    return db->exec("DELETE FROM workflow_actions WHERE action_id=?;",
                    {BindParam::text(actionId)});
}

bool WorkflowAction::canStart(const std::vector<WorkflowAction>& allActions) const {
    if (status != "pending" && status != "in_progress") return false;
    if (predecessorActionIds.empty()) return true;

    // Parse comma-separated predecessor IDs
    std::istringstream ss(predecessorActionIds);
    std::string predId;
    while (std::getline(ss, predId, ',')) {
        while (!predId.empty() && predId.front() == ' ') predId.erase(predId.begin());
        if (predId.empty()) continue;
        // Find the predecessor in the action list
        bool predDone = false;
        for (auto& a : allActions) {
            if (a.actionId == predId) {
                predDone = a.isComplete();
                break;
            }
        }
        if (!predDone) return false;
    }
    return true;
}

// ═════════════════════════════════════════════════════════════
// WorkflowTemplateAction
// ═════════════════════════════════════════════════════════════
void WorkflowTemplateAction::fromRow(const Row& r) {
    tplActionId     = rowGet(r,"tpl_action_id");
    templateId      = rowGet(r,"template_id");
    title           = rowGet(r,"title");
    description     = rowGet(r,"description");
    sequenceOrder   = rowGetInt(r,"sequence_order");
    executionType   = rowGetOr(r,"execution_type","sequential");
    predecessorIds  = rowGet(r,"predecessor_ids");
    requiredRole    = rowGet(r,"required_role");
    slaHours        = rowGetInt(r,"sla_hours");
    autoApprove     = rowGetBool(r,"auto_approve");
    isInitialize    = rowGetBool(r,"is_initialize");
    isFinal         = rowGetBool(r,"is_final");
    requiresDecisionLogEntry   = rowGetBool(r,"requires_decision_log_entry");
    requiresLessonLearnedEntry = rowGetBool(r,"requires_lesson_learned_entry");
    requiresComment = rowGetBool(r,"requires_comment");
    notes           = rowGet(r,"notes");
}

bool WorkflowTemplateAction::save() const {
    auto* db = wfDB(); if (!db) return false;
    return db->exec(R"SQL(
        INSERT OR REPLACE INTO workflow_template_actions
        (tpl_action_id,template_id,title,description,sequence_order,
         execution_type,predecessor_ids,required_role,sla_hours,
         auto_approve,is_initialize,is_final,
         requires_decision_log_entry,requires_lesson_learned_entry,
         requires_comment,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(tplActionId), BindParam::text(templateId),
        BindParam::text(title), textOrNull(description),
        BindParam::int64(sequenceOrder), BindParam::text(executionType),
        textOrNull(predecessorIds), textOrNull(requiredRole),
        BindParam::int64(slaHours),
        BindParam::int64(autoApprove?1:0), BindParam::int64(isInitialize?1:0),
        BindParam::int64(isFinal?1:0),
        BindParam::int64(requiresDecisionLogEntry?1:0),
        BindParam::int64(requiresLessonLearnedEntry?1:0),
        BindParam::int64(requiresComment?1:0),
        textOrNull(notes)
    });
}

// ═════════════════════════════════════════════════════════════
// WorkflowTemplate
// ═════════════════════════════════════════════════════════════
void WorkflowTemplate::fromRow(const Row& r) {
    templateId      = rowGet(r,"template_id");
    name            = rowGet(r,"name");
    version         = rowGetOr(r,"version","1.0");
    description     = rowGet(r,"description");
    entityTypes     = rowGet(r,"entity_types");
    executionType   = rowGetOr(r,"execution_type","sequential");
    slaEnforced     = rowGetBool(r,"sla_enforced");
    defaultSlaHours = rowGetInt(r,"default_sla_hours");
    status          = rowGetOr(r,"status","active");
    createdBy       = rowGet(r,"created_by");
    createdAt       = rowGet(r,"created_at");
    updatedAt       = rowGet(r,"updated_at");
}

bool WorkflowTemplate::loadTemplateActions() {
    auto* db = wfDB(); if (!db) return false;
    templateActions.clear();
    auto rows = db->query(
        "SELECT * FROM workflow_template_actions WHERE template_id=? ORDER BY sequence_order;",
        {BindParam::text(templateId)});
    for (auto& r : rows) {
        WorkflowTemplateAction a;
        a.fromRow(r);
        templateActions.push_back(a);
    }
    return true;
}

bool WorkflowTemplate::save() const {
    auto* db = wfDB(); if (!db) return false;
    bool ok = db->exec(R"SQL(
        INSERT OR REPLACE INTO workflow_templates
        (template_id,name,version,description,entity_types,
         execution_type,sla_enforced,default_sla_hours,status,
         created_by,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(templateId), BindParam::text(name),
        BindParam::text(version), textOrNull(description),
        textOrNull(entityTypes), BindParam::text(executionType),
        BindParam::int64(slaEnforced?1:0), BindParam::int64(defaultSlaHours),
        BindParam::text(status), textOrNull(createdBy),
        BindParam::text(createdAt), BindParam::text(updatedAt)
    });
    for (auto& a : templateActions) ok &= a.save();
    return ok;
}

bool WorkflowTemplate::remove() const {
    auto* db = wfDB(); if (!db) return false;
    db->exec("DELETE FROM workflow_template_actions WHERE template_id=?;",
             {BindParam::text(templateId)});
    return db->exec("DELETE FROM workflow_templates WHERE template_id=?;",
                    {BindParam::text(templateId)});
}

std::shared_ptr<WorkflowTemplate> WorkflowTemplate::create(
    const std::string& name, const std::string& execType)
{
    auto t = std::make_shared<WorkflowTemplate>();
    t->templateId   = genId("WFD");
    t->name         = name;
    t->executionType= execType;
    t->createdAt    = nowIso();
    t->updatedAt    = nowIso();
    return t;
}

std::shared_ptr<WorkflowTemplate> WorkflowTemplate::loadById(const std::string& id) {
    auto* db = wfDB(); if (!db) return nullptr;
    auto rows = db->query("SELECT * FROM workflow_templates WHERE template_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) return nullptr;
    auto t = std::make_shared<WorkflowTemplate>();
    t->fromRow(rows[0]);
    t->loadTemplateActions();
    return t;
}

std::vector<std::shared_ptr<WorkflowTemplate>> WorkflowTemplate::loadAll() {
    auto* db = wfDB();
    std::vector<std::shared_ptr<WorkflowTemplate>> result;
    if (!db) return result;
    for (auto& r : db->query("SELECT * FROM workflow_templates ORDER BY name;")) {
        auto t = std::make_shared<WorkflowTemplate>();
        t->fromRow(r);
        result.push_back(t);
    }
    return result;
}

std::vector<std::shared_ptr<WorkflowTemplate>> WorkflowTemplate::loadForEntityType(
    const std::string& et)
{
    auto* db = wfDB();
    std::vector<std::shared_ptr<WorkflowTemplate>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM workflow_templates WHERE status='active' "
        "AND (entity_types LIKE ? OR entity_types IS NULL OR entity_types='') ORDER BY name;",
        {BindParam::text("%" + et + "%")});
    for (auto& r : rows) {
        auto t = std::make_shared<WorkflowTemplate>();
        t->fromRow(r);
        result.push_back(t);
    }
    return result;
}

// ═════════════════════════════════════════════════════════════
// WorkflowParticipant
// ═════════════════════════════════════════════════════════════
void WorkflowParticipant::fromRow(const Row& r) {
    participantId  = rowGet(r,"participant_id");
    instanceId     = rowGet(r,"instance_id");
    personId       = rowGet(r,"person_id");
    role           = rowGet(r,"role");
    active         = rowGetBool(r,"active");
    delegationFrom = rowGet(r,"delegation_from");
    addedAt        = rowGet(r,"added_at");
}

bool WorkflowParticipant::save() const {
    auto* db = wfDB(); if (!db) return false;
    return db->exec(R"SQL(
        INSERT OR REPLACE INTO workflow_participants
        (participant_id,instance_id,person_id,role,active,delegation_from,added_at)
        VALUES(?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(participantId), BindParam::text(instanceId),
        BindParam::text(personId), BindParam::text(role),
        BindParam::int64(active?1:0), textOrNull(delegationFrom),
        BindParam::text(addedAt)
    });
}

// ═════════════════════════════════════════════════════════════
// WorkflowInstance
// ═════════════════════════════════════════════════════════════
void WorkflowInstance::fromRow(const Row& r) {
    instanceId     = rowGet(r,"instance_id");
    templateId     = rowGet(r,"template_id");
    name           = rowGet(r,"name");
    entityType     = rowGet(r,"entity_type");
    entityId       = rowGet(r,"entity_id");
    executionType  = rowGetOr(r,"execution_type","sequential");
    status         = rowGetOr(r,"status","active");
    initiatedBy    = rowGet(r,"initiated_by");
    initiatedDate  = rowGet(r,"initiated_date");
    dueDate        = rowGet(r,"due_date");
    completedDate  = rowGet(r,"completed_date");
    slaHours       = rowGetInt(r,"sla_hours");
    slaBreached    = rowGetBool(r,"sla_breached");
    slaBreachDate  = rowGet(r,"sla_breach_date");
    escalatedTo    = rowGet(r,"escalated_to");
    escalatedDate  = rowGet(r,"escalated_date");
    priority       = rowGetOr(r,"priority","medium");
    outcome        = rowGet(r,"outcome");
    notes          = rowGetOr(r,"notes","{}");
    createdAt      = rowGet(r,"created_at");
    updatedAt      = rowGet(r,"updated_at");
}

// ============================================================
// WorkflowInstance CRUD
// ============================================================

// ------------------------------
// Persist the instance to workflow.db (upsert).
// Does NOT save child actions or participants —
// call save() on each WorkflowAction separately.
// ------------------------------
bool WorkflowInstance::save() const {
    auto* db = wfDB(); if (!db) return false;
    return db->exec(R"SQL(
        INSERT OR REPLACE INTO workflow_instances
        (instance_id,template_id,name,entity_type,entity_id,execution_type,
         status,initiated_by,initiated_date,due_date,completed_date,
         sla_hours,sla_breached,sla_breach_date,escalated_to,escalated_date,
         priority,outcome,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(instanceId), textOrNull(templateId),
        BindParam::text(name), BindParam::text(entityType), BindParam::text(entityId),
        BindParam::text(executionType), BindParam::text(status),
        textOrNull(initiatedBy), BindParam::text(initiatedDate),
        textOrNull(dueDate), textOrNull(completedDate),
        BindParam::int64(slaHours), BindParam::int64(slaBreached?1:0),
        textOrNull(slaBreachDate), textOrNull(escalatedTo), textOrNull(escalatedDate),
        BindParam::text(priority), textOrNull(outcome),
        BindParam::text(notes.empty()?"{}":notes),
        BindParam::text(createdAt), BindParam::text(updatedAt)
    });
}

bool WorkflowInstance::update() const { return save(); }
bool WorkflowInstance::remove() const {
    auto* db = wfDB(); if (!db) return false;
    db->exec("DELETE FROM workflow_actions WHERE instance_id=?;",{BindParam::text(instanceId)});
    db->exec("DELETE FROM workflow_participants WHERE instance_id=?;",{BindParam::text(instanceId)});
    db->exec("DELETE FROM workflow_sla_log WHERE instance_id=?;",{BindParam::text(instanceId)});
    return db->exec("DELETE FROM workflow_instances WHERE instance_id=?;",{BindParam::text(instanceId)});
}

bool WorkflowInstance::loadActions() {
    auto* db = wfDB(); if (!db) return false;
    actions.clear();
    auto rows = db->query(
        "SELECT * FROM workflow_actions WHERE instance_id=? ORDER BY sequence_order,created_at;",
        {BindParam::text(instanceId)});
    for (auto& r : rows) {
        WorkflowAction a;
        a.fromRow(r);
        actions.push_back(a);
    }
    return true;
}

bool WorkflowInstance::loadParticipants() {
    auto* db = wfDB(); if (!db) return false;
    participants.clear();
    auto rows = db->query(
        "SELECT * FROM workflow_participants WHERE instance_id=?;",
        {BindParam::text(instanceId)});
    for (auto& r : rows) {
        WorkflowParticipant p;
        p.fromRow(r);
        participants.push_back(p);
    }
    return true;
}

// ------------------------------
// addNote
//
// Parameters:
//   authorId             : Person-ID of the note author
//   text                 : note content (plain text)
//   noteType             : general|decision|action|blocker
//
// Behavior:
//   Appends a JSON object to the notes field
//   Format: [{id, author, type, text, at}, …]
//   Calls update() to persist immediately
//
// Returns:
//   true on success
// ------------------------------
bool WorkflowInstance::addNote(const std::string& authorId,
                                const std::string& text,
                                const std::string& noteType) {
    std::string noteId = genId("NOTE");
    std::string entry  = "{\"id\":\"" + noteId + "\",\"author\":\"" + authorId +
                         "\",\"type\":\"" + noteType + "\",\"text\":\"" +
                         text + "\",\"at\":\"" + nowIso() + "\"}";
    if (notes == "{}" || notes.empty()) notes = "[]";
    if (!notes.empty() && notes.back() == ']') {
        notes.pop_back();
        if (notes.size() > 1) notes += ",";
        notes += entry + "]";
    } else {
        notes = "[" + entry + "]";
    }
    return update();
}

WorkflowAction* WorkflowInstance::findAction(const std::string& aid) {
    for (auto& a : actions)
        if (a.actionId == aid) return &a;
    return nullptr;
}

std::vector<WorkflowAction*> WorkflowInstance::readyActions() {
    std::vector<WorkflowAction*> ready;
    for (auto& a : actions) {
        if (a.isComplete() || a.status == "cancelled") continue;
        if (a.canStart(actions)) ready.push_back(&a);
    }
    return ready;
}

bool WorkflowInstance::isComplete() const {
    for (auto& a : actions)
        if (!a.isComplete() && a.status != "cancelled") return false;
    return !actions.empty();
}

std::shared_ptr<WorkflowInstance> WorkflowInstance::create(
    const std::string& et, const std::string& eid,
    const std::string& name_, const std::string& exec)
{
    auto i = std::make_shared<WorkflowInstance>();
    i->instanceId    = genId("WFI");
    i->entityType    = et;
    i->entityId      = eid;
    i->name          = name_;
    i->executionType = exec;
    i->initiatedDate = nowIso();
    i->createdAt     = nowIso();
    i->updatedAt     = nowIso();
    return i;
}

std::shared_ptr<WorkflowInstance> WorkflowInstance::loadById(const std::string& id) {
    auto* db = wfDB(); if (!db) return nullptr;
    auto rows = db->query("SELECT * FROM workflow_instances WHERE instance_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) return nullptr;
    auto inst = std::make_shared<WorkflowInstance>();
    inst->fromRow(rows[0]);
    inst->loadActions();
    inst->loadParticipants();
    return inst;
}

std::vector<std::shared_ptr<WorkflowInstance>> WorkflowInstance::loadForEntity(
    const std::string& et, const std::string& eid)
{
    auto* db = wfDB();
    std::vector<std::shared_ptr<WorkflowInstance>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM workflow_instances WHERE entity_type=? AND entity_id=? "
        "ORDER BY initiated_date DESC;",
        {BindParam::text(et), BindParam::text(eid)});
    for (auto& r : rows) {
        auto inst = std::make_shared<WorkflowInstance>();
        inst->fromRow(r);
        inst->loadActions();
        result.push_back(inst);
    }
    return result;
}

std::vector<std::shared_ptr<WorkflowInstance>> WorkflowInstance::loadActive() {
    auto* db = wfDB();
    std::vector<std::shared_ptr<WorkflowInstance>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM workflow_instances WHERE status='active' ORDER BY initiated_date DESC;");
    for (auto& r : rows) {
        auto inst = std::make_shared<WorkflowInstance>();
        inst->fromRow(r);
        inst->loadActions();
        result.push_back(inst);
    }
    return result;
}

std::vector<std::shared_ptr<WorkflowInstance>> WorkflowInstance::loadBreached() {
    auto* db = wfDB();
    std::vector<std::shared_ptr<WorkflowInstance>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM workflow_instances WHERE sla_breached=1 OR "
        "(status='active' AND due_date < datetime('now')) ORDER BY due_date;");
    for (auto& r : rows) {
        auto inst = std::make_shared<WorkflowInstance>();
        inst->fromRow(r);
        result.push_back(inst);
    }
    return result;
}

// ═════════════════════════════════════════════════════════════
// WorkflowEngine
// ═════════════════════════════════════════════════════════════
std::string WorkflowEngine::nowIso() { return Rosenholz::nowIso(); }

std::shared_ptr<WorkflowAction> WorkflowEngine::createInitializeAction(
    const std::string& instanceId)
{
    auto a = std::make_shared<WorkflowAction>();
    a->actionId      = genId("WFA");
    a->instanceId    = instanceId;
    a->title         = "Init";
    a->description   = "Automatischer Startschritt — wird beim ersten Tick genehmigt.";
    a->sequenceOrder = 0;
    a->executionType = "sequential";
    a->isInitialize  = true;
    a->autoApprove   = true;
    a->status        = "pending";
    a->createdAt     = nowIso();
    a->updatedAt     = nowIso();
    a->save();
    LOG_INFO("[WFEngine] Initialize action created for instance " + instanceId);
    return a;
}

// ------------------------------
// createFinalizeAction
//
// Creates the mandatory "End" finalization step.
// The End action is always the last action in any workflow.
// It has isFinal=true and is NOT auto-approved — a user or
// the system must explicitly fire it to close the workflow.
//
// Parameters:
//   instanceId  : the owning instance
//   predecessors: comma-sep actionIds that must complete first
//                 (typically: all non-End actions)
// ------------------------------
std::shared_ptr<WorkflowAction> WorkflowEngine::createFinalizeAction(
    const std::string& instanceId,
    const std::string& predecessors)
{
    auto a = std::make_shared<WorkflowAction>();
    a->actionId              = genId("WFA");
    a->instanceId            = instanceId;
    a->title                 = "End";
    a->description           = "Abschlussschritt — beendet den Workflow.";
    a->sequenceOrder         = 9999;
    a->executionType         = "sequential";
    a->isFinal               = true;
    a->autoApprove           = false;
    a->predecessorActionIds  = predecessors;
    a->status                = "pending";
    a->createdAt             = nowIso();
    a->updatedAt             = nowIso();
    a->save();
    LOG_INFO("[WFEngine] End action created for instance " + instanceId);
    return a;
}

std::shared_ptr<WorkflowInstance> WorkflowEngine::startAdHoc(
    const std::string& et, const std::string& eid,
    const std::string& name, const std::string& exec,
    const std::string& initiatedBy)
{
    auto inst = WorkflowInstance::create(et, eid, name, exec);
    inst->initiatedBy = initiatedBy;
    if (!inst->save()) {
        LOG_ERROR("[WFEngine] Failed to save instance for " + et + "/" + eid);
        return nullptr;
    }

    // Create the mandatory Init action
    auto initAction = createInitializeAction(inst->instanceId);
    inst->actions.push_back(*initAction);

    // Create the mandatory End action (predecessor = Init)
    auto endAction = createFinalizeAction(inst->instanceId, initAction->actionId);
    inst->actions.push_back(*endAction);

    // Auto-tick to approve Init
    tick(*inst);

    LOG_INFO("[WFEngine] Ad-hoc instance started: " + inst->instanceId +
             " on " + et + "/" + eid);
    return inst;
}

// ============================================================
// WorkflowEngine — Instance creation
// ============================================================

// ------------------------------
// Create a WorkflowInstance from a named template.
// See WorkflowEngine.h for full parameter documentation.
//
// Two-pass instantiation to resolve predecessor chains:
//   Pass 1: Create all actions, collect tplActionId→actionId map
//   Pass 2: Rewrite predecessorActionIds using the map
// This ensures predecessor references work even if actions
// are added in non-sequential order.
// ------------------------------
std::shared_ptr<WorkflowInstance> WorkflowEngine::startFromTemplate(
    const std::string& templateId,
    const std::string& et, const std::string& eid,
    const std::string& name, const std::string& initiatedBy)
{
    auto tpl = WorkflowTemplate::loadById(templateId);
    if (!tpl) {
        LOG_ERROR("[WFEngine] Template not found: " + templateId);
        return nullptr;
    }

    auto inst = WorkflowInstance::create(et, eid, name, tpl->executionType);
    inst->templateId  = templateId;
    inst->initiatedBy = initiatedBy;
    inst->slaHours    = tpl->defaultSlaHours;
    if (!inst->save()) return nullptr;

    // Create initialize action first, linked to template's init action
    auto initAction = createInitializeAction(inst->instanceId);
    // Link to template's initialize action so predecessor mapping works
    for (auto& ta : tpl->templateActions) {
        if (ta.isInitialize) {
            initAction->tplActionId = ta.tplActionId;
            initAction->save();
            break;
        }
    }
    inst->actions.push_back(*initAction);

    // Build tplActionId -> instanceActionId map for predecessor resolution
    std::map<std::string,std::string> tplToInst;
    if (!initAction->tplActionId.empty())
        tplToInst[initAction->tplActionId] = initAction->actionId;

    // First pass: create all actions and record the ID mapping
    int order = 1;
    std::vector<WorkflowAction> created;
    for (auto& ta : tpl->templateActions) {
        if (ta.isInitialize) continue;
        WorkflowAction a;
        a.actionId     = genId("WFA");
        a.instanceId   = inst->instanceId;
        a.tplActionId  = ta.tplActionId;
        a.title        = ta.title;
        a.description  = ta.description;
        a.sequenceOrder= ta.sequenceOrder > 0 ? ta.sequenceOrder : order++;
        a.executionType= ta.executionType;
        a.predecessorActionIds = ta.predecessorIds;  // still template IDs for now
        a.requiredRole = ta.requiredRole;
        a.slaHours     = ta.slaHours > 0 ? ta.slaHours : tpl->defaultSlaHours;
        a.autoApprove  = ta.autoApprove;
        a.isFinal      = ta.isFinal;
        a.requiresDecisionLogEntry   = ta.requiresDecisionLogEntry;
        a.requiresLessonLearnedEntry = ta.requiresLessonLearnedEntry;
        a.requiresComment = ta.requiresComment;
        a.status       = "pending";
        a.createdAt    = nowIso();
        a.updatedAt    = nowIso();
        tplToInst[ta.tplActionId] = a.actionId;  // record mapping BEFORE saving
        created.push_back(a);
    }

    // Second pass: resolve predecessor IDs from template IDs to instance IDs, then save
    for (auto& a : created) {
        if (!a.predecessorActionIds.empty()) {
            std::string remapped;
            std::istringstream ss(a.predecessorActionIds);
            std::string pred;
            while (std::getline(ss, pred, ',')) {
                while (!pred.empty() && pred.front() == ' ') pred.erase(pred.begin());
                if (pred.empty()) continue;
                auto it = tplToInst.find(pred);
                if (!remapped.empty()) remapped += ',';
                remapped += (it != tplToInst.end()) ? it->second : pred;
            }
            a.predecessorActionIds = remapped;
        }
        a.save();
        inst->actions.push_back(a);
    }

    // Ensure an End action exists (add if template has none with isFinal=true)
    {
        bool hasEnd = false;
        for (auto& a : inst->actions) if (a.isFinal) { hasEnd = true; break; }
        if (!hasEnd) {
            std::string preds;
            for (auto& a : inst->actions)
                if (!a.isInitialize) { if (!preds.empty()) preds += ","; preds += a.actionId; }
            if (preds.empty()) preds = initAction->actionId;
            auto endAct = createFinalizeAction(inst->instanceId, preds);
            inst->actions.push_back(*endAct);
        }
    }

    // Tick to auto-approve Init
    tick(*inst);

    LOG_INFO("[WFEngine] Instance from template started: " + inst->instanceId);
    return inst;
}

std::shared_ptr<WorkflowAction> WorkflowEngine::addAction(
    WorkflowInstance& inst,
    const std::string& title,
    const std::string& execType,
    int seqOrder,
    const std::string& predecessors,
    const std::string& assignedTo,
    const std::string& dueDate,
    int slaHours)
{
    // Guard: End action must never be used as a predecessor.
    // A step after End would be unreachable — the workflow is already closed.
    if (!predecessors.empty()) {
        for (auto& a : inst.actions) {
            if (a.isFinal) {
                // Split the predecessor list and check each ID
                std::istringstream ss(predecessors);
                std::string tok;
                while (std::getline(ss, tok, ',')) {
                    // Trim whitespace
                    auto s = tok.find_first_not_of(" \t");
                    auto e = tok.find_last_not_of(" \t");
                    if (s != std::string::npos) tok = tok.substr(s, e-s+1);
                    if (tok == a.actionId) {
                        LOG_ERROR("[WFEngine] addAction rejected: End action (" +
                                  a.actionId + ") cannot be a predecessor.");
                        return nullptr;
                    }
                }
            }
        }
    }

    // Find the End action index BEFORE any vector modification
    // (push_back may reallocate, invalidating pointers)
    int endActIdx = -1;
    for (int i = 0; i < (int)inst.actions.size(); ++i)
        if (inst.actions[i].isFinal) { endActIdx = i; break; }

    auto a = std::make_shared<WorkflowAction>();
    a->actionId     = genId("WFA");
    a->instanceId   = inst.instanceId;
    a->title        = title;
    a->executionType= execType;

    // Assign sequence order: always one before End (9999)
    int maxMid = 0;
    for (auto& x : inst.actions)
        if (!x.isInitialize && !x.isFinal && x.sequenceOrder > maxMid)
            maxMid = x.sequenceOrder;
    a->sequenceOrder = seqOrder > 0 ? seqOrder : maxMid + 1;

    a->predecessorActionIds = predecessors;
    a->assignedTo   = assignedTo;
    a->dueDate      = dueDate;
    a->slaHours     = slaHours;
    a->status       = "pending";
    a->createdAt    = nowIso();
    a->updatedAt    = nowIso();
    a->save();
    inst.actions.push_back(*a);

    // Update the End action's predecessors to include this new step.
    // Use index (not pointer) — push_back may have reallocated the vector.
    if (endActIdx >= 0) {
        auto& endRef = inst.actions[endActIdx];
        if (endRef.predecessorActionIds.empty())
            endRef.predecessorActionIds = a->actionId;
        else
            endRef.predecessorActionIds += "," + a->actionId;
        endRef.save();
    }

    LOG_INFO("[WFEngine] Action added: " + a->actionId + " '" + title +
             "' to instance " + inst.instanceId);
    return a;
}

// ============================================================
// WorkflowEngine — State transitions
// ============================================================

// ------------------------------
// Fire an action — the core state transition.
// See WorkflowEngine.h for full documentation.
//
// Transaction safety: entire operation wrapped in
// beginTransaction / commit / rollback so that
// partial state changes cannot persist on error.
// ------------------------------
bool WorkflowEngine::fireAction(
    WorkflowInstance& inst,
    const std::string& actionId,
    const std::string& decision,
    const std::string& actorId,
    const std::string& comment)
{
    WorkflowAction* action = inst.findAction(actionId);
    if (!action) {
        LOG_WARN("[WFEngine] Action not found: " + actionId);
        return false;
    }
    if (action->isComplete()) {
        LOG_WARN("[WFEngine] Action already complete: " + actionId);
        return false;
    }
    if (!action->canStart(inst.actions)) {
        LOG_WARN("[WFEngine] Action predecessors not complete: " + actionId);
        return false;
    }
    if (action->requiresComment && comment.empty()) {
        LOG_WARN("[WFEngine] Action requires comment: " + actionId);
        return false;
    }

    // Atomic transaction: action update + instance completion check
    auto* db = wfDB();
    if (!db) return false;
    db->beginTransaction();

    action->status        = decision; // "approved"|"rejected"|"skipped"
    action->decision      = decision;
    action->decisionBy    = actorId;
    action->decisionDate  = nowIso();
    action->completedDate = nowIso();
    action->comment       = comment;
    action->updatedAt     = nowIso();
    bool ok = action->save();
    if (!ok) { db->rollbackTransaction(); return false; }

    LOG_INFO("[WFEngine] Action fired: " + actionId + " decision=" + decision +
             " by=" + actorId);

    checkAndCompleteInstance(inst);

    if (!db->commitTransaction()) {
        LOG_ERROR("[WFEngine] fireAction commit failed: " + actionId);
        return false;
    }
    syncEntityWorkflowFields(inst);
    // Auto-tick after every action fire so the End bookend can auto-approve
    // and the instance can transition to "completed" without manual tick calls.
    tick(inst);
    return true;
}

bool WorkflowEngine::tick(WorkflowInstance& inst) {
    if (inst.status != "active") return false;

    bool changed = false;

    // Auto-approve initialize actions
    for (auto& a : inst.actions) {
        if (a.isInitialize && a.status == "pending" && a.autoApprove) {
            a.status        = "approved";
            a.decision      = "approved";
            a.decisionBy    = "system";
            a.decisionDate  = nowIso();
            a.completedDate = nowIso();
            a.updatedAt     = nowIso();
            a.save();
            LOG_INFO("[WFEngine] Initialize action auto-approved: " + a.actionId);
            changed = true;
        }
        // Auto-approve other auto_approve actions whose predecessors are done
        if (!a.isInitialize && a.autoApprove && a.status == "pending" &&
            a.canStart(inst.actions)) {
            a.status        = "approved";
            a.decision      = "approved";
            a.decisionBy    = "system";
            a.decisionDate  = nowIso();
            a.completedDate = nowIso();
            a.updatedAt     = nowIso();
            a.save();
            LOG_INFO("[WFEngine] Action auto-approved: " + a.actionId);
            changed = true;
        }
        // Auto-approve the End bookend when all its predecessors are complete
        // AND there is at least one real mid-step (not just Init→End with no work).
        // This prevents an empty ad-hoc workflow from auto-completing immediately.
        if (a.isFinal && (a.status == "pending" || a.status == "in_progress")
            && a.canStart(inst.actions)) {
            // Count real work steps (not Init, not End)
            int midSteps = 0;
            int midDone  = 0;
            for (auto& x : inst.actions) {
                if (!x.isInitialize && !x.isFinal) {
                    midSteps++;
                    if (x.isComplete()) midDone++;
                }
            }
            // Only auto-approve End if there are mid-steps and all are done
            if (midSteps > 0 && midDone == midSteps) {
                a.status        = "approved";
                a.decision      = "approved";
                a.decisionBy    = "system";
                a.decisionDate  = nowIso();
                a.completedDate = nowIso();
                a.updatedAt     = nowIso();
                a.save();
                LOG_INFO("[WFEngine] End action auto-approved — workflow closing: " + a.actionId);
                changed = true;
            }
        }
        // Mark in_progress for ready actions (never for the End bookend)
        if (a.status == "pending" && a.canStart(inst.actions) &&
            !a.autoApprove && !a.isFinal) {
            a.status      = "in_progress";
            a.startedDate = nowIso();
            a.updatedAt   = nowIso();
            a.save();
            changed = true;
        }
        // SLA check
        checkSLA(a);
    }

    if (changed) checkAndCompleteInstance(inst);
    return changed;
}

bool WorkflowEngine::checkAndCompleteInstance(WorkflowInstance& inst) {
    if (!inst.isComplete()) return false;

    inst.status        = "completed";
    inst.completedDate = nowIso();
    inst.updatedAt     = nowIso();
    inst.save();
    LOG_INFO("[WFEngine] Instance completed: " + inst.instanceId);
    syncEntityWorkflowFields(inst);
    return true;
}

bool WorkflowEngine::checkSLA(WorkflowAction& action) {
    if (action.slaHours <= 0 || action.isComplete()) return false;
    if (action.startedDate.empty()) return false;
    // Simple SLA check via SQLite datetime arithmetic
    auto* db = wfDB();
    if (!db) return false;
    std::string breached = db->queryScalar(
        "SELECT CASE WHEN datetime(?, '+' || ? || ' hours') < datetime('now') "
        "THEN '1' ELSE '0' END;",
        {BindParam::text(action.startedDate), BindParam::int64(action.slaHours)});
    if (breached == "1" && !action.slaBreached) {
        action.slaBreached = true;
        action.updatedAt   = nowIso();
        action.save();
        LOG_WARN("[WFEngine] SLA breached for action: " + action.actionId);
        return true;
    }
    return false;
}

bool WorkflowEngine::escalate(WorkflowInstance& inst,
                               const std::string& escalateTo,
                               const std::string& reason)
{
    inst.escalatedTo   = escalateTo;
    inst.escalatedDate = nowIso();
    inst.updatedAt     = nowIso();
    if (!reason.empty()) {
        inst.notes = "{\"escalation_reason\":\"" + reason + "\"}";
    }
    bool ok = inst.save();
    LOG_INFO("[WFEngine] Instance escalated: " + inst.instanceId +
             " -> " + escalateTo);
    return ok;
}

bool WorkflowEngine::addParticipant(WorkflowInstance& inst,
                                     const std::string& personId,
                                     const std::string& role)
{
    WorkflowParticipant p;
    p.participantId = genId("WFP");
    p.instanceId    = inst.instanceId;
    p.personId      = personId;
    p.role          = role;
    p.active        = true;
    p.addedAt       = nowIso();
    p.save();
    inst.participants.push_back(p);
    return true;
}

bool WorkflowEngine::attachDocumentToAction(const std::string& actionId,
                                             const std::string& documentId,
                                             const std::string& relationship)
{
    auto* db = docDB();
    if (!db) return false;
    std::string linkId = genId("DOK");
    return db->exec(R"SQL(
        INSERT OR IGNORE INTO entity_documents
        (link_id, entity_type, entity_id, document_id, relationship, linked_at)
        VALUES(?,?,?,?,?,?)
    )SQL", {
        BindParam::text(linkId),
        BindParam::text("workflow_action"),
        BindParam::text(actionId),
        BindParam::text(documentId),
        BindParam::text(relationship),
        BindParam::text(nowIso())
    });
}

bool WorkflowEngine::attachDocumentToInstance(
    const std::string& instanceId,
    const std::string& documentId,
    const std::string& relationship,
    const std::string& notes)
{
    if (instanceId.empty() || documentId.empty()) {
        LOG_WARN("[WFEngine] attachDocumentToInstance: empty id");
        return false;
    }
    auto* db = docDB();
    if (!db) {
        LOG_ERROR("[WFEngine] attachDocumentToInstance: no documents db");
        return false;
    }
    // Verify document actually exists before linking
    auto doc = Document::loadById(documentId);
    if (!doc) {
        LOG_WARN("[WFEngine] attachDocumentToInstance: document not found: " + documentId);
        return false;
    }
    std::string linkId = genId("DOK");
    bool ok = db->exec(R"SQL(
        INSERT OR IGNORE INTO entity_documents
        (link_id, entity_type, entity_id, document_id, relationship, notes, linked_at)
        VALUES(?,?,?,?,?,?,?))SQL", {
        BindParam::text(linkId),
        BindParam::text("workflow_instance"),
        BindParam::text(instanceId),
        BindParam::text(documentId),
        BindParam::text(relationship.empty() ? "reference" : relationship),
        textOrNull(notes),
        BindParam::text(nowIso())
    });
    if (ok) LOG_INFO("[WFEngine] Document attached to instance: " + instanceId +
                     " doc=" + documentId);
    else     LOG_WARN("[WFEngine] attachDocumentToInstance failed for: " + instanceId);
    return ok;
}

std::vector<std::shared_ptr<Document>> WorkflowEngine::loadDocumentsForInstance(
    const std::string& instanceId)
{
    auto* docdb = docDB();
    auto* wfdb  = wfDB();
    std::vector<std::shared_ptr<Document>> result;
    if (!docdb || !wfdb) return result;

    // Fetch document IDs linked to the instance
    auto idRows = docdb->query(
        "SELECT document_id FROM entity_documents "
        "WHERE entity_type='workflow_instance' AND entity_id=? ORDER BY linked_at;",
        {BindParam::text(instanceId)});
    for (auto& r : idRows) {
        auto doc = Document::loadById(rowGet(r,"document_id"));
        if (doc) result.push_back(doc);
    }

    // Also fetch documents linked to each action of this instance
    auto actionRows = wfdb->query(
        "SELECT action_id FROM workflow_actions WHERE instance_id=?;",
        {BindParam::text(instanceId)});
    for (auto& ar : actionRows) {
        auto actionDocs = loadDocumentsForAction(rowGet(ar,"action_id"));
        result.insert(result.end(), actionDocs.begin(), actionDocs.end());
    }
    return result;
}

std::vector<std::shared_ptr<Document>> WorkflowEngine::loadDocumentsForAction(
    const std::string& actionId)
{
    auto* db = docDB();
    std::vector<std::shared_ptr<Document>> result;
    if (!db) return result;
    auto idRows = db->query(
        "SELECT document_id FROM entity_documents "
        "WHERE entity_type='workflow_action' AND entity_id=? ORDER BY linked_at;",
        {BindParam::text(actionId)});
    for (auto& r : idRows) {
        auto doc = Document::loadById(rowGet(r,"document_id"));
        if (doc) result.push_back(doc);
    }
    return result;
}

bool WorkflowEngine::createDecisionLogEntry(
    const std::string& /*actionId*/,
    const std::string& entityType,
    const std::string& entityId,
    const std::string& title,
    const std::string& rationale)
{
    // DecisionLog is now an F18Workflow (vorgangType="decisionLog").
    // Create or append to a decisionLog F18 for this entity.
    auto* db = DatabasePool::instance().get("f18");
    if (!db) return false;
    // Find existing decisionLog for entity
    std::string parentId = entityType == "project" ? entityId : "";
    std::string taskId   = entityType == "task"    ? entityId : "";
    auto f18 = F18Workflow::create(parentId, title, F18VorgangType::DECISION_LOG, taskId);
    if (!f18) return false;
    f18->decisionType = "workflow-decision";
    f18->rationale    = rationale;
    f18->update();
    LOG_INFO("[WFEngine] DecisionLog F18 created: " + f18->vorgangId);
    return true;
}


bool WorkflowEngine::createLessonLearnedEntry(
    const std::string& /*actionId*/,
    const std::string& entityType,
    const std::string& entityId,
    const std::string& title,
    const std::string& lesson)
{
    // LessonsLearned is now an F18Workflow (vorgangType="lessonsLearned").
    std::string parentId = entityType == "project" ? entityId : "";
    std::string taskId   = entityType == "task"    ? entityId : "";
    auto f18 = F18Workflow::create(parentId, title, F18VorgangType::LESSONS_LEARNED, taskId);
    if (!f18) return false;
    f18->lessonType     = "observation";
    f18->recommendation = lesson;
    f18->update();
    LOG_INFO("[WFEngine] LessonsLearned F18 created: " + f18->vorgangId);
    return true;
}


bool WorkflowEngine::syncEntityWorkflowFields(const WorkflowInstance& inst) {
    // Find the most recent in_progress or last approved action
    std::string curState;
    auto* wfdb = wfDB();
    if (!wfdb) return false;

    auto rows = wfdb->query(
        "SELECT title FROM workflow_actions WHERE instance_id=? AND status='in_progress' "
        "ORDER BY sequence_order LIMIT 1;",
        {BindParam::text(inst.instanceId)});
    if (!rows.empty()) {
        curState = rowGet(rows[0], "title");
    } else {
        curState = inst.status == "completed" ? "abgeschlossen" : inst.status;
    }

    auto updateEntity = [&](Database* db, const std::string& table,
                             const std::string& idCol) {
        if (!db) return;
        db->exec(
            "UPDATE " + table + " SET "
            "workflow_instance_id=?, workflow_status=?, workflow_current_state=? "
            "WHERE " + idCol + "=?;",
            {BindParam::text(inst.instanceId),
             BindParam::text(inst.status),
             BindParam::text(curState),
             BindParam::text(inst.entityId)});
    };

    auto* pdb = DatabasePool::instance().get("projects");
    if      (inst.entityType == "project")  updateEntity(pdb, "projects",  "project_id");
    else if (inst.entityType == "task")     updateEntity(pdb, "tasks",     "task_id");
    else if (inst.entityType == "incident") updateEntity(pdb, "incidents", "incident_id");

    auto* ddb = DatabasePool::instance().get("documents");
    if (inst.entityType == "document") updateEntity(ddb, "documents", "document_id");

    auto* f18db = DatabasePool::instance().get("f18");
    if (inst.entityType == "risk") updateEntity(f18db, "f18_workflows", "vorgang_id");

    return true;
}

void WorkflowEngine::createStandardTemplates() {
    auto* db = wfDB();
    if (!db) return;

    auto existing = db->queryScalar(
        "SELECT COUNT(*) FROM workflow_templates WHERE name='Standardgenehmigung';");
    if (existing != "0") return;  // already seeded

    // ── Template 1: Standard approval workflow ───────────────
    auto tpl = WorkflowTemplate::create("Standardgenehmigung","sequential");
    tpl->description = "Einfacher Genehmigungsworkflow: Einreichen → Prüfen → Entscheiden";
    tpl->entityTypes = "project,task,document,change_request";
    tpl->slaEnforced = true;
    tpl->defaultSlaHours = 72;

    WorkflowTemplateAction init;
    init.tplActionId   = genId("WFT");
    init.templateId    = tpl->templateId;
    init.title         = "Init";
    init.sequenceOrder = 0;
    init.isInitialize  = true;
    init.autoApprove   = true;
    tpl->templateActions.push_back(init);

    WorkflowTemplateAction submit;
    submit.tplActionId   = genId("WFT");
    submit.templateId    = tpl->templateId;
    submit.title         = "Einreichen";
    submit.description   = "Antragsteller reicht den Vorgang formal ein.";
    submit.sequenceOrder = 1;
    submit.slaHours      = 24;
    tpl->templateActions.push_back(submit);

    WorkflowTemplateAction review;
    review.tplActionId   = genId("WFT");
    review.templateId    = tpl->templateId;
    review.title         = "Prüfen";
    review.description   = "Gutachter prüft den Vorgang auf Vollständigkeit.";
    review.sequenceOrder = 2;
    review.requiredRole  = "reviewer";
    review.slaHours      = 48;
    review.predecessorIds= submit.tplActionId;
    tpl->templateActions.push_back(review);

    WorkflowTemplateAction decide;
    decide.tplActionId   = genId("WFT");
    decide.templateId    = tpl->templateId;
    decide.title         = "Entscheiden";
    decide.description   = "Genehmiger trifft die finale Entscheidung.";
    decide.sequenceOrder = 3;
    decide.requiredRole  = "approver";
    decide.slaHours      = 72;
    decide.predecessorIds= review.tplActionId;
    decide.requiresDecisionLogEntry = true;
    tpl->templateActions.push_back(decide);

    WorkflowTemplateAction end1;
    end1.tplActionId   = genId("WFT");
    end1.templateId    = tpl->templateId;
    end1.title         = "End";
    end1.description   = "Workflow abgeschlossen.";
    end1.sequenceOrder = 4;
    end1.isFinal       = true;
    end1.autoApprove   = false;
    end1.predecessorIds= decide.tplActionId;
    tpl->templateActions.push_back(end1);

    tpl->save();
    LOG_INFO("[WFEngine] Standard template created: " + tpl->templateId);

    // ── Template 2: Project closure ─────────────────────────
    auto tpl2 = WorkflowTemplate::create("Projektabschluss","sequential");
    tpl2->description = "Formaler Projektabschluss mit Lessons Learned und Archivierung";
    tpl2->entityTypes = "project";

    WorkflowTemplateAction i2;
    i2.tplActionId=genId("WFT"); i2.templateId=tpl2->templateId;
    i2.title="Init"; i2.sequenceOrder=0;
    i2.isInitialize=true; i2.autoApprove=true;
    tpl2->templateActions.push_back(i2);

    WorkflowTemplateAction ll;
    ll.tplActionId=genId("WFT"); ll.templateId=tpl2->templateId;
    ll.title="Lessons Learned erfassen";
    ll.description="Projektteam dokumentiert Erkenntnisse.";
    ll.sequenceOrder=1; ll.requiresLessonLearnedEntry=true;
    ll.predecessorIds=i2.tplActionId;
    tpl2->templateActions.push_back(ll);

    WorkflowTemplateAction dl;
    dl.tplActionId=genId("WFT"); dl.templateId=tpl2->templateId;
    dl.title="Abschlussentscheidung dokumentieren";
    dl.description="Abschlussentscheid und -begründung festhalten.";
    dl.sequenceOrder=2; dl.requiresDecisionLogEntry=true;
    dl.predecessorIds=ll.tplActionId;
    tpl2->templateActions.push_back(dl);

    WorkflowTemplateAction arch;
    arch.tplActionId=genId("WFT"); arch.templateId=tpl2->templateId;
    arch.title="Archivieren"; arch.description="Vorgang in MFS archivieren und Status auf 'closed' setzen.";
    arch.sequenceOrder=3;
    arch.predecessorIds=dl.tplActionId;
    tpl2->templateActions.push_back(arch);

    WorkflowTemplateAction end2;
    end2.tplActionId=genId("WFT"); end2.templateId=tpl2->templateId;
    end2.title="End"; end2.description="Projektworkflow abgeschlossen.";
    end2.sequenceOrder=4; end2.isFinal=true;
    end2.predecessorIds=arch.tplActionId;
    tpl2->templateActions.push_back(end2);

    tpl2->save();
    LOG_INFO("[WFEngine] Closure template created: " + tpl2->templateId);
}

// ------------------------------
// searchInstances
// Filter workflow instances by multiple criteria.
// See WorkflowEngine.h for parameter documentation.
// ------------------------------
std::vector<std::shared_ptr<WorkflowInstance>> WorkflowEngine::searchInstances(
    const std::string& entityType,
    const std::string& status,
    const std::string& nameContains,
    bool slaOnly)
{
    std::vector<std::shared_ptr<WorkflowInstance>> result;

    // Load instances — when no status filter, load ALL; otherwise filter by status
    auto* db = wfDB();
    std::vector<std::shared_ptr<WorkflowInstance>> all;
    if (!db) return result;

    if (status.empty()) {
        // Load all instances regardless of status
        auto rows = db->query(
            "SELECT * FROM workflow_instances ORDER BY created_at DESC LIMIT 500;");
        for (auto& r : rows) {
            auto inst = std::make_shared<WorkflowInstance>();
            inst->fromRow(r);
            inst->loadActions();
            inst->loadParticipants();
            all.push_back(inst);
        }
    } else {
        // Load only instances with the requested status
        auto rows = db->query(
            "SELECT * FROM workflow_instances WHERE status=? ORDER BY created_at DESC;",
            {BindParam::text(status)});
        for (auto& r : rows) {
            auto inst = std::make_shared<WorkflowInstance>();
            inst->fromRow(r);
            inst->loadActions();
            inst->loadParticipants();
            all.push_back(inst);
        }
    }

    for (auto& inst : all) {
        if (!entityType.empty() && inst->entityType != entityType) continue;
        if (!status.empty()     && inst->status     != status)     continue;
        if (slaOnly && !inst->slaBreached)                         continue;
        if (!nameContains.empty()) {
            std::string lname = inst->name;
            std::string lsub  = nameContains;
            // Case-insensitive substring
            for (char& c : lname) c = (char)std::tolower(c);
            for (char& c : lsub)  c = (char)std::tolower(c);
            if (lname.find(lsub) == std::string::npos) continue;
        }
        result.push_back(inst);
    }
    return result;
}

} // namespace Rosenholz

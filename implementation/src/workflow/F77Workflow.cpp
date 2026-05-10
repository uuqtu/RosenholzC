// ============================================================
// F77Workflow.cpp  —  Workflow Engine Implementation
//
// SECTION MAP (search for ── <name>):
//   ── Template CRUD      F77W_Template / F77W_TemplateStep CRUD
//   ── Operation CRUD     F77W_Operation (step instance) logic
//   ── Workflow CRUD      F77W (instance) CRUD + fromRow
//   ── Entity context     entityContext() — table+column map per type
//   ── wfLocked           propagateWfLockedToChildren, storeWorkflowId
//   ── Attach/Detach      attachWorkflow, detachWorkflow, cancelWorkflow
//   ── defaultOperations  step chain config per entity type
//   ── startFromTemplate  create workflow from template
//   ── startDefault       create minimal workflow
//   ── MFS helpers        pruneMFSRevisions
//   ── executeSystemStep  CHECK_CHILDREN, SCAN, COMMIT handlers
//   ── tick               drive workflow forward
//   ── fireStep           admin: manually fire a step
//   ── checkAndComplete   fire End when all steps done
//   ── applyTargetState   set entity status when WF completes
//   ── canRelease/lockAll capability queries
//   ── seedDefaultTemplates bootstrap
//   ── handleTaskAction   execute task's encoded action
//   ── checkPropagationComplete re-apply after all child tasks done
// ============================================================

// ============================================================
#include "F77Workflow.h"
#include "../model/Guard.h"
#include "F77Task.h"
#include "../core/FileOps.h"
#include "../model/akt/FolderObject.h"
#include "../model/akt/FolderRevision.h"
#include "../core/Config.h"
#include <set>
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../model/Utils.h"
#include "../model/f16/F16.h"
#include "../mfs/MFSWriter.h"
#include "../model/f22/F22.h"
#include "../model/akt/Folder.h"
#include "../model/akt/FolderRevision.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>



namespace Rosenholz {

// ── WorkflowStatus / StepStatus serialization ────────────────────────────
std::string toString(WorkflowStatus s) {
    switch (s) {
        case WorkflowStatus::ACTIVE:    return "active";
        case WorkflowStatus::COMPLETED: return "completed";
        case WorkflowStatus::LOCKED:    return "locked";
        case WorkflowStatus::CANCELLED: return "cancelled";
    }
    return "active";
}
WorkflowStatus workflowStatusFrom(const std::string& s) {
    if (s == "completed") return WorkflowStatus::COMPLETED;
    if (s == "locked")    return WorkflowStatus::LOCKED;
    if (s == "cancelled") return WorkflowStatus::CANCELLED;
    return WorkflowStatus::ACTIVE;
}
std::string toString(StepStatus s) {
    switch (s) {
        case StepStatus::PENDING:     return "pending";
        case StepStatus::IN_PROGRESS: return "in_progress";
        case StepStatus::APPROVED:    return "approved";
        case StepStatus::REJECTED:    return "rejected";
        case StepStatus::SKIPPED:     return "skipped";
        case StepStatus::CANCELLED:   return "cancelled";
    }
    return "pending";
}
StepStatus stepStatusFrom(const std::string& s) {
    if (s == "in_progress") return StepStatus::IN_PROGRESS;
    if (s == "approved")    return StepStatus::APPROVED;
    if (s == "rejected")    return StepStatus::REJECTED;
    if (s == "skipped")     return StepStatus::SKIPPED;
    if (s == "cancelled")   return StepStatus::CANCELLED;
    return StepStatus::PENDING;
}


// ── Display symbol enums — model returns enum; UI layer maps to strings/icons ──
StepSymbol stepSymbol(StepStatus s) {
    switch (s) {
        case StepStatus::APPROVED:    return StepSymbol::APPROVED;
        case StepStatus::REJECTED:    return StepSymbol::REJECTED;
        case StepStatus::SKIPPED:     return StepSymbol::SKIPPED;
        case StepStatus::IN_PROGRESS: return StepSymbol::IN_PROGRESS;
        case StepStatus::CANCELLED:   return StepSymbol::SKIPPED; // cancelled shown as skipped
        case StepStatus::PENDING:     return StepSymbol::PENDING;
    }
    return StepSymbol::PENDING;
}

WorkflowSymbol workflowSymbol(WorkflowStatus s) {
    switch (s) {
        case WorkflowStatus::COMPLETED: return WorkflowSymbol::COMPLETED;
        case WorkflowStatus::LOCKED:    return WorkflowSymbol::LOCKED;
        case WorkflowStatus::CANCELLED: return WorkflowSymbol::CANCELLED;
        case WorkflowStatus::ACTIVE:    return WorkflowSymbol::ACTIVE;
    }
    return WorkflowSymbol::ACTIVE;
}

StepSymbol F77W_Operation::stepSymbol() const {
    return Rosenholz::stepSymbol(status);
}

WorkflowSymbol F77W::workflowSymbol() const {
    return Rosenholz::workflowSymbol(status);
}


// ── DB helper ────────────────────────────────────────────────
static Database* wfDB() { return DatabasePool::instance().get("f77"); }

// ─────────────────────────────────────────────────────────────
// F77W_TemplateStep
// ─────────────────────────────────────────────────────────────
Database* F77W_Template::db() { return wfDB(); }

void F77W_TemplateStep::fromRow(const Row& r) {
    auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
    auto gb= [&](const std::string& k){ return g(k)=="1"; };
    auto gi= [&](const std::string& k){ auto v=g(k); return v.empty()?0:std::stoi(v); };
    tplStepId                = g("tpl_step_id");
    templateId               = g("template_id");
    title                    = g("title");
    description              = g("description");
    sequenceOrder            = gi("sequence_order");
    isInitialize             = gb("is_initialize");
    isFinal                  = gb("is_final");
    executionMode            = g("execution_mode");
    // Template step predecessors stored as plain CSV (simple, single level)
    predecessorTplStepIds    = g("predecessor_tpl_step_ids");
    requiredRole             = g("required_role");
    slaHours                 = gi("sla_hours");
    autoApprove              = gb("auto_approve");
    requiresComment          = gb("requires_comment");
    requiresDocument         = gb("requires_document");
    createdAt                = g("created_at");
    updatedAt                = g("updated_at");
}

OperationResult F77W_TemplateStep::save() const {
    auto* d = wfDB(); if (!d) return OperationResult::DB_ERROR;
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_template_steps
        (tpl_step_id,template_id,title,description,sequence_order,is_initialize,is_final,
         execution_mode,predecessor_tpl_step_ids,required_role,sla_hours,
         auto_approve,requires_comment,requires_document,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(tplStepId),BindParam::text(templateId),BindParam::text(title),BindParam::nullOrText(description),BindParam::int64(sequenceOrder),
        BindParam::int64(isInitialize?1:0),BindParam::int64(isFinal?1:0),
        BindParam::text(executionMode),BindParam::nullOrText(predecessorTplStepIds),
        BindParam::nullOrText(""), // required_role
        BindParam::int64(0),       // sla_hours
        BindParam::int64(autoApprove?1:0),BindParam::int64(requiresComment?1:0),
        BindParam::int64(requiresDocument?1:0),
        BindParam::text(createdAt),BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77W_TemplateStep::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec("DELETE FROM f77_workflow_template_steps WHERE tpl_step_id=?;",
                   {BindParam::text(tplStepId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

// ─────────────────────────────────────────────────────────────
// F77W_Template
// ─────────────────────────────────────────────────────────────
void F77W_Template::fromRow(const Row& r) {
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    templateId  = g("template_id");
    name        = g("name");
    version     = g("version");
    description = g("description");
    entityTypes = g("entity_types");
    targetState = entityStatusFrom(g("target_state"));
    status      = templateStatusFrom(g("status"));
    createdBy   = g("created_by");
    createdAt   = g("created_at");
    updatedAt   = g("updated_at");
}

OperationResult F77W_Template::save() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_templates
        (template_id,name,version,description,entity_types,target_state,status,
         created_by,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?)
    )SQL", {BindParam::text(templateId),BindParam::text(name),BindParam::text(version),BindParam::nullOrText(description),BindParam::nullOrText(entityTypes),
            BindParam::text(entityStatusToString(targetState)),BindParam::text(templateStatusToString(status)),BindParam::nullOrText(createdBy),BindParam::text(createdAt),BindParam::text(nowIso())}) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77W_Template::deactivate() {
    status    = TemplateStatus::INACTIVE;
    updatedAt = nowIso();
    return save();
}

OperationResult F77W_Template::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    d->exec("DELETE FROM f77_workflow_template_steps WHERE template_id=?;",{BindParam::text(templateId)});
    return d->exec("DELETE FROM f77_workflow_templates WHERE template_id=?;",{BindParam::text(templateId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

bool F77W_Template::loadSteps() {
    auto* d=wfDB(); if(!d) return false;
    auto rows=d->query(
        "SELECT * FROM f77_workflow_template_steps WHERE template_id=? ORDER BY sequence_order;",
        {BindParam::text(templateId)});
    steps.clear();
    for(auto& r:rows){F77W_TemplateStep s; s.fromRow(r); steps.push_back(s);}
    return true;
}

F77W_TemplateStep F77W_Template::addTemplateStep(
    const std::string& title, const std::string& executionMode,
    bool isInit, bool isFinal)
{
    F77W_TemplateStep s;
    s.tplStepId    = genId("F77T");
    s.templateId   = templateId;
    s.title        = title;
    s.executionMode= executionMode;
    s.isInitialize = isInit;
    s.isFinal      = isFinal;
    s.sequenceOrder= (int)steps.size();
    s.autoApprove  = isInit;  // Init steps are auto-approved
    s.createdAt    = nowIso();
    s.updatedAt    = nowIso();
    steps.push_back(s);
    return s;
}

std::shared_ptr<F77W_Template> F77W_Template::create(
    const std::string& name, EntityStatus targetState,
    const std::string& entityTypes)
{
    auto t = std::make_shared<F77W_Template>();
    t->templateId  = genId("F77D");
    t->name        = name;
    t->targetState = targetState;
    t->entityTypes = entityTypes;
    t->createdAt   = nowIso();
    t->updatedAt   = nowIso();
    return t;
}

std::shared_ptr<F77W_Template> F77W_Template::loadById(const std::string& id) {
    auto* d=wfDB(); if(!d) return nullptr;
    auto rows=d->query("SELECT * FROM f77_workflow_templates WHERE template_id=?;",{BindParam::text(id)});
    if(rows.empty()) return nullptr;
    auto t=std::make_shared<F77W_Template>(); t->fromRow(rows[0]); t->loadSteps();
    return t;
}

std::vector<std::shared_ptr<F77W_Template>> F77W_Template::loadAll() {
    auto* d=wfDB(); std::vector<std::shared_ptr<F77W_Template>> res;
    if(!d) return res;
    for(auto& r:d->query("SELECT * FROM f77_workflow_templates ORDER BY name;",{})){
        auto t=std::make_shared<F77W_Template>(); t->fromRow(r); res.push_back(t);
    }
    return res;
}

std::vector<std::shared_ptr<F77W_Template>> F77W_Template::loadForEntityType(
    const std::string& entityType)
{
    auto all=loadAll();
    std::vector<std::shared_ptr<F77W_Template>> res;
    for(auto& t:all)
        if(t->entityTypes.empty() || t->entityTypes.find(entityType)!=std::string::npos)
            res.push_back(t);
    return res;
}

// ─────────────────────────────────────────────────────────────
// F77W_Operation
// ─────────────────────────────────────────────────────────────
Database* F77W_Operation::db() { return wfDB(); }


// ── predecessors CSV helpers ──────────────────────────────────
std::string F77W_Operation::predecessorsToString() const {
    std::string out;
    for (size_t i = 0; i < predecessors.size(); ++i) {
        if (i) out += ',';
        out += predecessors[i];
    }
    return out;
}

std::vector<std::string> F77W_Operation::predecessorsFromString(const std::string& csv) {
    std::vector<std::string> result;
    std::istringstream ss(csv);
    std::string id;
    while (std::getline(ss, id, ',')) {
        id.erase(0, id.find_first_not_of(' '));
        id.erase(id.find_last_not_of(' ') + 1);
        if (!id.empty()) result.push_back(id);
    }
    return result;
}

void F77W_Operation::fromRow(const Row& r) {
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    auto gb=[&](const std::string& k){return g(k)=="1";};
    auto gi=[&](const std::string& k){auto v=g(k);return v.empty()?0:std::stoi(v);};
    stepId              = g("step_id");
    workflowId          = g("workflow_id");
    tplStepId           = g("tpl_step_id");
    title               = g("title");
    sequenceOrder       = gi("sequence_order");
    isInitialize        = gb("is_initialize");
    isFinal             = gb("is_final");
    executionMode       = g("execution_mode");
    predecessors = predecessorsFromString(g("predecessor_step_ids"));
    status              = stepStatusFrom(g("status"));
    autoApprove         = gb("auto_approve");
    isSystem            = gb("is_system");
    systemAction        = static_cast<SystemAction>(gi("system_action"));
    requiresComment     = gb("requires_comment");
    requiresDocument    = gb("requires_document");
    completedDate       = g("completed_date");
    createdAt           = g("created_at");
    updatedAt           = g("updated_at");
}

OperationResult F77W_Operation::save() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_steps
        (step_id,workflow_id,tpl_step_id,title,sequence_order,is_initialize,is_final,
         execution_mode,predecessor_step_ids,status,auto_approve,is_system,system_action,
         requires_comment,requires_document,completed_date,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(stepId),BindParam::text(workflowId),BindParam::nullOrText(tplStepId),
        BindParam::text(title),BindParam::int64(sequenceOrder),
        BindParam::int64(isInitialize?1:0),BindParam::int64(isFinal?1:0),
        BindParam::text(executionMode),BindParam::text(predecessorsToString()),
        BindParam::text(toString(status)),BindParam::int64(autoApprove?1:0),
        BindParam::int64(isSystem?1:0),BindParam::int64(static_cast<int>(systemAction)),
        BindParam::int64(requiresComment?1:0),BindParam::int64(requiresDocument?1:0),
        BindParam::nullOrText(completedDate),BindParam::text(createdAt),BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77W_Operation::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec("DELETE FROM f77_workflow_steps WHERE step_id=?;",{BindParam::text(stepId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}


bool F77W_Operation::canStart(const std::vector<F77W_Operation>& all) const {
    if (status!=StepStatus::PENDING && status!=StepStatus::IN_PROGRESS) return false;
    for (const auto& predId : predecessors) {
        bool done = false;
        for (const auto& s : all) {
            if (s.stepId == predId) { done = s.isComplete(); break; }
        }
        if (!done) return false;
    }
    return true;
}

std::shared_ptr<F77W_Operation> F77W_Operation::loadById(const std::string& id) {
    auto* d=wfDB(); if(!d) return nullptr;
    auto rows=d->query("SELECT * FROM f77_workflow_steps WHERE step_id=?;",{BindParam::text(id)});
    if(rows.empty()) return nullptr;
    auto s=std::make_shared<F77W_Operation>(); s->fromRow(rows[0]); return s;
}

std::vector<F77W_Operation> F77W_Operation::loadForWorkflow(const std::string& wfId) {
    auto* d=wfDB(); std::vector<F77W_Operation> res;
    if(!d) return res;
    for(auto& r:d->query(
        "SELECT * FROM f77_workflow_steps WHERE workflow_id=? ORDER BY sequence_order;",
        {BindParam::text(wfId)}))
    { F77W_Operation s; s.fromRow(r); res.push_back(s); }
    return res;
}

// ─────────────────────────────────────────────────────────────
// F77W
// ─────────────────────────────────────────────────────────────
Database* F77W::db() { return wfDB(); }

void F77W::fromRow(const Row& r) {
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    workflowId    = g("workflow_id");
    templateId    = g("template_id");
    templateName  = g("template_name");
    entityType    = g("entity_type");
    entityId      = g("entity_id");
    targetState   = entityStatusFrom(g("target_state"));
    status        = workflowStatusFrom(g("status"));
    initiatedBy   = g("initiated_by");
    initiatedDate = g("initiated_date");
    completedDate = g("completed_date");
    notes         = g("notes");
    createdAt     = g("created_at");
    updatedAt     = g("updated_at");
}

OperationResult F77W::save() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflows
        (workflow_id,template_id,template_name,entity_type,entity_id,target_state,
         status,initiated_by,initiated_date,completed_date,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {BindParam::text(workflowId),BindParam::nullOrText(templateId),BindParam::text(templateName),BindParam::text(entityType),BindParam::text(entityId),
            BindParam::text(entityStatusToString(targetState)),BindParam::text(toString(status)),BindParam::nullOrText(initiatedBy),BindParam::text(initiatedDate),BindParam::nullOrText(completedDate),
            BindParam::text(notes),BindParam::text(createdAt),BindParam::text(nowIso())}) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77W::update() const { return save(); }

OperationResult F77W::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    d->exec("DELETE FROM f77_workflow_steps WHERE workflow_id=?;",{BindParam::text(workflowId)});
    return d->exec("DELETE FROM f77_workflows WHERE workflow_id=?;",{BindParam::text(workflowId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

bool F77W::loadSteps() {
    steps = F77W_Operation::loadForWorkflow(workflowId);
    return true;
}

std::vector<F77W_Operation*> F77W::readySteps() {
    std::vector<F77W_Operation*> res;
    for(auto& s:steps)
        if(!s.isComplete() && s.canStart(steps)) res.push_back(&s);
    return res;
}

std::shared_ptr<F77W> F77W::create(
    const std::string& entityType, const std::string& entityId,
    const std::string& templateName, EntityStatus targetState,
    const std::string& initiatedBy)
{
    auto wf = std::make_shared<F77W>();
    wf->workflowId   = genId("F77W");
    wf->templateName = templateName;
    wf->entityType   = entityType;
    wf->entityId     = entityId;
    wf->targetState  = targetState;
    wf->status       = WorkflowStatus::ACTIVE;
    wf->initiatedBy  = initiatedBy;
    wf->initiatedDate= nowIso();
    wf->createdAt    = nowIso();
    wf->updatedAt    = nowIso();
    return wf;
}

std::shared_ptr<F77W> F77W::loadById(const std::string& id) {
    auto* d=wfDB(); if(!d) return nullptr;
    auto rows=d->query("SELECT * FROM f77_workflows WHERE workflow_id=?;",{BindParam::text(id)});
    if(rows.empty()) return nullptr;
    auto wf=std::make_shared<F77W>(); wf->fromRow(rows[0]); wf->loadSteps(); return wf;
}

std::vector<std::shared_ptr<F77W>> F77W::loadForEntity(
    const std::string& entityType, const std::string& entityId)
{
    auto* d=wfDB(); std::vector<std::shared_ptr<F77W>> res;
    if(!d) return res;
    for(auto& r:d->query(
        "SELECT * FROM f77_workflows WHERE entity_type=? AND entity_id=? ORDER BY initiated_date DESC;",
        {BindParam::text(entityType),BindParam::text(entityId)}))
    { auto wf=std::make_shared<F77W>(); wf->fromRow(r); wf->loadSteps(); res.push_back(wf); }
    return res;
}

std::vector<std::shared_ptr<F77W>> F77W::loadActive() {
    auto* d=wfDB(); std::vector<std::shared_ptr<F77W>> res;
    if(!d) return res;
    for(auto& r:d->query("SELECT * FROM f77_workflows WHERE status='active' ORDER BY initiated_date DESC;",{}))
    { auto wf=std::make_shared<F77W>(); wf->fromRow(r); wf->loadSteps(); res.push_back(wf); }
    return res;
}

// ─────────────────────────────────────────────────────────────
// F77Engine
// ─────────────────────────────────────────────────────────────
Database* F77Engine::db() { return wfDB(); }

// ── Helper: store workflow ID back into entity ────────────────
// ── Entity context lookup ─────────────────────────────────────
// Maps an entity type string to its database pool, table name, and ID column.
// Centralises the entityType switch that was duplicated in 18 places.
struct EntityCtx {
    Database*   db        { nullptr };
    std::string table;
    std::string idCol;
    std::string wfIdCol   { "release_workflow_id" }; ///< SQL column for F77 workflow ID
    bool valid() const { return db != nullptr; }

    // Read the attached F77 workflow ID without loading the full model object.
    std::string getWorkflowId(const std::string& entityId) const {
        if (!db) return {};
        auto rows = db->query(
            "SELECT " + wfIdCol + " FROM " + table + " WHERE " + idCol + "=?;",
            { BindParam::text(entityId) });
        if (rows.empty()) return {};
        auto it = rows[0].find(wfIdCol);
        return (it != rows[0].end() && !it->second.empty()) ? it->second : std::string{};
    }
};

static EntityCtx entityContext(const std::string& entityType) {
    auto& pool = DatabasePool::instance();
    if (entityType == "f16")
        return { pool.get("f16"), "projects",       "project_id",   "release_workflow_id" };
    if (entityType == "f22")
        return { pool.get("f22"), "tasks",          "task_id",      "release_workflow_id" };
    if (entityType == "f18")
        return { pool.get("f18"), "f18_operations", "operation_id", "release_workflow_id" };
    if (entityType == "akt")
        return { pool.get("akt"), "folders",        "folder_id",    "workflow_id" };
    return {};
}



/// Set wf_locked=1 on all direct child entities (cascading freeze).
static void propagateWfLockedToChildren(const std::string& entityType,
                                         const std::string& entityId,
                                         int locked) {
    if (entityType == "f22") {
        // Freeze F18 children:
        auto* f18db = DatabasePool::instance().get("f18");
        if (f18db) f18db->exec(
            "UPDATE f18_operations SET wf_locked=?, updated_at=? WHERE task_id=?;",
            {BindParam::int64(locked), BindParam::text(nowIso()), BindParam::text(entityId)});
        // Freeze AKT children:
        auto* aktdb = DatabasePool::instance().get("akt");
        if (aktdb) aktdb->exec(
            "UPDATE folders SET wf_locked=?, updated_at=? "
            "WHERE folder_id IN (SELECT folder_id FROM entity_folders "
            "WHERE entity_type='f22' AND entity_id=?);",
            {BindParam::int64(locked), BindParam::text(nowIso()), BindParam::text(entityId)});
    } else if (entityType == "f18") {
        // Freeze AKT children:
        auto* aktdb = DatabasePool::instance().get("akt");
        if (aktdb) aktdb->exec(
            "UPDATE folders SET wf_locked=?, updated_at=? "
            "WHERE folder_id IN (SELECT folder_id FROM entity_folders "
            "WHERE entity_type='f18' AND entity_id=?);",
            {BindParam::int64(locked), BindParam::text(nowIso()), BindParam::text(entityId)});
    }
}

static void storeWorkflowId(const std::string& entityType,
                              const std::string& entityId,
                              const std::string& workflowId) {
    auto ctx = entityContext(entityType);
    if (!ctx.valid()) { LOG_WARN("[F77] storeWorkflowId: unknown entityType: " + entityType); return; }
    std::string sql = std::string("UPDATE ") + ctx.table +
                      " SET " + ctx.wfIdCol + "=?, wf_locked=1, updated_at=? WHERE " + ctx.idCol + "=?;";
    ctx.db->exec(sql, { BindParam::text(workflowId), BindParam::text(nowIso()), BindParam::text(entityId) });
    propagateWfLockedToChildren(entityType, entityId, 1);
    LOG_INFO("[F77] wfLocked=1 propagated to children of " + entityType + "/" + entityId);
}

// ── Public Engine methods: attach / detach workflow ID on entity ──────────
void F77Engine::cancelWorkflow(F77W& wf) {
    wf.status = WorkflowStatus::CANCELLED;
    wf.update();
    detachWorkflow(wf.entityType, wf.entityId);
    LOG_INFO("[F77] Workflow cancelled: " + wf.workflowId +
             " for " + wf.entityType + "/" + wf.entityId);
}


void F77Engine::attachWorkflow(const std::string& entityType,
                                 const std::string& entityId,
                                 const std::string& workflowId) {
    storeWorkflowId(entityType, entityId, workflowId);
}

void F77Engine::detachWorkflow(const std::string& entityType,
                                 const std::string& entityId) {
    auto ctx = entityContext(entityType);
    if (!ctx.valid()) return;
    std::string sql = std::string("UPDATE ") + ctx.table +
                      " SET " + ctx.wfIdCol + "=NULL, wf_locked=0, updated_at=? WHERE " + ctx.idCol + "=?;";
    ctx.db->exec(sql, { BindParam::text(nowIso()), BindParam::text(entityId) });
    propagateWfLockedToChildren(entityType, entityId, 0);
    LOG_INFO("[F77] wfLocked=0 cleared from children of " + entityType + "/" + entityId);
}


// ── // When a DOK gets a F77 workflow, create an F24 on the parent
// F22 (via its tracking F18) so the document release is visible in the chain.



// ── F77Engine::defaultOperations ────────────────────────────────────────
// Single configuration point for the default workflow step chain per entity type.
// To add a system step: add an OperationSpec to the relevant list.
// To remove a step:   remove its OperationSpec.
// To reorder:         change the vector order — steps chain sequentially.
std::vector<F77Engine::OperationSpec> F77Engine::defaultOperations(
    const std::string& entityType)
{
    if (entityType == "akt") {
        // AKT has no child entities — just scan + commit.
        return {
            {"Objektverwaltung", true, SystemAction::SCAN_UNREGISTERED_FILES, true},
            {"DB schreiben",     true, SystemAction::COMMIT_DB_OBJECTS,       true},
        };
    }
    // F16, F22, F18: check children first, then scan for files, then commit.
    return {
        {"Kindelemente",     true, SystemAction::CHECK_CHILDREN,           true},
        {"Objektverwaltung", true, SystemAction::SCAN_UNREGISTERED_FILES,  true},
        {"DB schreiben",     true, SystemAction::COMMIT_DB_OBJECTS,        true},
    };
}


std::shared_ptr<F77W> F77Engine::startFromTemplate(
    const std::string& templateId, const std::string& entityType,
    const std::string& entityId, const std::string& initiatedBy)
{
    // Enforce: exactly one active workflow per entity at a time.
    {
        // EntityCtx reads releaseWorkflowId via SQL — no need to load full objects
        const auto ectx = entityContext(entityType);
        std::string existingId = ectx.valid() ? ectx.getWorkflowId(entityId) : std::string{};
        if (!existingId.empty()) {
            auto existing = F77W::loadById(existingId);
            if (existing && existing->status == WorkflowStatus::ACTIVE) {
                LOG_WARN("[F77] startFromTemplate verweigert: bereits aktiver Workflow ("
                         + existingId + ") fuer " + entityType + "/" + entityId);
                return nullptr;
            }
        }
    }
    auto tpl = F77W_Template::loadById(templateId);
    if(!tpl) { LOG_ERROR("[F77] Template not found: "+templateId); return nullptr; }

    auto wf = F77W::create(entityType, entityId,
                                    tpl->name, tpl->targetState, initiatedBy);
    wf->templateId = templateId;
    if (!opOk(wf->save())) return nullptr;

    // Snapshot template steps → F77W_Operation (+ F18_Operation per mid-step)
    // Build map: tpl_step_id → runtime step_id for predecessor resolution
    std::map<std::string,std::string> tplToRuntime;
    tpl->loadSteps();


    for(auto& ts : tpl->steps) {
        F77W_Operation rs;
        rs.stepId        = genId("F77S");
        rs.workflowId    = wf->workflowId;
        rs.tplStepId     = ts.tplStepId;
        rs.title         = ts.title;
        rs.sequenceOrder = ts.sequenceOrder;
        rs.isInitialize  = ts.isInitialize;
        rs.isFinal       = ts.isFinal;
        rs.executionMode = ts.executionMode;
        rs.autoApprove   = ts.autoApprove;
        rs.isSystem      = ts.isSystem;
        rs.systemAction  = ts.systemAction;
        rs.requiresComment   = ts.requiresComment;
        rs.requiresDocument  = ts.requiresDocument;
                rs.status        = ts.isInitialize ? StepStatus::APPROVED : StepStatus::PENDING;
        rs.completedDate = ts.isInitialize ? nowIso() : "";
        rs.createdAt     = nowIso();
        rs.updatedAt     = nowIso();

        // Resolve predecessor template IDs → runtime step IDs
        if(!ts.predecessorTplStepIds.empty()) {
            std::string resolved;
            std::istringstream ss(ts.predecessorTplStepIds); std::string tid;
            while(std::getline(ss, tid, ',')) {
                tid.erase(0,tid.find_first_not_of(' ')); tid.erase(tid.find_last_not_of(' ')+1);
                if(tid.empty()) continue;
                auto it=tplToRuntime.find(tid);
                if(it!=tplToRuntime.end()) {
                    if(!resolved.empty()) resolved+=",";
                    resolved+=it->second;
                }
            }
            rs.predecessors = F77W_Operation::predecessorsFromString(resolved);
        }


        rs.save();
        tplToRuntime[ts.tplStepId] = rs.stepId;
        wf->steps.push_back(rs);
    }

    // Inject Objektverwaltung + DB schreiben before End (template workflows).
    {
        std::string endStepId, lastManualId;
        int maxNonEndSeq = -1;
        for (auto& s : wf->steps) {
            if (s.isFinal) { endStepId = s.stepId; continue; }
            if (s.sequenceOrder > maxNonEndSeq) { maxNonEndSeq = s.sequenceOrder; lastManualId = s.stepId; }
        }
        if (!endStepId.empty() && !lastManualId.empty()) {
            // Objektverwaltung: auto if no loose files, else spawns F77Tasks
            F77W_Operation objStep;
            objStep.stepId        = genId("F77S"); objStep.workflowId = wf->workflowId;
            objStep.title         = "Objektverwaltung";
            objStep.sequenceOrder = maxNonEndSeq + 1;
            objStep.executionMode = "sequential";
            objStep.predecessors  = { lastManualId };
            objStep.autoApprove   = true; objStep.isSystem = true;
            objStep.systemAction  = SystemAction::SCAN_UNREGISTERED_FILES;
            objStep.status        = StepStatus::PENDING;
            objStep.createdAt     = nowIso(); objStep.updatedAt = nowIso();
            objStep.save(); wf->steps.push_back(objStep);

            // DB schreiben: runs after Objektverwaltung
            F77W_Operation dbStep;
            dbStep.stepId        = genId("F77S"); dbStep.workflowId = wf->workflowId;
            dbStep.title         = "DB schreiben";
            dbStep.sequenceOrder = maxNonEndSeq + 2;
            dbStep.executionMode = "sequential";
            dbStep.predecessors  = { objStep.stepId };
            dbStep.autoApprove   = true; dbStep.isSystem = true;
            dbStep.systemAction  = SystemAction::COMMIT_DB_OBJECTS;
            dbStep.status        = StepStatus::PENDING;
            dbStep.createdAt     = nowIso(); dbStep.updatedAt = nowIso();
            dbStep.save(); wf->steps.push_back(dbStep);

            // Rewire End step:
            for (auto& s : wf->steps) {
                if (s.stepId == endStepId) {
                    s.predecessors = { dbStep.stepId };
                    s.save(); break;
                }
            }
            LOG_INFO("[F77] Injected Objektverwaltung + DB schreiben before End");
        }
    }

    LOG_INFO("[F77] Workflow started from template '"+tpl->name+"' for "+entityType+"/"+entityId);
    // NOTE: tick() is NOT called here.
    // The caller (startWfInstanceWizard or test) calls tick() after any manual
    // operations are added. This prevents premature auto-completion.
    storeWorkflowId(entityType, entityId, wf->workflowId);
    return wf;
}


std::shared_ptr<F77W> F77Engine::startDefault(
    const std::string& entityType, const std::string& entityId,
    EntityStatus targetState, const std::string& initiatedBy)
{
    // Enforce: exactly one active workflow per entity at a time.
    {
        auto ctx = entityContext(entityType);
        if (!ctx.valid()) { LOG_ERROR("[F77] startDefault: unknown entityType: " + entityType); return nullptr; }
        auto rows = ctx.db->query(
            std::string("SELECT ") + ctx.wfIdCol + " FROM " + ctx.table +
            " WHERE " + ctx.idCol + "=?;",
            { BindParam::text(entityId) });
        if (!rows.empty()) {
            auto it = rows[0].find(ctx.wfIdCol);
            if (it != rows[0].end() && !it->second.empty()) {
                auto existing = F77W::loadById(it->second);
                if (existing && existing->status == WorkflowStatus::ACTIVE) {
                    LOG_WARN("[F77] startDefault: workflow already active: " + it->second);
                    return nullptr;
                }
            }
        }
    }

    // Use first matching active template, else build minimal Init→Freigabe→End
    auto templates = F77W_Template::loadForEntityType(entityType);
    for(auto& t : templates) {
        if(t->status == TemplateStatus::ACTIVE && t->targetState==targetState) {
            return startFromTemplate(t->templateId, entityType, entityId, initiatedBy);
        }
    }

    // No template found — minimal workflow
    auto wf = F77W::create(entityType, entityId,
                                    "Standard-Freigabe", targetState, initiatedBy);
    if (!opOk(wf->save())) return nullptr;

    std::string projId;
    if(entityType=="f22"){ auto t=F22::loadById(entityId); if(t) projId=t->projectId; }

    // Init step (auto-approved)
    F77W_Operation init;
    init.stepId=genId("F77S"); init.workflowId=wf->workflowId;
    init.title="Init"; init.sequenceOrder=0; init.isInitialize=true;
    init.autoApprove=true; init.status=StepStatus::APPROVED; init.executionMode="sequential";
    init.completedDate=nowIso(); init.createdAt=nowIso(); init.updatedAt=nowIso();
    init.save(); wf->steps.push_back(init);

    // Build step chain from declarative OperationSpec list.
    // To change which steps run for an entity: edit defaultOperations().
    auto specs = defaultOperations(entityType);
    std::string prevStepId = init.stepId;
    int seq = 1;
    for (auto& spec : specs) {
        F77W_Operation op;
        op.stepId        = genId("F77S"); op.workflowId = wf->workflowId;
        op.title         = spec.title;
        op.sequenceOrder = seq++;
        op.executionMode = "sequential";
        op.predecessors  = {prevStepId};
        op.autoApprove   = spec.autoApprove;
        op.isSystem      = spec.isSystem;
        op.systemAction  = spec.action;
        op.status        = StepStatus::PENDING;
        op.createdAt     = nowIso(); op.updatedAt = nowIso();
        op.save(); wf->steps.push_back(op);
        prevStepId = op.stepId;
    }

    // End step — always last, chains from final system step.
    F77W_Operation end;
    end.stepId=genId("F77S"); end.workflowId=wf->workflowId;
    end.title="End"; end.sequenceOrder=9999; end.isFinal=true;
    end.autoApprove=true; end.predecessors={prevStepId};
    end.status=StepStatus::PENDING; end.executionMode="sequential";
    end.createdAt=nowIso(); end.updatedAt=nowIso();
    end.save(); wf->steps.push_back(end);

    storeWorkflowId(entityType, entityId, wf->workflowId);

    // NOTE: tick() is NOT called here.
    // startWfInstanceWizard calls tick() after adding any manual operations.
    // Direct callers that don't use the wizard must call tick() themselves.
    return wf;
}

// ── MFS pruning helper ────────────────────────────────────────
// When saveSpace=true: removes MFS copies of revisions that are neither
// in_work nor the superseded=false (active) revision.
static void pruneMFSRevisions(const std::string& docId) {
    auto* db = DatabasePool::instance().get("akt");
    if (!db) return;

    // Find revisions to keep: in_work OR superseded=0
    auto keepRows = db->query(
        "SELECT rev FROM folder_revisions WHERE folder_id=? "
        "AND (rev_state='in_work' OR superseded=0);",
        {BindParam::text(docId)});

    std::set<uint32_t> keepRevs;
    for (auto& r : keepRows) {
        auto it = r.find("rev");
        if (it != r.end() && !it->second.empty())
            keepRevs.insert((uint32_t)std::stoi(it->second));
    }

    // Find all revisions
    auto allRows = db->query(
        "SELECT rev FROM folder_revisions WHERE folder_id=?;",
        {BindParam::text(docId)});

    for (auto& r : allRows) {
        auto it = r.find("rev");
        if (it == r.end() || it->second.empty()) continue;
        uint32_t rev = (uint32_t)std::stoi(it->second);
        if (keepRevs.count(rev)) continue;

        // Remove MFS folder for this revision
        std::string revDir = Rosenholz::FolderObject::mfsRevDir(docId, rev);
        if (!revDir.empty() && Rosenholz::FileOps::fileExists(revDir)) {
            // Use system call to remove directory tree
            std::string cmd = "rm -rf "" + revDir + """;
            (void)std::system(cmd.c_str());
            LOG_INFO("[F77] pruned MFS rev folder: " + revDir);
        }
    }
}


// ── F77Engine::scanLooseFiles ────────────────────────────────────────────
// Delegates to each entity's own scan logic. F77 does not know internals.
// Adding a new entity type: add one case here.
std::vector<std::pair<std::string,std::string>>
F77Engine::scanLooseFiles(const std::string& entityType,
                            const std::string& entityId)
{
    using namespace Rosenholz;
    if (entityType == "f22") {
        auto t = F22::loadById(entityId);
        return t ? t->scanMfsForUnregistered()
                 : std::vector<std::pair<std::string,std::string>>{};
    }
    if (entityType == "akt") {
        auto d = Folder::loadById(entityId);
        if (!d) return {};
        auto curRev = FolderRevision::currentRevision(entityId);
        if (!curRev) return {};
        auto files = FolderObject::scanForUnregisteredFiles(entityId, curRev->rev);
        std::vector<std::pair<std::string,std::string>> result;
        for (auto& f : files) {
            std::string dir = d->mfsDir();
            std::string rel = (f.size() > dir.size() + 1)
                ? f.substr(dir.size() + 1) : FileOps::baseName(f);
            result.emplace_back(f, rel);
        }
        return result;
    }
    if (entityType == "f18") {
        auto op = F18Operation::loadById(entityId);
        if (!op) return {};
        // F18 scan: look for files in its MFS sub-folder
        const std::string& root = Config::instance().mfsPath();
        std::string dir = FileOps::joinPath(
            FileOps::joinPath(root, "F18"), entityId);
        std::vector<std::pair<std::string,std::string>> result;
        for (auto& f : FileOps::listFiles(dir, true)) {
            auto b = FileOps::baseName(f);
            if (b == "_SCHLUESSEL.txt" || b == "00_KARTE.txt"
                || b == "owner_key.txt" || b.empty()) continue;
            std::string rel = (f.size() > dir.size() + 1)
                ? f.substr(dir.size() + 1) : b;
            result.emplace_back(f, rel);
        }
        return result;
    }
    return {}; // unknown entity type
}


// Execute a system step (isSystem=true). Currently only one action exists:
// SystemAction::COMMIT_DB_OBJECTS — commits all uncommitted FolderObjects to LMDB.
// Adding a new system action = adding a case to the switch below.
static void executeSystemStep(F77W_Operation& step,
                               F77W& wf, bool& changed) {
    // ── 3b: SystemAction enum replaces magic string comparison ──────────────
    // step.systemAction determines what this step does.
    // COMMIT_DB_OBJECTS is the only action currently; the enum makes it
    // extensible without touching tick().
    // ── CHECK_CHILDREN ───────────────────────────────────────────────────────
    // Must run BEFORE the step is considered done.
    // If blocking children exist, revert step to IN_PROGRESS and spawn F77Tasks.
    // tick() will re-enter here on every tick and check if tasks are now closed.
    if (step.systemAction == SystemAction::CHECK_CHILDREN) {
        // Check if previously spawned tasks are all done:
        auto prevTasks = F77Task::loadForOperation(step.stepId);
        if (!prevTasks.empty()) {
            bool allClosed = true;
            for (auto& t : prevTasks) if (!t->isClosed()) { allClosed = false; break; }
            if (allClosed) {
                LOG_INFO("[F77] CHECK_CHILDREN: all child tasks closed for " + wf.entityId);
                return;  // step stays APPROVED
            }
            // Still waiting — revert:
            step.status = StepStatus::IN_PROGRESS; step.completedDate = ""; step.save();
            changed = false;
            LOG_INFO("[F77] CHECK_CHILDREN: still waiting for child tasks on " + wf.entityId);
            return;
        }

        // No tasks yet — collect blocking children:
        std::vector<std::pair<std::string,std::string>> blockingChildren;

        // AKT children of this entity:
        auto* aktdb = DatabasePool::instance().get("akt");
        if (aktdb) {
            // entity_folders links AKTs to F22/F18:
            auto rows = aktdb->query(
                "SELECT f.folder_id FROM folders f "
                "JOIN entity_folders ef ON f.folder_id=ef.folder_id "
                "WHERE ef.entity_type=? AND ef.entity_id=?;",
                {BindParam::text(wf.entityType), BindParam::text(wf.entityId)});
            for (auto& r : rows) {
                std::string cid = r.count("folder_id") ? r.at("folder_id") : "";
                if (cid.empty()) continue;
                auto rev = FolderRevision::currentRevision(cid);
                if (rev && rev->revState == RevState::IN_WORK)
                    blockingChildren.push_back({"akt", cid});
            }
        }

        // F18 children of F22:
        if (wf.entityType == "f22") {
            auto* f18db = DatabasePool::instance().get("f18");
            if (f18db) {
                auto rows = f18db->query(
                    "SELECT operation_id, status FROM f18_operations WHERE task_id=?;",
                    {BindParam::text(wf.entityId)});
                for (auto& r : rows) {
                    std::string cid    = r.count("operation_id") ? r.at("operation_id") : "";
                    std::string cstatus = r.count("status") ? r.at("status") : "in_work";
                    if (!cid.empty() && entityStatusFrom(cstatus) == EntityStatus::IN_WORK)
                        blockingChildren.push_back({"f18", cid});
                }
            }
        }

        if (blockingChildren.empty()) {
            // No blocking children — step is correctly approved.
            LOG_INFO("[F77] CHECK_CHILDREN: no blocking children for " + wf.entityId);
            return;
        }

        // Blocking children found: revert step, spawn one F77Task per child:
        step.status = StepStatus::IN_PROGRESS; step.completedDate = ""; step.save();
        changed = false;
        LOG_INFO("[F77] CHECK_CHILDREN: " + std::to_string(blockingChildren.size())
                 + " blocking child(ren) for " + wf.entityId + " — spawning tasks");
        for (auto& [ct, cid] : blockingChildren) {
            std::string title = "Freigabe: " + ct + " " + cid
                + " (ausgeloest durch: " + wf.entityType + "/" + wf.entityId + ")";
            F77Task::create(wf.workflowId, step.stepId, title, ct, cid,
                            "start_release_workflow");
            LOG_INFO("[F77] CHECK_CHILDREN: spawned F77Task for " + ct + "/" + cid);
        }
        return;
    }

    // ── SCAN_UNREGISTERED_FILES ─────────────────────────────────────────────
    if (step.systemAction == SystemAction::SCAN_UNREGISTERED_FILES) {
        // Scan the entity's MFS folder for files not registered as AKT objects.
        // If none: auto-approve immediately (step was already approved by tick).
        // If some: revoke the auto-approval, spawn one F77Task per file,
        //          and block until all tasks are closed.
        std::vector<std::pair<std::string,std::string>> loose;

        loose = Rosenholz::F77Engine::scanLooseFiles(wf.entityType, wf.entityId);

        if (loose.empty()) {
            LOG_INFO("[F77] Objektverwaltung: no loose files — auto-approved for "
                     + wf.entityId);
            return; // step stays approved
        }

        // Files found: revoke auto-approval, spawn F77Tasks
        step.status = StepStatus::IN_PROGRESS; step.completedDate = ""; step.save();
        changed = false; // don't let tick advance further

        // Check if tasks already spawned for this operation:
        auto existing = F77Task::loadForOperation(step.stepId);
        if (!existing.empty()) {
            LOG_INFO("[F77] Objektverwaltung: tasks already spawned, waiting ("
                     + std::to_string(existing.size()) + " tasks)");
            return;
        }

        for (auto& [fpath, fname] : loose) {
            std::string taskTitle = "Nicht abgelegte Datei verwalten: " + fname;
            F77Task::create(
                wf.workflowId,
                step.stepId,       // operationId
                taskTitle,
                wf.entityType,
                wf.entityId,
                "nacherfassen",    // targetAction
                fpath,             // filePath
                fname);            // fileName
        }
        LOG_INFO("[F77] Objektverwaltung: spawned " + std::to_string(loose.size())
                 + " F77Tasks for " + wf.entityId);
        return;
    }

    if (step.systemAction != SystemAction::COMMIT_DB_OBJECTS) return;

    // For non-DOK entities: write MFS index files (F22/F18 only — F16 has no workflow).
    if (wf.entityType != "akt") {
        const std::string mfsRoot = Rosenholz::Config::instance().mfsPath();
        if (wf.entityType == "f22") {
            auto task = Rosenholz::F22::loadById(wf.entityId);
            if (task) { MFSWriter::writeTask(*task, mfsRoot);
                LOG_INFO("[F77] MFS geschrieben fuer F22 " + wf.entityId); }
        } else if (wf.entityType == "f18") {
            auto operation = Rosenholz::F18Operation::loadById(wf.entityId);
            if (operation) { MFSWriter::writeF18(*operation, mfsRoot);
                LOG_INFO("[F77] MFS geschrieben fuer F18 " + wf.entityId); }
        }
        return;
    }

    auto curRev = Rosenholz::FolderRevision::currentRevision(wf.entityId);
    if (!curRev) return;

    auto objs = Rosenholz::FolderObject::loadForRevision(wf.entityId, curRev->rev);
    bool allOk = true;
    for (auto& obj : objs) {
        if (!obj->committed) {
            auto res = obj->commitToLMDB();
            if (!Rosenholz::opOk(res)) {
                allOk = false;
                LOG_ERROR("[F77] COMMIT_DB_OBJECTS: failed for " + obj->objectId +
                          " — " + Rosenholz::opResultMessage(res));
            }
        }
    }

    if (!allOk) {
        step.status = StepStatus::PENDING; step.completedDate = ""; step.save(); changed = false;
        LOG_ERROR("[F77] COMMIT_DB_OBJECTS step FAILED — reverted to pending. "
                  "Fix MFS files and re-run tick.");
        return;
    }

    if (Rosenholz::Config::instance().storage().saveSpace)
        pruneMFSRevisions(wf.entityId);

    LOG_INFO("[F77] COMMIT_DB_OBJECTS: all committed for " +
             wf.entityId + " rev " + std::to_string(curRev->rev));
}


bool F77Engine::tick(F77W& wf) {
    if(wf.status!=WorkflowStatus::ACTIVE) return false;
    wf.loadSteps();
    bool changed=false;

    for(auto& s : wf.steps) {
        // Sync status from linked F18_Operation

        // Auto-approve Init and autoApprove steps
        if(s.status==StepStatus::PENDING && s.autoApprove && s.canStart(wf.steps)) {
            s.status=StepStatus::APPROVED; s.completedDate=nowIso(); s.save(); changed=true;
            LOG_INFO("[F77] Auto-approved step '"+s.title+"'");
            if(s.isSystem) executeSystemStep(s, wf, changed);
        }

        // Re-check in_progress steps.
        if(s.status==StepStatus::IN_PROGRESS) {
            if (s.isSystem) {
                // System steps (CHECK_CHILDREN, SCAN) re-run their logic
                // to verify children are actually in target state, not just
                // that the tasks are closed.
                executeSystemStep(s, wf, changed);
                // executeSystemStep will approve or keep IN_PROGRESS.
            } else {
                // Manual operation: approve when all F77Tasks are closed.
                auto tasks = F77Task::loadForOperation(s.stepId);
                if (!tasks.empty() && F77Task::checkOperationComplete(s.stepId)) {
                    s.status=StepStatus::APPROVED; s.completedDate=nowIso(); s.save(); changed=true;
                    LOG_INFO("[F77] Manual step approved (all tasks closed): " + s.title);
                }
            }
        }

        // Mark in_progress for ready mid-steps
        if(s.status==StepStatus::PENDING && !s.isInitialize && !s.isFinal
           && s.canStart(wf.steps)) {
            s.status=StepStatus::IN_PROGRESS; s.save(); changed=true;
        }

        // End step: auto-approve when all mid-steps done AND no open F77Tasks remain
        if(s.isFinal && s.status==StepStatus::PENDING && wf.isComplete()) {
            // Guard: check all F77Tasks spawned by this workflow are closed
            auto pendingTasks = F77Task::loadForWorkflow(wf.workflowId);
            bool hasOpenTasks = false;
            for (auto& t : pendingTasks) if (t->isOpen()) { hasOpenTasks = true; break; }
            if (hasOpenTasks) {
                LOG_INFO("[F77] End step blocked: open F77Tasks pending for "
                         + wf.workflowId);
            } else {
                s.status=StepStatus::APPROVED; s.completedDate=nowIso(); s.save(); changed=true;
                LOG_INFO("[F77] End step auto-approved — workflow closing");
            }
        }
    }

    if(changed) checkAndComplete(wf);
    return changed;
}


// ── F77Engine::addManualOperation ───────────────────────────────────────
// Adds a non-auto-approved operation to a running workflow,
// then creates a F77Task as the actionable item for -tasks.
// No F18 Operation is spawned — the F77Task IS the work item.
std::string F77Engine::addManualOperation(
    F77W&      wf,
    const std::string& title,
    const std::string& /*description*/,
    const std::string& assignedTo)
{
    // Always reload from DB to get current status:
    auto fresh = F77W::loadById(wf.workflowId);
    if (fresh) { wf.status = fresh->status; wf.steps = fresh->steps; }
    if (wf.status != WorkflowStatus::ACTIVE) {
        LOG_WARN("[F77] addManualOperation: workflow not active (status=" + std::string(toString(wf.status)) + "): " + wf.workflowId);
        return "";
    }

    wf.loadSteps();

    // Manual operations insert BEFORE Objektverwaltung (SCAN step).
    // Chain: Init → [manual ops] → Objektverwaltung → DB schreiben → End
    int insertBefore = 9998;
    std::string lastManualId;   // last manual (non-auto, non-system) step
    std::string scanStepId;     // the Objektverwaltung step
    for (auto& s : wf.steps) {
        if (s.isFinal) { insertBefore = s.sequenceOrder - 1; continue; }
        if (s.isSystem && s.systemAction == SystemAction::SCAN_UNREGISTERED_FILES)
            { scanStepId = s.stepId; insertBefore = s.sequenceOrder - 1; continue; }
        if (s.isSystem && s.systemAction == SystemAction::COMMIT_DB_OBJECTS)
            { insertBefore = std::min(insertBefore, s.sequenceOrder - 1); continue; }
        if (!s.isInitialize && !s.autoApprove)
            lastManualId = s.stepId;
    }

    // Build the operation:
    F77W_Operation op;
    op.stepId        = genId("F77S");
    op.workflowId    = wf.workflowId;
    op.title         = title;
    op.autoApprove   = false;
    op.isInitialize  = false;
    op.isFinal       = false;
    op.isSystem      = false;
    op.status        = StepStatus::IN_PROGRESS; // task-driven — tick polls this
    op.sequenceOrder = insertBefore;
    op.executionMode = "sequential";
    // Chain after last manual step (or Init if none)
    if (!lastManualId.empty()) {
        op.predecessors = {lastManualId};
    } else {
        for (auto& s : wf.steps)
            if (s.isInitialize) { op.predecessors = {s.stepId}; break; }
    }
    op.createdAt = nowIso();
    op.updatedAt = nowIso();
    if (!opOk(op.save())) {
        LOG_ERROR("[F77] addManualOperation: save failed");
        return "";
    }
    wf.steps.push_back(op);

    // Re-wire successors: manual op must complete before the next pending system step.
    // If SCAN (Objektverwaltung) is already approved, wire into DB schreiben.
    for (auto& s : wf.steps) {
        bool isScan = (s.isSystem && s.systemAction == SystemAction::SCAN_UNREGISTERED_FILES);
        bool isDb   = (s.isSystem && s.systemAction == SystemAction::COMMIT_DB_OBJECTS);
        // Wire into SCAN if pending/in_progress, into DB if SCAN already done
        bool shouldWire = false;
        if (isScan && !s.isComplete()) shouldWire = true;
        if (isDb && !s.isComplete() && !isScan) {
            // Only wire DB if SCAN is already complete (or absent)
            bool scanDone = true;
            for (auto& s2 : wf.steps)
                if (s2.isSystem && s2.systemAction == SystemAction::SCAN_UNREGISTERED_FILES
                    && !s2.isComplete()) { scanDone = false; break; }
            if (scanDone) shouldWire = true;
        }
        if (s.isFinal && !s.isComplete()) shouldWire = true;
        if (shouldWire) {
            bool already = false;
            for (auto& p : s.predecessors) if (p == op.stepId) { already = true; break; }
            if (!already) s.predecessors.push_back(op.stepId);
            s.save();
        }
    }

    // ── Create a F77Task as the actionable work item ─────────────────────
    std::string taskTitle = title;
    auto task = F77Task::create(
        wf.workflowId,
        op.stepId,        // operationId — so checkOperationComplete can close the op
        taskTitle,
        wf.entityType,    // navigate back to the entity
        wf.entityId,
        "review",         // generic hint
        "",               // no file path for user-added steps
        "",
        assignedTo);
    if (task) {
        LOG_INFO("[F77] F77Task created: " + task->taskId + " for operation " + op.stepId);
    }

    LOG_INFO("[F77] Manual operation added: " + op.stepId + " \"" + title + "\"");
    return op.stepId;
}

// Validate whether a step can be fired — dry-run, no state change
std::string F77Engine::validateStep(
    const F77W& wf,
    const std::string& stepId)
{
    for(const auto& s : wf.steps) {
        if(s.stepId != stepId) continue;
        if(s.isComplete())
            return std::string("BLOCKED: Schritt ist bereits abgeschlossen (") + toString(s.status) + ")";
        if(!s.canStart(wf.steps))
            return "BLOCKED: Vorgaenger-Schritte noch nicht abgeschlossen";
        if(s.isInitialize)
            return "INFO: Init-Schritt wird automatisch genehmigt";
        if(s.isFinal) {
            // Check all mid-steps done
            bool allDone = true;
            for(const auto& other : wf.steps)
                if(!other.isInitialize && !other.isFinal && !other.isComplete())
                    allDone = false;
            if(!allDone)
                return "BLOCKED: Nicht alle Zwischen-Schritte abgeschlossen";
            return "OK: End-Schritt kann ausgefuehrt werden — Workflow wird abgeschlossen";
        }
        return "OK: Schritt kann ausgefuehrt werden";
    }
    return "FEHLER: Schritt nicht gefunden: " + stepId;
}

bool F77Engine::fireStep(F77W& wf, const std::string& stepId,
                           const std::string& decision, const std::string& actorId,
                           const std::string& comment)
{
    wf.loadSteps();
    F77W_Operation* step=nullptr;
    for(auto& s:wf.steps) if(s.stepId==stepId){step=&s;break;}
    if(!step) { LOG_WARN("[F77] Step not found: "+stepId); return false; }
    if(step->isComplete()) { LOG_WARN("[F77] Step already complete: "+stepId); return false; }
    if(!step->canStart(wf.steps)) { LOG_WARN("[F77] Prerequisites not met: "+stepId); return false; }
    if(step->requiresComment && comment.empty()) { LOG_WARN("[F77] Comment required: "+stepId); return false; }


    step->status = stepStatusFrom(decision);
    step->completedDate = nowIso();
    step->save();


    LOG_INFO("[F77] Step fired: "+stepId+" decision="+decision+" by="+actorId);
    tick(wf);
    return true;
}


bool F77Engine::checkAndComplete(F77W& wf) {
    // All steps done (including End)?
    for(auto& s:wf.steps) if(!s.isComplete()) return false;
    wf.status = WorkflowStatus::COMPLETED;
    wf.completedDate = nowIso();
    wf.save();
    LOG_INFO("[F77] Workflow completed: "+wf.workflowId);
    applyTargetState(wf);
    return true;
}

bool F77Engine::applyTargetState(const F77W& wf) {
    // ── AKT: transition via FolderRevision state machine ─────────────────────
    if (wf.entityType == "akt") {
        auto rev = FolderRevision::currentRevision(wf.entityId);
        if (!rev) {
            auto folder = Folder::loadById(wf.entityId);
            if (folder) {
                auto newRev = folder->revise("Initial — Freigabe-Workflow", "system");
                if (newRev) rev = FolderRevision::currentRevision(wf.entityId);
            }
        }
        if (rev && FolderRevision::isTransitionAllowed(rev->revState,
             revStateFromString(entityStatusToString(wf.targetState)))) {
            rev->transitionState(revStateFromString(entityStatusToString(wf.targetState)), true);
            LOG_INFO("[F77] AKT revision transitioned: " + wf.entityId
                     + " → " + std::string(entityStatusToString(wf.targetState)));
        }
        return true;
    }

    // ── F22 / F18 / F16: directly set the target status ──────────────────────
    // Child-handling is done by the CHECK_CHILDREN system step BEFORE this runs.
    // By the time applyTargetState fires, all children are already in target state.
    auto actx = entityContext(wf.entityType);
    if (actx.valid()) {
        actx.db->exec(
            "UPDATE " + actx.table + " SET status=?, wf_locked=0, updated_at=? WHERE "
            + actx.idCol + "=?;",
            {BindParam::text(entityStatusToString(wf.targetState)),
             BindParam::text(nowIso()), BindParam::text(wf.entityId)});
        LOG_INFO("[F77] Entity status set to '"
                 + std::string(entityStatusToString(wf.targetState))
                 + "': " + wf.entityType + "/" + wf.entityId);
    }
    return true;
}



bool F77Engine::canRelease(const std::string& entityType, const std::string& entityId,
                              const std::string& releaseWorkflowId, int& blockerCount)
{
    blockerCount = 0;

    // Check the main workflow itself: all steps must be complete.
    auto mainWf = F77W::loadById(releaseWorkflowId);
    if (!mainWf) {
        blockerCount = 1;
        return false;
    }
    if (mainWf->status != WorkflowStatus::COMPLETED) {
        mainWf->loadSteps();
        for (auto& s : mainWf->steps) {
            if (!s.isInitialize && !s.isComplete()) blockerCount++;
        }
        if (blockerCount > 0) return false;
    }
    // Legacy: also count any OTHER active workflows (should be 0 under new model)
    auto all = F77W::loadForEntity(entityType, entityId);
    for (auto& wf : all) {
        if (wf->workflowId == releaseWorkflowId) continue;
        if (wf->status == WorkflowStatus::ACTIVE) blockerCount++;
    }
    return blockerCount == 0;
}

int F77Engine::lockAll(const std::string& entityType, const std::string& entityId,
                         const std::string& releaseWorkflowId, bool confirmLock)
{
    if(!confirmLock) return -1;
    auto all = F77W::loadForEntity(entityType, entityId);
    int locked=0;
    for(auto& wf : all) {
        if(wf->workflowId==releaseWorkflowId || wf->status != WorkflowStatus::ACTIVE) continue;
        wf->status = WorkflowStatus::LOCKED; wf->save(); locked++;
    }
    return locked;
}

void F77Engine::seedDefaultTemplates() {
    auto* d = wfDB(); if(!d) return;
    try {
        auto existing = d->query("SELECT COUNT(*) as n FROM f77_workflow_templates;",{});
        if (!existing.empty() && existing[0].begin()->second != "0") return;
    } catch(...) { return; }

    bool adminMode = Config::instance().admin().enabled;

    // Helper: Init → SCAN_UNREGISTERED_FILES → COMMIT_DB_OBJECTS → End
    // No fixed "Prüfung" step — CLI asks interactively whether to add manual steps.
    // COMMIT_DB_OBJECTS runs for ALL lifecycle transitions, not just RELEASED.
    auto makeTemplate = [&](const std::string& name,
                             EntityStatus targetState,
                             const std::string& entityTypes,
                             bool adminOnly = false) {
        if (adminOnly && !adminMode) return;
        auto t = F77W_Template::create(name, targetState, entityTypes);
        t->save();

        auto init = t->addTemplateStep("Init", "sequential", true, false);
        init.autoApprove = true;
        init.save();

        auto scan = t->addTemplateStep("Dateien prüfen", "sequential", false, false);
        scan.predecessorTplStepIds = init.tplStepId;
        scan.autoApprove  = true;
        scan.isSystem     = true;
        scan.systemAction = SystemAction::SCAN_UNREGISTERED_FILES;
        scan.save();

        auto commit = t->addTemplateStep("Objekte sichern", "sequential", false, false);
        commit.predecessorTplStepIds = scan.tplStepId;
        commit.autoApprove  = true;
        commit.isSystem     = true;
        commit.systemAction = SystemAction::COMMIT_DB_OBJECTS;
        commit.save();

        auto end = t->addTemplateStep("End", "sequential", false, true);
        end.predecessorTplStepIds = commit.tplStepId;
        end.autoApprove = true;
        end.save();
    };

    const std::string aktTypes = "akt";
    const std::string objTypes = "f22,f18,akt";

    // ── Standard templates ──────────────────────────────────────
    makeTemplate("F77: Sperren",      EntityStatus::LOCKED,       objTypes);
    makeTemplate("F77: Entsperren",   EntityStatus::IN_WORK,      objTypes);
    makeTemplate("F77: Freigabe",     EntityStatus::RELEASED,     objTypes);
    makeTemplate("F77: Schliessen",   EntityStatus::CLOSED,       objTypes);
    makeTemplate("Dokument: Zur Prüfung", EntityStatus::PRE_RELEASED, aktTypes);

    // ── Admin-only templates ─────────────────────────────────────
    if (adminMode) {
        makeTemplate("ADMIN: Dok → in_work",      EntityStatus::IN_WORK,      aktTypes, true);
        makeTemplate("ADMIN: Dok → pre_released", EntityStatus::PRE_RELEASED, aktTypes, true);
        makeTemplate("ADMIN: Dok → released",     EntityStatus::RELEASED,     aktTypes, true);
        makeTemplate("ADMIN: Dok → locked",       EntityStatus::LOCKED,       aktTypes, true);
        makeTemplate("ADMIN: Dok → closed",       EntityStatus::CLOSED,       aktTypes, true);
        makeTemplate("ADMIN: Obj → released",     EntityStatus::RELEASED,     "f22,f18", true);
        makeTemplate("ADMIN: Obj → closed",       EntityStatus::CLOSED,       "f22,f18", true);
    }
}

std::vector<std::shared_ptr<F77W>> F77W::loadAll(int limit) {
    auto* db = wfDB(); if (!db) return {};
    auto rows = db->query(
        "SELECT * FROM f77_workflows ORDER BY created_at DESC LIMIT ?;",
        {BindParam::int64(limit)});
    std::vector<std::shared_ptr<F77W>> result;
    for (auto& r : rows) {
        auto w = std::make_shared<F77W>(); w->fromRow(r); result.push_back(w);
    }
    return result;
}
bool F77Engine::canAddManualOperation(const F77W& wf) {
    if (wf.status != WorkflowStatus::ACTIVE) return false;
    for (auto& s : wf.steps) if (s.isFinal && s.isComplete()) return false;
    return true;
}

// ── F77Engine::handleTaskAction ───────────────────────────────────────────────
// Decodes the task's target_action and executes it.
// Previously this logic was spread across cli_tasks.cpp.
// Centralizing here keeps CLI free of workflow decision logic.
std::string F77Engine::handleTaskAction(F77Task& task,
                                         EntityStatus targetState,
                                         const std::string& actor) {
    const std::string& action = task.targetAction;
    const std::string& cType  = task.targetEntityType;
    const std::string& cId    = task.targetEntityId;

    // start_release_workflow / start_lock_workflow / start_unlock_workflow:
    // Spawn a new F77W on the child entity, then complete this task.
    if (action == "start_release_workflow" ||
        action == "start_lock_workflow"    ||
        action == "start_unlock_workflow") {

        EntityStatus ts = (action == "start_release_workflow") ? EntityStatus::RELEASED
                        : (action == "start_lock_workflow")    ? EntityStatus::LOCKED
                        :                                        EntityStatus::IN_WORK;
        auto childWf = F77Engine::startDefault(cType, cId, ts, actor);
        if (childWf) {
            F77Engine::tick(*childWf);
            return childWf->workflowId;
        }
        return "";
    }

    // "review" or generic: nothing automatic — caller handles UI.
    return "";
}


// ── checkPropagationComplete ──────────────────────────────────────────────────
// Called after any F77Task closes. If the parent WF is COMPLETED and ALL its
// propagation tasks are now closed, re-apply the WF's targetState to the entity.
// This handles the case: F22 WF completed but was blocked waiting for child AKTs.
// Once those AKTs are released, the F22 can finally advance to its target state.
bool F77Engine::checkPropagationComplete(const std::string& workflowId) {
    auto wf = F77W::loadById(workflowId);
    if (!wf || wf->status != WorkflowStatus::COMPLETED) return false;

    // Check all F77Tasks for this workflow:
    auto tasks = F77Task::loadForWorkflow(workflowId);
    for (auto& t : tasks) {
        if (!t->isClosed()) return false;  // still open tasks
    }

    // All tasks done — re-apply the target state to the parent entity.
    // Only if the parent entity is still in_work (it was reverted by propagation):
    auto actx = entityContext(wf->entityType);
    if (!actx.valid()) return false;

    std::string currentStatus;
    auto rows = actx.db->query(
        "SELECT status FROM " + actx.table + " WHERE " + actx.idCol + "=?;",
        {BindParam::text(wf->entityId)});
    if (!rows.empty() && rows[0].count("status"))
        currentStatus = rows[0].at("status");

    if (currentStatus != "in_work") return false;  // already advanced

    LOG_INFO("[F77] All propagation tasks done for " + workflowId +
             " — re-applying target state to " + wf->entityType + "/" + wf->entityId);
    applyTargetState(*wf);
    return true;
}


} // namespace Rosenholz

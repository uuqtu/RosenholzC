// ============================================================
// F77Workflow.cpp — F77 Freigabe-Workflow Engine implementation
// ============================================================
#include "F77Workflow.h"
#include "F77Task.h"
#include "../core/FileOps.h"
#include "../model/dok/DocumentObject.h"
#include "../repository/DocumentRevision.h"
#include "../core/Config.h"
#include <set>
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../model/Utils.h"
#include "../model/f16/ProjectF16.h"
#include "../mfs/MFSWriter.h"
#include "../model/f22/TaskF22.h"
#include "../model/dok/Document.h"
#include "../repository/DocumentRevision.h"
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


// ── DB helper ────────────────────────────────────────────────
static Database* wfDB() { return DatabasePool::instance().get("f77"); }

// ─────────────────────────────────────────────────────────────
// F77_WorkflowTemplateStep
// ─────────────────────────────────────────────────────────────
Database* F77_WorkflowTemplate::db() { return wfDB(); }

void F77_WorkflowTemplateStep::fromRow(const Row& r) {
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

OperationResult F77_WorkflowTemplateStep::save() const {
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

OperationResult F77_WorkflowTemplateStep::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec("DELETE FROM f77_workflow_template_steps WHERE tpl_step_id=?;",
                   {BindParam::text(tplStepId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

// ─────────────────────────────────────────────────────────────
// F77_WorkflowTemplate
// ─────────────────────────────────────────────────────────────
void F77_WorkflowTemplate::fromRow(const Row& r) {
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    templateId  = g("template_id");
    name        = g("name");
    version     = g("version");
    description = g("description");
    entityTypes = g("entity_types");
    targetState = g("target_state");
    status      = g("status");
    createdBy   = g("created_by");
    createdAt   = g("created_at");
    updatedAt   = g("updated_at");
}

OperationResult F77_WorkflowTemplate::save() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_templates
        (template_id,name,version,description,entity_types,target_state,status,
         created_by,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?)
    )SQL", {BindParam::text(templateId),BindParam::text(name),BindParam::text(version),BindParam::nullOrText(description),BindParam::nullOrText(entityTypes),
            BindParam::text(targetState),BindParam::text(status),BindParam::nullOrText(createdBy),BindParam::text(createdAt),BindParam::text(nowIso())}) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77_WorkflowTemplate::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    d->exec("DELETE FROM f77_workflow_template_steps WHERE template_id=?;",{BindParam::text(templateId)});
    return d->exec("DELETE FROM f77_workflow_templates WHERE template_id=?;",{BindParam::text(templateId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

bool F77_WorkflowTemplate::loadSteps() {
    auto* d=wfDB(); if(!d) return false;
    auto rows=d->query(
        "SELECT * FROM f77_workflow_template_steps WHERE template_id=? ORDER BY sequence_order;",
        {BindParam::text(templateId)});
    steps.clear();
    for(auto& r:rows){F77_WorkflowTemplateStep s; s.fromRow(r); steps.push_back(s);}
    return true;
}

F77_WorkflowTemplateStep F77_WorkflowTemplate::addTemplateStep(
    const std::string& title, const std::string& executionMode,
    bool isInit, bool isFinal)
{
    F77_WorkflowTemplateStep s;
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

std::shared_ptr<F77_WorkflowTemplate> F77_WorkflowTemplate::create(
    const std::string& name, const std::string& targetState,
    const std::string& entityTypes)
{
    auto t = std::make_shared<F77_WorkflowTemplate>();
    t->templateId  = genId("F77D");
    t->name        = name;
    t->targetState = targetState;
    t->entityTypes = entityTypes;
    t->createdAt   = nowIso();
    t->updatedAt   = nowIso();
    return t;
}

std::shared_ptr<F77_WorkflowTemplate> F77_WorkflowTemplate::loadById(const std::string& id) {
    auto* d=wfDB(); if(!d) return nullptr;
    auto rows=d->query("SELECT * FROM f77_workflow_templates WHERE template_id=?;",{BindParam::text(id)});
    if(rows.empty()) return nullptr;
    auto t=std::make_shared<F77_WorkflowTemplate>(); t->fromRow(rows[0]); t->loadSteps();
    return t;
}

std::vector<std::shared_ptr<F77_WorkflowTemplate>> F77_WorkflowTemplate::loadAll() {
    auto* d=wfDB(); std::vector<std::shared_ptr<F77_WorkflowTemplate>> res;
    if(!d) return res;
    for(auto& r:d->query("SELECT * FROM f77_workflow_templates ORDER BY name;",{})){
        auto t=std::make_shared<F77_WorkflowTemplate>(); t->fromRow(r); res.push_back(t);
    }
    return res;
}

std::vector<std::shared_ptr<F77_WorkflowTemplate>> F77_WorkflowTemplate::loadForEntityType(
    const std::string& entityType)
{
    auto all=loadAll();
    std::vector<std::shared_ptr<F77_WorkflowTemplate>> res;
    for(auto& t:all)
        if(t->entityTypes.empty() || t->entityTypes.find(entityType)!=std::string::npos)
            res.push_back(t);
    return res;
}

// ─────────────────────────────────────────────────────────────
// F77_WorkflowOperation
// ─────────────────────────────────────────────────────────────
Database* F77_WorkflowOperation::db() { return wfDB(); }


// ── predecessors CSV helpers ──────────────────────────────────
std::string F77_WorkflowOperation::predecessorsToString() const {
    std::string out;
    for (size_t i = 0; i < predecessors.size(); ++i) {
        if (i) out += ',';
        out += predecessors[i];
    }
    return out;
}

std::vector<std::string> F77_WorkflowOperation::predecessorsFromString(const std::string& csv) {
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

void F77_WorkflowOperation::fromRow(const Row& r) {
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
    status              = g("status");
    autoApprove         = gb("auto_approve");
    isSystem            = gb("is_system");
    systemAction        = static_cast<SystemAction>(gi("system_action"));
    requiresComment     = gb("requires_comment");
    requiresDocument    = gb("requires_document");
    completedDate       = g("completed_date");
    createdAt           = g("created_at");
    updatedAt           = g("updated_at");
}

OperationResult F77_WorkflowOperation::save() const {
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
        BindParam::text(status),BindParam::int64(autoApprove?1:0),
        BindParam::int64(isSystem?1:0),BindParam::int64(static_cast<int>(systemAction)),
        BindParam::int64(requiresComment?1:0),BindParam::int64(requiresDocument?1:0),
        BindParam::nullOrText(completedDate),BindParam::text(createdAt),BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77_WorkflowOperation::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec("DELETE FROM f77_workflow_steps WHERE step_id=?;",{BindParam::text(stepId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}


bool F77_WorkflowOperation::canStart(const std::vector<F77_WorkflowOperation>& all) const {
    if (stepStatusFrom(status)!=StepStatus::PENDING && stepStatusFrom(status)!=StepStatus::IN_PROGRESS) return false;
    for (const auto& predId : predecessors) {
        bool done = false;
        for (const auto& s : all) {
            if (s.stepId == predId) { done = s.isComplete(); break; }
        }
        if (!done) return false;
    }
    return true;
}

std::shared_ptr<F77_WorkflowOperation> F77_WorkflowOperation::loadById(const std::string& id) {
    auto* d=wfDB(); if(!d) return nullptr;
    auto rows=d->query("SELECT * FROM f77_workflow_steps WHERE step_id=?;",{BindParam::text(id)});
    if(rows.empty()) return nullptr;
    auto s=std::make_shared<F77_WorkflowOperation>(); s->fromRow(rows[0]); return s;
}

std::vector<F77_WorkflowOperation> F77_WorkflowOperation::loadForWorkflow(const std::string& wfId) {
    auto* d=wfDB(); std::vector<F77_WorkflowOperation> res;
    if(!d) return res;
    for(auto& r:d->query(
        "SELECT * FROM f77_workflow_steps WHERE workflow_id=? ORDER BY sequence_order;",
        {BindParam::text(wfId)}))
    { F77_WorkflowOperation s; s.fromRow(r); res.push_back(s); }
    return res;
}

// ─────────────────────────────────────────────────────────────
// F77_Workflow
// ─────────────────────────────────────────────────────────────
Database* F77_Workflow::db() { return wfDB(); }

void F77_Workflow::fromRow(const Row& r) {
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    workflowId    = g("workflow_id");
    templateId    = g("template_id");
    templateName  = g("template_name");
    entityType    = g("entity_type");
    entityId      = g("entity_id");
    targetState   = g("target_state");
    status        = g("status");
    initiatedBy   = g("initiated_by");
    initiatedDate = g("initiated_date");
    completedDate = g("completed_date");
    notes         = g("notes");
    createdAt     = g("created_at");
    updatedAt     = g("updated_at");
}

OperationResult F77_Workflow::save() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflows
        (workflow_id,template_id,template_name,entity_type,entity_id,target_state,
         status,initiated_by,initiated_date,completed_date,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {BindParam::text(workflowId),BindParam::nullOrText(templateId),BindParam::text(templateName),BindParam::text(entityType),BindParam::text(entityId),
            BindParam::text(targetState),BindParam::text(status),BindParam::nullOrText(initiatedBy),BindParam::text(initiatedDate),BindParam::nullOrText(completedDate),
            BindParam::text(notes),BindParam::text(createdAt),BindParam::text(nowIso())}) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77_Workflow::update() const { return save(); }

OperationResult F77_Workflow::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    d->exec("DELETE FROM f77_workflow_steps WHERE workflow_id=?;",{BindParam::text(workflowId)});
    return d->exec("DELETE FROM f77_workflows WHERE workflow_id=?;",{BindParam::text(workflowId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

bool F77_Workflow::loadSteps() {
    steps = F77_WorkflowOperation::loadForWorkflow(workflowId);
    return true;
}

bool F77_Workflow::isComplete() const {
    for(auto& s:steps) if(!s.isInitialize && !s.isFinal && !s.isComplete()) return false;
    int mid=0;
    for(auto& s:steps) if(!s.isInitialize && !s.isFinal) mid++;
    return mid>0;
}

std::vector<F77_WorkflowOperation*> F77_Workflow::readySteps() {
    std::vector<F77_WorkflowOperation*> res;
    for(auto& s:steps)
        if(!s.isComplete() && s.canStart(steps)) res.push_back(&s);
    return res;
}

std::shared_ptr<F77_Workflow> F77_Workflow::create(
    const std::string& entityType, const std::string& entityId,
    const std::string& templateName, const std::string& targetState,
    const std::string& initiatedBy)
{
    auto wf = std::make_shared<F77_Workflow>();
    wf->workflowId   = genId("F77W");
    wf->templateName = templateName;
    wf->entityType   = entityType;
    wf->entityId     = entityId;
    wf->targetState  = targetState;
    wf->status       = "active";
    wf->initiatedBy  = initiatedBy;
    wf->initiatedDate= nowIso();
    wf->createdAt    = nowIso();
    wf->updatedAt    = nowIso();
    return wf;
}

std::shared_ptr<F77_Workflow> F77_Workflow::loadById(const std::string& id) {
    auto* d=wfDB(); if(!d) return nullptr;
    auto rows=d->query("SELECT * FROM f77_workflows WHERE workflow_id=?;",{BindParam::text(id)});
    if(rows.empty()) return nullptr;
    auto wf=std::make_shared<F77_Workflow>(); wf->fromRow(rows[0]); wf->loadSteps(); return wf;
}

std::vector<std::shared_ptr<F77_Workflow>> F77_Workflow::loadForEntity(
    const std::string& entityType, const std::string& entityId)
{
    auto* d=wfDB(); std::vector<std::shared_ptr<F77_Workflow>> res;
    if(!d) return res;
    for(auto& r:d->query(
        "SELECT * FROM f77_workflows WHERE entity_type=? AND entity_id=? ORDER BY initiated_date DESC;",
        {BindParam::text(entityType),BindParam::text(entityId)}))
    { auto wf=std::make_shared<F77_Workflow>(); wf->fromRow(r); wf->loadSteps(); res.push_back(wf); }
    return res;
}

std::vector<std::shared_ptr<F77_Workflow>> F77_Workflow::loadActive() {
    auto* d=wfDB(); std::vector<std::shared_ptr<F77_Workflow>> res;
    if(!d) return res;
    for(auto& r:d->query("SELECT * FROM f77_workflows WHERE status='active' ORDER BY initiated_date DESC;",{}))
    { auto wf=std::make_shared<F77_Workflow>(); wf->fromRow(r); wf->loadSteps(); res.push_back(wf); }
    return res;
}

// ─────────────────────────────────────────────────────────────
// F77_Engine
// ─────────────────────────────────────────────────────────────
Database* F77_Engine::db() { return wfDB(); }

// ── Helper: store workflow ID back into entity ────────────────
// ── Entity context lookup ─────────────────────────────────────
// Maps an entity type string to its database pool, table name, and ID column.
// Centralises the entityType switch that was duplicated in 18 places.
struct EntityCtx {
    Database*   db        { nullptr };
    std::string table;
    std::string idCol;
    bool valid() const { return db != nullptr; }

    // Read release_workflow_id for an entity — avoids loading the full model object.
    std::string getWorkflowId(const std::string& entityId) const {
        if (!db) return {};
        auto rows = db->query(
            "SELECT release_workflow_id FROM " + table + " WHERE " + idCol + "=?;",
            { BindParam::text(entityId) });
        if (rows.empty()) return {};
        auto it = rows[0].find("release_workflow_id");
        return (it != rows[0].end() && !it->second.empty()) ? it->second : std::string{};
    }
};

static EntityCtx entityContext(const std::string& entityType) {
    auto& pool = DatabasePool::instance();
    if (entityType == "f16")
        return { pool.get("f16"), "projects",      "project_id" };
    if (entityType == "f22")
        return { pool.get("f22"), "tasks",          "task_id" };
    if (entityType == "f18")
        return { pool.get("f18"), "f18_operations", "vorgang_id" };
    if (entityType == "akt")
        return { pool.get("akt"), "akten",          "document_id" };
    return { nullptr, nullptr, nullptr };
}



static void storeWorkflowId(const std::string& entityType,
                              const std::string& entityId,
                              const std::string& workflowId) {
    auto ctx = entityContext(entityType);
    if (!ctx.valid()) { LOG_WARN("[F77] storeWorkflowId: unknown entityType: " + entityType); return; }
    std::string sql = std::string("UPDATE ") + ctx.table +
                      " SET release_workflow_id=?, updated_at=? WHERE " + ctx.idCol + "=?;";
    ctx.db->exec(sql, { BindParam::text(workflowId), BindParam::text(nowIso()), BindParam::text(entityId) });
}

// ── Public Engine methods: attach / detach workflow ID on entity ──────────
void F77_Engine::cancelWorkflow(F77_Workflow& wf) {
    wf.status = "cancelled";
    wf.update();
    detachWorkflow(wf.entityType, wf.entityId);
    LOG_INFO("[F77] Workflow cancelled: " + wf.workflowId +
             " for " + wf.entityType + "/" + wf.entityId);
}


void F77_Engine::attachWorkflow(const std::string& entityType,
                                 const std::string& entityId,
                                 const std::string& workflowId) {
    storeWorkflowId(entityType, entityId, workflowId);
}

void F77_Engine::detachWorkflow(const std::string& entityType,
                                 const std::string& entityId) {
    auto ctx = entityContext(entityType);
    if (!ctx.valid()) return;
    std::string sql = std::string("UPDATE ") + ctx.table +
                      " SET release_workflow_id=NULL, updated_at=? WHERE " + ctx.idCol + "=?;";
    ctx.db->exec(sql, { BindParam::text(nowIso()), BindParam::text(entityId) });
}


// ── // When a DOK gets a F77 workflow, create an F18OperationStep on the parent
// F22 (via its tracking F18) so the document release is visible in the chain.



// ── F77_Engine::defaultOperations ────────────────────────────────────────
// Single configuration point for the default workflow step chain per entity type.
// To add a system step: add an OperationSpec to the relevant list.
// To remove a step:   remove its OperationSpec.
// To reorder:         change the vector order — steps chain sequentially.
std::vector<F77_Engine::OperationSpec> F77_Engine::defaultOperations(
    const std::string& entityType)
{
    if (entityType == "f16") {
        // F16 owns no Akten directly — just commit.
        return {
            {"DB schreiben", true, SystemAction::COMMIT_DB_OBJECTS, true},
        };
    }
    // f22, akt, f18 — scan for unregistered files, then commit.
    return {
        {"Objektverwaltung", true, SystemAction::SCAN_UNREGISTERED_FILES, true},
        {"DB schreiben",     true, SystemAction::COMMIT_DB_OBJECTS,       true},
    };
}


std::shared_ptr<F77_Workflow> F77_Engine::startFromTemplate(
    const std::string& templateId, const std::string& entityType,
    const std::string& entityId, const std::string& initiatedBy)
{
    // Enforce: exactly one active workflow per entity at a time.
    {
        // EntityCtx reads releaseWorkflowId via SQL — no need to load full objects
        const auto ectx = entityContext(entityType);
        std::string existingId = ectx.valid() ? ectx.getWorkflowId(entityId) : std::string{};
        if (!existingId.empty()) {
            auto existing = F77_Workflow::loadById(existingId);
            if (existing && workflowStatusFrom(existing->status) == WorkflowStatus::ACTIVE) {
                LOG_WARN("[F77] startFromTemplate verweigert: bereits aktiver Workflow ("
                         + existingId + ") fuer " + entityType + "/" + entityId);
                return nullptr;
            }
        }
    }
    auto tpl = F77_WorkflowTemplate::loadById(templateId);
    if(!tpl) { LOG_ERROR("[F77] Template not found: "+templateId); return nullptr; }

    auto wf = F77_Workflow::create(entityType, entityId,
                                    tpl->name, tpl->targetState, initiatedBy);
    wf->templateId = templateId;
    if (!opOk(wf->save())) return nullptr;

    // Snapshot template steps → F77_WorkflowOperation (+ F18_Operation per mid-step)
    // Build map: tpl_step_id → runtime step_id for predecessor resolution
    std::map<std::string,std::string> tplToRuntime;
    tpl->loadSteps();


    for(auto& ts : tpl->steps) {
        F77_WorkflowOperation rs;
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
                rs.status        = ts.isInitialize ? "approved" : "pending";
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
            rs.predecessors = F77_WorkflowOperation::predecessorsFromString(resolved);
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
            // Objektverwaltung: auto if no loose files, else spawns F77_Tasks
            F77_WorkflowOperation objStep;
            objStep.stepId        = genId("F77S"); objStep.workflowId = wf->workflowId;
            objStep.title         = "Objektverwaltung";
            objStep.sequenceOrder = maxNonEndSeq + 1;
            objStep.executionMode = "sequential";
            objStep.predecessors  = { lastManualId };
            objStep.autoApprove   = true; objStep.isSystem = true;
            objStep.systemAction  = SystemAction::SCAN_UNREGISTERED_FILES;
            objStep.status        = "pending";
            objStep.createdAt     = nowIso(); objStep.updatedAt = nowIso();
            objStep.save(); wf->steps.push_back(objStep);

            // DB schreiben: runs after Objektverwaltung
            F77_WorkflowOperation dbStep;
            dbStep.stepId        = genId("F77S"); dbStep.workflowId = wf->workflowId;
            dbStep.title         = "DB schreiben";
            dbStep.sequenceOrder = maxNonEndSeq + 2;
            dbStep.executionMode = "sequential";
            dbStep.predecessors  = { objStep.stepId };
            dbStep.autoApprove   = true; dbStep.isSystem = true;
            dbStep.systemAction  = SystemAction::COMMIT_DB_OBJECTS;
            dbStep.status        = "pending";
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


std::shared_ptr<F77_Workflow> F77_Engine::startDefault(
    const std::string& entityType, const std::string& entityId,
    const std::string& targetState, const std::string& initiatedBy)
{
    // Enforce: exactly one active workflow per entity at a time.
    {
        auto ctx = entityContext(entityType);
        if (!ctx.valid()) { LOG_ERROR("[F77] startDefault: unknown entityType: " + entityType); return nullptr; }
        auto rows = ctx.db->query(
            std::string("SELECT release_workflow_id FROM ") + ctx.table +
            " WHERE " + ctx.idCol + "=?;",
            { BindParam::text(entityId) });
        if (!rows.empty()) {
            auto it = rows[0].find("release_workflow_id");
            if (it != rows[0].end() && !it->second.empty()) {
                auto existing = F77_Workflow::loadById(it->second);
                if (existing && workflowStatusFrom(existing->status) == WorkflowStatus::ACTIVE) {
                    LOG_WARN("[F77] startDefault: workflow already active: " + it->second);
                    return nullptr;
                }
            }
        }
    }

    // Use first matching active template, else build minimal Init→Freigabe→End
    auto templates = F77_WorkflowTemplate::loadForEntityType(entityType);
    for(auto& t : templates) {
        if(workflowStatusFrom(t->status) == WorkflowStatus::ACTIVE && t->targetState==targetState) {
            return startFromTemplate(t->templateId, entityType, entityId, initiatedBy);
        }
    }

    // No template found — minimal workflow
    auto wf = F77_Workflow::create(entityType, entityId,
                                    "Standard-Freigabe", targetState, initiatedBy);
    if (!opOk(wf->save())) return nullptr;

    std::string projId;
    if(entityType=="f22"){ auto t=TaskF22::loadById(entityId); if(t) projId=t->projectId; }

    // Init step (auto-approved)
    F77_WorkflowOperation init;
    init.stepId=genId("F77S"); init.workflowId=wf->workflowId;
    init.title="Init"; init.sequenceOrder=0; init.isInitialize=true;
    init.autoApprove=true; init.status="approved"; init.executionMode="sequential";
    init.completedDate=nowIso(); init.createdAt=nowIso(); init.updatedAt=nowIso();
    init.save(); wf->steps.push_back(init);

    // Build step chain from declarative OperationSpec list.
    // To change which steps run for an entity: edit defaultOperations().
    auto specs = defaultOperations(entityType);
    std::string prevStepId = init.stepId;
    int seq = 1;
    for (auto& spec : specs) {
        F77_WorkflowOperation op;
        op.stepId        = genId("F77S"); op.workflowId = wf->workflowId;
        op.title         = spec.title;
        op.sequenceOrder = seq++;
        op.executionMode = "sequential";
        op.predecessors  = {prevStepId};
        op.autoApprove   = spec.autoApprove;
        op.isSystem      = spec.isSystem;
        op.systemAction  = spec.action;
        op.status        = "pending";
        op.createdAt     = nowIso(); op.updatedAt = nowIso();
        op.save(); wf->steps.push_back(op);
        prevStepId = op.stepId;
    }

    // End step — always last, chains from final system step.
    F77_WorkflowOperation end;
    end.stepId=genId("F77S"); end.workflowId=wf->workflowId;
    end.title="End"; end.sequenceOrder=9999; end.isFinal=true;
    end.autoApprove=true; end.predecessors={prevStepId};
    end.status="pending"; end.executionMode="sequential";
    end.createdAt=nowIso(); end.updatedAt=nowIso();
    end.save(); wf->steps.push_back(end);

    // NOTE: tick() is NOT called here.
    // The caller (startWfInstanceWizard or test) calls tick() after any manual
    // operations are added. This prevents premature auto-completion.
    storeWorkflowId(entityType, entityId, wf->workflowId);

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
        "SELECT rev FROM akt_revisionen WHERE document_id=? "
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
        "SELECT rev FROM akt_revisionen WHERE document_id=?;",
        {BindParam::text(docId)});

    for (auto& r : allRows) {
        auto it = r.find("rev");
        if (it == r.end() || it->second.empty()) continue;
        uint32_t rev = (uint32_t)std::stoi(it->second);
        if (keepRevs.count(rev)) continue;

        // Remove MFS folder for this revision
        std::string revDir = Rosenholz::DocumentObject::mfsRevDir(docId, rev);
        if (!revDir.empty() && Rosenholz::FileOps::fileExists(revDir)) {
            // Use system call to remove directory tree
            std::string cmd = "rm -rf "" + revDir + """;
            std::system(cmd.c_str());
            LOG_INFO("[F77] pruned MFS rev folder: " + revDir);
        }
    }
}


// ── F77_Engine::scanLooseFiles ────────────────────────────────────────────
// Delegates to each entity's own scan logic. F77 does not know internals.
// Adding a new entity type: add one case here.
std::vector<std::pair<std::string,std::string>>
F77_Engine::scanLooseFiles(const std::string& entityType,
                            const std::string& entityId)
{
    using namespace Rosenholz;
    if (entityType == "f22") {
        auto t = TaskF22::loadById(entityId);
        return t ? t->scanMfsForUnregistered()
                 : std::vector<std::pair<std::string,std::string>>{};
    }
    if (entityType == "akt") {
        auto d = Document::loadById(entityId);
        if (!d) return {};
        auto curRev = DocumentRevision::currentRevision(entityId);
        if (!curRev) return {};
        auto files = DocumentObject::scanForUnregisteredFiles(entityId, curRev->rev);
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
// SystemAction::COMMIT_DB_OBJECTS — commits all uncommitted DocumentObjects to LMDB.
// Adding a new system action = adding a case to the switch below.
static void executeSystemStep(F77_WorkflowOperation& step,
                               F77_Workflow& wf, bool& changed) {
    // ── 3b: SystemAction enum replaces magic string comparison ──────────────
    // step.systemAction determines what this step does.
    // COMMIT_DB_OBJECTS is the only action currently; the enum makes it
    // extensible without touching tick().
    // ── SCAN_UNREGISTERED_FILES ─────────────────────────────────────────────
    if (step.systemAction == SystemAction::SCAN_UNREGISTERED_FILES) {
        // Scan the entity's MFS folder for files not registered as AKT objects.
        // If none: auto-approve immediately (step was already approved by tick).
        // If some: revoke the auto-approval, spawn one F77_Task per file,
        //          and block until all tasks are closed.
        std::vector<std::pair<std::string,std::string>> loose;

        loose = Rosenholz::F77_Engine::scanLooseFiles(wf.entityType, wf.entityId);

        if (loose.empty()) {
            LOG_INFO("[F77] Objektverwaltung: no loose files — auto-approved for "
                     + wf.entityId);
            return; // step stays approved
        }

        // Files found: revoke auto-approval, spawn F77_Tasks
        step.status = "in_progress"; step.completedDate = ""; step.save();
        changed = false; // don't let tick advance further

        // Check if tasks already spawned for this operation:
        auto existing = F77_Task::loadForOperation(step.stepId);
        if (!existing.empty()) {
            LOG_INFO("[F77] Objektverwaltung: tasks already spawned, waiting ("
                     + std::to_string(existing.size()) + " tasks)");
            return;
        }

        for (auto& [fpath, fname] : loose) {
            std::string taskTitle = "Nicht abgelegte Datei verwalten: " + fname;
            F77_Task::create(
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
                 + " F77_Tasks for " + wf.entityId);
        return;
    }

    if (step.systemAction != SystemAction::COMMIT_DB_OBJECTS) return;

    // For non-DOK entities: write MFS index files (tracking data).
    if (wf.entityType != "akt") {
        const std::string& mfsRoot = Rosenholz::Config::instance().mfsPath();
        if (wf.entityType == "f16") {
            auto p = Rosenholz::ProjectF16::loadById(wf.entityId);
            if (p) { MFSWriter::writeProject(*p, mfsRoot); LOG_INFO("[F77] DB schreiben: MFS written for F16 " + wf.entityId); }
        } else if (wf.entityType == "f22") {
            auto t = Rosenholz::TaskF22::loadById(wf.entityId);
            if (t) { MFSWriter::writeTask(*t, mfsRoot); LOG_INFO("[F77] DB schreiben: MFS written for F22 " + wf.entityId); }
        } else if (wf.entityType == "f18") {
            auto op = Rosenholz::F18Operation::loadById(wf.entityId);
            if (op) { MFSWriter::writeF18(*op, mfsRoot); LOG_INFO("[F77] DB schreiben: MFS written for F18 " + wf.entityId); }
        }
        return;
    }

    auto curRev = Rosenholz::DocumentRevision::currentRevision(wf.entityId);
    if (!curRev) return;

    auto objs = Rosenholz::DocumentObject::loadForRevision(wf.entityId, curRev->rev);
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
        step.status = "pending"; step.completedDate = ""; step.save(); changed = false;
        LOG_ERROR("[F77] COMMIT_DB_OBJECTS step FAILED — reverted to pending. "
                  "Fix MFS files and re-run tick.");
        return;
    }

    if (Rosenholz::Config::instance().storage().saveSpace)
        pruneMFSRevisions(wf.entityId);

    LOG_INFO("[F77] COMMIT_DB_OBJECTS: all committed for " +
             wf.entityId + " rev " + std::to_string(curRev->rev));
}


bool F77_Engine::tick(F77_Workflow& wf) {
    if(wf.status!="active") return false;
    wf.loadSteps();
    bool changed=false;

    for(auto& s : wf.steps) {
        // Sync status from linked F18_Operation

        // Auto-approve Init and autoApprove steps
        if(stepStatusFrom(s.status)==StepStatus::PENDING && s.autoApprove && s.canStart(wf.steps)) {
            s.status="approved"; s.completedDate=nowIso(); s.save(); changed=true;
            LOG_INFO("[F77] Auto-approved step '"+s.title+"'");
            if(s.isSystem) executeSystemStep(s, wf, changed);
        }

        // Re-check in_progress steps: if all F77_Tasks closed, approve.
        // This handles both SCAN steps (Objektverwaltung) and manual operations.
        if(stepStatusFrom(s.status)==StepStatus::IN_PROGRESS) {
            auto tasks = F77_Task::loadForOperation(s.stepId);
            if (!tasks.empty() && F77_Task::checkOperationComplete(s.stepId)) {
                s.status="approved"; s.completedDate=nowIso(); s.save(); changed=true;
                LOG_INFO("[F77] Step approved (all tasks closed): " + s.title
                         + " [" + s.stepId + "]");
            }
        }

        // Mark in_progress for ready mid-steps
        if(stepStatusFrom(s.status)==StepStatus::PENDING && !s.isInitialize && !s.isFinal
           && s.canStart(wf.steps)) {
            s.status="in_progress"; s.save(); changed=true;
        }

        // End step: auto-approve when all mid-steps done AND no open F77_Tasks remain
        if(s.isFinal && stepStatusFrom(s.status)==StepStatus::PENDING && wf.isComplete()) {
            // Guard: check all F77_Tasks spawned by this workflow are closed
            auto pendingTasks = F77_Task::loadForWorkflow(wf.workflowId);
            bool hasOpenTasks = false;
            for (auto& t : pendingTasks) if (t->isOpen()) { hasOpenTasks = true; break; }
            if (hasOpenTasks) {
                LOG_INFO("[F77] End step blocked: open F77_Tasks pending for "
                         + wf.workflowId);
            } else {
                s.status="approved"; s.completedDate=nowIso(); s.save(); changed=true;
                LOG_INFO("[F77] End step auto-approved — workflow closing");
            }
        }
    }

    if(changed) checkAndComplete(wf);
    return changed;
}


// ── F77_Engine::addManualOperation ───────────────────────────────────────
// Adds a non-auto-approved operation to a running workflow,
// then creates a F77_Task as the actionable item for -tasks.
// No F18 Operation is spawned — the F77_Task IS the work item.
std::string F77_Engine::addManualOperation(
    F77_Workflow&      wf,
    const std::string& title,
    const std::string& description,
    const std::string& assignedTo)
{
    // Always reload from DB to get current status:
    auto fresh = F77_Workflow::loadById(wf.workflowId);
    if (fresh) { wf.status = fresh->status; wf.steps = fresh->steps; }
    if (workflowStatusFrom(wf.status) != WorkflowStatus::ACTIVE) {
        LOG_WARN("[F77] addManualOperation: workflow not active (status=" + wf.status + "): " + wf.workflowId);
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
    F77_WorkflowOperation op;
    op.stepId        = genId("F77S");
    op.workflowId    = wf.workflowId;
    op.title         = title;
    op.autoApprove   = false;
    op.isInitialize  = false;
    op.isFinal       = false;
    op.isSystem      = false;
    op.status        = "in_progress"; // task-driven — tick polls this
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

    // ── Create a F77_Task as the actionable work item ─────────────────────
    std::string taskTitle = title;
    auto task = F77_Task::create(
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
        LOG_INFO("[F77] F77_Task created: " + task->taskId + " for operation " + op.stepId);
    }

    LOG_INFO("[F77] Manual operation added: " + op.stepId + " \"" + title + "\"");
    return op.stepId;
}

// Validate whether a step can be fired — dry-run, no state change
std::string F77_Engine::validateStep(
    const F77_Workflow& wf,
    const std::string& stepId)
{
    for(const auto& s : wf.steps) {
        if(s.stepId != stepId) continue;
        if(s.isComplete())
            return "BLOCKED: Schritt ist bereits abgeschlossen ("+s.status+")";
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

bool F77_Engine::fireStep(F77_Workflow& wf, const std::string& stepId,
                           const std::string& decision, const std::string& actorId,
                           const std::string& comment)
{
    wf.loadSteps();
    F77_WorkflowOperation* step=nullptr;
    for(auto& s:wf.steps) if(s.stepId==stepId){step=&s;break;}
    if(!step) { LOG_WARN("[F77] Step not found: "+stepId); return false; }
    if(step->isComplete()) { LOG_WARN("[F77] Step already complete: "+stepId); return false; }
    if(!step->canStart(wf.steps)) { LOG_WARN("[F77] Prerequisites not met: "+stepId); return false; }
    if(step->requiresComment && comment.empty()) { LOG_WARN("[F77] Comment required: "+stepId); return false; }


    step->status = decision;
    step->completedDate = nowIso();
    step->save();


    LOG_INFO("[F77] Step fired: "+stepId+" decision="+decision+" by="+actorId);
    tick(wf);
    return true;
}

bool F77_Engine::checkAndComplete(F77_Workflow& wf) {
    // All steps done (including End)?
    for(auto& s:wf.steps) if(!s.isComplete()) return false;
    wf.status = "completed";
    wf.completedDate = nowIso();
    wf.save();
    LOG_INFO("[F77] Workflow completed: "+wf.workflowId);
    applyTargetState(wf);
    return true;
}

bool F77_Engine::applyTargetState(const F77_Workflow& wf) {
    if(wf.entityType == "akt") {
        auto rev = DocumentRevision::currentRevision(wf.entityId);
        if(rev && DocumentRevision::isTransitionAllowed(rev->revState,
             revStateFromString(wf.targetState))) {
            rev->transitionState(revStateFromString(wf.targetState), true); // F77-gated
            LOG_INFO("[F77] AKT revision transitioned: "+wf.entityId+" → "+wf.targetState);
        }
    } else {
        // F16/F22/F18: set status via EntityCtx (single dispatch — no scattered if/else)
        auto actx = entityContext(wf.entityType);
        if (actx.valid())
            actx.db->exec(
                "UPDATE " + actx.table + " SET status=?, updated_at=? WHERE " + actx.idCol + "=?;",
                {BindParam::text(wf.targetState), BindParam::text(nowIso()),
                 BindParam::text(wf.entityId)});
        LOG_INFO("[F77] Entity status → '" + wf.targetState + "': " + wf.entityType + "/" + wf.entityId);
    }
    return true;
}

bool F77_Engine::canRelease(const std::string& entityType, const std::string& entityId,
                              const std::string& releaseWorkflowId, int& blockerCount)
{
    blockerCount = 0;

    // Check the main workflow itself: all steps must be complete.
    auto mainWf = F77_Workflow::loadById(releaseWorkflowId);
    if (!mainWf) {
        blockerCount = 1;
        return false;
    }
    if (workflowStatusFrom(mainWf->status) != WorkflowStatus::COMPLETED) {
        mainWf->loadSteps();
        for (auto& s : mainWf->steps) {
            if (!s.isInitialize && !s.isComplete()) blockerCount++;
        }
        if (blockerCount > 0) return false;
    }
    // Legacy: also count any OTHER active workflows (should be 0 under new model)
    auto all = F77_Workflow::loadForEntity(entityType, entityId);
    for (auto& wf : all) {
        if (wf->workflowId == releaseWorkflowId) continue;
        if (workflowStatusFrom(wf->status) == WorkflowStatus::ACTIVE) blockerCount++;
    }
    return blockerCount == 0;
}

int F77_Engine::lockAll(const std::string& entityType, const std::string& entityId,
                         const std::string& releaseWorkflowId, bool confirmLock)
{
    if(!confirmLock) return -1;
    auto all = F77_Workflow::loadForEntity(entityType, entityId);
    int locked=0;
    for(auto& wf : all) {
        if(wf->workflowId==releaseWorkflowId || workflowStatusFrom(wf->status) != WorkflowStatus::ACTIVE) continue;
        wf->status="locked"; wf->save(); locked++;
    }
    return locked;
}

void F77_Engine::seedDefaultTemplates() {
    auto* d = wfDB(); if(!d) return;
    // Always re-seed: drop and recreate to pick up template changes.
    // Existing workflow INSTANCES are not affected (they snapshot template steps).
    try {
        auto existing = d->query("SELECT COUNT(*) as n FROM f77_workflow_templates;",{});
        if (!existing.empty()) {
            std::string cnt = existing[0].begin()->second;
            if (cnt != "0") return;   // already seeded
        }
    } catch(...) { return; }

    bool adminMode = Config::instance().admin().enabled;

    // Helper: create a template with Init → Create DB Objects → Mid → End structure
    // "Create DB Objects" is a system step added to every template.
    // It is auto-approved, immutable, and commits document objects to LMDB.
    auto makeTemplate = [&](const std::string& name,
                             const std::string& targetState,
                             const std::string& entityTypes,
                             const std::string& midTitle,
                             bool adminOnly = false) {
        if (adminOnly && !adminMode) return;
        auto t = F77_WorkflowTemplate::create(name, targetState, entityTypes);
        t->description = midTitle;
        t->save();

        // Step 1: Init (always auto-approved)
        auto init = t->addTemplateStep("Init", "sequential", true, false);
        init.autoApprove = true;
        init.save();

        // Step 2: Create DB Objects (system step — auto-approved, immutable)
        auto createDb = t->addTemplateStep("Create DB Objects", "sequential", false, false);
        createDb.predecessorTplStepIds = init.tplStepId;
        createDb.autoApprove   = true;
        createDb.isSystem      = true;
        createDb.systemAction  = SystemAction::COMMIT_DB_OBJECTS;
        createDb.save();

        // Step 3: User-defined mid step
        auto mid = t->addTemplateStep(midTitle, "sequential", false, false);
        mid.predecessorTplStepIds = createDb.tplStepId;
        mid.save();

        // Step 3: End (auto-approved; Objektverwaltung+DBschreiben injected before this)
        auto end = t->addTemplateStep("End", "sequential", false, true);
        end.predecessorTplStepIds = mid.tplStepId;
        end.autoApprove = true;
        end.save();
    };

    // ── Standard templates (all entity types) ──────────────────
    // Dok gets all 5-state transitions; F16/F22/F18 only get released/closed
    const std::string dokTypes  = "akt";
    const std::string objTypes  = "f22,f18"; // f16 no longer uses F77; dok → akt

    // 1. Lock Workflow: → locked  (available from in_work and pre_released)
    makeTemplate("Dokument: Einfrieren",        "locked",       dokTypes,  "Sperren prüfen");
    makeTemplate("Objekt: Einfrieren",          "locked",       "f22,f18", "Sperren prüfen");

    // 2. PreRelease Workflow: in_work → pre_released
    makeTemplate("Dokument: Zur Prüfung",       "pre_released", dokTypes,  "Prüfung durchführen");

    // 3. Release Workflow: → released
    makeTemplate("Dokument: Freigabe",          "released",     dokTypes,  "Freigabe erteilen");
    makeTemplate("Objekt: Freigabe",            "released",     "f22,f18", "Freigabe erteilen");

    // 4. Close Workflow: → closed (= Ungültig markieren)
    makeTemplate("Dokument: Schliessen",        "closed",       dokTypes,  "Ungültigkeit bestätigen");
    makeTemplate("Objekt: Schliessen",          "closed",       "f22,f18", "Ungültigkeit bestätigen");

    // 5. Unlock Workflow: locked → in_work
    makeTemplate("Dokument: Entsperren",        "in_work",      dokTypes,  "Entsperrung prüfen");

    // ── Admin-only templates ──────────────────────────────────
    if (adminMode) {
        makeTemplate("ADMIN: Dok → in_work",       "in_work",      dokTypes,  "Admin-Übergang", true);
        makeTemplate("ADMIN: Dok → pre_released",  "pre_released", dokTypes,  "Admin-Übergang", true);
        makeTemplate("ADMIN: Dok → released",      "released",     dokTypes,  "Admin-Übergang", true);
        makeTemplate("ADMIN: Dok → locked",        "locked",       dokTypes,  "Admin-Übergang", true);
        makeTemplate("ADMIN: Dok → closed",        "closed",       dokTypes,  "Admin-Übergang", true);
        makeTemplate("ADMIN: Obj → released",      "released",     "f16,f22,f18", "Admin-Übergang", true);
        makeTemplate("ADMIN: Obj → closed",        "closed",       "f16,f22,f18", "Admin-Übergang", true);
    }
}
} // namespace Rosenholz

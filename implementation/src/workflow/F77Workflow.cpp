// ============================================================
// F77Workflow.cpp — F77 Freigabe-Workflow Engine implementation
// ============================================================
#include "F77Workflow.h"
#include "../core/FileOps.h"
#include "../model/dok/DocumentObject.h"
#include "../repository/DocumentRevision.h"
#include "../core/Config.h"
#include <set>
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../model/Utils.h"
#include "../model/f16/ProjectF16.h"
#include "../model/f22/TaskF22.h"
#include "../model/dok/Document.h"
#include "../model/f18/F18Operation.h"
#include "../repository/DocumentRevision.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

namespace Rosenholz {

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
    waitConditionF18Type     = g("wait_condition_f18_type");
    waitConditionTitle       = g("wait_condition_title");
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
         execution_mode,predecessor_tpl_step_ids,wait_condition_f18_type,wait_condition_title,
         required_role,sla_hours,auto_approve,requires_comment,requires_document,
         created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(tplStepId),BindParam::text(templateId),BindParam::text(title),BindParam::nullOrText(description),BindParam::int64(sequenceOrder),
        BindParam::int64(isInitialize?1:0),BindParam::int64(isFinal?1:0),BindParam::text(executionMode),BindParam::nullOrText(predecessorTplStepIds),
        BindParam::nullOrText(waitConditionF18Type),BindParam::nullOrText(waitConditionTitle),BindParam::nullOrText(requiredRole),BindParam::int64(slaHours),
        BindParam::int64(autoApprove?1:0),BindParam::int64(requiresComment?1:0),BindParam::int64(requiresDocument?1:0),
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
// F77_WorkflowStep
// ─────────────────────────────────────────────────────────────
Database* F77_WorkflowStep::db() { return wfDB(); }


// ── predecessors CSV helpers ──────────────────────────────────
std::string F77_WorkflowStep::predecessorsToString() const {
    std::string out;
    for (size_t i = 0; i < predecessors.size(); ++i) {
        if (i) out += ',';
        out += predecessors[i];
    }
    return out;
}

std::vector<std::string> F77_WorkflowStep::predecessorsFromString(const std::string& csv) {
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

void F77_WorkflowStep::fromRow(const Row& r) {
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
    f18OperationId      = g("f18_operation_id");
    waitF18OperationId  = g("wait_f18_operation_id");
    waitConditionF18Type= g("wait_condition_f18_type");
    status              = g("status");
    autoApprove         = gb("auto_approve");
    requiresComment     = gb("requires_comment");
    requiresDocument    = gb("requires_document");
    completedDate       = g("completed_date");
    createdAt           = g("created_at");
    updatedAt           = g("updated_at");
}

OperationResult F77_WorkflowStep::save() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_steps
        (step_id,workflow_id,tpl_step_id,title,sequence_order,is_initialize,is_final,
         execution_mode,predecessor_step_ids,f18_operation_id,wait_f18_operation_id,
         wait_condition_f18_type,status,auto_approve,requires_comment,requires_document,
         completed_date,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(stepId),BindParam::text(workflowId),BindParam::nullOrText(tplStepId),BindParam::text(title),BindParam::int64(sequenceOrder),
        BindParam::int64(isInitialize?1:0),BindParam::int64(isFinal?1:0),BindParam::text(executionMode),BindParam::nullOrText(predecessorsToString()),
        BindParam::nullOrText(f18OperationId),BindParam::nullOrText(waitF18OperationId),BindParam::nullOrText(waitConditionF18Type),
        BindParam::text(status),BindParam::int64(autoApprove?1:0),BindParam::int64(requiresComment?1:0),BindParam::int64(requiresDocument?1:0),
        BindParam::nullOrText(completedDate),BindParam::text(createdAt),BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77_WorkflowStep::remove() const {
    auto* d=wfDB(); if(!d) return OperationResult::DB_ERROR;
    return d->exec("DELETE FROM f77_workflow_steps WHERE step_id=?;",{BindParam::text(stepId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

void F77_WorkflowStep::syncFromF18() {
    if(f18OperationId.empty()) return;
    auto op = F18Operation::loadById(f18OperationId);
    if(!op) return;
    // Map F18 operation status to F77 step status
    if     (op->status == "released") status = "approved";
    else if(op->status == "cancelled")status = "cancelled";
    else if(op->status == "draft" || op->status == "in_work") status = "in_progress";
    save();
}

bool F77_WorkflowStep::canStart(const std::vector<F77_WorkflowStep>& all) const {
    if (status != "pending" && status != "in_progress") return false;
    for (const auto& predId : predecessors) {
        bool done = false;
        for (const auto& s : all) {
            if (s.stepId == predId) { done = s.isComplete(); break; }
        }
        if (!done) return false;
    }
    return true;
}

std::shared_ptr<F77_WorkflowStep> F77_WorkflowStep::loadById(const std::string& id) {
    auto* d=wfDB(); if(!d) return nullptr;
    auto rows=d->query("SELECT * FROM f77_workflow_steps WHERE step_id=?;",{BindParam::text(id)});
    if(rows.empty()) return nullptr;
    auto s=std::make_shared<F77_WorkflowStep>(); s->fromRow(rows[0]); return s;
}

std::vector<F77_WorkflowStep> F77_WorkflowStep::loadForWorkflow(const std::string& wfId) {
    auto* d=wfDB(); std::vector<F77_WorkflowStep> res;
    if(!d) return res;
    for(auto& r:d->query(
        "SELECT * FROM f77_workflow_steps WHERE workflow_id=? ORDER BY sequence_order;",
        {BindParam::text(wfId)}))
    { F77_WorkflowStep s; s.fromRow(r); res.push_back(s); }
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
    steps = F77_WorkflowStep::loadForWorkflow(workflowId);
    return true;
}

bool F77_Workflow::isComplete() const {
    for(auto& s:steps) if(!s.isInitialize && !s.isFinal && !s.isComplete()) return false;
    int mid=0;
    for(auto& s:steps) if(!s.isInitialize && !s.isFinal) mid++;
    return mid>0;
}

std::vector<F77_WorkflowStep*> F77_Workflow::readySteps() {
    std::vector<F77_WorkflowStep*> res;
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
    if (entityType == "dok")
        return { pool.get("dok"), "documents",      "document_id" };
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
            if (existing && existing->status == "active") {
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

    // Snapshot template steps → F77_WorkflowStep (+ F18_Operation per mid-step)
    // Build map: tpl_step_id → runtime step_id for predecessor resolution
    std::map<std::string,std::string> tplToRuntime;
    tpl->loadSteps();

    // For F22: create ONE F18Operation for the whole workflow.
    // Each mid-step becomes an F18OperationStep inside it.
    std::shared_ptr<F18Operation> wfOp;
    if (entityType == "f22") {
        wfOp = F18Operation::create(entityId,
                   "Workflow-Aufgaben [" + wf->workflowId + "]",
                   F18OperationType::F77_STEP);
        if (wfOp) {
            wfOp->releaseWorkflowId = wf->workflowId;
            wfOp->save();
            wfOp->loadSteps();
        }
    }

    for(auto& ts : tpl->steps) {
        F77_WorkflowStep rs;
        rs.stepId        = genId("F77S");
        rs.workflowId    = wf->workflowId;
        rs.tplStepId     = ts.tplStepId;
        rs.title         = ts.title;
        rs.sequenceOrder = ts.sequenceOrder;
        rs.isInitialize  = ts.isInitialize;
        rs.isFinal       = ts.isFinal;
        rs.executionMode = ts.executionMode;
        rs.autoApprove   = ts.autoApprove;
        rs.requiresComment   = ts.requiresComment;
        rs.requiresDocument  = ts.requiresDocument;
        rs.waitConditionF18Type = ts.waitConditionF18Type;
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
            rs.predecessors = F77_WorkflowStep::predecessorsFromString(resolved);
        }

        // Link mid-steps to the single workflow F18 (F22 only).
        if(!ts.isInitialize && !ts.isFinal && !ts.isSystem && entityType == "f22" && wfOp) {
            rs.f18OperationId = wfOp->vorgangId;
            wfOp->addStep(ts.title, "review", "", true);
        }

        rs.save();
        tplToRuntime[ts.tplStepId] = rs.stepId;
        wf->steps.push_back(rs);
    }

    LOG_INFO("[F77] Workflow started from template '"+tpl->name+"' for "+entityType+"/"+entityId);
    tick(*wf);

    // Store the workflow ID back into the entity's releaseWorkflowId field
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
                if (existing && existing->status == "active") {
                    LOG_WARN("[F77] startDefault: workflow already active: " + it->second);
                    return nullptr;
                }
            }
        }
    }

    // Use first matching active template, else build minimal Init→Freigabe→End
    auto templates = F77_WorkflowTemplate::loadForEntityType(entityType);
    for(auto& t : templates) {
        if(t->status=="active" && t->targetState==targetState)
            return startFromTemplate(t->templateId, entityType, entityId, initiatedBy);
    }

    // No template found — minimal workflow
    auto wf = F77_Workflow::create(entityType, entityId,
                                    "Standard-Freigabe", targetState, initiatedBy);
    if (!opOk(wf->save())) return nullptr;

    std::string projId;
    if(entityType=="f16") projId=entityId;
    else if(entityType=="f22"){ auto t=TaskF22::loadById(entityId); if(t) projId=t->projectId; }
    else if(entityType=="f18"){ auto op=F18Operation::loadById(entityId); if(op) projId=op->taskId; }

    // Init step (auto-approved)
    F77_WorkflowStep init;
    init.stepId=genId("F77S"); init.workflowId=wf->workflowId;
    init.title="Init"; init.sequenceOrder=0; init.isInitialize=true;
    init.autoApprove=true; init.status="approved"; init.executionMode="sequential";
    init.completedDate=nowIso(); init.createdAt=nowIso(); init.updatedAt=nowIso();
    init.save(); wf->steps.push_back(init);

    // Mid step — for F22: backed by F18_Operation containing the workflow tasks
    // The F18 is named after the workflow ID so it is easily identifiable.
    F77_WorkflowStep mid;
    mid.stepId=genId("F77S"); mid.workflowId=wf->workflowId;
    mid.title="Freigabe vorbereiten"; mid.sequenceOrder=1;
    mid.executionMode="sequential"; mid.predecessors={init.stepId};
    mid.status="pending"; mid.createdAt=nowIso(); mid.updatedAt=nowIso();
    if (entityType == "f22") {
        // Create ONE F18 for all workflow tasks, named after the workflow ID.
        auto midOp = F18Operation::create(entityId,
                         "Workflow-Aufgaben [" + wf->workflowId + "]",
                         F18OperationType::F77_STEP);
        if (midOp) {
            midOp->releaseWorkflowId = wf->workflowId;
            midOp->save();
            midOp->loadSteps(); // initialises Init + End bookend steps
            // Add one F18OperationStep for the mid workflow step
            midOp->addStep("Freigabe vorbereiten", "review", "", true);
            mid.f18OperationId = midOp->vorgangId;
        }
    }
    mid.save(); wf->steps.push_back(mid);

    // End step
    F77_WorkflowStep end;
    end.stepId=genId("F77S"); end.workflowId=wf->workflowId;
    end.title="End"; end.sequenceOrder=9999; end.isFinal=true;
    end.autoApprove=true; end.predecessors={mid.stepId};
    end.status="pending"; end.executionMode="sequential";
    end.createdAt=nowIso(); end.updatedAt=nowIso();
    end.save(); wf->steps.push_back(end);

    tick(*wf);

    // Store the workflow ID back into the entity's releaseWorkflowId field
    storeWorkflowId(entityType, entityId, wf->workflowId);

    return wf;
}

// ── MFS pruning helper ────────────────────────────────────────
// When saveSpace=true: removes MFS copies of revisions that are neither
// in_work nor the superseded=false (active) revision.
static void pruneMFSRevisions(const std::string& docId) {
    auto* db = DatabasePool::instance().get("dok");
    if (!db) return;

    // Find revisions to keep: in_work OR superseded=0
    auto keepRows = db->query(
        "SELECT rev FROM document_revisions WHERE document_id=? "
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
        "SELECT rev FROM document_revisions WHERE document_id=?;",
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


// Execute a system step (isSystem=true). Currently only one action exists:
// SystemAction::COMMIT_DB_OBJECTS — commits all uncommitted DocumentObjects to LMDB.
// Adding a new system action = adding a case to the switch below.
static void executeSystemStep(F77_WorkflowStep& step,
                               F77_Workflow& wf, bool& changed) {
    // ── 3b: SystemAction enum replaces magic string comparison ──────────────
    // step.systemAction determines what this step does.
    // COMMIT_DB_OBJECTS is the only action currently; the enum makes it
    // extensible without touching tick().
    if (step.systemAction != SystemAction::COMMIT_DB_OBJECTS) return;
    if (wf.entityType != "dok") return;

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
        if(!s.f18OperationId.empty()) s.syncFromF18();

        // Auto-approve Init and autoApprove steps
        if(s.status=="pending" && s.autoApprove && s.canStart(wf.steps)) {
            s.status="approved"; s.completedDate=nowIso(); s.save(); changed=true;
            LOG_INFO("[F77] Auto-approved step '"+s.title+"'");

            // Delegate system step execution — each action is self-contained.
            if(s.isSystem) executeSystemStep(s, wf, changed);
        }

        // Spawn wait-condition F18_Operation if needed
        // Spawn wait condition for pending/in_progress steps
        if((s.status=="pending" || s.status=="in_progress")
           && !s.waitConditionF18Type.empty()
           && s.waitF18OperationId.empty() && s.canStart(wf.steps)) {
            spawnWaitConditionF18(s, wf.entityId);
            changed=true;
        }

        // Mark in_progress for ready mid-steps
        if(s.status=="pending" && !s.isInitialize && !s.isFinal
           && s.canStart(wf.steps) && s.waitF18OperationId.empty()) {
            s.status="in_progress"; s.save(); changed=true;
        }

        // End step: auto-approve when all mid-steps done
        if(s.isFinal && s.status=="pending" && wf.isComplete()) {
            s.status="approved"; s.completedDate=nowIso(); s.save(); changed=true;
            LOG_INFO("[F77] End step auto-approved — workflow closing");
        }
    }

    if(changed) checkAndComplete(wf);
    return changed;
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
        if(!s.waitF18OperationId.empty()) {
            auto waitOp = F18Operation::loadById(s.waitF18OperationId);
            if(waitOp && waitOp->status != "released")
                return "BLOCKED: Wartebedingung F18-Operation noch nicht freigegeben ("
                       + waitOp->status + ")";
        }
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
    F77_WorkflowStep* step=nullptr;
    for(auto& s:wf.steps) if(s.stepId==stepId){step=&s;break;}
    if(!step) { LOG_WARN("[F77] Step not found: "+stepId); return false; }
    if(step->isComplete()) { LOG_WARN("[F77] Step already complete: "+stepId); return false; }
    if(!step->canStart(wf.steps)) { LOG_WARN("[F77] Prerequisites not met: "+stepId); return false; }
    if(step->requiresComment && comment.empty()) { LOG_WARN("[F77] Comment required: "+stepId); return false; }

    // Check wait condition
    if(!step->waitF18OperationId.empty()) {
        auto waitOp = F18Operation::loadById(step->waitF18OperationId);
        if(waitOp && waitOp->status != "released") {
            LOG_WARN("[F77] Wait condition not satisfied for step: "+stepId);
            return false;
        }
    }

    step->status = decision;
    step->completedDate = nowIso();
    step->save();

    // Update the linked F18_Operation status via raw SQL
    // (bypasses the model guard since this is a system-controlled transition).
    if(!step->f18OperationId.empty()) {
        auto* f18db = DatabasePool::instance().get("f18");
        if (f18db) {
            std::string newStatus = (decision == "approved") ? "released" : "cancelled";
            f18db->exec(
                "UPDATE f18_operations SET status=?, updated_at=? WHERE vorgang_id=?;",
                {BindParam::text(newStatus), BindParam::text(nowIso()),
                 BindParam::text(step->f18OperationId)});
        }
    }

    LOG_INFO("[F77] Step fired: "+stepId+" decision="+decision+" by="+actorId);
    tick(wf);
    return true;
}

void F77_Engine::spawnWaitConditionF18(F77_WorkflowStep& step, const std::string& entityId) {
    std::string projId;
    // F18 resolves to its owning task (taskId)
    auto op = F18Operation::loadById(entityId);
    if(op) projId = op->taskId;
    if(projId.empty()) projId = entityId; // fallback

    if(projId.empty()) projId=entityId;
    std::string title = step.waitConditionF18Type.empty() ? "Wartebedingung" : step.waitConditionF18Type + " — Wartebedingung";
    if(step.waitConditionF18Type.empty()) { LOG_WARN("[F77] No wait condition type set"); return; }
    auto waitOp = F18Operation::create(entityId, title, step.waitConditionF18Type);
    if(waitOp) {
        step.waitF18OperationId = waitOp->vorgangId;
        step.save();
        LOG_INFO("[F77] Wait condition F18_Operation spawned: "+waitOp->vorgangId);
    }
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
    if(wf.entityType == "dok") {
        auto rev = DocumentRevision::currentRevision(wf.entityId);
        if(rev && DocumentRevision::isTransitionAllowed(rev->revState,
             revStateFromString(wf.targetState))) {
            rev->transitionState(wf.targetState);
            LOG_INFO("[F77] DOK revision transitioned: "+wf.entityId+" → "+wf.targetState);
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
        blockerCount = 1; // can't release without a workflow
        return false;
    }
    if (mainWf->status != "completed") {
        // Load steps and count incomplete non-Init ones as blockers
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
        if (wf->status == "active") blockerCount++;
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
        if(wf->workflowId==releaseWorkflowId || wf->status!="active") continue;
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

        // Step 4: End (auto-approved)
        auto end = t->addTemplateStep("End", "sequential", false, true);
        end.predecessorTplStepIds = mid.tplStepId;
        end.autoApprove = true;
        end.save();
    };

    // ── Standard templates (all entity types) ──────────────────
    // Dok gets all 5-state transitions; F16/F22/F18 only get released/closed
    const std::string dokTypes  = "dok";
    const std::string objTypes  = "f16,f22,f18,dok";

    // 1. Lock Workflow: → locked  (available from in_work and pre_released)
    makeTemplate("Dokument: Einfrieren",        "locked",       dokTypes,  "Sperren prüfen");
    makeTemplate("Objekt: Einfrieren",          "locked",       "f16,f22,f18", "Sperren prüfen");

    // 2. PreRelease Workflow: in_work → pre_released
    makeTemplate("Dokument: Zur Prüfung",       "pre_released", dokTypes,  "Prüfung durchführen");

    // 3. Release Workflow: → released
    makeTemplate("Dokument: Freigabe",          "released",     dokTypes,  "Freigabe erteilen");
    makeTemplate("Objekt: Freigabe",            "released",     "f16,f22,f18", "Freigabe erteilen");

    // 4. Close Workflow: → closed (= Ungültig markieren)
    makeTemplate("Dokument: Schliessen",        "closed",       dokTypes,  "Ungültigkeit bestätigen");
    makeTemplate("Objekt: Schliessen",          "closed",       "f16,f22,f18", "Ungültigkeit bestätigen");

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

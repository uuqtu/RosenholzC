// ============================================================
// F77Workflow.cpp — F77 Freigabe-Workflow Engine implementation
// ============================================================
#include "F77Workflow.h"
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

static std::string now() {
    auto t = std::time(nullptr);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

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

bool F77_WorkflowTemplateStep::save() const {
    auto* d = wfDB(); if (!d) return false;
    auto t=[](const std::string& s){return BindParam::text(s);};
    auto n=[](const std::string& s){return s.empty()?BindParam::null():BindParam::text(s);};
    auto i=[](int v){return BindParam::int64(v);};
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_template_steps
        (tpl_step_id,template_id,title,description,sequence_order,is_initialize,is_final,
         execution_mode,predecessor_tpl_step_ids,wait_condition_f18_type,wait_condition_title,
         required_role,sla_hours,auto_approve,requires_comment,requires_document,
         created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        t(tplStepId),t(templateId),t(title),n(description),i(sequenceOrder),
        i(isInitialize?1:0),i(isFinal?1:0),t(executionMode),n(predecessorTplStepIds),
        n(waitConditionF18Type),n(waitConditionTitle),n(requiredRole),i(slaHours),
        i(autoApprove?1:0),i(requiresComment?1:0),i(requiresDocument?1:0),
        t(createdAt),t(now())
    });
}

bool F77_WorkflowTemplateStep::remove() const {
    auto* d=wfDB(); if(!d) return false;
    return d->exec("DELETE FROM f77_workflow_template_steps WHERE tpl_step_id=?;",
                   {BindParam::text(tplStepId)});
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

bool F77_WorkflowTemplate::save() const {
    auto* d=wfDB(); if(!d) return false;
    auto t=[](const std::string& s){return BindParam::text(s);};
    auto n=[](const std::string& s){return s.empty()?BindParam::null():BindParam::text(s);};
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_templates
        (template_id,name,version,description,entity_types,target_state,status,
         created_by,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?)
    )SQL", {t(templateId),t(name),t(version),n(description),n(entityTypes),
            t(targetState),t(status),n(createdBy),t(createdAt),t(now())});
}

bool F77_WorkflowTemplate::remove() const {
    auto* d=wfDB(); if(!d) return false;
    d->exec("DELETE FROM f77_workflow_template_steps WHERE template_id=?;",{BindParam::text(templateId)});
    return d->exec("DELETE FROM f77_workflow_templates WHERE template_id=?;",{BindParam::text(templateId)});
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
    s.createdAt    = now();
    s.updatedAt    = now();
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
    t->createdAt   = now();
    t->updatedAt   = now();
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
    predecessorStepIds  = g("predecessor_step_ids");
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

bool F77_WorkflowStep::save() const {
    auto* d=wfDB(); if(!d) return false;
    auto t=[](const std::string& s){return BindParam::text(s);};
    auto n=[](const std::string& s){return s.empty()?BindParam::null():BindParam::text(s);};
    auto i=[](int v){return BindParam::int64(v);};
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflow_steps
        (step_id,workflow_id,tpl_step_id,title,sequence_order,is_initialize,is_final,
         execution_mode,predecessor_step_ids,f18_operation_id,wait_f18_operation_id,
         wait_condition_f18_type,status,auto_approve,requires_comment,requires_document,
         completed_date,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        t(stepId),t(workflowId),n(tplStepId),t(title),i(sequenceOrder),
        i(isInitialize?1:0),i(isFinal?1:0),t(executionMode),n(predecessorStepIds),
        n(f18OperationId),n(waitF18OperationId),n(waitConditionF18Type),
        t(status),i(autoApprove?1:0),i(requiresComment?1:0),i(requiresDocument?1:0),
        n(completedDate),t(createdAt),t(now())
    });
}

bool F77_WorkflowStep::remove() const {
    auto* d=wfDB(); if(!d) return false;
    return d->exec("DELETE FROM f77_workflow_steps WHERE step_id=?;",{BindParam::text(stepId)});
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
    if(status!="pending" && status!="in_progress") return false;
    if(predecessorStepIds.empty()) return true;
    std::istringstream ss(predecessorStepIds); std::string id;
    while(std::getline(ss, id, ',')) {
        id.erase(0, id.find_first_not_of(' '));
        id.erase(id.find_last_not_of(' ')+1);
        if(id.empty()) continue;
        bool done=false;
        for(auto& s:all) if(s.stepId==id){done=s.isComplete();break;}
        if(!done) return false;
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

bool F77_Workflow::save() const {
    auto* d=wfDB(); if(!d) return false;
    auto t=[](const std::string& s){return BindParam::text(s);};
    auto n=[](const std::string& s){return s.empty()?BindParam::null():BindParam::text(s);};
    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f77_workflows
        (workflow_id,template_id,template_name,entity_type,entity_id,target_state,
         status,initiated_by,initiated_date,completed_date,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {t(workflowId),n(templateId),t(templateName),t(entityType),t(entityId),
            t(targetState),t(status),n(initiatedBy),t(initiatedDate),n(completedDate),
            t(notes),t(createdAt),t(now())});
}

bool F77_Workflow::update() const { return save(); }

bool F77_Workflow::remove() const {
    auto* d=wfDB(); if(!d) return false;
    d->exec("DELETE FROM f77_workflow_steps WHERE workflow_id=?;",{BindParam::text(workflowId)});
    return d->exec("DELETE FROM f77_workflows WHERE workflow_id=?;",{BindParam::text(workflowId)});
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
    wf->initiatedDate= now();
    wf->createdAt    = now();
    wf->updatedAt    = now();
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
std::string F77_Engine::nowIso() { return now(); }

// ── Helper: store workflow ID back into entity ────────────────
static void storeWorkflowId(const std::string& entityType,
                              const std::string& entityId,
                              const std::string& workflowId) {
    std::string ts = nowIso();
    auto* f16db = DatabasePool::instance().get("f16");
    auto* f22db = DatabasePool::instance().get("f22");
    auto* f18db = DatabasePool::instance().get("f18");
    auto* dokdb = DatabasePool::instance().get("dok");
    if (entityType == "f16" && f16db)
        f16db->exec("UPDATE projects SET release_workflow_id=?, updated_at=? WHERE project_id=?;",
            {BindParam::text(workflowId), BindParam::text(ts), BindParam::text(entityId)});
    else if (entityType == "f22" && f22db)
        f22db->exec("UPDATE tasks SET release_workflow_id=?, updated_at=? WHERE task_id=?;",
            {BindParam::text(workflowId), BindParam::text(ts), BindParam::text(entityId)});
    else if (entityType == "f18" && f18db)
        f18db->exec("UPDATE f18_operations SET release_workflow_id=?, updated_at=? WHERE vorgang_id=?;",
            {BindParam::text(workflowId), BindParam::text(ts), BindParam::text(entityId)});
    else if (entityType == "dok" && dokdb)
        dokdb->exec("UPDATE documents SET release_workflow_id=?, updated_at=? WHERE document_id=?;",
            {BindParam::text(workflowId), BindParam::text(ts), BindParam::text(entityId)});
}


std::shared_ptr<F77_Workflow> F77_Engine::startFromTemplate(
    const std::string& templateId, const std::string& entityType,
    const std::string& entityId, const std::string& initiatedBy)
{
    // Enforce: exactly one active workflow per entity at a time.
    {
        std::string existingId;
        if (entityType == "f16") {
            auto p = ProjectF16::loadById(entityId);
            if (p) existingId = p->releaseWorkflowId;
        } else if (entityType == "f22") {
            auto t = TaskF22::loadById(entityId);
            if (t) existingId = t->releaseWorkflowId;
        } else if (entityType == "f18") {
            auto v = F18Operation::loadById(entityId);
            if (v) existingId = v->releaseWorkflowId;
        } else if (entityType == "dok") {
            auto d = Document::loadById(entityId);
            if (d) existingId = d->releaseWorkflowId;
        }
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
    if(!wf->save()) return nullptr;

    // Snapshot template steps → F77_WorkflowStep (+ F18_Operation per mid-step)
    // Build map: tpl_step_id → runtime step_id for predecessor resolution
    std::map<std::string,std::string> tplToRuntime;
    tpl->loadSteps();

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
        rs.completedDate = ts.isInitialize ? now() : "";
        rs.createdAt     = now();
        rs.updatedAt     = now();

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
            rs.predecessorStepIds = resolved;
        }

        // Spawn F18_Operation for mid-steps (not Init/End)
        if(!ts.isInitialize && !ts.isFinal) {
            std::string projId;
            if(entityType=="f16") projId=entityId;
            else if(entityType=="f22") { auto t=TaskF22::loadById(entityId); if(t) projId=t->projectId; }
            else if(entityType=="f18") { auto op=F18Operation::loadById(entityId); if(op) projId=op->projectId; }
            if(projId.empty()) projId=entityId;  // fallback: use entity id as project
            auto op = F18Operation::create(projId, ts.title, F18OperationType::F77_STEP, "");
            if(op) { rs.f18OperationId=op->vorgangId; }
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
    // Check releaseWorkflowId on the entity and refuse if active.
    {
        std::string existingId;
        if (entityType == "f16") {
            auto p = ProjectF16::loadById(entityId);
            if (p) existingId = p->releaseWorkflowId;
        } else if (entityType == "f22") {
            auto t = TaskF22::loadById(entityId);
            if (t) existingId = t->releaseWorkflowId;
        } else if (entityType == "f18") {
            auto v = F18Operation::loadById(entityId);
            if (v) existingId = v->releaseWorkflowId;
        } else if (entityType == "dok") {
            auto d = Document::loadById(entityId);
            if (d) existingId = d->releaseWorkflowId;
        }
        if (!existingId.empty()) {
            auto existing = F77_Workflow::loadById(existingId);
            if (existing && existing->status == "active") {
                LOG_WARN("[F77] startDefault verweigert: bereits ein aktiver Workflow ("
                         + existingId + ") fuer " + entityType + "/" + entityId);
                return nullptr;
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
    if(!wf->save()) return nullptr;

    std::string projId;
    if(entityType=="f16") projId=entityId;
    else if(entityType=="f22"){ auto t=TaskF22::loadById(entityId); if(t) projId=t->projectId; }
    else if(entityType=="f18"){ auto op=F18Operation::loadById(entityId); if(op) projId=op->projectId; }

    // Init step (auto-approved)
    F77_WorkflowStep init;
    init.stepId=genId("F77S"); init.workflowId=wf->workflowId;
    init.title="Init"; init.sequenceOrder=0; init.isInitialize=true;
    init.autoApprove=true; init.status="approved"; init.executionMode="sequential";
    init.completedDate=now(); init.createdAt=now(); init.updatedAt=now();
    init.save(); wf->steps.push_back(init);

    // Mid step — backed by F18_Operation
    F77_WorkflowStep mid;
    mid.stepId=genId("F77S"); mid.workflowId=wf->workflowId;
    mid.title="Freigabe vorbereiten"; mid.sequenceOrder=1;
    mid.executionMode="sequential"; mid.predecessorStepIds=init.stepId;
    mid.status="pending"; mid.createdAt=now(); mid.updatedAt=now();
    if(projId.empty()) projId=entityId;
    auto midOp = F18Operation::create(projId,"Freigabe vorbereiten",F18OperationType::F77_STEP,"");
    if(midOp) mid.f18OperationId=midOp->vorgangId;
    mid.save(); wf->steps.push_back(mid);

    // End step
    F77_WorkflowStep end;
    end.stepId=genId("F77S"); end.workflowId=wf->workflowId;
    end.title="End"; end.sequenceOrder=9999; end.isFinal=true;
    end.autoApprove=true; end.predecessorStepIds=mid.stepId;
    end.status="pending"; end.executionMode="sequential";
    end.createdAt=now(); end.updatedAt=now();
    end.save(); wf->steps.push_back(end);

    tick(*wf);

    // Store the workflow ID back into the entity's releaseWorkflowId field
    storeWorkflowId(entityType, entityId, wf->workflowId);

    return wf;
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
            s.status="approved"; s.completedDate=now(); s.save(); changed=true;
            LOG_INFO("[F77] Auto-approved step '"+s.title+"'");
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
            s.status="approved"; s.completedDate=now(); s.save(); changed=true;
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
    step->completedDate = now();
    step->save();

    // Update the linked F18_Operation status via raw SQL
    // (bypasses the model guard since this is a system-controlled transition).
    if(!step->f18OperationId.empty()) {
        auto* f18db = DatabasePool::instance().get("f18");
        if (f18db) {
            std::string newStatus = (decision == "approved") ? "released" : "cancelled";
            f18db->exec(
                "UPDATE f18_operations SET status=?, updated_at=? WHERE vorgang_id=?;",
                {BindParam::text(newStatus), BindParam::text(now()),
                 BindParam::text(step->f18OperationId)});
        }
    }

    LOG_INFO("[F77] Step fired: "+stepId+" decision="+decision+" by="+actorId);
    tick(wf);
    return true;
}

void F77_Engine::spawnWaitConditionF18(F77_WorkflowStep& step, const std::string& entityId) {
    std::string projId;
    // Try to find projectId from entityId
    auto op = F18Operation::loadById(entityId);
    if(op) projId = op->projectId;
    if(projId.empty()) projId = entityId; // fallback

    if(projId.empty()) projId=entityId;
    std::string title = step.waitConditionF18Type.empty() ? "Wartebedingung" : step.waitConditionF18Type + " — Wartebedingung";
    if(step.waitConditionF18Type.empty()) { LOG_WARN("[F77] No wait condition type set"); return; }
    auto waitOp = F18Operation::create(projId, title, step.waitConditionF18Type, "");
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
    wf.completedDate = now();
    wf.save();
    LOG_INFO("[F77] Workflow completed: "+wf.workflowId);
    applyTargetState(wf);
    return true;
}

bool F77_Engine::applyTargetState(const F77_Workflow& wf) {
    if(wf.entityType == "dok") {
        auto rev = DocumentRevision::currentRevision(wf.entityId);
        if(rev && DocumentRevision::isTransitionAllowed(rev->revState, wf.targetState)) {
            rev->transitionState(wf.targetState);
            LOG_INFO("[F77] DOK revision transitioned: "+wf.entityId+" → "+wf.targetState);
        }
    } else {
        // F16/F22/F18: set status = targetState in the entity DB
        auto* pdb  = DatabasePool::instance().get("f16");
        auto* t22db= DatabasePool::instance().get("f22");
        auto* f18db= DatabasePool::instance().get("f18");
        auto ts = wf.targetState;
        auto id = wf.entityId;
        auto upd = [&](Database* d, const std::string& tbl, const std::string& col){
            if(!d) return;
            d->exec("UPDATE "+tbl+" SET status=?, updated_at=? WHERE "+col+"=?;",
                    {BindParam::text(ts),BindParam::text(now()),BindParam::text(id)});
        };
        if(wf.entityType=="f16") upd(pdb,  "projects","project_id");
        else if(wf.entityType=="f22") upd(t22db,"tasks","task_id");
        else if(wf.entityType=="f18") upd(f18db,"f18_operations","vorgang_id");
        LOG_INFO("[F77] Entity status set to '"+ts+"': "+wf.entityType+"/"+id);
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
    // Check if already seeded
    try {
        auto existing = d->query("SELECT COUNT(*) FROM f77_workflow_templates;",{});
        if(!existing.empty()) {
            std::string cnt = existing[0].begin()->second;
            if(cnt != "0") return;
        }
    } catch(...) { return; }

    // Template 1: Standard Freigabe → released
    {
        auto t1 = F77_WorkflowTemplate::create("Standard-Freigabe","released","f16,f22,f18,dok");
        t1->save();
        auto init1 = t1->addTemplateStep("Init","sequential",true,false);
        init1.save();
        auto mid1 = t1->addTemplateStep("Freigabe vorbereiten","sequential",false,false);
        mid1.predecessorTplStepIds = init1.tplStepId;
        mid1.save();
        auto end1 = t1->addTemplateStep("End","sequential",false,true);
        end1.predecessorTplStepIds = mid1.tplStepId;
        end1.autoApprove=true; end1.save();
    }

    {
        auto t2 = F77_WorkflowTemplate::create("Qualitätssicherung","pre_released","f16,f18,dok");
        t2->save();
        auto init2 = t2->addTemplateStep("Init","sequential",true,false); init2.save();
        auto qa = t2->addTemplateStep("Qualitätsprüfung","sequential",false,false);
        qa.predecessorTplStepIds=init2.tplStepId;
        qa.waitConditionF18Type="qualityGate";
        qa.waitConditionTitle="QG — Qualitätsprüfung";
        qa.save();
        auto approve2 = t2->addTemplateStep("Abnahme","sequential",false,false);
        approve2.predecessorTplStepIds=qa.tplStepId; approve2.save();
        auto end2 = t2->addTemplateStep("End","sequential",false,true);
        end2.predecessorTplStepIds=approve2.tplStepId;
        end2.autoApprove=true; end2.save();
    }

    LOG_INFO("[F77] Default templates seeded");
}

} // namespace Rosenholz

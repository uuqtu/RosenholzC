// ReportingModels.cpp
#include "ReportingModels.h"
#include "../core/Repository.h"
#include "../core/Database.h"
#include "../core/Logger.h"

namespace Rosenholz {

// ════════════════════════════════════════════════════════════
// MEASURE
// ════════════════════════════════════════════════════════════
std::shared_ptr<Measure> Measure::create(const std::string& pid, const std::string& t, const std::string& mt) {
    auto m=std::make_shared<Measure>();
    m->measureId=genId("MSN"); m->projectId=pid; m->title=t; m->measureType=mt;
    m->status="planned"; m->createdAt=nowIso(); m->notes="{}";
    LOG_INFO("Creating Measure: "+t); return m;
}
bool Measure::save() const {
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    bool ok=db->exec(R"(INSERT OR REPLACE INTO measures
        (measure_id,workflow_instance_id,workflow_status,workflow_current_state,
         risk_id,incident_id,project_id,task_id,owner_id,title,description,
         measure_type,measure_category,status,planned_date,actual_date,
         cost_planned,cost_actual,effort_hrs,effectiveness,verification_method,
         verified_date,verified_by,outcome,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(measureId),textOrNull(workflowInstanceId),textOrNull(workflowStatus),textOrNull(workflowCurrentState),
        textOrNull(riskId),textOrNull(incidentId),textOrNull(projectId),textOrNull(taskId),textOrNull(ownerId),
        BindParam::text(title),textOrNull(description),textOrNull(measureType),textOrNull(measureCategory),
        BindParam::text(status.empty()?"planned":status),textOrNull(plannedDate),textOrNull(actualDate),
        BindParam::real(costPlanned),BindParam::real(costActual),BindParam::real(effortHrs),
        textOrNull(effectiveness),textOrNull(verificationMethod),textOrNull(verifiedDate),textOrNull(verifiedBy),
        textOrNull(outcome),textOrNull(links),BindParam::text(notes.empty()?"{}":notes)});
    if(ok)LOG_INFO("Measure saved: "+measureId); else LOG_ERROR("Measure save failed: "+measureId);
    return ok;
}
void Measure::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    measureId=g("measure_id"); workflowInstanceId=g("workflow_instance_id");
    riskId=g("risk_id"); incidentId=g("incident_id"); projectId=g("project_id");
    taskId=g("task_id"); ownerId=g("owner_id"); title=g("title"); description=g("description");
    measureType=g("measure_type"); measureCategory=g("measure_category"); status=g("status");
    plannedDate=g("planned_date"); actualDate=g("actual_date");
    auto gd=[&](const std::string& k){auto v=g(k);return v.empty()?0.0:std::stod(v);};
    costPlanned=gd("cost_planned"); costActual=gd("cost_actual"); effortHrs=gd("effort_hrs");
    effectiveness=g("effectiveness"); verificationMethod=g("verification_method");
    verifiedDate=g("verified_date"); verifiedBy=g("verified_by");
    outcome=g("outcome"); links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool Measure::load(const std::string& id){
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    auto rows=db->query("SELECT * FROM measures WHERE measure_id=?;",{BindParam::text(id)});
    if(rows.empty()){LOG_WARN("Measure not found: "+id);return false;}
    fromRow(rows[0]); return true;
}
bool Measure::remove(){auto* db=DatabasePool::instance().get("reporting");return db&&db->exec("DELETE FROM measures WHERE measure_id=?;",{BindParam::text(measureId)});}
bool Measure::update(){return save();}
std::shared_ptr<Measure> Measure::loadById(const std::string& id){auto m=std::make_shared<Measure>();if(!m->load(id))return nullptr;return m;}
std::vector<std::shared_ptr<Measure>> Measure::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<Measure>> r; if(!db) return r;
    auto rows=db->query("SELECT * FROM measures WHERE project_id=? ORDER BY planned_date;",{BindParam::text(pid)});
    for(auto& row:rows){auto m=std::make_shared<Measure>();m->fromRow(row);r.push_back(m);}return r;
}
std::vector<std::shared_ptr<Measure>> Measure::loadForRisk(const std::string& rid){
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<Measure>> r; if(!db) return r;
    auto rows=db->query("SELECT * FROM measures WHERE risk_id=?;",{BindParam::text(rid)});
    for(auto& row:rows){auto m=std::make_shared<Measure>();m->fromRow(row);r.push_back(m);}return r;
}
bool Measure::reassignOwner(const std::string& id){ownerId=id;return update();}
nlohmann::json Measure::toJson() const{return{{"measureId",measureId},{"title",title},{"status",status},{"measureType",measureType}};}

// ════════════════════════════════════════════════════════════
// QUALITY GATE
// ════════════════════════════════════════════════════════════
std::shared_ptr<QualityGate> QualityGate::create(const std::string& pid, const std::string& t, const std::string& ph){
    auto g=std::make_shared<QualityGate>();
    g->gateId=genId("QT"); g->projectId=pid; g->title=t; g->phase=ph;
    g->result="pending"; g->createdAt=nowIso(); g->notes="{}";
    LOG_INFO("Creating QualityGate: "+t); return g;
}
bool QualityGate::save() const {
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    bool ok=db->exec(R"(INSERT OR REPLACE INTO quality_gates
        (gate_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,reviewer_id,title,phase,planned_date,actual_date,
         criteria,standards_applied,quality_objectives,acceptance_criteria,
         audit_schedule,tools_methods,result,decision,findings,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(gateId),textOrNull(workflowInstanceId),textOrNull(workflowStatus),textOrNull(workflowCurrentState),
        textOrNull(projectId),textOrNull(reviewerId),BindParam::text(title),textOrNull(phase),
        textOrNull(plannedDate),textOrNull(actualDate),textOrNull(criteria),textOrNull(standardsApplied),
        textOrNull(qualityObjectives),textOrNull(acceptanceCriteria),textOrNull(auditSchedule),textOrNull(toolsMethods),
        BindParam::text(result.empty()?"pending":result),textOrNull(decision),textOrNull(findings),
        textOrNull(links),BindParam::text(notes.empty()?"{}":notes)});
    if(ok)LOG_INFO("QualityGate saved: "+gateId); return ok;
}
void QualityGate::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    gateId=g("gate_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); reviewerId=g("reviewer_id"); title=g("title"); phase=g("phase");
    plannedDate=g("planned_date"); actualDate=g("actual_date"); criteria=g("criteria");
    standardsApplied=g("standards_applied"); qualityObjectives=g("quality_objectives");
    acceptanceCriteria=g("acceptance_criteria"); auditSchedule=g("audit_schedule");
    toolsMethods=g("tools_methods"); result=g("result"); decision=g("decision");
    findings=g("findings"); links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool QualityGate::load(const std::string& id){
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    auto rows=db->query("SELECT * FROM quality_gates WHERE gate_id=?;",{BindParam::text(id)});
    if(rows.empty()) return false; fromRow(rows[0]); return true;
}
bool QualityGate::remove(){auto* db=DatabasePool::instance().get("reporting");return db&&db->exec("DELETE FROM quality_gates WHERE gate_id=?;",{BindParam::text(gateId)});}
bool QualityGate::update(){return save();}
std::shared_ptr<QualityGate> QualityGate::loadById(const std::string& id){auto g=std::make_shared<QualityGate>();if(!g->load(id))return nullptr;return g;}
std::vector<std::shared_ptr<QualityGate>> QualityGate::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<QualityGate>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM quality_gates WHERE project_id=? ORDER BY planned_date;",{BindParam::text(pid)});
    for(auto& r:rows){auto g=std::make_shared<QualityGate>();g->fromRow(r);res.push_back(g);}return res;
}
bool QualityGate::recordResult(const std::string& res, const std::string& dec, const std::string& find){
    result=res; decision=dec; findings=find; actualDate=nowIso(); return update();
}

// ═════════════════════════════════════════════════════════════
// MEETING
// ═════════════════════════════════════════════════════════════
std::shared_ptr<Meeting> Meeting::create(const std::string& taskId,
                                          const std::string& title,
                                          const std::string& organiserId) {
    auto m = std::make_shared<Meeting>();
    m->meetingId   = genId("BSP");
    m->taskId      = taskId;
    m->title       = title;
    m->organiserId = organiserId;
    m->status      = "scheduled";
    m->createdAt   = nowIso();
    return m;
}

void Meeting::fromRow(const Row& r) {
    meetingId            = rowGet(r,"meeting_id");
    workflowInstanceId   = rowGet(r,"workflow_instance_id");
    workflowStatus       = rowGet(r,"workflow_status");
    workflowCurrentState = rowGet(r,"workflow_current_state");
    taskId               = rowGet(r,"task_id");
    projectId            = rowGet(r,"project_id");
    organiserId          = rowGet(r,"organiser_id");
    title                = rowGet(r,"title");
    meetingType          = rowGetOr(r,"meeting_type","general");
    status               = rowGetOr(r,"status","scheduled");
    scheduledDate        = rowGet(r,"scheduled_date");
    actualDate           = rowGet(r,"actual_date");
    durationMins         = rowGetInt(r,"duration_mins");
    location             = rowGet(r,"location");
    channel              = rowGet(r,"channel");
    agenda               = rowGet(r,"agenda");
    decisions            = rowGet(r,"decisions");
    actions              = rowGet(r,"actions");
    nextMeetingId        = rowGet(r,"next_meeting_id");
    links                = rowGet(r,"links");
    notes                = rowGetOr(r,"notes","{}");
    createdAt            = rowGet(r,"created_at");
}

bool Meeting::save() const {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    return db->exec(R"SQL(
        INSERT OR REPLACE INTO meetings
        (meeting_id,workflow_instance_id,workflow_status,workflow_current_state,
         task_id,project_id,organiser_id,title,meeting_type,status,
         scheduled_date,actual_date,duration_mins,location,channel,
         agenda,decisions,actions,next_meeting_id,links,notes,created_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))SQL", {
        BindParam::text(meetingId), textOrNull(workflowInstanceId),
        textOrNull(workflowStatus), textOrNull(workflowCurrentState),
        BindParam::text(taskId), textOrNull(projectId), textOrNull(organiserId),
        BindParam::text(title), BindParam::text(meetingType), BindParam::text(status),
        textOrNull(scheduledDate), textOrNull(actualDate),
        BindParam::int64(durationMins),
        textOrNull(location), textOrNull(channel),
        textOrNull(agenda), textOrNull(decisions), textOrNull(actions),
        textOrNull(nextMeetingId), textOrNull(links),
        BindParam::text(notes.empty()?"{}":notes), BindParam::text(createdAt)
    });
}

bool Meeting::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM meetings WHERE meeting_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) return false;
    fromRow(rows[0]);
    return true;
}

bool Meeting::update() { return save(); }

bool Meeting::remove() {
    auto* db = DatabasePool::instance().get("projects");
    return db && db->exec("DELETE FROM meetings WHERE meeting_id=?;",
                          {BindParam::text(meetingId)});
}

std::shared_ptr<Meeting> Meeting::loadById(const std::string& id) {
    auto m = std::make_shared<Meeting>();
    if (!m->load(id)) return nullptr;
    return m;
}

std::vector<std::shared_ptr<Meeting>> Meeting::loadForProject(const std::string& pid) {
    auto* db = DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<Meeting>> res;
    if (!db) return res;
    for (auto& r : db->query(
            "SELECT * FROM meetings WHERE project_id=? ORDER BY scheduled_date;",
            {BindParam::text(pid)})) {
        auto m = std::make_shared<Meeting>(); m->fromRow(r); res.push_back(m);
    }
    return res;
}

std::vector<std::shared_ptr<Meeting>> Meeting::loadForTask(const std::string& tid) {
    auto* db = DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<Meeting>> res;
    if (!db) return res;
    for (auto& r : db->query(
            "SELECT * FROM meetings WHERE task_id=? ORDER BY scheduled_date;",
            {BindParam::text(tid)})) {
        auto m = std::make_shared<Meeting>(); m->fromRow(r); res.push_back(m);
    }
    return res;
}

bool Meeting::complete(const std::string& dec, const std::string& act) {
    status    = "completed";
    actualDate = nowIso();
    decisions = dec;
    actions   = act;
    return update();
}

// ═════════════════════════════════════════════════════════════
// CHANGE REQUEST
// ═════════════════════════════════════════════════════════════
std::shared_ptr<ChangeRequest> ChangeRequest::create(
    const std::string& projectId, const std::string& title, const std::string& type)
{
    auto cr = std::make_shared<ChangeRequest>();
    cr->crId       = genId("AEA");
    cr->projectId  = projectId;
    cr->title      = title;
    cr->changeType = type.empty() ? "general" : type;
    cr->status     = "draft";
    cr->raisedDate = nowIso();
    cr->createdAt  = nowIso();
    return cr;
}

void ChangeRequest::fromRow(const Row& r) {
    crId                = rowGet(r,"cr_id");
    workflowInstanceId  = rowGet(r,"workflow_instance_id");
    workflowStatus      = rowGet(r,"workflow_status");
    workflowCurrentState= rowGet(r,"workflow_current_state");
    projectId           = rowGet(r,"project_id");
    taskId              = rowGet(r,"task_id");
    raisedBy            = rowGet(r,"raised_by");
    title               = rowGet(r,"title");
    description         = rowGet(r,"description");
    changeType          = rowGetOr(r,"change_type","general");
    status              = rowGetOr(r,"status","draft");
    raisedDate          = rowGet(r,"raised_date");
    decisionDate        = rowGet(r,"decision_date");
    implementedDate     = rowGet(r,"implemented_date");
    costImpact          = rowGetDbl(r,"cost_impact");
    scheduleImpactDays  = rowGetInt(r,"schedule_impact_days");
    scopeImpact         = rowGet(r,"scope_impact");
    qualityImpact       = rowGet(r,"quality_impact");
    justification       = rowGet(r,"justification");
    decisionRationale   = rowGet(r,"decision_rationale");
    links               = rowGet(r,"links");
    notes               = rowGetOr(r,"notes","{}");
    createdAt           = rowGet(r,"created_at");
}

bool ChangeRequest::save() const {
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return false;
    return db->exec(R"SQL(
        INSERT OR REPLACE INTO change_requests
        (cr_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,raised_by,title,description,change_type,status,
         raised_date,decision_date,implemented_date,cost_impact,
         schedule_impact_days,scope_impact,quality_impact,
         justification,decision_rationale,links,notes,created_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )SQL", {
        BindParam::text(crId), textOrNull(workflowInstanceId),
        textOrNull(workflowStatus), textOrNull(workflowCurrentState),
        textOrNull(projectId), textOrNull(taskId), textOrNull(raisedBy),
        BindParam::text(title), textOrNull(description),
        BindParam::text(changeType), BindParam::text(status),
        textOrNull(raisedDate), textOrNull(decisionDate), textOrNull(implementedDate),
        BindParam::real(costImpact), BindParam::int64(scheduleImpactDays),
        textOrNull(scopeImpact), textOrNull(qualityImpact),
        textOrNull(justification), textOrNull(decisionRationale),
        textOrNull(links), BindParam::text(notes.empty()?"{}":notes),
        BindParam::text(createdAt)
    });
}

bool ChangeRequest::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM change_requests WHERE cr_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) return false;
    fromRow(rows[0]);
    return true;
}

bool ChangeRequest::update() { return save(); }

bool ChangeRequest::remove() {
    auto* db = DatabasePool::instance().get("tracking");
    return db && db->exec("DELETE FROM change_requests WHERE cr_id=?;",
                          {BindParam::text(crId)});
}

std::shared_ptr<ChangeRequest> ChangeRequest::loadById(const std::string& id) {
    auto cr = std::make_shared<ChangeRequest>();
    if (!cr->load(id)) return nullptr;
    return cr;
}

std::vector<std::shared_ptr<ChangeRequest>> ChangeRequest::loadForProject(
    const std::string& pid)
{
    auto* db = DatabasePool::instance().get("tracking");
    std::vector<std::shared_ptr<ChangeRequest>> result;
    if (!db) return result;
    for (auto& r : db->query(
            "SELECT * FROM change_requests WHERE project_id=? ORDER BY raised_date DESC;",
            {BindParam::text(pid)})) {
        auto cr = std::make_shared<ChangeRequest>(); cr->fromRow(r); result.push_back(cr);
    }
    return result;
}

std::vector<std::shared_ptr<ChangeRequest>> ChangeRequest::loadOpen() {
    auto* db = DatabasePool::instance().get("tracking");
    std::vector<std::shared_ptr<ChangeRequest>> result;
    if (!db) return result;
    for (auto& r : db->query(
            "SELECT * FROM change_requests WHERE status NOT IN "
            "('approved','rejected','implemented','withdrawn') "
            "ORDER BY raised_date DESC;")) {
        auto cr = std::make_shared<ChangeRequest>(); cr->fromRow(r); result.push_back(cr);
    }
    return result;
}

// ------------------------------
// approve
//
// Parameters:
//   rationale            : text explaining the approval decision
//
// Behavior:
//   Sets status="approved", decisionDate=now, decisionRationale
//   Calls update() to persist
//
// Returns:
//   true on success
// ------------------------------
bool ChangeRequest::approve(const std::string& rationale) {
    status           = "approved";
    decisionDate     = nowIso();
    decisionRationale= rationale;
    return update();
}

// ------------------------------
// reject
//
// Parameters:
//   rationale            : text explaining the rejection
//
// Behavior:
//   Sets status="rejected", decisionDate=now, decisionRationale
//   Calls update() to persist
//
// Returns:
//   true on success
// ------------------------------
bool ChangeRequest::reject(const std::string& rationale) {
    status           = "rejected";
    decisionDate     = nowIso();
    decisionRationale= rationale;
    return update();
}

nlohmann::json ChangeRequest::toJson() const {
    return {{"crId",crId},{"title",title},{"status",status},{"changeType",changeType}};
}

} // namespace Rosenholz

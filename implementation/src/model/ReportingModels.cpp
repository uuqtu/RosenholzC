// ReportingModels.cpp
#include "ReportingModels.h"
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
        BindParam::text(measureId),ton(workflowInstanceId),ton(workflowStatus),ton(workflowCurrentState),
        ton(riskId),ton(incidentId),ton(projectId),ton(taskId),ton(ownerId),
        BindParam::text(title),ton(description),ton(measureType),ton(measureCategory),
        BindParam::text(status.empty()?"planned":status),ton(plannedDate),ton(actualDate),
        BindParam::real(costPlanned),BindParam::real(costActual),BindParam::real(effortHrs),
        ton(effectiveness),ton(verificationMethod),ton(verifiedDate),ton(verifiedBy),
        ton(outcome),ton(links),BindParam::text(notes.empty()?"{}":notes)});
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
        BindParam::text(gateId),ton(workflowInstanceId),ton(workflowStatus),ton(workflowCurrentState),
        ton(projectId),ton(reviewerId),BindParam::text(title),ton(phase),
        ton(plannedDate),ton(actualDate),ton(criteria),ton(standardsApplied),
        ton(qualityObjectives),ton(acceptanceCriteria),ton(auditSchedule),ton(toolsMethods),
        BindParam::text(result.empty()?"pending":result),ton(decision),ton(findings),
        ton(links),BindParam::text(notes.empty()?"{}":notes)});
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
nlohmann::json QualityGate::toJson() const{return{{"gateId",gateId},{"title",title},{"phase",phase},{"result",result},{"decision",decision}};}

// ════════════════════════════════════════════════════════════
// KPI
// ════════════════════════════════════════════════════════════
std::shared_ptr<KPI> KPI::create(const std::string& pid, const std::string& t, const std::string& u){
    auto k=std::make_shared<KPI>();
    k->kpiId=genId("KPI"); k->projectId=pid; k->title=t; k->unit=u;
    k->ragStatus="amber"; k->createdAt=nowIso(); k->notes="{}";
    LOG_INFO("Creating KPI: "+t); return k;
}
bool KPI::save() const {
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    return db->exec(R"(INSERT OR REPLACE INTO kpis
        (kpi_id,project_id,task_id,owner_id,title,description,category,dimension,unit,
         target_value,actual_value,baseline_value,threshold_green,threshold_amber,threshold_red,
         rag_status,measurement_method,measurement_frequency,
         last_measured_date,next_measurement_date,trend,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(kpiId),ton(projectId),ton(taskId),ton(ownerId),
        BindParam::text(title),ton(description),ton(category),ton(dimension),ton(unit),
        BindParam::real(targetValue),BindParam::real(actualValue),BindParam::real(baselineValue),
        BindParam::real(thresholdGreen),BindParam::real(thresholdAmber),BindParam::real(thresholdRed),
        BindParam::text(ragStatus.empty()?"amber":ragStatus),ton(measurementMethod),ton(measurementFrequency),
        ton(lastMeasuredDate),ton(nextMeasurementDate),ton(trend),ton(links),
        BindParam::text(notes.empty()?"{}":notes)});
}
void KPI::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    kpiId=g("kpi_id"); projectId=g("project_id"); taskId=g("task_id"); ownerId=g("owner_id");
    title=g("title"); description=g("description"); category=g("category");
    dimension=g("dimension"); unit=g("unit");
    auto gd=[&](const std::string& k){auto v=g(k);return v.empty()?0.0:std::stod(v);};
    targetValue=gd("target_value"); actualValue=gd("actual_value"); baselineValue=gd("baseline_value");
    thresholdGreen=gd("threshold_green"); thresholdAmber=gd("threshold_amber"); thresholdRed=gd("threshold_red");
    ragStatus=g("rag_status"); measurementMethod=g("measurement_method");
    measurementFrequency=g("measurement_frequency"); lastMeasuredDate=g("last_measured_date");
    nextMeasurementDate=g("next_measurement_date"); trend=g("trend");
    links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool KPI::load(const std::string& id){
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    auto rows=db->query("SELECT * FROM kpis WHERE kpi_id=?;",{BindParam::text(id)});
    if(rows.empty()) return false; fromRow(rows[0]); return true;
}
bool KPI::remove(){auto* db=DatabasePool::instance().get("reporting");return db&&db->exec("DELETE FROM kpis WHERE kpi_id=?;",{BindParam::text(kpiId)});}
bool KPI::update(){return save();}
std::shared_ptr<KPI> KPI::loadById(const std::string& id){auto k=std::make_shared<KPI>();if(!k->load(id))return nullptr;return k;}
std::vector<std::shared_ptr<KPI>> KPI::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<KPI>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM kpis WHERE project_id=? ORDER BY title;",{BindParam::text(pid)});
    for(auto& r:rows){auto k=std::make_shared<KPI>();k->fromRow(r);res.push_back(k);}return res;
}
void KPI::updateRAG(){
    if(actualValue>=thresholdGreen) ragStatus="green";
    else if(actualValue>=thresholdAmber) ragStatus="amber";
    else ragStatus="red";
    if(actualValue>0&&targetValue>0){
        trend=(actualValue>=targetValue)?"up":"down";
    }
}
bool KPI::recordMeasurement(double v, const std::string& d){
    actualValue=v; lastMeasuredDate=d.empty()?nowIso():d; updateRAG(); return update();
}
nlohmann::json KPI::toJson() const{return{{"kpiId",kpiId},{"title",title},{"unit",unit},{"actualValue",actualValue},{"targetValue",targetValue},{"ragStatus",ragStatus}};}

// ════════════════════════════════════════════════════════════
// LESSON LEARNED
// ════════════════════════════════════════════════════════════
std::shared_ptr<LessonLearned> LessonLearned::create(const std::string& pid, const std::string& t){
    auto l=std::make_shared<LessonLearned>();
    l->lessonId=genId("LE"); l->projectId=pid; l->title=t;
    l->status="draft"; l->identifiedDate=nowIso(); l->createdAt=nowIso(); l->notes="{}";
    return l;
}
bool LessonLearned::save() const {
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    return db->exec(R"(INSERT OR REPLACE INTO lessons_learned
        (lesson_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,incident_id,submitted_by,reviewed_by,title,description,
         category,dimension,identified_date,reviewed_date,status,impact,
         recommendation,action_taken,added_to_kb,tags,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(lessonId),ton(workflowInstanceId),ton(workflowStatus),ton(workflowCurrentState),
        ton(projectId),ton(taskId),ton(incidentId),ton(submittedBy),ton(reviewedBy),
        BindParam::text(title),ton(description),ton(category),ton(dimension),
        ton(identifiedDate),ton(reviewedDate),BindParam::text(status.empty()?"draft":status),
        ton(impact),ton(recommendation),ton(actionTaken),
        BindParam::int64(addedToKb?1:0),ton(tags),ton(links),
        BindParam::text(notes.empty()?"{}":notes)});
}
void LessonLearned::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    lessonId=g("lesson_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id"); incidentId=g("incident_id");
    submittedBy=g("submitted_by"); reviewedBy=g("reviewed_by"); title=g("title");
    description=g("description"); category=g("category"); dimension=g("dimension");
    identifiedDate=g("identified_date"); reviewedDate=g("reviewed_date"); status=g("status");
    impact=g("impact"); recommendation=g("recommendation"); actionTaken=g("action_taken");
    addedToKb=g("added_to_kb")=="1"; tags=g("tags"); links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool LessonLearned::load(const std::string& id){
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    auto rows=db->query("SELECT * FROM lessons_learned WHERE lesson_id=?;",{BindParam::text(id)});
    if(rows.empty()) return false; fromRow(rows[0]); return true;
}
bool LessonLearned::remove(){auto* db=DatabasePool::instance().get("reporting");return db&&db->exec("DELETE FROM lessons_learned WHERE lesson_id=?;",{BindParam::text(lessonId)});}
bool LessonLearned::update(){return save();}
std::shared_ptr<LessonLearned> LessonLearned::loadById(const std::string& id){auto l=std::make_shared<LessonLearned>();if(!l->load(id))return nullptr;return l;}
std::vector<std::shared_ptr<LessonLearned>> LessonLearned::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<LessonLearned>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM lessons_learned WHERE project_id=? ORDER BY identified_date DESC;",{BindParam::text(pid)});
    for(auto& r:rows){auto l=std::make_shared<LessonLearned>();l->fromRow(r);res.push_back(l);}return res;
}
std::vector<std::shared_ptr<LessonLearned>> LessonLearned::loadKnowledgeBase(){
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<LessonLearned>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM lessons_learned WHERE added_to_kb=1 ORDER BY identified_date DESC;");
    for(auto& r:rows){auto l=std::make_shared<LessonLearned>();l->fromRow(r);res.push_back(l);}
    LOG_INFO("Knowledge base: "+std::to_string(res.size())+" lessons"); return res;
}
nlohmann::json LessonLearned::toJson() const{return{{"lessonId",lessonId},{"title",title},{"status",status},{"category",category}};}

// ════════════════════════════════════════════════════════════
// DECISION LOG
// ════════════════════════════════════════════════════════════
std::shared_ptr<DecisionLog> DecisionLog::create(const std::string& pid, const std::string& t){
    auto d=std::make_shared<DecisionLog>();
    d->decisionId=genId("ENT"); d->projectId=pid; d->title=t;
    d->status="open"; d->decisionDate=nowIso(); d->createdAt=nowIso(); d->notes="{}";
    return d;
}
bool DecisionLog::save() const {
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    return db->exec(R"(INSERT OR REPLACE INTO decision_log
        (decision_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,meeting_id,decided_by,title,description,decision_type,
         status,decision_date,review_date,options_considered,rationale,
         impact_cost,impact_schedule,impact_scope,impact_quality,assumptions_made,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(decisionId),ton(workflowInstanceId),ton(workflowStatus),ton(workflowCurrentState),
        ton(projectId),ton(taskId),ton(meetingId),ton(decidedBy),
        BindParam::text(title),ton(description),ton(decisionType),
        BindParam::text(status.empty()?"open":status),ton(decisionDate),ton(reviewDate),
        ton(optionsConsidered),ton(rationale),ton(impactCost),ton(impactSchedule),
        ton(impactScope),ton(impactQuality),ton(assumptionsMade),ton(links),
        BindParam::text(notes.empty()?"{}":notes)});
}
void DecisionLog::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    decisionId=g("decision_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id"); meetingId=g("meeting_id");
    decidedBy=g("decided_by"); title=g("title"); description=g("description");
    decisionType=g("decision_type"); status=g("status"); decisionDate=g("decision_date");
    reviewDate=g("review_date"); optionsConsidered=g("options_considered");
    rationale=g("rationale"); impactCost=g("impact_cost"); impactSchedule=g("impact_schedule");
    impactScope=g("impact_scope"); impactQuality=g("impact_quality");
    assumptionsMade=g("assumptions_made"); links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool DecisionLog::load(const std::string& id){
    auto* db=DatabasePool::instance().get("reporting"); if(!db) return false;
    auto rows=db->query("SELECT * FROM decision_log WHERE decision_id=?;",{BindParam::text(id)});
    if(rows.empty()) return false; fromRow(rows[0]); return true;
}
bool DecisionLog::remove(){auto* db=DatabasePool::instance().get("reporting");return db&&db->exec("DELETE FROM decision_log WHERE decision_id=?;",{BindParam::text(decisionId)});}
bool DecisionLog::update(){return save();}
std::shared_ptr<DecisionLog> DecisionLog::loadById(const std::string& id){auto d=std::make_shared<DecisionLog>();if(!d->load(id))return nullptr;return d;}
std::vector<std::shared_ptr<DecisionLog>> DecisionLog::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<DecisionLog>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM decision_log WHERE project_id=? ORDER BY decision_date DESC;",{BindParam::text(pid)});
    for(auto& r:rows){auto d=std::make_shared<DecisionLog>();d->fromRow(r);res.push_back(d);}return res;
}
nlohmann::json DecisionLog::toJson() const{return{{"decisionId",decisionId},{"title",title},{"status",status},{"decidedBy",decidedBy}};}

// ════════════════════════════════════════════════════════════
// CHANGE REQUEST
// ════════════════════════════════════════════════════════════
std::shared_ptr<ChangeRequest> ChangeRequest::create(const std::string& pid, const std::string& t, const std::string& ct){
    auto c=std::make_shared<ChangeRequest>();
    c->crId=genId("AEA"); c->projectId=pid; c->title=t; c->changeType=ct;
    c->status="draft"; c->raisedDate=nowIso(); c->createdAt=nowIso(); c->notes="{}";
    LOG_INFO("Creating ChangeRequest: "+t); return c;
}
bool ChangeRequest::save() const {
    auto* db=DatabasePool::instance().get("tracking"); if(!db) return false;
    bool ok=db->exec(R"(INSERT OR REPLACE INTO change_requests
        (cr_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,raised_by,title,description,change_type,
         status,raised_date,decision_date,implemented_date,
         cost_impact,schedule_impact_days,scope_impact,quality_impact,
         justification,decision_rationale,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(crId),ton(workflowInstanceId),ton(workflowStatus),ton(workflowCurrentState),
        ton(projectId),ton(taskId),ton(raisedBy),BindParam::text(title),ton(description),ton(changeType),
        BindParam::text(status.empty()?"draft":status),ton(raisedDate),ton(decisionDate),ton(implementedDate),
        BindParam::real(costImpact),BindParam::int64(scheduleImpactDays),
        ton(scopeImpact),ton(qualityImpact),ton(justification),ton(decisionRationale),
        ton(links),BindParam::text(notes.empty()?"{}":notes)});
    if(ok)LOG_INFO("ChangeRequest saved: "+crId); else LOG_ERROR("ChangeRequest save failed: "+crId);
    return ok;
}
void ChangeRequest::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    crId=g("cr_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id"); raisedBy=g("raised_by");
    title=g("title"); description=g("description"); changeType=g("change_type");
    status=g("status"); raisedDate=g("raised_date"); decisionDate=g("decision_date");
    implementedDate=g("implemented_date");
    auto gd=[&](const std::string& k){auto v=g(k);return v.empty()?0.0:std::stod(v);};
    auto gi=[&](const std::string& k){auto v=g(k);return v.empty()?0:std::stoi(v);};
    costImpact=gd("cost_impact"); scheduleImpactDays=gi("schedule_impact_days");
    scopeImpact=g("scope_impact"); qualityImpact=g("quality_impact");
    justification=g("justification"); decisionRationale=g("decision_rationale");
    links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool ChangeRequest::load(const std::string& id){
    auto* db=DatabasePool::instance().get("tracking"); if(!db) return false;
    auto rows=db->query("SELECT * FROM change_requests WHERE cr_id=?;",{BindParam::text(id)});
    if(rows.empty()) return false; fromRow(rows[0]); return true;
}
bool ChangeRequest::remove(){auto* db=DatabasePool::instance().get("tracking");return db&&db->exec("DELETE FROM change_requests WHERE cr_id=?;",{BindParam::text(crId)});}
bool ChangeRequest::update(){return save();}
std::shared_ptr<ChangeRequest> ChangeRequest::loadById(const std::string& id){auto c=std::make_shared<ChangeRequest>();if(!c->load(id))return nullptr;return c;}
std::vector<std::shared_ptr<ChangeRequest>> ChangeRequest::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("tracking");
    std::vector<std::shared_ptr<ChangeRequest>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM change_requests WHERE project_id=? ORDER BY raised_date DESC;",{BindParam::text(pid)});
    for(auto& r:rows){auto c=std::make_shared<ChangeRequest>();c->fromRow(r);res.push_back(c);}return res;
}
std::vector<std::shared_ptr<ChangeRequest>> ChangeRequest::loadOpen(){
    auto* db=DatabasePool::instance().get("tracking");
    std::vector<std::shared_ptr<ChangeRequest>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM change_requests WHERE status IN ('draft','submitted','under-review') ORDER BY raised_date DESC;");
    for(auto& r:rows){auto c=std::make_shared<ChangeRequest>();c->fromRow(r);res.push_back(c);}return res;
}
bool ChangeRequest::approve(const std::string& rat){status="approved";decisionDate=nowIso();decisionRationale=rat;return update();}
bool ChangeRequest::reject(const std::string& rat){status="rejected";decisionDate=nowIso();decisionRationale=rat;return update();}
nlohmann::json ChangeRequest::toJson() const{return{{"crId",crId},{"title",title},{"status",status},{"changeType",changeType},{"costImpact",costImpact}};}

// ════════════════════════════════════════════════════════════
// ASSUMPTION / CONSTRAINT
// ════════════════════════════════════════════════════════════
std::shared_ptr<AssumptionConstraint> AssumptionConstraint::create(const std::string& pid, const std::string& t, const std::string& tp){
    auto a=std::make_shared<AssumptionConstraint>();
    a->acId=genId("ABE"); a->projectId=pid; a->title=t; a->acType=tp;
    a->status="active"; a->identifiedDate=nowIso(); a->createdAt=nowIso(); a->notes="{}";
    return a;
}
bool AssumptionConstraint::save() const {
    auto* db=DatabasePool::instance().get("tracking"); if(!db) return false;
    return db->exec(R"(INSERT OR REPLACE INTO assumption_constraints
        (ac_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,owner_id,title,description,ac_type,category,dimension,
         status,identified_date,review_date,validation_method,validated_date,validated_by,
         breached,breached_date,impact_if_wrong,mitigation,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(acId),ton(workflowInstanceId),ton(workflowStatus),ton(workflowCurrentState),
        ton(projectId),ton(taskId),ton(ownerId),BindParam::text(title),ton(description),
        BindParam::text(acType.empty()?"assumption":acType),ton(category),ton(dimension),
        BindParam::text(status.empty()?"active":status),ton(identifiedDate),ton(reviewDate),
        ton(validationMethod),ton(validatedDate),ton(validatedBy),
        BindParam::int64(breached?1:0),ton(breachedDate),ton(impactIfWrong),ton(mitigation),
        ton(links),BindParam::text(notes.empty()?"{}":notes)});
}
void AssumptionConstraint::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    acId=g("ac_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id"); ownerId=g("owner_id");
    title=g("title"); description=g("description"); acType=g("ac_type");
    category=g("category"); dimension=g("dimension"); status=g("status");
    identifiedDate=g("identified_date"); reviewDate=g("review_date");
    validationMethod=g("validation_method"); validatedDate=g("validated_date");
    validatedBy=g("validated_by"); breached=g("breached")=="1";
    breachedDate=g("breached_date"); impactIfWrong=g("impact_if_wrong");
    mitigation=g("mitigation"); links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool AssumptionConstraint::load(const std::string& id){
    auto* db=DatabasePool::instance().get("tracking"); if(!db) return false;
    auto rows=db->query("SELECT * FROM assumption_constraints WHERE ac_id=?;",{BindParam::text(id)});
    if(rows.empty()) return false; fromRow(rows[0]); return true;
}
bool AssumptionConstraint::remove(){auto* db=DatabasePool::instance().get("tracking");return db&&db->exec("DELETE FROM assumption_constraints WHERE ac_id=?;",{BindParam::text(acId)});}
bool AssumptionConstraint::update(){return save();}
std::shared_ptr<AssumptionConstraint> AssumptionConstraint::loadById(const std::string& id){auto a=std::make_shared<AssumptionConstraint>();if(!a->load(id))return nullptr;return a;}
std::vector<std::shared_ptr<AssumptionConstraint>> AssumptionConstraint::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("tracking");
    std::vector<std::shared_ptr<AssumptionConstraint>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM assumption_constraints WHERE project_id=? ORDER BY ac_type,title;",{BindParam::text(pid)});
    for(auto& r:rows){auto a=std::make_shared<AssumptionConstraint>();a->fromRow(r);res.push_back(a);}return res;
}
bool AssumptionConstraint::markBreached(const std::string& d){breached=true;breachedDate=d.empty()?nowIso():d;status="breached";return update();}
nlohmann::json AssumptionConstraint::toJson() const{return{{"acId",acId},{"title",title},{"acType",acType},{"status",status}};}

// ════════════════════════════════════════════════════════════
// MEETING
// ════════════════════════════════════════════════════════════
std::shared_ptr<Meeting> Meeting::create(const std::string& tid, const std::string& t, const std::string& sd){
    auto m=std::make_shared<Meeting>();
    m->meetingId=genId("BSP"); m->taskId=tid; m->title=t; m->scheduledDate=sd;
    m->status="scheduled"; m->meetingType="general"; m->createdAt=nowIso(); m->notes="{}";
    LOG_INFO("Creating Meeting: "+t); return m;
}
bool Meeting::save() const {
    auto* db=DatabasePool::instance().get("projects"); if(!db) return false;
    bool ok=db->exec(R"(INSERT OR REPLACE INTO meetings
        (meeting_id,workflow_instance_id,workflow_status,workflow_current_state,
         task_id,project_id,organiser_id,title,meeting_type,status,
         scheduled_date,actual_date,duration_mins,location,channel,
         agenda,decisions,actions,next_meeting_id,links,notes,created_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?))",{
        BindParam::text(meetingId),ton(workflowInstanceId),ton(workflowStatus),ton(workflowCurrentState),
        BindParam::text(taskId),ton(projectId),ton(organiserId),
        BindParam::text(title),ton(meetingType),BindParam::text(status.empty()?"scheduled":status),
        ton(scheduledDate),ton(actualDate),BindParam::int64(durationMins),
        ton(location),ton(channel),ton(agenda),ton(decisions),ton(actions),
        ton(nextMeetingId),ton(links),BindParam::text(notes.empty()?"{}":notes),
        BindParam::text(createdAt)});
    if(ok)LOG_INFO("Meeting saved: "+meetingId); else LOG_ERROR("Meeting save failed: "+meetingId);
    return ok;
}
void Meeting::fromRow(const Row& r){
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    meetingId=g("meeting_id"); workflowInstanceId=g("workflow_instance_id");
    taskId=g("task_id"); projectId=g("project_id"); organiserId=g("organiser_id");
    title=g("title"); meetingType=g("meeting_type"); status=g("status");
    scheduledDate=g("scheduled_date"); actualDate=g("actual_date");
    auto gi=[&](const std::string& k){auto v=g(k);return v.empty()?0:std::stoi(v);};
    durationMins=gi("duration_mins"); location=g("location"); channel=g("channel");
    agenda=g("agenda"); decisions=g("decisions"); actions=g("actions");
    nextMeetingId=g("next_meeting_id"); links=g("links"); notes=g("notes"); createdAt=g("created_at");
}
bool Meeting::load(const std::string& id){
    auto* db=DatabasePool::instance().get("projects"); if(!db) return false;
    auto rows=db->query("SELECT * FROM meetings WHERE meeting_id=?;",{BindParam::text(id)});
    if(rows.empty()) return false; fromRow(rows[0]); return true;
}
bool Meeting::remove(){auto* db=DatabasePool::instance().get("projects");return db&&db->exec("DELETE FROM meetings WHERE meeting_id=?;",{BindParam::text(meetingId)});}
bool Meeting::update(){return save();}
std::shared_ptr<Meeting> Meeting::loadById(const std::string& id){auto m=std::make_shared<Meeting>();if(!m->load(id))return nullptr;return m;}
std::vector<std::shared_ptr<Meeting>> Meeting::loadForTask(const std::string& tid){
    auto* db=DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<Meeting>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM meetings WHERE task_id=? ORDER BY scheduled_date;",{BindParam::text(tid)});
    for(auto& r:rows){auto m=std::make_shared<Meeting>();m->fromRow(r);res.push_back(m);}return res;
}
std::vector<std::shared_ptr<Meeting>> Meeting::loadForProject(const std::string& pid){
    auto* db=DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<Meeting>> res; if(!db) return res;
    auto rows=db->query("SELECT * FROM meetings WHERE project_id=? ORDER BY scheduled_date DESC;",{BindParam::text(pid)});
    for(auto& r:rows){auto m=std::make_shared<Meeting>();m->fromRow(r);res.push_back(m);}return res;
}
bool Meeting::complete(const std::string& dec, const std::string& act){
    status="completed"; actualDate=nowIso();
    if(!dec.empty()) decisions=dec;
    if(!act.empty()) actions=act;
    return update();
}
nlohmann::json Meeting::toJson() const{return{{"meetingId",meetingId},{"title",title},{"status",status},{"scheduledDate",scheduledDate}};}

} // namespace Rosenholz

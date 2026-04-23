// Risk.cpp
#include "Risk.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#include "../core/Repository.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>

namespace Rosenholz {



std::shared_ptr<Risk> Risk::create(const std::string& pid, const std::string& t, const std::string& lvl) {
    LOG_INFO("Creating Risk: " + t + " for project " + pid);
    auto r = std::make_shared<Risk>();
    r->riskId = genId("RSK"); r->projectId = pid; r->title = t;
    r->riskLevel = lvl; r->status = "open";
    r->identifiedDate = nowIso(); r->createdAt = nowIso(); r->updatedAt = r->createdAt;
    r->notes = "{}";
    return r;
}

bool Risk::save() const {
    auto* db = DatabasePool::instance().get("reporting");
    if (!db) { LOG_ERROR("Risk::save — reporting DB unavailable"); return false; }
    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO risks
        (risk_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,owner_id,identified_by,title,description,
         category,subcategory,risk_type,status,identified_date,review_date,closed_date,
         probability_score,impact_score_time,impact_score_cost,
         impact_score_quality,impact_score_scope,overall_risk_score,risk_level,
         response_strategy,contingency_plan,cost_reserve,schedule_reserve_days,
         trigger_condition,early_warning,residual_risk_level,
         escalated,escalated_to,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(riskId), textOrNull(workflowInstanceId),
        textOrNull(workflowStatus), textOrNull(workflowCurrentState),
        BindParam::text(projectId), textOrNull(taskId), textOrNull(ownerId),
        textOrNull(identifiedBy), BindParam::text(title), textOrNull(description),
        textOrNull(category), textOrNull(subcategory), textOrNull(riskType),
        BindParam::text(status), textOrNull(identifiedDate),
        textOrNull(reviewDate), textOrNull(closedDate),
        BindParam::int64(probabilityScore), BindParam::int64(impactScoreTime),
        BindParam::int64(impactScoreCost), BindParam::int64(impactScoreQuality),
        BindParam::int64(impactScoreScope), BindParam::int64(overallRiskScore),
        textOrNull(riskLevel), textOrNull(responseStrategy),
        textOrNull(contingencyPlan), BindParam::real(costReserve),
        BindParam::int64(scheduleReserveDays), textOrNull(triggerCondition),
        textOrNull(earlyWarning), textOrNull(residualRiskLevel),
        BindParam::int64(escalated?1:0), textOrNull(escalatedTo),
        textOrNull(links), BindParam::text(notes)
    });
    if (ok) LOG_INFO("Risk saved: " + riskId); else LOG_ERROR("Risk save failed: " + riskId);
    return ok;
}

void Risk::fromRow(const Row& r) {
    auto g=[&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
    auto gi=[&](const std::string& k){ auto v=g(k); return v.empty()?0:std::stoi(v); };
    riskId=g("risk_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id"); ownerId=g("owner_id");
    identifiedBy=g("identified_by"); title=g("title"); description=g("description");
    category=g("category"); subcategory=g("subcategory"); riskType=g("risk_type");
    status=g("status"); identifiedDate=g("identified_date"); reviewDate=g("review_date");
    closedDate=g("closed_date"); riskLevel=g("risk_level");
    probabilityScore=gi("probability_score"); impactScoreTime=gi("impact_score_time");
    impactScoreCost=gi("impact_score_cost"); impactScoreQuality=gi("impact_score_quality");
    impactScoreScope=gi("impact_score_scope"); overallRiskScore=gi("overall_risk_score");
    responseStrategy=g("response_strategy"); contingencyPlan=g("contingency_plan");
    auto gd=[&](const std::string& k){ auto v=g(k); return v.empty()?0.0:std::stod(v); };
    costReserve=gd("cost_reserve"); scheduleReserveDays=gi("schedule_reserve_days");
    triggerCondition=g("trigger_condition"); earlyWarning=g("early_warning");
    residualRiskLevel=g("residual_risk_level"); escalated=g("escalated")=="1";
    escalatedTo=g("escalated_to"); links=g("links"); notes=g("notes");
    createdAt=g("created_at"); updatedAt=g("updated_at");
}
bool Risk::load(const std::string& id) {
    auto* db=DatabasePool::instance().get("reporting");
    if (!db) return false;
    auto rows=db->query("SELECT * FROM risks WHERE risk_id=?;",{BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("Risk not found: "+id); return false; }
    fromRow(rows[0]); return true;
}
bool Risk::remove() {
    auto* db=DatabasePool::instance().get("reporting");
    return db?db->exec("DELETE FROM risks WHERE risk_id=?;",{BindParam::text(riskId)}):false;
}
bool Risk::update() { updatedAt=nowIso(); return save(); }
std::shared_ptr<Risk> Risk::loadById(const std::string& id) {
    auto r=std::make_shared<Risk>(); if(!r->load(id)) return nullptr; return r;
}
std::vector<std::shared_ptr<Risk>> Risk::loadForProject(const std::string& pid) {
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<Risk>> result;
    if (!db) return result;
    auto rows=db->query("SELECT * FROM risks WHERE project_id=? ORDER BY overall_risk_score DESC;",
                        {BindParam::text(pid)});
    for (auto& r:rows) { auto ri=std::make_shared<Risk>(); ri->fromRow(r); result.push_back(ri); }
    return result;
}
std::vector<std::shared_ptr<Risk>> Risk::loadHighRisks() {
    auto* db=DatabasePool::instance().get("reporting");
    std::vector<std::shared_ptr<Risk>> result;
    if (!db) return result;
    auto rows=db->query("SELECT * FROM risks WHERE risk_level IN ('critical','high') AND status='open' ORDER BY overall_risk_score DESC;");
    for (auto& r:rows) { auto ri=std::make_shared<Risk>(); ri->fromRow(r); result.push_back(ri); }
    LOG_INFO("Loaded "+std::to_string(result.size())+" high/critical open risks");
    return result;
}
void Risk::recalcScore() {
    overallRiskScore = probabilityScore *
        std::max({impactScoreTime, impactScoreCost, impactScoreQuality, impactScoreScope});
    LOG_DEBUG("Risk score recalculated: " + riskId + " = " + std::to_string(overallRiskScore));
}
bool Risk::reassignOwner(const std::string& id) { ownerId=id; return update(); }
bool Risk::reassignToProject(const std::string& id) { projectId=id; return update(); }
std::shared_ptr<TrackableItem> Risk::addTrackable(const std::string& t, const std::string& by) {
    auto ti=TrackableItem::create("risk",riskId,t,by); ti->save(); trackables.push_back(ti); return ti;
}
void Risk::loadTrackables() { trackables=TrackableItem::loadForEntity("risk",riskId); }
nlohmann::json Risk::toJson() const {
    return {{"riskId",riskId},{"title",title},{"riskLevel",riskLevel},{"status",status},
            {"overallRiskScore",overallRiskScore},{"projectId",projectId}};
}
} // namespace Rosenholz

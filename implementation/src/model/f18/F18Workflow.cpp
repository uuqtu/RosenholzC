// ============================================================
// F18Workflow.cpp  —  Implementation of the unified F18Workflow entity
// ============================================================
#include "F18Workflow.h"
#include "../../workflow/WorkflowEngine.h"
#include "F18WorkflowStep.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../../core/RegNumber.h"
#include "../../core/Repository.h"
#include "../Utils.h"
#include <sstream>
#include <algorithm>

namespace Rosenholz {

// ------------------------------
// db() — returns the f18 database handle
// ------------------------------
Database* F18Workflow::db() {
    return DatabasePool::instance().get("f18");
}

// ── fromRow ──────────────────────────────────────────────────
void F18Workflow::fromRow(const Row& r) {
    auto g  = [&](const char* k) { return rowGet(r,k); };
    auto gi = [&](const char* k) { return rowGetInt(r,k); };
    auto gd = [&](const char* k) -> double {
        auto s = rowGet(r,k); return s.empty() ? 0.0 : std::stod(s);
    };

    vorgangId          = g("vorgang_id");
    vorgangType        = g("vorgang_type");
    projectId          = g("project_id");
    taskId             = g("task_id");
    parentVorgangId    = g("parent_vorgang_id");
    mainWorkflowId     = g("main_workflow_id");
    title              = g("title");
    description        = g("description");
    status             = g("status");
    ownerId            = g("owner_id");
    priority           = g("priority");
    notes              = g("notes");
    links              = g("links");
    createdAt          = g("created_at");
    updatedAt          = g("updated_at");

    // incident
    incidentType       = g("incident_type");
    severity           = g("severity");
    occurredDate       = g("occurred_date");
    resolvedDate       = g("resolved_date");
    rootCause          = g("root_cause");
    immediateAction    = g("immediate_action");
    resolution         = g("resolution");
    costImpact         = gd("cost_impact");
    scheduleImpactDays = gi("schedule_impact_days");
    scopeImpact        = g("scope_impact");
    qualityImpact      = g("quality_impact");

    // risk
    riskLevel          = g("risk_level");
    probabilityScore   = gi("probability_score");
    impactScoreTime    = gi("impact_score_time");
    impactScoreCost    = gi("impact_score_cost");
    impactScoreQuality = gi("impact_score_quality");
    impactScoreScope   = gi("impact_score_scope");
    overallRiskScore   = gi("overall_risk_score");
    responseStrategy   = g("response_strategy");
    contingencyPlan    = g("contingency_plan");
    triggerCondition   = g("trigger_condition");
    residualRiskLevel  = g("residual_risk_level");
    costReserve        = gd("cost_reserve");
    scheduleReserveDays= gi("schedule_reserve_days");

    // measure
    measureCategory    = g("measure_category");
    plannedDate        = g("planned_date");
    actualDate         = g("actual_date");
    effectiveness      = g("effectiveness");
    verificationMethod = g("verification_method");
    verifiedDate       = g("verified_date");
    verifiedBy         = g("verified_by");

    // quality gate
    phase              = g("phase");
    criteria           = g("criteria");
    acceptanceCriteria = g("acceptance_criteria");
    findings           = g("findings");
    gateResult         = g("gate_result");
    gateDecision       = g("gate_decision");

    // assumption/constraint
    acType             = g("ac_type");
    validatedDate      = g("validated_date");
    validatedBy        = g("validated_by");
    impact             = g("impact");

    // communication plan
    audience           = g("audience");
    frequency          = g("frequency");
    channel            = g("channel");
    responsible        = g("responsible");

    // lessons learned
    lessonType         = g("lesson_type");
    recommendation     = g("recommendation");
    applicablePhases   = g("applicable_phases");

    // decision log
    decisionType       = g("decision_type");
    rationale          = g("rationale");
    decisionDate       = g("decision_date");
    decisionBy         = g("decision_by");
    alternativesConsidered = g("alternatives_considered");

    // change request
    changeType         = g("change_type");
    justification      = g("justification");
    crImpact           = g("cr_impact");
    raisedDate         = g("raised_date");
    crDecisionDate     = g("cr_decision_date");
    crDecisionRationale= g("cr_decision_rationale");
    crScheduleImpactDays= gi("cr_schedule_impact_days");

    // change object
    executedBy         = g("executed_by");
    executionDate      = g("execution_date");
}

// ── save ─────────────────────────────────────────────────────
bool F18Workflow::save() const {
    auto* d = db(); if (!d) return false;
    auto t = [](const std::string& s) { return BindParam::text(s); };
    auto n = [](const std::string& s) { return s.empty() ? BindParam::null() : BindParam::text(s); };
    auto i = [](int v)     { return BindParam::int64(v); };
    auto r = [](double v)  { return BindParam::real(v); };

    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f18_workflows
        (vorgang_id, vorgang_type, project_id, task_id, parent_vorgang_id, main_workflow_id,
         title, description, status, owner_id, priority,
         incident_type, severity, occurred_date, resolved_date,
         root_cause, immediate_action, resolution,
         cost_impact, schedule_impact_days, scope_impact, quality_impact,
         risk_level, probability_score,
         impact_score_time, impact_score_cost, impact_score_quality, impact_score_scope,
         overall_risk_score, response_strategy, contingency_plan,
         trigger_condition, residual_risk_level, cost_reserve, schedule_reserve_days,
         measure_category, planned_date, actual_date, effectiveness,
         verification_method, verified_date, verified_by,
         phase, criteria, acceptance_criteria, findings, gate_result, gate_decision,
         ac_type, validated_date, validated_by, impact,
         audience, frequency, channel, responsible,
         lesson_type, recommendation, applicable_phases,
         decision_type, rationale, decision_date, decision_by, alternatives_considered,
         change_type, justification, cr_impact, raised_date,
         cr_decision_date, cr_decision_rationale, cr_schedule_impact_days,
         executed_by, execution_date,
         notes, links, created_at, updated_at)
        VALUES(?,?,?,?,?,?, ?,?,?,?,?,
               ?,?,?,?, ?,?,?, ?,?,?,?,
               ?,?, ?,?,?,?, ?,?,?, ?,?,?,?,
               ?,?,?,?, ?,?,?,
               ?,?,?,?,?,?, ?,?,?,?,
               ?,?,?,?,
               ?,?,?,
               ?,?,?,?,?,
               ?,?,?,?,
               ?,?,?,
               ?,?,
               ?,?,?,?)
    )SQL", {
        t(vorgangId), t(vorgangType), n(projectId), n(taskId), n(parentVorgangId), n(mainWorkflowId),
        t(title), n(description), t(status), n(ownerId), t(priority),
        // incident
        n(incidentType), n(severity), n(occurredDate), n(resolvedDate),
        n(rootCause), n(immediateAction), n(resolution),
        r(costImpact), i(scheduleImpactDays), n(scopeImpact), n(qualityImpact),
        // risk
        t(riskLevel.empty()?"medium":riskLevel), i(probabilityScore),
        i(impactScoreTime), i(impactScoreCost), i(impactScoreQuality), i(impactScoreScope),
        i(overallRiskScore), n(responseStrategy), n(contingencyPlan),
        n(triggerCondition), n(residualRiskLevel), r(costReserve), i(scheduleReserveDays),
        // measure
        n(measureCategory), n(plannedDate), n(actualDate), n(effectiveness),
        n(verificationMethod), n(verifiedDate), n(verifiedBy),
        // quality gate
        n(phase), n(criteria), n(acceptanceCriteria), n(findings), n(gateResult), n(gateDecision),
        // ac
        n(acType), n(validatedDate), n(validatedBy), n(impact),
        // comm plan
        n(audience), n(frequency), n(channel), n(responsible),
        // ll
        n(lessonType), n(recommendation), n(applicablePhases),
        // dl
        n(decisionType), n(rationale), n(decisionDate), n(decisionBy), n(alternativesConsidered),
        // cr
        n(changeType), n(justification), n(crImpact), n(raisedDate),
        n(crDecisionDate), n(crDecisionRationale), i(crScheduleImpactDays),
        // co
        n(executedBy), n(executionDate),
        // tail
        t(notes.empty()?"{}":notes), n(links), t(createdAt), t(updatedAt)
    });
}

bool F18Workflow::update() {
    updatedAt = nowIso();
    return save();
}

bool F18Workflow::remove() const {
    auto* d = db(); if (!d) return false;
    // Steps are cascade-deleted via FK or explicit delete
    d->exec("DELETE FROM f18_workflow_steps WHERE vorgang_id=?;",
            {BindParam::text(vorgangId)});
    return d->exec("DELETE FROM f18_workflows WHERE vorgang_id=?;",
                   {BindParam::text(vorgangId)});
}

bool F18Workflow::load(const std::string& id) {
    auto* d = db(); if (!d) return false;
    auto rows = d->query("SELECT * FROM f18_workflows WHERE vorgang_id=?;",
                         {BindParam::text(id)});
    if (rows.empty()) return false;
    fromRow(rows[0]);
    return true;
}

// ── Factory ───────────────────────────────────────────────────
std::shared_ptr<F18Workflow> F18Workflow::create(
    const std::string& projectId,
    const std::string& title,
    const std::string& type,
    const std::string& taskId)
{
    auto v = std::make_shared<F18Workflow>();
    v->vorgangId   = genId("F18");
    v->vorgangType = type.empty() ? F18VorgangType::GENERIC : type;
    v->projectId   = projectId;
    v->taskId      = taskId;
    v->title       = title;
    v->status      = "draft";
    v->priority    = "medium";
    v->createdAt   = nowIso();
    v->updatedAt   = nowIso();
    v->notes       = "{}";

    // Type-specific defaults
    if (type == F18VorgangType::RISK)
        v->riskLevel = "medium";

    if (!v->save()) {
        LOG_ERROR("[F18Workflow] Failed to save new vorgang: " + title);
        return nullptr;
    }

    // Auto-create Init and End steps
    auto initStep = std::make_shared<F18WorkflowStep>();
    initStep->stepId        = genId("WFS");
    initStep->vorgangId     = v->vorgangId;
    initStep->title         = "Init";
    initStep->description   = "Automatischer Startschritt.";
    initStep->sequenceOrder = 0;
    initStep->isInitialize  = true;
    initStep->autoApprove   = true;
    initStep->status        = "pending";
    initStep->createdAt     = nowIso();
    initStep->updatedAt     = nowIso();
    initStep->save();

    auto endStep = std::make_shared<F18WorkflowStep>();
    endStep->stepId         = genId("WFS");
    endStep->vorgangId      = v->vorgangId;
    endStep->title          = "End";
    endStep->description    = "Abschlussschritt.";
    endStep->sequenceOrder  = 9999;
    endStep->isFinal        = true;
    endStep->autoApprove    = false;
    endStep->predecessorStepIds = initStep->stepId;
    endStep->status         = "pending";
    endStep->createdAt      = nowIso();
    endStep->updatedAt      = nowIso();
    endStep->save();

    v->steps.clear();
    v->steps.push_back(*initStep);
    v->steps.push_back(*endStep);
    LOG_INFO("[F18Workflow] Created: " + v->vorgangId + " type=" + type);
    // Auto-create the Main WFI that controls this F18Workflow's lifecycle
    v->ensureMainWorkflow();
    return v;
}

std::shared_ptr<F18Workflow> F18Workflow::loadById(const std::string& id) {
    auto v = std::make_shared<F18Workflow>();
    if (!v->load(id)) return nullptr;
    return v;
}

std::vector<std::shared_ptr<F18Workflow>> F18Workflow::loadForProject(
    const std::string& projectId, const std::string& type)
{
    auto* d = db();
    std::vector<std::shared_ptr<F18Workflow>> result;
    if (!d) return result;
    std::string sql = "SELECT * FROM f18_workflows WHERE project_id=?";
    std::vector<BindParam> params = {BindParam::text(projectId)};
    if (!type.empty()) { sql += " AND vorgang_type=?"; params.push_back(BindParam::text(type)); }
    sql += " ORDER BY created_at DESC;";
    for (auto& r : d->query(sql, params)) {
        auto v = std::make_shared<F18Workflow>(); v->fromRow(r); result.push_back(v);
    }
    return result;
}

std::vector<std::shared_ptr<F18Workflow>> F18Workflow::loadForTask(
    const std::string& taskId, const std::string& type)
{
    auto* d = db();
    std::vector<std::shared_ptr<F18Workflow>> result;
    if (!d) return result;
    std::string sql = "SELECT * FROM f18_workflows WHERE task_id=?";
    std::vector<BindParam> params = {BindParam::text(taskId)};
    if (!type.empty()) { sql += " AND vorgang_type=?"; params.push_back(BindParam::text(type)); }
    sql += " ORDER BY created_at DESC;";
    for (auto& r : d->query(sql, params)) {
        auto v = std::make_shared<F18Workflow>(); v->fromRow(r); result.push_back(v);
    }
    return result;
}

std::vector<std::shared_ptr<F18Workflow>> F18Workflow::loadRecent(int n) {
    auto* d = db();
    std::vector<std::shared_ptr<F18Workflow>> result;
    if (!d) return result;
    for (auto& r : d->query("SELECT * FROM f18_workflows ORDER BY created_at DESC LIMIT ?;",
                             {BindParam::int64(n)})) {
        auto v = std::make_shared<F18Workflow>(); v->fromRow(r); result.push_back(v);
    }
    return result;
}

// ── Step management ───────────────────────────────────────────
std::shared_ptr<F18WorkflowStep> F18Workflow::addStep(
    const std::string& title,
    const std::string& stepType,
    const std::string& assigneeId)
{
    // Find End step index and max sequence order
    int endIdx = -1, maxSeq = 0;
    for (int i = 0; i < (int)steps.size(); ++i) {
        if (steps[i].isFinal) { endIdx = i; continue; }
        if (!steps[i].isInitialize && steps[i].sequenceOrder > maxSeq)
            maxSeq = steps[i].sequenceOrder;
    }

    // Determine predecessor: last non-Init, non-End step (or Init if none)
    std::string predId;
    for (auto& s : steps) {
        if (!s.isInitialize && !s.isFinal) predId = s.stepId;
    }
    if (predId.empty()) {
        for (auto& s : steps) if (s.isInitialize) { predId = s.stepId; break; }
    }

    F18WorkflowStep newStep;
    newStep.stepId             = genId("WFS");
    newStep.vorgangId          = vorgangId;
    newStep.title              = title;
    newStep.stepType           = stepType;
    newStep.sequenceOrder      = maxSeq + 1;
    newStep.predecessorStepIds = predId;
    newStep.assignedTo         = assigneeId;
    newStep.status             = "pending";
    newStep.createdAt          = nowIso();
    newStep.updatedAt          = nowIso();
    newStep.save();
    auto step = std::make_shared<F18WorkflowStep>(newStep);

    // Update End's predecessors (use index — push_back may reallocate)
    if (endIdx >= 0) {
        auto& endRef = steps[endIdx];
        if (endRef.predecessorStepIds.empty())
            endRef.predecessorStepIds = step->stepId;
        else
            endRef.predecessorStepIds += "," + step->stepId;
        endRef.save();
    }

    steps.push_back(newStep);
    LOG_INFO("[F18Workflow] Step added: " + step->stepId + " to " + vorgangId);
    return step;
}

void F18Workflow::loadSteps() {
    steps = F18WorkflowStep::loadForVorgang(vorgangId);
}

// ── Risk score calculation ────────────────────────────────────
void F18Workflow::recalcRiskScore() {
    int maxImpact = std::max({impactScoreTime, impactScoreCost,
                              impactScoreQuality, impactScoreScope});
    overallRiskScore = probabilityScore * maxImpact;
    if (overallRiskScore >= 15)      riskLevel = "critical";
    else if (overallRiskScore >= 9)  riskLevel = "high";
    else if (overallRiskScore >= 4)  riskLevel = "medium";
    else                             riskLevel = "low";
    update();
}

// ── Note management ───────────────────────────────────────────
bool F18Workflow::addNote(const std::string& authorId,
                           const std::string& text,
                           const std::string& noteType) {
    std::string noteId = genId("NOTE");
    std::string entry = "{\"id\":\"" + noteId + "\",\"author\":\"" + authorId +
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


void F18Workflow::ensureMainWorkflow() {
    if (!mainWorkflowId.empty()) return;
    auto inst = Rosenholz::WorkflowEngine::createMainWorkflow("f18", vorgangId, title);
    if (!inst) return;
    mainWorkflowId = inst->instanceId;
    status         = "in_work";
    auto* db = F18Workflow::db();
    if (db) db->exec(
        "UPDATE f18_workflows SET main_workflow_id=?, status='in_work', updated_at=? "
        "WHERE vorgang_id=?;",
        {BindParam::text(mainWorkflowId), BindParam::text(nowIso()),
         BindParam::text(vorgangId)});
    LOG_INFO("[Lifecycle] Main WFI ensured for F18: " + vorgangId +
             " wfi=" + mainWorkflowId);
}

} // namespace Rosenholz

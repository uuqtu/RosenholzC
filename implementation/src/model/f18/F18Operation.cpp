// ============================================================
// F18Operation.cpp  —  Implementation of the unified F18Operation entity
// ============================================================
#include "F18Operation.h"
#include "../dok/Document.h"
#include "../../core/OperationResult.h"
#include "../../workflow/F77Workflow.h"
#include "F18OperationStep.h"
#include "../../core/Database.h"
#include "../../workflow/F77Workflow.h"
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
Database* F18Operation::db() {
    return DatabasePool::instance().get("f18");
}

// ── fromRow ──────────────────────────────────────────────────
void F18Operation::fromRow(const Row& r) {
    auto g  = [&](const char* k) { return rowGet(r,k); };
    auto gi = [&](const char* k) { return rowGetInt(r,k); };
    auto gd = [&](const char* k) -> double {
        auto s = rowGet(r,k); return s.empty() ? 0.0 : std::stod(s);
    };

    vorgangId          = g("vorgang_id");
    vorgangType        = g("vorgang_type");
    taskId             = g("task_id");
    parentVorgangId    = g("parent_vorgang_id");
    releaseWorkflowId     = g("release_workflow_id");
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
OperationResult F18Operation::save() const {
    auto* d = db();
    if (!d) return OperationResult::DB_ERROR;

    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f18_operations
        (vorgang_id, vorgang_type, task_id, parent_vorgang_id, release_workflow_id, title, description, status, owner_id, priority, incident_type, severity, occurred_date, resolved_date, root_cause, immediate_action, resolution, cost_impact, schedule_impact_days, scope_impact, quality_impact, risk_level, probability_score, impact_score_time, impact_score_cost, impact_score_quality, impact_score_scope, overall_risk_score, response_strategy, contingency_plan, trigger_condition, residual_risk_level, cost_reserve, schedule_reserve_days, measure_category, planned_date, actual_date, effectiveness, verification_method, verified_date, verified_by, phase, criteria, acceptance_criteria, findings, gate_result, gate_decision, ac_type, validated_date, validated_by, impact, audience, frequency, channel, responsible, lesson_type, recommendation, applicable_phases, decision_type, rationale, decision_date, decision_by, alternatives_considered, change_type, justification, cr_impact, raised_date, cr_decision_date, cr_decision_rationale, cr_schedule_impact_days, executed_by, execution_date, notes, links, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL", {
        BindParam::text(vorgangId),
        BindParam::text(vorgangType),
        BindParam::nullOrText(taskId),
        BindParam::nullOrText(parentVorgangId),
        BindParam::nullOrText(releaseWorkflowId),
        BindParam::text(title),
        BindParam::nullOrText(description),
        BindParam::text(status),
        BindParam::nullOrText(ownerId),
        BindParam::text(priority),
        BindParam::nullOrText(incidentType),
        BindParam::nullOrText(severity),
        BindParam::nullOrText(occurredDate),
        BindParam::nullOrText(resolvedDate),
        BindParam::nullOrText(rootCause),
        BindParam::nullOrText(immediateAction),
        BindParam::nullOrText(resolution),
        BindParam::real(costImpact),
        BindParam::int64(scheduleImpactDays),
        BindParam::nullOrText(scopeImpact),
        BindParam::nullOrText(qualityImpact),
        BindParam::nullOrText(riskLevel),
        BindParam::int64(probabilityScore),
        BindParam::int64(impactScoreTime),
        BindParam::int64(impactScoreCost),
        BindParam::int64(impactScoreQuality),
        BindParam::int64(impactScoreScope),
        BindParam::int64(overallRiskScore),
        BindParam::nullOrText(responseStrategy),
        BindParam::nullOrText(contingencyPlan),
        BindParam::nullOrText(triggerCondition),
        BindParam::nullOrText(residualRiskLevel),
        BindParam::real(costReserve),
        BindParam::int64(scheduleReserveDays),
        BindParam::nullOrText(measureCategory),
        BindParam::nullOrText(plannedDate),
        BindParam::nullOrText(actualDate),
        BindParam::nullOrText(effectiveness),
        BindParam::nullOrText(verificationMethod),
        BindParam::nullOrText(verifiedDate),
        BindParam::nullOrText(verifiedBy),
        BindParam::nullOrText(phase),
        BindParam::nullOrText(criteria),
        BindParam::nullOrText(acceptanceCriteria),
        BindParam::nullOrText(findings),
        BindParam::nullOrText(gateResult),
        BindParam::nullOrText(gateDecision),
        BindParam::nullOrText(acType),
        BindParam::nullOrText(validatedDate),
        BindParam::nullOrText(validatedBy),
        BindParam::nullOrText(impact),
        BindParam::nullOrText(audience),
        BindParam::nullOrText(frequency),
        BindParam::nullOrText(channel),
        BindParam::nullOrText(responsible),
        BindParam::nullOrText(lessonType),
        BindParam::nullOrText(recommendation),
        BindParam::nullOrText(applicablePhases),
        BindParam::nullOrText(decisionType),
        BindParam::nullOrText(rationale),
        BindParam::nullOrText(decisionDate),
        BindParam::nullOrText(decisionBy),
        BindParam::nullOrText(alternativesConsidered),
        BindParam::nullOrText(changeType),
        BindParam::nullOrText(justification),
        BindParam::nullOrText(crImpact),
        BindParam::nullOrText(raisedDate),
        BindParam::nullOrText(crDecisionDate),
        BindParam::nullOrText(crDecisionRationale),
        BindParam::int64(crScheduleImpactDays),
        BindParam::nullOrText(executedBy),
        BindParam::nullOrText(executionDate),
        BindParam::text(notes.empty() ? "{}" : notes),
        BindParam::nullOrText(links),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F18Operation::update() {
    if (isReleased()) {
        LOG_WARN("[F18] update() verweigert: Vorgang ist released — " + vorgangId);
        return OperationResult::ENTITY_RELEASED;
    }
    updatedAt = nowIso();
    return save();
}

OperationResult F18Operation::remove() const {
    auto* d = db(); if (!d) return OperationResult::DB_ERROR;
    // Steps are cascade-deleted via FK or explicit delete
    d->exec("DELETE FROM f18_operation_steps WHERE vorgang_id=?;",
            {BindParam::text(vorgangId)});
    return d->exec("DELETE FROM f18_operations WHERE vorgang_id=?;",
                   {BindParam::text(vorgangId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

bool F18Operation::load(const std::string& id) {
    auto* d = db(); if (!d) return false;
    auto rows = d->query("SELECT * FROM f18_operations WHERE vorgang_id=?;",
                         {BindParam::text(id)});
    if (rows.empty()) return false;
    fromRow(rows[0]);
    return true;
}

// ── Factory ───────────────────────────────────────────────────
std::shared_ptr<F18Operation> F18Operation::create(
    const std::string& taskId,
    const std::string& title,
    const std::string& type)
{
    auto v = std::make_shared<F18Operation>();
    v->vorgangId   = genId("F18");
    v->vorgangType = type.empty() ? F18OperationType::GENERIC : type;
    v->taskId      = taskId;
    v->title       = title;
    v->status      = "in_work";
    v->priority    = "medium";
    v->createdAt   = nowIso();
    v->updatedAt   = nowIso();
    v->notes       = "{}";

    // Type-specific defaults
    if (type == F18OperationType::RISK)
        v->riskLevel = "medium";

    if (!opOk(v->save())) {
        LOG_ERROR("[F18Operation] Failed to save new vorgang: " + title);
        return nullptr;
    }

    // Auto-create Init and End steps
    auto initStep = std::make_shared<F18OperationStep>();
    initStep->stepId        = genId("WFS");
    initStep->vorgangId     = v->vorgangId;
    initStep->title         = "Init";
    initStep->description   = "Automatischer Startschritt.";
    initStep->sequenceOrder = 0;
    initStep->isInitialize  = true;
    initStep->autoApprove   = true;
    // Init step is always auto-approved immediately
    initStep->status        = "done";   // auto-approved at creation
    initStep->decision      = "approved";
    initStep->decisionDate  = nowIso();
    initStep->completedDate = nowIso();
    initStep->createdAt     = nowIso();
    initStep->updatedAt     = nowIso();
    initStep->save();

    auto endStep = std::make_shared<F18OperationStep>();
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
    LOG_INFO("[F18Operation] Created: " + v->vorgangId + " type=" + type);
    return v;
}

std::shared_ptr<F18Operation> F18Operation::loadById(const std::string& id) {
    auto v = std::make_shared<F18Operation>();
    if (!v->load(id)) return nullptr;
    return v;
}

std::vector<std::shared_ptr<F18Operation>> F18Operation::loadForTask(
    const std::string& taskId, const std::string& type)
{
    auto* d = db();
    std::vector<std::shared_ptr<F18Operation>> result;
    if (!d) return result;
    std::string sql = "SELECT * FROM f18_operations WHERE task_id=?";
    std::vector<BindParam> params = {BindParam::text(taskId)};
    if (!type.empty()) { sql += " AND vorgang_type=?"; params.push_back(BindParam::text(type)); }
    sql += " ORDER BY created_at DESC;";
    for (auto& r : d->query(sql, params)) {
        auto v = std::make_shared<F18Operation>(); v->fromRow(r); result.push_back(v);
    }
    return result;
}

std::vector<std::shared_ptr<F18Operation>> F18Operation::loadRecent(int n) {
    auto* d = db();
    std::vector<std::shared_ptr<F18Operation>> result;
    if (!d) return result;
    for (auto& r : d->query("SELECT * FROM f18_operations ORDER BY created_at DESC LIMIT ?;",
                             {BindParam::int64(n)})) {
        auto v = std::make_shared<F18Operation>(); v->fromRow(r); result.push_back(v);
    }
    return result;
}

// ── Step management ───────────────────────────────────────────
std::shared_ptr<F18OperationStep> F18Operation::addStep(
    const std::string& title,
    const std::string& stepType,
    const std::string& assigneeId,
    bool               isFree)
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

    F18OperationStep newStep;
    newStep.stepId             = genId("WFS");
    newStep.vorgangId          = vorgangId;
    newStep.title              = title;
    newStep.stepType           = stepType;
    newStep.sequenceOrder      = maxSeq + 1;
    newStep.isFree             = isFree;
    newStep.assignedTo         = assigneeId;
    newStep.status             = "pending";
    newStep.createdAt          = nowIso();
    newStep.updatedAt          = nowIso();

    // Free steps have no predecessor dependencies and are not wired into
    // the main chain.  Regular steps are connected to the last non-End
    // step and also become a predecessor of the End step.
    if (!isFree) {
        newStep.predecessorStepIds = predId;
    }
    newStep.save();
    auto step = std::make_shared<F18OperationStep>(newStep);

    // Only wire regular steps into the End step's predecessor list
    if (!isFree && endIdx >= 0) {
        auto& endRef = steps[endIdx];
        if (endRef.predecessorStepIds.empty())
            endRef.predecessorStepIds = step->stepId;
        else
            endRef.predecessorStepIds += "," + step->stepId;
        endRef.save();
    }

    steps.push_back(newStep);
    LOG_INFO("[F18Operation] Step added: " + step->stepId + " to " + vorgangId);
    return step;
}

// ── insertAfter ───────────────────────────────────────────────
// Inserts a new step between predecessorStepId and its current successor.
// The new step takes over as the predecessor of the old successor.
std::shared_ptr<F18OperationStep> F18Operation::insertAfter(
    const std::string& predecessorStepId,
    const std::string& title,
    const std::string& stepType,
    const std::string& assigneeId)
{
    loadSteps();

    // Find the predecessor step
    F18OperationStep* pred = nullptr;
    for (auto& s : steps)
        if (s.stepId == predecessorStepId) { pred = &s; break; }
    if (!pred) {
        LOG_ERROR("[F18Operation] insertAfter: predecessor not found: " + predecessorStepId);
        return nullptr;
    }

    // Find the current successor of predecessor
    // (the step whose predecessorStepIds contains predecessorStepId)
    F18OperationStep* successor = nullptr;
    for (auto& s : steps) {
        if (!s.isInitialize && s.predecessorStepIds.find(predecessorStepId) != std::string::npos) {
            successor = &s;
            break;
        }
    }

    // Determine sequence order between predecessor and successor
    int newSeq = pred->sequenceOrder + 1;
    if (successor && successor->sequenceOrder > newSeq)
        newSeq = (pred->sequenceOrder + successor->sequenceOrder) / 2;
    if (newSeq <= pred->sequenceOrder) newSeq = pred->sequenceOrder + 1;

    // Create the new step
    F18OperationStep newStep;
    newStep.stepId             = genId("WFS");
    newStep.vorgangId          = vorgangId;
    newStep.title              = title;
    newStep.stepType           = stepType;
    newStep.sequenceOrder      = newSeq;
    newStep.predecessorStepIds = predecessorStepId;
    newStep.assignedTo         = assigneeId;
    newStep.status             = "pending";
    newStep.createdAt          = nowIso();
    newStep.updatedAt          = nowIso();
    newStep.save();

    // Update successor to point to the new step instead of the predecessor
    if (successor) {
        // Replace predecessorStepId with newStep.stepId in successor's predecessor list
        std::string& preds = successor->predecessorStepIds;
        size_t pos = preds.find(predecessorStepId);
        if (pos != std::string::npos)
            preds.replace(pos, predecessorStepId.size(), newStep.stepId);
        successor->save();
    }

    auto step = std::make_shared<F18OperationStep>(newStep);
    steps.push_back(newStep);
    LOG_INFO("[F18Operation] Step inserted after " + predecessorStepId + ": " + newStep.stepId);
    return step;
}

void F18Operation::loadSteps() {
    steps = F18OperationStep::loadForVorgang(vorgangId);
}

// ── Risk score calculation ────────────────────────────────────
void F18Operation::recalcRiskScore() {
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
OperationResult F18Operation::addNote(const std::string& authorId,
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


bool F18Operation::isWorkflowComplete() const {
    if (releaseWorkflowId.empty()) return false;
    auto wf = Rosenholz::F77_Workflow::loadById(releaseWorkflowId);
    return wf && wf->status == "completed";
}

void F18Operation::ensureReleaseWorkflow() {
    if (!releaseWorkflowId.empty()) return;
    // startDefault creates the WF and calls storeWorkflowId (one place, in F77Engine).
    auto wf = Rosenholz::F77_Engine::startDefault("f18", vorgangId);
    if (!wf) return;
    releaseWorkflowId = wf->workflowId;
    LOG_INFO("[F77] Workflow ensured: " + releaseWorkflowId + " for f18/" + vorgangId);
}



std::string F18Operation::mfsSchluesselText() const {
    std::ostringstream s;
    s << "  ID      : " << vorgangId << "\n"
      << "  Typ     : " << vorgangType << "\n"
      << "  Status  : " << status << "\n"
      << "  F22     : " << taskId << "\n";
    auto docs = Rosenholz::Document::loadForEntity("f18", vorgangId);
    if (!docs.empty()) {
        s << "  DOK     :";
        for (auto& d : docs) s << " " << d->documentId;
        s << "\n";
    }
    s << "\n";
    return s.str();
}

// ── F18Operation::tick ───────────────────────────────────────────────────
// Auto-approve Init and End steps when conditions are met.
bool F18Operation::tick() {
    loadSteps();
    bool changed = false;

    // Auto-approve Init step:
    for (auto& s : steps) {
        if (s.isInitialize && s.status == "pending") {
            s.status = "done";
            s.completedDate = nowIso();
            s.save();
            changed = true;
            LOG_INFO("[F18] Auto-approved Init step: " + s.stepId);
        }
    }

    // Check if all mid-steps (non-Init, non-End) are done:
    bool allMidDone = true;
    bool hasMid = false;
    for (auto& s : steps) {
        if (s.isInitialize || s.isFinal) continue;
        hasMid = true;
        if (s.status != "done" && s.status != "completed"
            && s.status != "approved" && s.status != "skipped"
            && s.status != "rejected") {
            allMidDone = false;
        }
    }

    // Auto-approve End step when all mid-steps are done:
    for (auto& s : steps) {
        if (s.isFinal && s.status == "pending" && (allMidDone || !hasMid)) {
            s.status = "done";
            s.completedDate = nowIso();
            s.save();
            // Advance operation status to released:
            if (status != "released" && status != "cancelled") {
                status = "released";
                updatedAt = nowIso();
                update();
                LOG_INFO("[F18] Operation auto-completed: " + vorgangId);
            }
            changed = true;
            LOG_INFO("[F18] Auto-approved End step: " + s.stepId);
        }
    }

    return changed;
}


} // namespace Rosenholz

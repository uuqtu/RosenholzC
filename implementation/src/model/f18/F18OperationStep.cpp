// ============================================================
// F18OperationStep.cpp  —  Step inside an F18Operation
// ============================================================
#include "F18OperationStep.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../../core/Repository.h"
#include "../Utils.h"
#include <sstream>

namespace Rosenholz {

Database* F18OperationStep::db() {
    return DatabasePool::instance().get("f18");
}

// ── fromRow ──────────────────────────────────────────────────
void F18OperationStep::fromRow(const Row& r) {
    auto g  = [&](const char* k) { return rowGet(r,k); };
    auto gi = [&](const char* k) { return rowGetInt(r,k); };
    auto gb = [&](const char* k) { return rowGetBool(r,k); };

    stepId            = g("step_id");
    operationId         = g("operation_id");
    tplStepId         = g("tpl_step_id");
    title             = g("title");
    description       = g("description");
    stepType          = g("step_type");
    sequenceOrder     = gi("sequence_order");
    predecessorStepIds= g("predecessor_step_ids");
    isInitialize      = gb("is_initialize");
    isFinal           = gb("is_final");
    isFree            = gb("is_free");
    assignedTo        = g("assigned_to");
    requiredRole      = g("required_role");
    dueDate           = g("due_date");
    startedDate       = g("started_date");
    completedDate     = g("completed_date");
    slaHours          = gi("sla_hours");
    slaBreached       = gb("sla_breached");
    status            = f18StepStatusFrom(g("status"));
    autoApprove       = gb("auto_approve");
    requiresComment   = gb("requires_comment");
    requiresDocument  = gb("requires_document");
    decision          = g("decision");
    decisionBy        = g("decision_by");
    decisionDate      = g("decision_date");
    comment           = g("comment");
    trackingStatus    = rowGetOr(r,"tracking_status","planned");
    focusDate         = g("focus_date");
    priority          = rowGetOr(r,"priority","medium");
    assignedToGroup   = g("assigned_to_group");
    progressNote      = g("progress_note");
    percentComplete   = gi("percent_complete");
    notes             = rowGetOr(r,"notes","{}");
    createdAt         = g("created_at");
    updatedAt         = g("updated_at");
}

// ── save ─────────────────────────────────────────────────────
bool F18OperationStep::save() const {
    auto* d = db(); if (!d) return false;

    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f18_operation_steps
        (step_id, operation_id, tpl_step_id, title, description, step_type,
         sequence_order, predecessor_step_ids,
         is_initialize, is_final, is_free,
         assigned_to, required_role, due_date, started_date, completed_date,
         sla_hours, sla_breached,
         status, auto_approve, requires_comment, requires_document,
         decision, decision_by, decision_date, comment,
         tracking_status, focus_date, in_work_since,
         priority, assigned_to_group, progress_note, percent_complete,
         notes, created_at, updated_at)
        VALUES(?,?,?,?,?,?, ?,?, ?,?,?, ?,?,?,?,?, ?,?,
               ?,?,?,?, ?,?,?,?,
               ?,?,?, ?,?,?,?,
               ?,?,?)
    )SQL", {
        BindParam::text(stepId), BindParam::text(operationId), BindParam::nullOrText(tplStepId), BindParam::text(title), BindParam::nullOrText(description), BindParam::text(stepType),
        BindParam::int64(sequenceOrder), BindParam::nullOrText(predecessorStepIds),
        BindParam::int64(isInitialize?1:0), BindParam::int64(isFinal?1:0), BindParam::int64(isFree?1:0),
        BindParam::nullOrText(assignedTo), BindParam::nullOrText(requiredRole), BindParam::nullOrText(dueDate), BindParam::nullOrText(startedDate), BindParam::nullOrText(completedDate),
        BindParam::int64(slaHours), BindParam::int64(slaBreached?1:0),
        BindParam::text(f18StepStatusToString(status)), BindParam::int64(autoApprove?1:0), BindParam::int64(requiresComment?1:0), BindParam::int64(requiresDocument?1:0),
        BindParam::nullOrText(decision), BindParam::nullOrText(decisionBy), BindParam::nullOrText(decisionDate), BindParam::nullOrText(comment),
        BindParam::text(trackingStatus), BindParam::nullOrText(focusDate), BindParam::nullOrText(inWorkSince),
        BindParam::text(priority.empty()?"medium":priority), BindParam::nullOrText(assignedToGroup), BindParam::nullOrText(progressNote),
        BindParam::int64(percentComplete),
        BindParam::text(notes.empty()?"{}":notes), BindParam::text(createdAt), BindParam::text(updatedAt)
    });
}

bool F18OperationStep::remove() const {
    auto* d = db(); if (!d) return false;
    return d->exec("DELETE FROM f18_operation_steps WHERE step_id=?;",
                   {BindParam::text(stepId)});
}

// ── computeTrackingStatus ────────────────────────────────────
void F18OperationStep::computeTrackingStatus() {
    if (status == F18StepStatus::DONE
    || status == F18StepStatus::SKIPPED) {
        trackingStatus = "archived";
    } else if (!inWorkSince.empty()) {
        trackingStatus = "in_work";
    } else {
        std::string now = nowIso().substr(0, 10);
        if (!dueDate.empty() && dueDate.substr(0,10) < now)
            trackingStatus = "due";
        else if (!focusDate.empty() && focusDate.substr(0,10) < now)
            trackingStatus = "focused";
        else
            trackingStatus = "planned";
    }
    updatedAt = nowIso();
    save();
}

// ── canStart ─────────────────────────────────────────────────
bool F18OperationStep::canStart(const std::vector<F18OperationStep>& allSteps) const {
    // Terminal states can never be restarted
    if (status == F18StepStatus::DONE
    || status == F18StepStatus::SKIPPED) return false;
    // Free steps have no predecessor dependencies — always startable
    if (isFree) return true;
    // Non-free steps in waiting/blocked can still be started once deps clear
    if (predecessorStepIds.empty()) return true;

    std::istringstream ss(predecessorStepIds);
    std::string predId;
    while (std::getline(ss, predId, ',')) {
        // Trim whitespace
        while (!predId.empty() && predId.front() == ' ') predId.erase(predId.begin());
        while (!predId.empty() && predId.back()  == ' ') predId.pop_back();
        if (predId.empty()) continue;
        bool found = false;
        for (auto& a : allSteps) {
            if (a.stepId == predId) { found = a.isComplete(); break; }
        }
        if (!found) return false;
    }
    return true;
}

// ── Loaders ──────────────────────────────────────────────────
std::shared_ptr<F18OperationStep> F18OperationStep::loadById(const std::string& id) {
    auto* d = db();
    if (!d) return nullptr;
    auto rows = d->query("SELECT * FROM f18_operation_steps WHERE step_id=?;",
                         {BindParam::text(id)});
    if (rows.empty()) return nullptr;
    auto s = std::make_shared<F18OperationStep>();
    s->fromRow(rows[0]);
    return s;
}

std::vector<F18OperationStep> F18OperationStep::loadForVorgang(const std::string& operationId) {
    auto* d = db();
    std::vector<F18OperationStep> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM f18_operation_steps WHERE operation_id=? ORDER BY sequence_order,created_at;",
        {BindParam::text(operationId)});
    for (auto& r : rows) {
        F18OperationStep s; s.fromRow(r); result.push_back(s);
    }
    return result;
}

F18StepSymbol F18OperationStep::displaySymbol() const {
    switch (status) {
        case F18StepStatus::DONE:
        case F18StepStatus::APPROVED:    return F18StepSymbol::DONE;
        case F18StepStatus::SKIPPED:
        case F18StepStatus::REJECTED:    return F18StepSymbol::SKIPPED;
        case F18StepStatus::IN_PROGRESS: return F18StepSymbol::IN_PROGRESS;
        case F18StepStatus::PENDING:     return F18StepSymbol::PENDING;
        case F18StepStatus::WAITING: return F18StepSymbol::WAITING;
        case F18StepStatus::BLOCKED: return F18StepSymbol::BLOCKED;
    }
    return F18StepSymbol::PENDING;
}

bool F18OperationStep::complete() {
    if (isComplete()) return true;
    status        = F18StepStatus::DONE;
    completedDate = nowIso();
    updatedAt     = nowIso();
    return save();
}

} // namespace Rosenholz

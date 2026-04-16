// ============================================================
// F18WorkflowStep.cpp  —  Step inside an F18Workflow
// ============================================================
#include "F18WorkflowStep.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../../core/Repository.h"
#include "../Utils.h"
#include <sstream>

namespace Rosenholz {

Database* F18WorkflowStep::db() {
    return DatabasePool::instance().get("f18");
}

// ── fromRow ──────────────────────────────────────────────────
void F18WorkflowStep::fromRow(const Row& r) {
    auto g  = [&](const char* k) { return rowGet(r,k); };
    auto gi = [&](const char* k) { return rowGetInt(r,k); };
    auto gb = [&](const char* k) { return rowGetBool(r,k); };

    stepId            = g("step_id");
    vorgangId         = g("vorgang_id");
    tplStepId         = g("tpl_step_id");
    title             = g("title");
    description       = g("description");
    stepType          = g("step_type");
    sequenceOrder     = gi("sequence_order");
    executionType     = g("execution_type");
    predecessorStepIds= g("predecessor_step_ids");
    isInitialize      = gb("is_initialize");
    isFinal           = gb("is_final");
    assignedTo        = g("assigned_to");
    requiredRole      = g("required_role");
    dueDate           = g("due_date");
    startedDate       = g("started_date");
    completedDate     = g("completed_date");
    slaHours          = gi("sla_hours");
    slaBreached       = gb("sla_breached");
    status            = g("status");
    autoApprove       = gb("auto_approve");
    requiresComment   = gb("requires_comment");
    requiresDocument  = gb("requires_document");
    decision          = g("decision");
    decisionBy        = g("decision_by");
    decisionDate      = g("decision_date");
    comment           = g("comment");
    trackingStatus    = rowGetOr(r,"tracking_status","planned");
    plannedDate       = g("planned_date");
    focusDate         = g("focus_date");
    archivedDate      = g("archived_date");
    priority          = rowGetOr(r,"priority","medium");
    assignedToGroup   = g("assigned_to_group");
    progressNote      = g("progress_note");
    percentComplete   = gi("percent_complete");
    notes             = rowGetOr(r,"notes","{}");
    createdAt         = g("created_at");
    updatedAt         = g("updated_at");
}

// ── save ─────────────────────────────────────────────────────
bool F18WorkflowStep::save() const {
    auto* d = db(); if (!d) return false;
    auto t = [](const std::string& s) { return BindParam::text(s); };
    auto n = [](const std::string& s) { return s.empty() ? BindParam::null() : BindParam::text(s); };
    auto i = [](int v) { return BindParam::int64(v); };

    return d->exec(R"SQL(
        INSERT OR REPLACE INTO f18_workflow_steps
        (step_id, vorgang_id, tpl_step_id, title, description, step_type,
         sequence_order, execution_type, predecessor_step_ids,
         is_initialize, is_final,
         assigned_to, required_role, due_date, started_date, completed_date,
         sla_hours, sla_breached,
         status, auto_approve, requires_comment, requires_document,
         decision, decision_by, decision_date, comment,
         tracking_status, planned_date, focus_date, archived_date,
         priority, assigned_to_group, progress_note, percent_complete,
         notes, created_at, updated_at)
        VALUES(?,?,?,?,?,?, ?,?,?, ?,?, ?,?,?,?,?, ?,?,
               ?,?,?,?, ?,?,?,?,
               ?,?,?,?, ?,?,?,?,
               ?,?,?)
    )SQL", {
        t(stepId), t(vorgangId), n(tplStepId), t(title), n(description), t(stepType),
        i(sequenceOrder), t(executionType), n(predecessorStepIds),
        i(isInitialize?1:0), i(isFinal?1:0),
        n(assignedTo), n(requiredRole), n(dueDate), n(startedDate), n(completedDate),
        i(slaHours), i(slaBreached?1:0),
        t(status), i(autoApprove?1:0), i(requiresComment?1:0), i(requiresDocument?1:0),
        n(decision), n(decisionBy), n(decisionDate), n(comment),
        t(trackingStatus), n(plannedDate), n(focusDate), n(archivedDate),
        t(priority.empty()?"medium":priority), n(assignedToGroup), n(progressNote),
        i(percentComplete),
        t(notes.empty()?"{}":notes), t(createdAt), t(updatedAt)
    });
}

bool F18WorkflowStep::remove() const {
    auto* d = db(); if (!d) return false;
    return d->exec("DELETE FROM f18_workflow_steps WHERE step_id=?;",
                   {BindParam::text(stepId)});
}

// ── canStart ─────────────────────────────────────────────────
bool F18WorkflowStep::canStart(const std::vector<F18WorkflowStep>& allSteps) const {
    if (status != "pending" && status != "in_progress") return false;
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
std::shared_ptr<F18WorkflowStep> F18WorkflowStep::loadById(const std::string& id) {
    auto* d = db();
    if (!d) return nullptr;
    auto rows = d->query("SELECT * FROM f18_workflow_steps WHERE step_id=?;",
                         {BindParam::text(id)});
    if (rows.empty()) return nullptr;
    auto s = std::make_shared<F18WorkflowStep>();
    s->fromRow(rows[0]);
    return s;
}

std::vector<F18WorkflowStep> F18WorkflowStep::loadForVorgang(const std::string& vorgangId) {
    auto* d = db();
    std::vector<F18WorkflowStep> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM f18_workflow_steps WHERE vorgang_id=? ORDER BY sequence_order,created_at;",
        {BindParam::text(vorgangId)});
    for (auto& r : rows) {
        F18WorkflowStep s; s.fromRow(r); result.push_back(s);
    }
    return result;
}

} // namespace Rosenholz

// ============================================================
// F16.cpp  —  Project entity implementation
//
// F16 has NO lifecycle state machine:
//   - It is always editable.
//   - Use archived=true to soft-delete a project.
//   - F77 workflows are NOT attached to F16.
// ============================================================

#include "F16.h"
#include "../../mfs/MFSWriter.h"
#include "../akt/Folder.h"
#include "../f22/F22.h"
#include "../../core/OperationResult.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../Utils.h"
#include "../../core/Repository.h"
#include "../../core/RegNumber.h"
#include "../../core/FileOps.h"
#include "../../core/Config.h"
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace Rosenholz {

// ── Factory ──────────────────────────────────────────────────
std::shared_ptr<F16> F16::create(
    const std::string& title,
    const std::string& projectType,
    const std::string& sizeClass,
    const std::string& /*createdBy*/)
{
    auto project = std::make_shared<F16>();
    project->projectId   = genId("F16");
    project->regNumber   = RegNumber::fromString(project->projectId);
    project->title       = title;
    project->projectType = projectType;
    project->sizeClass   = sizeClass;
    project->currency    = "EUR";
    project->createdAt   = nowIso();
    project->updatedAt   = project->createdAt;
    project->notes       = "{}";
    LOG_INFO("F16 created: " + project->projectId);
    return project;
}

// ── CRUD ─────────────────────────────────────────────────────
OperationResult F16::save() const {
    auto* db = DatabasePool::instance().get("f16");
    if (!db) { LOG_ERROR("F16::save — f16 DB not available"); return OperationResult::DB_ERROR; }
    db->beginTransaction();
    OperationResult result = db->exec(R"(
        INSERT OR REPLACE INTO projects (
            project_id,             archived, reg_number, reg_dept, reg_sequence, reg_year,
            title, codename, project_type, size_class,
            owner_team_id, lead_id, sponsor_id,
            phase, methodology, classification, priority, complexity, strategic_alignment,
            start_date_planned, start_date_actual, end_date_planned, end_date_actual,
            duration_planned_days, duration_actual_days, schedule_variance_days,
            budget_planned, budget_approved, budget_committed, budget_actual, cost_variance,
            cost_performance_index, schedule_performance_index,
            earned_value, planned_value, actual_cost,
            estimate_at_completion, estimate_to_complete, variance_at_completion,
            scope_statement, scope_version, scope_last_changed, scope_change_reason, scope_change_count,
                        currency, external_ref, links, milestones, notes,
            created_at, updated_at
        ) VALUES (
            ?,?,?,?,?,?,
            ?,?,?,?,
            ?,?,?,
            ?,?,?,?,?,?,
            ?,?,?,?,
            ?,?,?,
            ?,?,?,?,?,
            ?,?,
            ?,?,?,
            ?,?,?,
            ?,?,?,?,?,
            ?,?,?,?,?,
            ?,?
        )
    )", {
        BindParam::text(projectId),
        BindParam::int64(archived ? 1 : 0),
        BindParam::text(regNumber.toString()),
        BindParam::text(regNumber.dept),
        BindParam::int64(regNumber.sequence),
        BindParam::int64(regNumber.year),
        BindParam::text(title),
        BindParam::nullOrText(codename),
        BindParam::text(projectType),
        BindParam::text(sizeClass),
        BindParam::nullOrText(ownerTeamId),
        BindParam::nullOrText(leadId),
        BindParam::nullOrText(sponsorId),
        BindParam::nullOrText(phase),
        BindParam::nullOrText(methodology),
        BindParam::nullOrText(classification),
        BindParam::nullOrText(priority),
        BindParam::nullOrText(complexity),
        BindParam::nullOrText(strategicAlignment),
        BindParam::nullOrText(startDatePlanned),
        BindParam::nullOrText(startDateActual),
        BindParam::nullOrText(endDatePlanned),
        BindParam::nullOrText(endDateActual),
        BindParam::int64(durationPlannedDays),
        BindParam::int64(durationActualDays),
        BindParam::int64(scheduleVarianceDays),
        BindParam::real(budgetPlanned),
        BindParam::real(budgetApproved),
        BindParam::real(budgetCommitted),
        BindParam::real(budgetActual),
        BindParam::real(costVariance),
        BindParam::real(costPerformanceIndex),
        BindParam::real(schedulePerformanceIndex),
        BindParam::real(earnedValue),
        BindParam::real(plannedValue),
        BindParam::real(actualCost),
        BindParam::real(estimateAtCompletion),
        BindParam::real(estimateToComplete),
        BindParam::real(varianceAtCompletion),
        BindParam::nullOrText(scopeStatement),
        BindParam::nullOrText(scopeVersion),
        BindParam::nullOrText(scopeLastChanged),
        BindParam::nullOrText(scopeChangeReason),
        BindParam::int64(scopeChangeCount),
        BindParam::text(currency),
        BindParam::nullOrText(externalRef),
        BindParam::nullOrText(links),
        BindParam::nullOrText(milestones),
        BindParam::text(notes),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;

    if (opOk(result)) { db->commitTransaction(); LOG_DEBUG("F16 saved: " + projectId); }
    else { db->rollbackTransaction(); LOG_ERROR("F16 save failed: " + projectId); }
    return result;
}

void F16::fromRow(const Row& row) {
    projectId            = rowGet(row, "project_id");
    archived             = rowGetBool(row, "archived");
    regNumber.dept       = rowGet(row, "reg_dept");
    regNumber.sequence   = rowGetInt(row, "reg_sequence");
    regNumber.year       = rowGetInt(row, "reg_year");
    title                = rowGet(row, "title");
    codename             = rowGet(row, "codename");
    projectType          = rowGet(row, "project_type");
    sizeClass            = rowGet(row, "size_class");
    ownerTeamId          = rowGet(row, "owner_team_id");
    leadId               = rowGet(row, "lead_id");
    sponsorId            = rowGet(row, "sponsor_id");
    phase                = rowGet(row, "phase");
    methodology          = rowGet(row, "methodology");
    classification       = rowGet(row, "classification");
    priority             = rowGet(row, "priority");
    complexity           = rowGet(row, "complexity");
    strategicAlignment   = rowGet(row, "strategic_alignment");
    startDatePlanned     = rowGet(row, "start_date_planned");
    startDateActual      = rowGet(row, "start_date_actual");
    endDatePlanned       = rowGet(row, "end_date_planned");
    endDateActual        = rowGet(row, "end_date_actual");
    durationPlannedDays  = rowGetInt(row, "duration_planned_days");
    durationActualDays   = rowGetInt(row, "duration_actual_days");
    scheduleVarianceDays = rowGetInt(row, "schedule_variance_days");
    budgetPlanned        = rowGetDbl(row, "budget_planned");
    budgetApproved       = rowGetDbl(row, "budget_approved");
    budgetCommitted      = rowGetDbl(row, "budget_committed");
    budgetActual         = rowGetDbl(row, "budget_actual");
    costVariance         = rowGetDbl(row, "cost_variance");
    costPerformanceIndex     = rowGetDbl(row, "cost_performance_index");
    schedulePerformanceIndex = rowGetDbl(row, "schedule_performance_index");
    earnedValue          = rowGetDbl(row, "earned_value");
    plannedValue         = rowGetDbl(row, "planned_value");
    actualCost           = rowGetDbl(row, "actual_cost");
    estimateAtCompletion = rowGetDbl(row, "estimate_at_completion");
    estimateToComplete   = rowGetDbl(row, "estimate_to_complete");
    varianceAtCompletion = rowGetDbl(row, "variance_at_completion");
    scopeStatement       = rowGet(row, "scope_statement");
    scopeVersion         = rowGet(row, "scope_version");
    scopeLastChanged     = rowGet(row, "scope_last_changed");
    scopeChangeReason    = rowGet(row, "scope_change_reason");
    scopeChangeCount     = rowGetInt(row, "scope_change_count");
    currency             = rowGet(row, "currency");
    externalRef          = rowGet(row, "external_ref");
    links                = rowGet(row, "links");
    milestones           = rowGet(row, "milestones");
    notes                = rowGet(row, "notes");
    createdAt            = rowGet(row, "created_at");
    updatedAt            = rowGet(row, "updated_at");
}

bool F16::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("f16");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM projects WHERE project_id=?;", {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("F16 not found: " + id); return false; }
    fromRow(rows[0]);
    return true;
}

OperationResult F16::update() {
    updatedAt = nowIso();
    return save();
}

OperationResult F16::updateScope(const std::string& scopeText) {
    scopeStatement   = scopeText;
    scopeLastChanged = nowIso();
    return update();
}

OperationResult F16::remove() {
    auto* db = DatabasePool::instance().get("f16");
    if (!db) return OperationResult::DB_ERROR;
    LOG_WARN("Removing F16: " + projectId);
    db->exec("DELETE FROM project_quality WHERE project_id=?;", {BindParam::text(projectId)});
    db->exec("DELETE FROM project_cost    WHERE project_id=?;", {BindParam::text(projectId)});
    db->exec("DELETE FROM project_time    WHERE project_id=?;", {BindParam::text(projectId)});
    db->exec("DELETE FROM project_scope   WHERE project_id=?;", {BindParam::text(projectId)});
    return db->exec("DELETE FROM projects WHERE project_id=?;", {BindParam::text(projectId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

// ── Queries ───────────────────────────────────────────────────
std::shared_ptr<F16> F16::loadById(const std::string& id) {
    auto project = std::make_shared<F16>();
    return project->load(id) ? project : nullptr;
}

std::vector<std::shared_ptr<F16>> F16::loadAll() {
    auto* db = DatabasePool::instance().get("f16");
    std::vector<std::shared_ptr<F16>> result;
    if (!db) return result;
    for (auto& row : db->query("SELECT * FROM projects WHERE archived=0 ORDER BY created_at DESC;")) {
        auto project = std::make_shared<F16>();
        project->fromRow(row);
        result.push_back(project);
    }
    return result;
}

std::vector<std::shared_ptr<F16>> F16::loadRecent(int maxCount) {
    auto* db = DatabasePool::instance().get("f16");
    std::vector<std::shared_ptr<F16>> result;
    if (!db) return result;
    for (auto& row : db->query(
            "SELECT * FROM projects ORDER BY created_at DESC LIMIT ?;",
            {BindParam::int64(maxCount)})) {
        auto project = std::make_shared<F16>();
        project->fromRow(row);
        result.push_back(project);
    }
    return result;
}

std::vector<std::shared_ptr<F16>> F16::loadWithDates() {
    auto* db = DatabasePool::instance().get("f16");
    std::vector<std::shared_ptr<F16>> result;
    if (!db) return result;
    for (auto& row : db->query(
            "SELECT * FROM projects "
            "WHERE (start_date_planned != '' OR end_date_planned != '') "
            "  AND archived=0 ORDER BY start_date_planned;", {})) {
        auto project = std::make_shared<F16>();
        project->fromRow(row);
        result.push_back(project);
    }
    return result;
}

std::vector<std::shared_ptr<F16>> F16::loadByStatus(const std::string& /*unused*/) {
    // F16 has no lifecycle status — returns all active (non-archived) projects.
    return loadAll();
}

int F16::count() {
    auto* db = DatabasePool::instance().get("f16");
    return db ? db->rowCount("projects") : 0;
}

OperationResult F16::reassignTeam(const std::string& newTeamId) {
    ownerTeamId = newTeamId; return update(); }
OperationResult F16::reassignSponsor(const std::string& newSponsorId) {
    sponsorId = newSponsorId; return update(); }

// ── Convert to Task ───────────────────────────────────────────
std::string F16::convertToTask(const std::string& parentProjectId) {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return "";
    RegNumber taskRegNumber = RegNumberGenerator::next(RegDept::TASK);
    std::string newTaskId   = genId("F22");
    db->exec(R"(
        INSERT OR REPLACE INTO tasks (
            task_id, reg_number, project_id, title, description,
            status, priority, cost_planned, start_date_planned, due_date_planned,
            wbs_code, notes, created_at, updated_at
        ) VALUES (?,?,?,?,?, ?,?,?,?,?, ?,?,?,?)
    )", {
        BindParam::text(newTaskId),
        BindParam::text(taskRegNumber.toString()),
        BindParam::text(parentProjectId),
        BindParam::text(title + " [aus F16/" + projectId + "]"),
        BindParam::nullOrText(scopeStatement),
        BindParam::text(entityStatusToString(EntityStatus::IN_WORK)),
        BindParam::nullOrText(priority),
        BindParam::real(budgetPlanned),
        BindParam::nullOrText(startDatePlanned),
        BindParam::nullOrText(endDatePlanned),
        BindParam::text(regNumber.toString()),
        BindParam::text(notes),
        BindParam::text(nowIso()),
        BindParam::text(nowIso())
    });
    LOG_INFO("F16 " + projectId + " converted to Task " + newTaskId);
    return newTaskId;
}

// ── Serialisation ─────────────────────────────────────────────
json F16::toJson() const {
    json j;
    j["projectId"]                = projectId;
    j["regNumber"]                = regNumber.toString();
    j["title"]                    = title;
    j["codename"]                 = codename;
    j["projectType"]              = projectType;
    j["sizeClass"]                = sizeClass;
    j["archived"]                 = archived;
    j["phase"]                    = phase;
    j["leadId"]                   = leadId;
    j["ownerTeamId"]              = ownerTeamId;
    j["budgetPlanned"]            = budgetPlanned;
    j["budgetActual"]             = budgetActual;
    j["costPerformanceIndex"]     = costPerformanceIndex;
    j["schedulePerformanceIndex"] = schedulePerformanceIndex;
    return j;
}

std::shared_ptr<F16> F16::fromJson(const json& j) {
    auto project = std::make_shared<F16>();
    project->projectId                = j.value("projectId",   "");
    project->title                    = j.value("title",       "");
    project->codename                 = j.value("codename",    "");
    project->projectType              = j.value("projectType", "OV");
    project->sizeClass                = j.value("sizeClass",   "medium");
    project->archived                 = j.value("archived",    false);
    project->costPerformanceIndex     = j.value("costPerformanceIndex",     1.0);
    project->schedulePerformanceIndex = j.value("schedulePerformanceIndex", 1.0);
    return project;
}

// ── MFS output ────────────────────────────────────────────────
bool F16::writeMFSFile(const std::string& mfsRoot) const {
    return MFSWriter::writeProject(*this, mfsRoot);
}

std::string F16::mfsSchluesselText() const {
    std::ostringstream text;
    text << "  ID       : " << projectId  << "\n"
         << "  Titel    : " << title       << "\n"
         << "  Typ      : " << projectType << "\n"
         << "  Groesse  : " << sizeClass   << "\n"
         << "  Archiv   : " << (archived ? "ja" : "nein") << "\n";
    auto tasks = Rosenholz::F22::loadForProject(projectId);
    if (!tasks.empty()) {
        text << "  F22      :";
        for (auto& task : tasks) text << " " << task->taskId;
        text << "\n";
    }
    return text.str();
}

// ── Earned value recalculation ────────────────────────────────
void F16::recalcEarnedValue() {
    if (actualCost > 0.0 && earnedValue > 0.0)
        costPerformanceIndex = earnedValue / actualCost;
    if (plannedValue > 0.0 && earnedValue > 0.0)
        schedulePerformanceIndex = earnedValue / plannedValue;
    estimateAtCompletion = (costPerformanceIndex != 0.0)
        ? budgetPlanned / costPerformanceIndex
        : budgetPlanned;
    estimateToComplete   = estimateAtCompletion - actualCost;
    varianceAtCompletion = budgetPlanned - estimateAtCompletion;
    costVariance         = earnedValue - actualCost;
    LOG_DEBUG("EV recalculated: " + projectId
        + " CostPI=" + std::to_string(costPerformanceIndex)
        + " SchedulePI=" + std::to_string(schedulePerformanceIndex));
}


OperationResult F16::reassignLead(const std::string& newLeadId) {
    leadId = newLeadId; return update(); }


OperationResult F16::archive() {
    if (archived) return OperationResult::OPERATION_ACK; // idempotent
    archived = true;
    LOG_INFO("[F16] Archived: " + projectId);
    return update();
}
} // namespace Rosenholz

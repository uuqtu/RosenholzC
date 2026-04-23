// ============================================================
// ProjectF16.cpp  —  Project entity implementation
// ============================================================

#include "../mfs/MFSWriter.h"
#include "ProjectF16.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#include "../core/Repository.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include "../core/RegNumber.h"
#include "../core/FileOps.h"
#include "../core/Config.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>

using json = nlohmann::json;

namespace Rosenholz {

// ── Helpers ──────────────────────────────────────────────────


// ── Factory ──────────────────────────────────────────────────
std::shared_ptr<ProjectF16> ProjectF16::create(
    const std::string& title_,
    const std::string& projectType_,
    const std::string& sizeClass_,
    const std::string& /*createdBy*/)
{
    LOG_INFO("Creating ProjectF16: " + title_);

    auto p = std::make_shared<ProjectF16>();
    p->projectId   = genId("F16");
    p->regNumber   = RegNumberGenerator::next(RegDept::PROJECT);
    p->title       = title_;
    p->projectType = projectType_;
    p->sizeClass   = sizeClass_;
    p->status      = "draft";
    p->currency    = "EUR";
    p->createdAt   = nowIso();
    p->updatedAt   = p->createdAt;
    p->notes       = "{}";

    LOG_INFO("ProjectF16 created: " + p->projectId + " reg=" + p->regNumber.toString());
    return p;
}

// ── CRUD ─────────────────────────────────────────────────────
bool ProjectF16::save() const {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) { LOG_ERROR("ProjectF16::save — projects DB not available"); return false; }
    // Wrap in transaction for atomicity
    db->beginTransaction();

    LOG_DEBUG("Saving ProjectF16: " + projectId);

    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO projects (
            project_id, workflow_instance_id, workflow_status, workflow_current_state,
            reg_number, reg_dept, reg_sequence, reg_year,
            title, codename, project_type, size_class,
            owner_team_id, lead_id, sponsor_id, status, phase,
            start_date_planned, start_date_actual, end_date_planned, end_date_actual,
            duration_planned_days, duration_actual_days, schedule_variance_days,
            budget_planned, budget_approved, budget_committed, budget_actual,
            cost_variance, cost_performance_index, schedule_performance_index,
            earned_value, planned_value, actual_cost,
            estimate_at_completion, estimate_to_complete, variance_at_completion,
            scope_statement, scope_version, scope_last_changed,
            scope_change_reason, scope_change_count,
            priority, complexity, strategic_alignment,
            quality_gate_id, communication_plan_id,
            currency, external_ref, methodology, classification, links, notes,
            created_at, updated_at
        ) VALUES (
            ?,?,?,?, ?,?,?,?, ?,?,?,?, ?,?,?,?,?,
            ?,?,?,?, ?,?,?, ?,?,?,?, ?,?,?, ?,?,?,
            ?,?,?, ?,?,?,?,?, ?,?,?,
            ?,?,?,?, ?,?,?,?, ?,?
        )
    )", {
        BindParam::text(projectId),
        textOrNull(workflowInstanceId),
        textOrNull(workflowStatus),
        textOrNull(workflowCurrentState),
        BindParam::text(regNumber.toString()),
        BindParam::text(regNumber.dept),
        BindParam::int64(regNumber.sequence),
        BindParam::int64(regNumber.year),
        BindParam::text(title),
        textOrNull(codename),
        BindParam::text(projectType),
        BindParam::text(sizeClass),
        textOrNull(ownerTeamId),
        textOrNull(leadId),
        textOrNull(sponsorId),
        BindParam::text(status),
        textOrNull(phase),
        textOrNull(startDatePlanned),
        textOrNull(startDateActual),
        textOrNull(endDatePlanned),
        textOrNull(endDateActual),
        BindParam::int64(durationPlannedDays),
        BindParam::int64(durationActualDays),
        BindParam::int64(scheduleVarianceDays),
        BindParam::real(budgetPlanned),
        BindParam::real(budgetApproved),
        BindParam::real(budgetCommitted),
        BindParam::real(budgetActual),
        BindParam::real(costVariance),
        BindParam::real(cpi),
        BindParam::real(spi),
        BindParam::real(earnedValue),
        BindParam::real(plannedValue),
        BindParam::real(actualCost),
        BindParam::real(eac),
        BindParam::real(etc),
        BindParam::real(vac),
        textOrNull(scopeStatement),
        textOrNull(scopeVersion),
        textOrNull(scopeLastChanged),
        textOrNull(scopeChangeReason),
        BindParam::int64(scopeChangeCount),
        textOrNull(priority),
        textOrNull(complexity),
        textOrNull(strategicAlignment),
        textOrNull(qualityGateId),
        textOrNull(communicationPlanId),
        BindParam::text(currency),
        textOrNull(externalRef),
        textOrNull(methodology),
        textOrNull(classification),
        textOrNull(links),
        BindParam::text(notes),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    });

    if (ok) {
        LOG_INFO("ProjectF16 saved: " + projectId);
        db->commitTransaction();
    } else {
        LOG_ERROR("ProjectF16 save failed: " + projectId);
        db->rollbackTransaction();
    }
    return ok;
}

void ProjectF16::fromRow(const Row& r) {
    auto get = [&](const std::string& k) -> std::string {
        auto it = r.find(k);
        return it != r.end() ? it->second : "";
    };
    auto getD = [&](const std::string& k) -> double {
        auto v = get(k); return v.empty() ? 0.0 : std::stod(v);
    };
    auto getI = [&](const std::string& k) -> int {
        auto v = get(k); return v.empty() ? 0 : std::stoi(v);
    };

    projectId            = rowGet(r,"project_id");
    workflowInstanceId   = rowGet(r,"workflow_instance_id");
    workflowStatus       = rowGet(r,"workflow_status");
    workflowCurrentState = rowGet(r,"workflow_current_state");
    regNumber.dept       = rowGet(r,"reg_dept");
    regNumber.sequence   = rowGetInt(r,"reg_sequence");
    regNumber.year       = rowGetInt(r,"reg_year");
    title                = rowGet(r,"title");
    codename             = rowGet(r,"codename");
    projectType          = rowGet(r,"project_type");
    sizeClass            = rowGet(r,"size_class");
    ownerTeamId          = rowGet(r,"owner_team_id");
    leadId               = rowGet(r,"lead_id");
    sponsorId            = rowGet(r,"sponsor_id");
    status               = rowGet(r,"status");
    phase                = rowGet(r,"phase");
    startDatePlanned     = rowGet(r,"start_date_planned");
    startDateActual      = rowGet(r,"start_date_actual");
    endDatePlanned       = rowGet(r,"end_date_planned");
    endDateActual        = rowGet(r,"end_date_actual");
    durationPlannedDays  = rowGetInt(r,"duration_planned_days");
    durationActualDays   = rowGetInt(r,"duration_actual_days");
    scheduleVarianceDays = rowGetInt(r,"schedule_variance_days");
    budgetPlanned        = rowGetDbl(r,"budget_planned");
    budgetApproved       = rowGetDbl(r,"budget_approved");
    budgetCommitted      = rowGetDbl(r,"budget_committed");
    budgetActual         = rowGetDbl(r,"budget_actual");
    costVariance         = rowGetDbl(r,"cost_variance");
    cpi                  = rowGetDbl(r,"cost_performance_index");
    spi                  = rowGetDbl(r,"schedule_performance_index");
    earnedValue          = rowGetDbl(r,"earned_value");
    plannedValue         = rowGetDbl(r,"planned_value");
    actualCost           = rowGetDbl(r,"actual_cost");
    eac                  = rowGetDbl(r,"estimate_at_completion");
    etc                  = rowGetDbl(r,"estimate_to_complete");
    vac                  = rowGetDbl(r,"variance_at_completion");
    scopeStatement       = rowGet(r,"scope_statement");
    scopeVersion         = rowGet(r,"scope_version");
    scopeLastChanged     = rowGet(r,"scope_last_changed");
    scopeChangeReason    = rowGet(r,"scope_change_reason");
    scopeChangeCount     = rowGetInt(r,"scope_change_count");
    priority             = rowGet(r,"priority");
    complexity           = rowGet(r,"complexity");
    strategicAlignment   = rowGet(r,"strategic_alignment");
    qualityGateId        = rowGet(r,"quality_gate_id");
    communicationPlanId  = rowGet(r,"communication_plan_id");
    currency             = rowGet(r,"currency");
    externalRef          = rowGet(r,"external_ref");
    methodology          = rowGet(r,"methodology");
    classification       = rowGet(r,"classification");
    links                = rowGet(r,"links");
    notes                = rowGet(r,"notes");
    createdAt            = rowGet(r,"created_at");
    updatedAt            = rowGet(r,"updated_at");
}

bool ProjectF16::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM projects WHERE project_id=?;", {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("ProjectF16 not found: " + id); return false; }
    fromRow(rows[0]);
    loadQTCSLinks();
    LOG_DEBUG("ProjectF16 loaded: " + projectId);
    return true;
}

bool ProjectF16::remove() {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    LOG_WARN("Removing ProjectF16: " + projectId);
    // Remove QTCS links
    db->exec("DELETE FROM project_quality WHERE project_id=?;", {BindParam::text(projectId)});
    db->exec("DELETE FROM project_cost    WHERE project_id=?;", {BindParam::text(projectId)});
    db->exec("DELETE FROM project_time    WHERE project_id=?;", {BindParam::text(projectId)});
    db->exec("DELETE FROM project_scope   WHERE project_id=?;", {BindParam::text(projectId)});
    return db->exec("DELETE FROM projects WHERE project_id=?;", {BindParam::text(projectId)});
}

bool ProjectF16::update() {
    updatedAt = nowIso();
    return save();
}

// ── Load helpers ─────────────────────────────────────────────
std::shared_ptr<ProjectF16> ProjectF16::loadById(const std::string& id) {
    auto p = std::make_shared<ProjectF16>();
    if (!p->load(id)) return nullptr;
    return p;
}

std::vector<std::shared_ptr<ProjectF16>> ProjectF16::loadAll() {
    auto* db = DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<ProjectF16>> result;
    if (!db) return result;
    auto rows = db->query("SELECT * FROM projects ORDER BY created_at DESC;");
    for (auto& r : rows) {
        auto p = std::make_shared<ProjectF16>();
        p->fromRow(r);
        result.push_back(p);
    }
    LOG_INFO("Loaded " + std::to_string(result.size()) + " projects");
    return result;
}

std::vector<std::shared_ptr<ProjectF16>> ProjectF16::loadByStatus(const std::string& status_) {
    auto* db = DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<ProjectF16>> result;
    if (!db) return result;
    auto rows = db->query("SELECT * FROM projects WHERE status=? ORDER BY created_at DESC;",
                          {BindParam::text(status_)});
    for (auto& r : rows) {
        auto p = std::make_shared<ProjectF16>();
        p->fromRow(r);
        result.push_back(p);
    }
    return result;
}

// ── QTCS multi-assignment ─────────────────────────────────────
bool ProjectF16::addQuality(const std::string& id) {
    qualityIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO project_quality VALUES (?,?);",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}
bool ProjectF16::addCost(const std::string& id) {
    costIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO project_cost VALUES (?,?);",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}
bool ProjectF16::addTime(const std::string& id) {
    timeIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO project_time VALUES (?,?);",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}
bool ProjectF16::addScope(const std::string& id) {
    scopeIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO project_scope VALUES (?,?);",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}
bool ProjectF16::removeQuality(const std::string& id) {
    qualityIds.erase(std::remove(qualityIds.begin(), qualityIds.end(), id), qualityIds.end());
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("DELETE FROM project_quality WHERE project_id=? AND quality_id=?;",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}
bool ProjectF16::removeCost(const std::string& id) {
    costIds.erase(std::remove(costIds.begin(), costIds.end(), id), costIds.end());
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("DELETE FROM project_cost WHERE project_id=? AND cost_id=?;",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}
bool ProjectF16::removeTime(const std::string& id) {
    timeIds.erase(std::remove(timeIds.begin(), timeIds.end(), id), timeIds.end());
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("DELETE FROM project_time WHERE project_id=? AND time_id=?;",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}
bool ProjectF16::removeScope(const std::string& id) {
    scopeIds.erase(std::remove(scopeIds.begin(), scopeIds.end(), id), scopeIds.end());
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("DELETE FROM project_scope WHERE project_id=? AND scope_id=?;",
                          {BindParam::text(projectId), BindParam::text(id)}) : false;
}

void ProjectF16::loadQTCSLinks() {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return;

    auto loadIds = [&](const std::string& table, const std::string& col) {
        std::vector<std::string> ids;
        auto rows = db->query("SELECT " + col + " FROM " + table + " WHERE project_id=?;",
                              {BindParam::text(projectId)});
        for (auto& r : rows) ids.push_back(r.begin()->second);
        return ids;
    };
    qualityIds = loadIds("project_quality", "quality_id");
    costIds    = loadIds("project_cost",    "cost_id");
    timeIds    = loadIds("project_time",    "time_id");
    scopeIds   = loadIds("project_scope",   "scope_id");
}

// ── Trackable management ─────────────────────────────────────
std::shared_ptr<TrackableItem> ProjectF16::addTrackable(
    const std::string& title_, const std::string& createdBy_)
{
    auto t = TrackableItem::create("project", projectId, title_, createdBy_);
    t->save();
    trackables.push_back(t);
    return t;
}

void ProjectF16::loadTrackables() {
    trackables = TrackableItem::loadForEntity("project", projectId);
}

// ── Earned value recalculation ────────────────────────────────
void ProjectF16::recalcEarnedValue() {
    if (actualCost > 0 && earnedValue > 0)
        cpi = earnedValue / actualCost;
    if (plannedValue > 0 && earnedValue > 0)
        spi = earnedValue / plannedValue;
    if (budgetPlanned > 0 && cpi != 0)
        eac = budgetPlanned / cpi;
    etc = eac - actualCost;
    vac = budgetPlanned - eac;
    costVariance = earnedValue - actualCost;
    LOG_DEBUG("EV recalculated for " + projectId + " CPI=" + std::to_string(cpi));
}

// ── Reassign connections ─────────────────────────────────────
bool ProjectF16::reassignLead(const std::string& newLeadId) {
    LOG_INFO("Reassigning lead for " + projectId + " -> " + newLeadId);
    leadId = newLeadId; return update();
}
bool ProjectF16::reassignTeam(const std::string& newTeamId) {
    LOG_INFO("Reassigning team for " + projectId + " -> " + newTeamId);
    ownerTeamId = newTeamId; return update();
}
bool ProjectF16::reassignSponsor(const std::string& newSponsorId) {
    sponsorId = newSponsorId; return update();
}
bool ProjectF16::reassignWorkflowInstance(const std::string& newInstanceId) {
    workflowInstanceId = newInstanceId; return update();
}

// ── Convert to Task ───────────────────────────────────────────
std::string ProjectF16::convertToTask(const std::string& parentProjectId) {
    LOG_INFO("Converting ProjectF16 " + projectId + " -> Task under " + parentProjectId);
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return "";

    std::string taskId = "f22_" + projectId;
    RegNumber   taskReg = RegNumberGenerator::next(RegDept::TASK);

    // Insert a task record mirroring key project fields
    db->exec(R"(
        INSERT OR REPLACE INTO tasks (
            task_id, reg_number, project_id, title, description,
            status, priority, cost_planned, start_date_planned, due_date_planned,
            wbs_code, notes, created_at
        ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(taskId),
        BindParam::text(taskReg.toString()),
        BindParam::text(parentProjectId),
        BindParam::text(title + " [converted from F16/" + projectId + "]"),
        textOrNull(scopeStatement),
        BindParam::text(status),
        textOrNull(priority),
        BindParam::real(budgetPlanned),
        textOrNull(startDatePlanned),
        textOrNull(endDatePlanned),
        BindParam::text(regNumber.toString()),
        BindParam::text(notes),
        BindParam::text(nowIso())
    });
    LOG_INFO("Task created from project: " + taskId);
    return taskId;
}

// ── Serialisation ─────────────────────────────────────────────
json ProjectF16::toJson() const {
    json j;
    j["projectId"]    = projectId;
    j["regNumber"]    = regNumber.toString();
    j["title"]        = title;
    j["codename"]     = codename;
    j["projectType"]  = projectType;
    j["sizeClass"]    = sizeClass;
    j["status"]       = status;
    j["phase"]        = phase;
    j["leadId"]       = leadId;
    j["ownerTeamId"]  = ownerTeamId;
    j["budgetPlanned"]= budgetPlanned;
    j["budgetActual"] = budgetActual;
    j["cpi"]          = cpi;
    j["spi"]          = spi;
    j["qualityIds"]   = qualityIds;
    j["costIds"]      = costIds;
    j["timeIds"]      = timeIds;
    j["scopeIds"]     = scopeIds;
    return j;
}

std::shared_ptr<ProjectF16> ProjectF16::fromJson(const json& j) {
    auto p = std::make_shared<ProjectF16>();
    p->projectId   = j.value("projectId",   "");
    p->title       = j.value("title",       "");
    p->codename    = j.value("codename",    "");
    p->projectType = j.value("projectType", "OV");
    p->sizeClass   = j.value("sizeClass",   "medium");
    p->status      = j.value("status",      "draft");
    return p;
}

// ── MFS plaintext output ─────────────────────────────────────
bool ProjectF16::writeMFSFile(const std::string& mfsRoot) const {
    return MFSWriter::writeProject(*this, mfsRoot);
}

} // namespace Rosenholz

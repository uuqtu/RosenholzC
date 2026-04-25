// ============================================================
// TaskF22.cpp  —  Task entity implementation
// ============================================================

#include "../../mfs/MFSWriter.h"
#include "TaskF22.h"
#include "../../core/OperationResult.h"
#include "../../workflow/F77Workflow.h"
#include "../../core/Database.h"
#include "../../workflow/F77Workflow.h"
#include "../../core/Logger.h"
#include "../Utils.h"
#include "../../core/Repository.h"
#ifndef _WIN32
#endif
#include "../../core/RegNumber.h"
#include "../../core/FileOps.h"

using json = nlohmann::json;

namespace Rosenholz {



// ── Factory ──────────────────────────────────────────────────
std::shared_ptr<TaskF22> TaskF22::create(
    const std::string& projectId_,
    const std::string& title_,
    const std::string& assigneeId_,
    const std::string& parentTaskId_)
{
    LOG_INFO("Creating TaskF22: " + title_ + " in project " + projectId_);
    auto t = std::make_shared<TaskF22>();
    t->taskId        = genId("F22");
    t->regNumber     = RegNumberGenerator::next(RegDept::TASK);
    t->projectId     = projectId_;
    t->title         = title_;
    t->assigneeId    = assigneeId_;
    t->parentTaskId  = parentTaskId_;
    t->status        = "in_work";
    t->createdAt     = nowIso();
    t->updatedAt     = t->createdAt;
    t->notes         = "{}";
    LOG_INFO("TaskF22 created: " + t->taskId + " reg=" + t->regNumber.toString());
    return t;
}

// ── CRUD ─────────────────────────────────────────────────────
// ------------------------------
// save
//
// Behavior:
//   INSERT OR REPLACE upsert into projects.db
//   Does not save child tasks — call save() recursively
//
// Returns:
//   true on success
// ------------------------------
OperationResult TaskF22::save() const {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) { LOG_ERROR("TaskF22::save — projects DB not available"); return OperationResult::DB_ERROR; }

    OperationResult ok = db->exec(R"(
        INSERT OR REPLACE INTO tasks (
            task_id, workflow_instance_id, workflow_status, workflow_current_state, release_workflow_id,
            reg_number, project_id, parent_task_id, assignee_id, assigned_by,
            task_code, title, description, task_type, status, priority,
            effort_planned_hrs, effort_actual_hrs, effort_remaining_hrs,
            cost_planned, cost_actual,
            start_date_planned, start_date_actual, due_date_planned, due_date_actual,
            schedule_variance_days, percent_complete,
            quality_criteria, acceptance_criteria, milestones,
            wbs_code, sprint_or_phase, links, notes, created_at, updated_at
        ) VALUES (?,?,?,?,?, ?,?,?,?,?, ?,?,?,?,?,?, ?,?,?, ?,?, ?,?,?,?, ?,?, ?,?,?, ?,?,?,?,?,?)
    )", {
        BindParam::text(taskId),
        textOrNull(workflowInstanceId),
        textOrNull(workflowStatus),
        textOrNull(workflowCurrentState),
        textOrNull(releaseWorkflowId),
        BindParam::text(regNumber.toString()),
        BindParam::text(projectId),
        textOrNull(parentTaskId),
        textOrNull(assigneeId),
        textOrNull(assignedBy),
        textOrNull(taskCode),
        BindParam::text(title),
        textOrNull(description),
        textOrNull(taskType),
        BindParam::text(status),
        textOrNull(priority),
        BindParam::real(effortPlannedHrs),
        BindParam::real(effortActualHrs),
        BindParam::real(effortRemainingHrs),
        BindParam::real(costPlanned),
        BindParam::real(costActual),
        textOrNull(startDatePlanned),
        textOrNull(startDateActual),
        textOrNull(dueDatePlanned),
        textOrNull(dueDateActual),
        BindParam::int64(scheduleVarianceDays),
        BindParam::int64(percentComplete),
        textOrNull(qualityCriteria),
        textOrNull(acceptanceCriteria),
        textOrNull(milestones),
        textOrNull(wbsCode),
        textOrNull(sprintOrPhase),
        textOrNull(links),
        BindParam::text(notes),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;

    return ok;
}

void TaskF22::fromRow(const Row& r) {
    taskId               = rowGet(r,"task_id");
    workflowInstanceId   = rowGet(r,"workflow_instance_id");
    releaseWorkflowId       = rowGet(r,"release_workflow_id");
    workflowStatus       = rowGet(r,"workflow_status");
    workflowCurrentState = rowGet(r,"workflow_current_state");
    regNumber            = RegNumber::fromString(rowGet(r,"reg_number"));
    projectId            = rowGet(r,"project_id");
    parentTaskId         = rowGet(r,"parent_task_id");
    assigneeId           = rowGet(r,"assignee_id");
    assignedBy           = rowGet(r,"assigned_by");
    taskCode             = rowGet(r,"task_code");
    title                = rowGet(r,"title");
    description          = rowGet(r,"description");
    taskType             = rowGet(r,"task_type");
    status               = rowGet(r,"status");
    priority             = rowGet(r,"priority");
    effortPlannedHrs     = rowGetDbl(r,"effort_planned_hrs");
    effortActualHrs      = rowGetDbl(r,"effort_actual_hrs");
    effortRemainingHrs   = rowGetDbl(r,"effort_remaining_hrs");
    costPlanned          = rowGetDbl(r,"cost_planned");
    costActual           = rowGetDbl(r,"cost_actual");
    startDatePlanned     = rowGet(r,"start_date_planned");
    startDateActual      = rowGet(r,"start_date_actual");
    dueDatePlanned       = rowGet(r,"due_date_planned");
    dueDateActual        = rowGet(r,"due_date_actual");
    scheduleVarianceDays = rowGetInt(r,"schedule_variance_days");
    percentComplete      = rowGetInt(r,"percent_complete");
    qualityCriteria      = rowGet(r,"quality_criteria");
    acceptanceCriteria   = rowGet(r,"acceptance_criteria");
    milestones           = rowGet(r,"milestones");
    wbsCode              = rowGet(r,"wbs_code");
    sprintOrPhase        = rowGet(r,"sprint_or_phase");
    links                = rowGet(r,"links");
    notes                = rowGet(r,"notes");
    createdAt            = rowGet(r,"created_at");
    updatedAt            = rowGet(r,"updated_at");
}

bool TaskF22::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM tasks WHERE task_id=?;", {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("TaskF22 not found: " + id); return false; }
    fromRow(rows[0]);
    loadQTCSLinks();
    return true;
}

OperationResult TaskF22::remove() {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return OperationResult::DB_ERROR;
    LOG_WARN("Removing TaskF22: " + taskId);
    db->exec("DELETE FROM task_quality WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_cost    WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_time    WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_scope   WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM meetings     WHERE task_id=?;", {BindParam::text(taskId)});
    return db->exec("DELETE FROM tasks WHERE task_id=?;", {BindParam::text(taskId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult TaskF22::update() {
    if (isReleased()) {
        LOG_WARN("[F22] update() verweigert: Aufgabe ist released — " + taskId);
        return OperationResult::ENTITY_RELEASED;
    } updatedAt = nowIso(); return save(); }

std::shared_ptr<TaskF22> TaskF22::loadById(const std::string& id) {
    auto t = std::make_shared<TaskF22>();
    if (!t->load(id)) return nullptr;
    return t;
}

std::vector<std::shared_ptr<TaskF22>> TaskF22::loadForProject(const std::string& pid) {
    auto* db = DatabasePool::instance().get("f22");
    std::vector<std::shared_ptr<TaskF22>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM tasks WHERE project_id=? ORDER BY wbs_code, created_at;",
        {BindParam::text(pid)});
    for (auto& r : rows) {
        auto t = std::make_shared<TaskF22>();
        t->fromRow(r);
        result.push_back(t);
    }
    LOG_DEBUG("Loaded " + std::to_string(result.size()) + " tasks for project " + pid);
    return result;
}

std::vector<std::shared_ptr<TaskF22>> TaskF22::loadChildren(const std::string& parentId) {
    auto* db = DatabasePool::instance().get("f22");
    std::vector<std::shared_ptr<TaskF22>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM tasks WHERE parent_task_id=? ORDER BY wbs_code;",
        {BindParam::text(parentId)});
    for (auto& r : rows) {
        auto t = std::make_shared<TaskF22>();
        t->fromRow(r);
        result.push_back(t);
    }
    return result;
}

// ── QTCS ─────────────────────────────────────────────────────

void TaskF22::loadQTCSLinks() {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return;
    auto loadIds = [&](const std::string& table, const std::string& col) {
        std::vector<std::string> ids;
        auto rows = db->query("SELECT " + col + " FROM " + table + " WHERE task_id=?;",
                              {BindParam::text(taskId)});
        for (auto& r : rows) ids.push_back(r.begin()->second);
        return ids;
    };
    qualityIds = loadIds("task_quality", "quality_id");
    costIds    = loadIds("task_cost",    "cost_id");
    timeIds    = loadIds("task_time",    "time_id");
    scopeIds   = loadIds("task_scope",   "scope_id");
}


// ── Reassign ─────────────────────────────────────────────────
OperationResult TaskF22::reassignTo(const std::string& newAssigneeId) {
    LOG_INFO("Reassigning task " + taskId + " to " + newAssigneeId);
    assigneeId = newAssigneeId; return update();
}
OperationResult TaskF22::reassignToProject(const std::string& newProjectId) {
    LOG_INFO("Moving task " + taskId + " to project " + newProjectId);
    projectId = newProjectId; parentTaskId = ""; return update();
}
OperationResult TaskF22::reassignParent(const std::string& newParentTaskId) {
    parentTaskId = newParentTaskId; return update();
}

// ── Convert to Project ────────────────────────────────────────
// ------------------------------
// convertToProject
//
// Parameters:
//   title                : new project title (empty = use task title)
//
// Behavior:
//   Creates a ProjectF16 from this task's metadata
//   Saves the new project and returns its ID
//   Does NOT delete the original task
//
// Returns:
//   New project ID, or "" on error
// ------------------------------
std::string TaskF22::convertToProject(const std::string& projectType_) {
    LOG_INFO("Promoting TaskF22 " + taskId + " -> ProjectF16");
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return "";

    std::string newProjId  = "f16_promoted_" + taskId;
    RegNumber   projReg    = RegNumberGenerator::next(RegDept::PROJECT);

    db->exec(R"(
        INSERT OR REPLACE INTO projects (
            project_id, reg_number, reg_dept, reg_sequence, reg_year,
            title, project_type, status, scope_statement,
            budget_planned, start_date_planned, end_date_planned,
            notes, created_at
        ) VALUES (?,?,?,?,?, ?,?,?,?, ?,?,?, ?,?)
    )", {
        BindParam::text(newProjId),
        BindParam::text(projReg.toString()),
        BindParam::text(projReg.dept),
        BindParam::int64(projReg.sequence),
        BindParam::int64(projReg.year),
        BindParam::text(title + " [promoted from F22/" + taskId + "]"),
        BindParam::text(projectType_),
        BindParam::text(status),
        textOrNull(acceptanceCriteria),
        BindParam::real(costPlanned),
        textOrNull(startDatePlanned),
        textOrNull(dueDatePlanned),
        BindParam::text(notes),
        BindParam::text(nowIso())
    });
    LOG_INFO("Project promoted from task: " + newProjId);
    return newProjId;
}

// ── Serialisation ─────────────────────────────────────────────
json TaskF22::toJson() const {
    json j;
    j["taskId"]      = taskId;
    j["regNumber"]   = regNumber.toString();
    j["projectId"]   = projectId;
    j["title"]       = title;
    j["status"]      = status;
    j["assigneeId"]  = assigneeId;
    j["priority"]    = priority;
    j["percentComplete"] = percentComplete;
    return j;
}

std::shared_ptr<TaskF22> TaskF22::fromJson(const json& j) {
    auto t = std::make_shared<TaskF22>();
    t->taskId     = j.value("taskId",    "");
    t->projectId  = j.value("projectId", "");
    t->title      = j.value("title",     "");
    t->status     = j.value("status",    "draft");
    t->assigneeId = j.value("assigneeId","");
    return t;
}

// ── MFS output ───────────────────────────────────────────────
bool TaskF22::writeMFSFile(const std::string& mfsRoot) const {
    return MFSWriter::writeTask(*this, mfsRoot);
}

// ------------------------------
// loadRecent
// Returns the n most recently created TaskF22 records.
// Parameters:
//   n : maximum number of results (default 20)
// ------------------------------
std::vector<std::shared_ptr<TaskF22>> TaskF22::loadRecent(int n) {
    std::vector<std::shared_ptr<TaskF22>> result;
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return result;
    auto rows = db->query("SELECT * FROM tasks ORDER BY created_at DESC LIMIT ?;", {BindParam::int64(n)});
    for (auto& r : rows) {
        auto obj = std::make_shared<TaskF22>();
        obj->fromRow(r);
        result.push_back(obj);
    }
    return result;
}


bool TaskF22::isWorkflowComplete() const {
    if (releaseWorkflowId.empty()) return false;
    auto wf = Rosenholz::F77_Workflow::loadById(releaseWorkflowId);
    return wf && wf->status == "completed";
}

void TaskF22::ensureReleaseWorkflow() {
    if (!releaseWorkflowId.empty()) return;
    auto wf = Rosenholz::F77_Engine::startDefault("f22", taskId);
    if (!wf) return;
    releaseWorkflowId = wf->workflowId;
    status         = "in_work";
    auto* db = DatabasePool::instance().get("f22");
    if (db) db->exec(
        "UPDATE tasks SET release_workflow_id=?, status='in_work', updated_at=? "
            "WHERE task_id=?;",
            {BindParam::text(releaseWorkflowId),
             BindParam::text(nowIso()),
             BindParam::text(taskId)});
    LOG_INFO("[F77] Main WFI ensured for F22: " + taskId +
             " wfi=" + releaseWorkflowId);
}

} // namespace Rosenholz

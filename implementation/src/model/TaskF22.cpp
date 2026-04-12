// ============================================================
// TaskF22.cpp  —  Task entity implementation
// ============================================================

#include "TaskF22.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include "../core/RegNumber.h"
#include "../core/FileOps.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>

using json = nlohmann::json;

namespace RH {



// ── Factory ──────────────────────────────────────────────────
std::shared_ptr<TaskF22> TaskF22::create(
    const std::string& projectId_,
    const std::string& title_,
    const std::string& assigneeId_,
    const std::string& parentTaskId_)
{
    LOG_INFO("Creating TaskF22: " + title_ + " in project " + projectId_);
    auto t = std::make_shared<TaskF22>();
    t->taskId        = genId("f22");
    t->regNumber     = RegNumberGenerator::next(RegDept::TASK);
    t->projectId     = projectId_;
    t->title         = title_;
    t->assigneeId    = assigneeId_;
    t->parentTaskId  = parentTaskId_;
    t->status        = "draft";
    t->createdAt     = nowIso();
    t->updatedAt     = t->createdAt;
    t->notes         = "{}";
    LOG_INFO("TaskF22 created: " + t->taskId + " reg=" + t->regNumber.toString());
    return t;
}

// ── CRUD ─────────────────────────────────────────────────────
bool TaskF22::save() const {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) { LOG_ERROR("TaskF22::save — projects DB not available"); return false; }

    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO tasks (
            task_id, workflow_instance_id, workflow_status, workflow_current_state,
            reg_number, project_id, parent_task_id, assignee_id, assigned_by,
            task_code, title, description, task_type, status, priority,
            effort_planned_hrs, effort_actual_hrs, effort_remaining_hrs,
            cost_planned, cost_actual,
            start_date_planned, start_date_actual, due_date_planned, due_date_actual,
            schedule_variance_days, percent_complete,
            quality_criteria, acceptance_criteria, is_milestone,
            wbs_code, sprint_or_phase, links, notes, created_at, updated_at
        ) VALUES (?,?,?,?, ?,?,?,?,?, ?,?,?,?,?,?, ?,?,?, ?,?, ?,?,?,?, ?,?, ?,?,?, ?,?,?,?,?,?)
    )", {
        BindParam::text(taskId),
        ton(workflowInstanceId),
        ton(workflowStatus),
        ton(workflowCurrentState),
        BindParam::text(regNumber.toString()),
        BindParam::text(projectId),
        ton(parentTaskId),
        ton(assigneeId),
        ton(assignedBy),
        ton(taskCode),
        BindParam::text(title),
        ton(description),
        ton(taskType),
        BindParam::text(status),
        ton(priority),
        BindParam::real(effortPlannedHrs),
        BindParam::real(effortActualHrs),
        BindParam::real(effortRemainingHrs),
        BindParam::real(costPlanned),
        BindParam::real(costActual),
        ton(startDatePlanned),
        ton(startDateActual),
        ton(dueDatePlanned),
        ton(dueDateActual),
        BindParam::int64(scheduleVarianceDays),
        BindParam::int64(percentComplete),
        ton(qualityCriteria),
        ton(acceptanceCriteria),
        BindParam::int64(isMilestone ? 1 : 0),
        ton(wbsCode),
        ton(sprintOrPhase),
        ton(links),
        BindParam::text(notes),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    });

    if (ok) LOG_INFO("TaskF22 saved: " + taskId);
    else    LOG_ERROR("TaskF22 save failed: " + taskId);
    return ok;
}

void TaskF22::fromRow(const Row& r) {
    auto get = [&](const std::string& k) {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    taskId               = get("task_id");
    workflowInstanceId   = get("workflow_instance_id");
    workflowStatus       = get("workflow_status");
    workflowCurrentState = get("workflow_current_state");
    regNumber            = RegNumber::fromString(get("reg_number"));
    projectId            = get("project_id");
    parentTaskId         = get("parent_task_id");
    assigneeId           = get("assignee_id");
    assignedBy           = get("assigned_by");
    taskCode             = get("task_code");
    title                = get("title");
    description          = get("description");
    taskType             = get("task_type");
    status               = get("status");
    priority             = get("priority");
    auto gd = [&](const std::string& k){ auto v=get(k); return v.empty()?0.0:std::stod(v); };
    auto gi = [&](const std::string& k){ auto v=get(k); return v.empty()?0:std::stoi(v); };
    effortPlannedHrs     = gd("effort_planned_hrs");
    effortActualHrs      = gd("effort_actual_hrs");
    effortRemainingHrs   = gd("effort_remaining_hrs");
    costPlanned          = gd("cost_planned");
    costActual           = gd("cost_actual");
    startDatePlanned     = get("start_date_planned");
    startDateActual      = get("start_date_actual");
    dueDatePlanned       = get("due_date_planned");
    dueDateActual        = get("due_date_actual");
    scheduleVarianceDays = gi("schedule_variance_days");
    percentComplete      = gi("percent_complete");
    qualityCriteria      = get("quality_criteria");
    acceptanceCriteria   = get("acceptance_criteria");
    isMilestone          = get("is_milestone") == "1";
    wbsCode              = get("wbs_code");
    sprintOrPhase        = get("sprint_or_phase");
    links                = get("links");
    notes                = get("notes");
    createdAt            = get("created_at");
    updatedAt            = get("updated_at");
}

bool TaskF22::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM tasks WHERE task_id=?;", {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("TaskF22 not found: " + id); return false; }
    fromRow(rows[0]);
    loadQTCSLinks();
    return true;
}

bool TaskF22::remove() {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    LOG_WARN("Removing TaskF22: " + taskId);
    db->exec("DELETE FROM task_quality WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_cost    WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_time    WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_scope   WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM meetings     WHERE task_id=?;", {BindParam::text(taskId)});
    return db->exec("DELETE FROM tasks WHERE task_id=?;", {BindParam::text(taskId)});
}

bool TaskF22::update() { updatedAt = nowIso(); return save(); }

std::shared_ptr<TaskF22> TaskF22::loadById(const std::string& id) {
    auto t = std::make_shared<TaskF22>();
    if (!t->load(id)) return nullptr;
    return t;
}

std::vector<std::shared_ptr<TaskF22>> TaskF22::loadForProject(const std::string& pid) {
    auto* db = DatabasePool::instance().get("projects");
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
    auto* db = DatabasePool::instance().get("projects");
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
bool TaskF22::addQuality(const std::string& id) {
    qualityIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO task_quality VALUES (?,?);",
        {BindParam::text(taskId), BindParam::text(id)}) : false;
}
bool TaskF22::addCost(const std::string& id) {
    costIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO task_cost VALUES (?,?);",
        {BindParam::text(taskId), BindParam::text(id)}) : false;
}
bool TaskF22::addTime(const std::string& id) {
    timeIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO task_time VALUES (?,?);",
        {BindParam::text(taskId), BindParam::text(id)}) : false;
}
bool TaskF22::addScope(const std::string& id) {
    scopeIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO task_scope VALUES (?,?);",
        {BindParam::text(taskId), BindParam::text(id)}) : false;
}

void TaskF22::loadQTCSLinks() {
    auto* db = DatabasePool::instance().get("projects");
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

// ── Trackable ────────────────────────────────────────────────
std::shared_ptr<TrackableItem> TaskF22::addTrackable(
    const std::string& title_, const std::string& createdBy_) {
    auto t = TrackableItem::create("task", taskId, title_, createdBy_);
    t->save();
    trackables.push_back(t);
    return t;
}
void TaskF22::loadTrackables() {
    trackables = TrackableItem::loadForEntity("task", taskId);
}

// ── Reassign ─────────────────────────────────────────────────
bool TaskF22::reassignTo(const std::string& newAssigneeId) {
    LOG_INFO("Reassigning task " + taskId + " to " + newAssigneeId);
    assigneeId = newAssigneeId; return update();
}
bool TaskF22::reassignToProject(const std::string& newProjectId) {
    LOG_INFO("Moving task " + taskId + " to project " + newProjectId);
    projectId = newProjectId; parentTaskId = ""; return update();
}
bool TaskF22::reassignParent(const std::string& newParentTaskId) {
    parentTaskId = newParentTaskId; return update();
}

// ── Convert to Project ────────────────────────────────────────
std::string TaskF22::convertToProject(const std::string& projectType_) {
    LOG_INFO("Promoting TaskF22 " + taskId + " -> ProjectF16");
    auto* db = DatabasePool::instance().get("projects");
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
        ton(acceptanceCriteria),
        BindParam::real(costPlanned),
        ton(startDatePlanned),
        ton(dueDatePlanned),
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
    std::string dir  = FileOps::joinPath(mfsRoot, "F22");
    std::string path = FileOps::joinPath(dir, "F22_" + regNumber.toString() + ".txt");

    std::ostringstream oss;
    oss << "REGISTRIERNUMMER:   " << regNumber.toString() << "\n";
    oss << "VORGANGSNUMMER:     " << projectId            << "\n";
    oss << "STATUS:             " << status               << "\n";
    oss << "PROZENT:            " << percentComplete      << "%\n";
    oss << "ANGELEGT:           " << createdAt            << "\n";
    oss << "---\n";
    oss << "VERBINDUNG-F16:     F16/" << projectId << "\n";
    if (!parentTaskId.empty())
        oss << "VERBINDUNG-F22:   F22/" << parentTaskId << "\n";

    FileOps::makeDirs(dir);
    bool ok = FileOps::writeTextFile(path, oss.str());
#ifndef _WIN32
    if (ok) chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
    LOG_DEBUG("MFS F22 file written: " + path);
    return ok;
}

} // namespace RH

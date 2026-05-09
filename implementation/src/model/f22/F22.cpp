// ============================================================
// F22.cpp  —  Task entity implementation
// ============================================================

#include <regex>
#include "F22.h"
#include "../../core/Config.h"
#include "../../core/FileOps.h"
#include "../../core/Database.h"
#include "../../workflow/F77Workflow.h"
#include "../akt/Folder.h"
#include "../../mfs/MFSWriter.h"
#include "../../core/FileOps.h"
#include "../../core/Logger.h"
#include "../../core/Repository.h"
#ifndef _WIN32
#endif
#include "../../core/RegNumber.h"
#include "../../core/FileOps.h"

using json = nlohmann::json;

namespace Rosenholz {



// ── Factory ──────────────────────────────────────────────────
std::shared_ptr<F22> F22::create(
    const std::string& projectId_,
    const std::string& title_,
    const std::string& assigneeId_,
    const std::string& parentTaskId_)
{
    LOG_INFO("Creating F22: " + title_ + " in project " + projectId_);
    auto t = std::make_shared<F22>();
    t->taskId        = genId("F22");
    t->regNumber     = RegNumber::fromString(t->taskId);
    t->projectId     = projectId_;
    t->title         = title_;
    t->assigneeId    = assigneeId_;
    t->parentTaskId  = parentTaskId_;
    t->status        = EntityStatus::IN_WORK;
    t->createdAt     = nowIso();
    t->updatedAt     = t->createdAt;
    t->notes         = "{}";
    LOG_INFO("F22 created: " + t->taskId + " reg=" + t->regNumber.toString());
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
OperationResult F22::save() const {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) { LOG_ERROR("F22::save — f22 DB not available"); return OperationResult::DB_ERROR; }
    db->beginTransaction();
    OperationResult ok = db->exec(R"(
        INSERT OR REPLACE INTO tasks (
            task_id, release_workflow_id, reg_number, project_id, parent_task_id,
            assignee_id, assigned_by, task_code, title, description,
            task_type, status, priority,
            effort_planned_hrs, effort_actual_hrs, effort_remaining_hrs,
            cost_planned, cost_actual,
            start_date_planned, start_date_actual, due_date_planned, due_date_actual,
            schedule_variance_days, percent_complete,
            quality_criteria, acceptance_criteria, milestones, wbs_code, sprint_or_phase,
            links, notes, created_at, updated_at
        ) VALUES (
            ?,?,?,?,?,  ?,?,?,?,?,  ?,?,?,
            ?,?,?,  ?,?,
            ?,?,?,?,  ?,?,
            ?,?,?,?,?,  ?,?,?,?
        )
    )", {
        BindParam::text(taskId),
        BindParam::nullOrText(releaseWorkflowId),
        BindParam::text(regNumber.toString()),
        BindParam::text(projectId),
        BindParam::nullOrText(parentTaskId),
        BindParam::nullOrText(assigneeId),
        BindParam::nullOrText(assignedBy),
        BindParam::nullOrText(taskCode),
        BindParam::text(title),
        BindParam::nullOrText(description),
        BindParam::text(taskType.empty() ? "task" : taskType),
        BindParam::text(entityStatusToString(status)),
        BindParam::text(priority.empty() ? "medium" : priority),
        BindParam::real(effortPlannedHrs),
        BindParam::real(effortActualHrs),
        BindParam::real(effortRemainingHrs),
        BindParam::real(costPlanned),
        BindParam::real(costActual),
        BindParam::nullOrText(startDatePlanned),
        BindParam::nullOrText(startDateActual),
        BindParam::nullOrText(dueDatePlanned),
        BindParam::nullOrText(dueDateActual),
        BindParam::int64(scheduleVarianceDays),
        BindParam::int64(percentComplete),
        BindParam::nullOrText(qualityCriteria),
        BindParam::nullOrText(acceptanceCriteria),
        BindParam::nullOrText(milestones),
        BindParam::nullOrText(wbsCode),
        BindParam::nullOrText(sprintOrPhase),
        BindParam::nullOrText(links),
        BindParam::text(notes),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;

    if (opOk(ok)) { db->commitTransaction(); LOG_DEBUG("F22 saved: " + taskId); }
    else { db->rollbackTransaction(); LOG_ERROR("F22 save failed: " + taskId); }

    // Auto-create "Allgemeine Akte" on first save if not already present
    if (opOk(ok)) {
        auto* aktDb = DatabasePool::instance().get("akt");
        if (aktDb) {
            auto existingCheck = aktDb->query(
                "SELECT COUNT(*) as n FROM folders f "
                "JOIN entity_folders ef ON f.folder_id=ef.folder_id "
                "WHERE ef.entity_type='f22' AND ef.entity_id=? AND f.doc_type='general';",
                {BindParam::text(taskId)});
            bool alreadyExists = !existingCheck.empty() &&
                                  existingCheck[0].begin()->second != "0";
            if (!alreadyExists) {
                auto allgAkte = Rosenholz::Folder::create("Allgemeine Akte " + taskId, "general", taskId, "");
                if (opOk(allgAkte->save())) {
                    allgAkte->attachToEntity("f22", taskId);
                    LOG_INFO("[F22] Allgemeine Akte angelegt: " + allgAkte->folderId + " fuer " + taskId);
                }
            }
        }
    }
    return ok;
}

void F22::fromRow(const Row& r) {
    taskId               = rowGet(r,"task_id");
    releaseWorkflowId       = rowGet(r,"release_workflow_id");
    wfLocked                = (rowGet(r,"wf_locked") == "1");
    regNumber            = RegNumber::fromString(rowGet(r,"reg_number"));
    projectId            = rowGet(r,"project_id");
    parentTaskId         = rowGet(r,"parent_task_id");
    assigneeId           = rowGet(r,"assignee_id");
    assignedBy           = rowGet(r,"assigned_by");
    taskCode             = rowGet(r,"task_code");
    title                = rowGet(r,"title");
    description          = rowGet(r,"description");
    taskType             = rowGet(r,"task_type");
    status               = entityStatusFrom(rowGet(r,"status"));
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

bool F22::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM tasks WHERE task_id=?;", {BindParam::text(id)});
    if (rows.empty()) { LOG_DEBUG("F22 not found: " + id); return false; }
    fromRow(rows[0]);
    return true;
}

OperationResult F22::remove() {
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return OperationResult::DB_ERROR;
    LOG_WARN("Removing F22: " + taskId);
    db->exec("DELETE FROM task_quality WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_cost    WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_time    WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM task_scope   WHERE task_id=?;", {BindParam::text(taskId)});
    db->exec("DELETE FROM meetings     WHERE task_id=?;", {BindParam::text(taskId)});
    return db->exec("DELETE FROM tasks WHERE task_id=?;", {BindParam::text(taskId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F22::update() {
    if (isReleased()) {
        LOG_WARN("[F22] update() verweigert: Aufgabe ist released — " + taskId);
        return OperationResult::ENTITY_RELEASED;
    }
    if (wfLocked) {
        LOG_WARN("[F22] update() verweigert: Freigabe-Workflow aktiv — " + taskId);
        return OperationResult::ENTITY_LOCKED;
    } updatedAt = nowIso(); return save(); }

std::shared_ptr<F22> F22::loadById(const std::string& id) {
    auto t = std::make_shared<F22>();
    if (!t->load(id)) return nullptr;
    return t;
}

std::vector<std::shared_ptr<F22>> F22::loadForProject(const std::string& pid) {
    auto* db = DatabasePool::instance().get("f22");
    std::vector<std::shared_ptr<F22>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM tasks WHERE project_id=? ORDER BY wbs_code, created_at;",
        {BindParam::text(pid)});
    for (auto& r : rows) {
        auto t = std::make_shared<F22>();
        t->fromRow(r);
        result.push_back(t);
    }
    LOG_DEBUG("Loaded " + std::to_string(result.size()) + " tasks for project " + pid);
    return result;
}

std::vector<std::shared_ptr<F22>> F22::loadChildren(const std::string& parentId) {
    auto* db = DatabasePool::instance().get("f22");
    std::vector<std::shared_ptr<F22>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM tasks WHERE parent_task_id=? ORDER BY wbs_code;",
        {BindParam::text(parentId)});
    for (auto& r : rows) {
        auto t = std::make_shared<F22>();
        t->fromRow(r);
        result.push_back(t);
    }
    return result;
}



// ── Reassign ─────────────────────────────────────────────────
OperationResult F22::reassignTo(const std::string& newAssigneeId) {
    LOG_INFO("Reassigning task " + taskId + " to " + newAssigneeId);
    assigneeId = newAssigneeId; return update();
}
OperationResult F22::reassignToProject(const std::string& newProjectId) {
    LOG_INFO("Moving task " + taskId + " to project " + newProjectId);
    projectId = newProjectId; parentTaskId = ""; return update();
}
OperationResult F22::reassignParent(const std::string& newParentTaskId) {
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
//   Creates a F16 from this task's metadata
//   Saves the new project and returns its ID
//   Does NOT delete the original task
//
// Returns:
//   New project ID, or "" on error
// ------------------------------
std::string F22::convertToProject(const std::string& projectType_) {
    LOG_INFO("Promoting F22 " + taskId + " -> F16");
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
        BindParam::text(entityStatusToString(EntityStatus::IN_WORK)),  // new project
        BindParam::nullOrText(acceptanceCriteria),
        BindParam::real(costPlanned),
        BindParam::nullOrText(startDatePlanned),
        BindParam::nullOrText(dueDatePlanned),
        BindParam::text(notes),
        BindParam::text(nowIso())
    });
    LOG_INFO("Project promoted from task: " + newProjId);
    return newProjId;
}

// ── Serialisation ─────────────────────────────────────────────
json F22::toJson() const {
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

std::shared_ptr<F22> F22::fromJson(const json& j) {
    auto t = std::make_shared<F22>();
    t->taskId     = j.value("taskId",    "");
    t->projectId  = j.value("projectId", "");
    t->title      = j.value("title",     "");
    t->status     = entityStatusFrom(j.value("status", "in_work"));
    t->assigneeId = j.value("assigneeId","");
    return t;
}

// ── MFS output ───────────────────────────────────────────────
bool F22::writeMFSFile(const std::string& mfsRoot) const {
    return MFSWriter::writeTask(*this, mfsRoot);
}

// ------------------------------
// loadRecent
// Returns the n most recently created F22 records.
// Parameters:
//   n : maximum number of results (default 20)
// ------------------------------
std::vector<std::shared_ptr<F22>> F22::loadRecent(int n) {
    std::vector<std::shared_ptr<F22>> result;
    auto* db = DatabasePool::instance().get("f22");
    if (!db) return result;
    auto rows = db->query("SELECT * FROM tasks ORDER BY created_at DESC LIMIT ?;", {BindParam::int64(n)});
    for (auto& r : rows) {
        auto obj = std::make_shared<F22>();
        obj->fromRow(r);
        result.push_back(obj);
    }
    return result;
}


bool F22::isWorkflowComplete() const {
    if (releaseWorkflowId.empty()) return false;
    auto wf = Rosenholz::F77W::loadById(releaseWorkflowId);
    return wf && wf->status == WorkflowStatus::COMPLETED;
}

void F22::ensureReleaseWorkflow() {
    if (!releaseWorkflowId.empty()) return;
    // startDefault creates the WF and calls storeWorkflowId (one place, in F77Engine).
    auto wf = Rosenholz::F77Engine::startDefault("f22", taskId);
    if (!wf) return;
    releaseWorkflowId = wf->workflowId;
    LOG_INFO("[F77] Workflow ensured: " + releaseWorkflowId + " for f22/" + taskId);
}



std::string F22::mfsSchluesselText() const {
    std::ostringstream s;
    s << "  ID      : " << taskId << "\n"
      << "  Titel   : " << title << "\n"
      << "  Status  : " << entityStatusToString(status) << "\n"
      << "  F16     : " << projectId << "\n";
    auto docs = Rosenholz::Folder::loadForEntity("f22", taskId);
    if (!docs.empty()) {
        s << "  AKT     :";
        for (auto& d : docs) s << " " << d->folderId;
        s << "\n";
    }
    s << "\n";
    return s.str();
}

// ── F22::mfsDir ──────────────────────────────────────────────────────
std::string F22::mfsDir() const {
    const std::string& root = Config::instance().mfsPath();
    if (root.empty()) return "";
    return FileOps::joinPath(FileOps::joinPath(root, "F22"),
                             sanitiseRegNr(regNumber.toString()));
}

// ── F22::scanMfsForUnregistered ──────────────────────────────────────
// Scan the MFS folder for this task for files not registered as Akte objects.
// Returns (filePath, suggestedTitle) pairs for each unregistered file.
std::vector<std::pair<std::string,std::string>>
F22::scanMfsForUnregistered() const {
    std::string taskDir = mfsDir();
    if (!FileOps::dirExists(taskDir)) return {};

    // Delegate AKT-layer knowledge to Document — F22 does not know AKT internals:
    auto knownPaths = Folder::knownMfsPaths("f22", taskId);

    // Scan task folder for any files not in knownPaths:
    std::vector<std::pair<std::string,std::string>> result;
    auto files = FileOps::listFiles(taskDir, true); // recursive
    for (auto& f : files) {
        std::string base = FileOps::baseName(f);
        if (base.empty()) continue;
        // Skip auto-generated metadata files:
        if (base == "_SCHLUESSEL.txt" || base == "00_KARTE.txt" ||
            base == "00_DECKBLATT.txt" || base == "owner_key.txt") continue;
        // Skip Schlüssel-pattern files (XV_XXX_NNNN_YY.txt):
        if (base.size() > 4 && base.substr(base.size()-4) == ".txt") {
            static const std::regex rz_id(R"(^[A-Z]{2}_[A-Z]+_\d+_\d+\.txt$)");
            if (std::regex_match(base, rz_id)) continue;
        }
        if (knownPaths.count(f)) continue;
        // Preserve relative path from taskDir as the display name:
        std::string relPath = (f.size() > taskDir.size() + 1)
            ? f.substr(taskDir.size() + 1) : base;
        // Suggested title: stem of filename, spaces
        std::string stem = base;
        auto dot = stem.rfind('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        for (char& c : stem) if (c == '_' || c == '-') c = ' ';
        // fileName = relPath so user sees "subdir/file.pdf" not just "file.pdf"
        result.emplace_back(f, relPath);
    }
    return result;
}


int F22::count() {
    auto* d = DatabasePool::instance().get("f22");
    return d ? d->rowCount("tasks") : 0;
}

} // namespace Rosenholz

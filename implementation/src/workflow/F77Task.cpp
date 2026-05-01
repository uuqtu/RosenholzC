// ============================================================
// F77Task.cpp — Workflow-spawned tasks
// ============================================================
#include "F77Task.h"
#include "../core/Logger.h"
#include "../core/RegNumber.h"
#include "../model/Utils.h"

namespace Rosenholz {

// ── Internal helpers ─────────────────────────────────────────
Database* F77Task::db() {
    return DatabasePool::instance().get("f77task");
}

// ── CRUD ─────────────────────────────────────────────────────
void F77Task::fromRow(const Row& r) {
    auto g = [&](const std::string& k) {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    taskId             = g("task_id");
    workflowId         = g("workflow_id");
    operationId        = g("operation_id");
    title              = g("title");
    targetEntityType   = g("target_entity_type");
    targetEntityId     = g("target_entity_id");
    targetAction       = g("target_action");
    filePath           = g("file_path");
    fileName           = g("file_name");
    status             = g("status");
    assignedTo         = g("assigned_to");
    createdAt          = g("created_at");
    updatedAt          = g("updated_at");
    completedAt        = g("completed_at");
    completionNote     = g("completion_note");
}

OperationResult F77Task::save() const {
    auto* d = db();
    if (!d) return OperationResult::DB_ERROR;
    bool ok = d->exec(
        "INSERT OR REPLACE INTO f77_tasks"
        " (task_id,workflow_id,operation_id,title,"
        "  target_entity_type,target_entity_id,target_action,"
        "  file_path,file_name,status,assigned_to,"
        "  created_at,updated_at,completed_at,completion_note)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
        {BindParam::text(taskId),
         BindParam::text(workflowId),
         BindParam::nullOrText(operationId),
         BindParam::text(title),
         BindParam::text(targetEntityType),
         BindParam::text(targetEntityId),
         BindParam::nullOrText(targetAction),
         BindParam::nullOrText(filePath),
         BindParam::nullOrText(fileName),
         BindParam::text(status.empty() ? "open" : status),
         BindParam::nullOrText(assignedTo),
         BindParam::text(createdAt.empty() ? nowIso() : createdAt),
         BindParam::text(nowIso()),
         BindParam::nullOrText(completedAt),
         BindParam::nullOrText(completionNote)});
    return ok ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult F77Task::update() const { return save(); }

OperationResult F77Task::remove() const {
    auto* d = db();
    if (!d) return OperationResult::DB_ERROR;
    d->exec("DELETE FROM f77_tasks WHERE task_id=?;", {BindParam::text(taskId)});
    return OperationResult::OPERATION_ACK;
}

// ── Lifecycle ────────────────────────────────────────────────
OperationResult F77Task::complete(const std::string& note) {
    status         = "completed";
    completedAt    = nowIso();
    completionNote = note;
    return update();
    // Note: caller (CLI/engine) should call F77Engine::tick() after close
    // to advance the workflow. F77Task does not call the engine directly.
}

OperationResult F77Task::skip(const std::string& reason) {
    status         = "skipped";
    completedAt    = nowIso();
    completionNote = reason;
    return update();
}

OperationResult F77Task::cancel() {
    status      = "cancelled";
    completedAt = nowIso();
    return update();
}

// ── Queries ──────────────────────────────────────────────────
std::shared_ptr<F77Task> F77Task::loadById(const std::string& id) {
    auto* d = db();
    if (!d) return nullptr;
    auto rows = d->query("SELECT * FROM f77_tasks WHERE task_id=?;",
                         {BindParam::text(id)});
    if (rows.empty()) return nullptr;
    auto t = std::make_shared<F77Task>(); t->fromRow(rows[0]); return t;
}

std::vector<std::shared_ptr<F77Task>> F77Task::loadOpen() {
    auto* d = db(); if (!d) return {};
    auto rows = d->query(
        "SELECT * FROM f77_tasks WHERE status='open'"
        " ORDER BY created_at ASC;", {});
    std::vector<std::shared_ptr<F77Task>> v;
    for (auto& r : rows) { auto t = std::make_shared<F77Task>(); t->fromRow(r); v.push_back(t); }
    return v;
}

std::vector<std::shared_ptr<F77Task>> F77Task::loadAll(int limit) {
    auto* d = db(); if (!d) return {};
    auto rows = d->query(
        "SELECT * FROM f77_tasks ORDER BY created_at DESC LIMIT ?;",
        {BindParam::int64(limit)});
    std::vector<std::shared_ptr<F77Task>> v;
    for (auto& r : rows) { auto t = std::make_shared<F77Task>(); t->fromRow(r); v.push_back(t); }
    return v;
}

std::vector<std::shared_ptr<F77Task>> F77Task::loadForWorkflow(const std::string& wfId) {
    auto* d = db(); if (!d) return {};
    auto rows = d->query(
        "SELECT * FROM f77_tasks WHERE workflow_id=? ORDER BY created_at ASC;",
        {BindParam::text(wfId)});
    std::vector<std::shared_ptr<F77Task>> v;
    for (auto& r : rows) { auto t = std::make_shared<F77Task>(); t->fromRow(r); v.push_back(t); }
    return v;
}

std::vector<std::shared_ptr<F77Task>> F77Task::loadForOperation(const std::string& opId) {
    auto* d = db(); if (!d) return {};
    auto rows = d->query(
        "SELECT * FROM f77_tasks WHERE operation_id=? ORDER BY created_at ASC;",
        {BindParam::text(opId)});
    std::vector<std::shared_ptr<F77Task>> v;
    for (auto& r : rows) { auto t = std::make_shared<F77Task>(); t->fromRow(r); v.push_back(t); }
    return v;
}

std::vector<std::shared_ptr<F77Task>> F77Task::loadForEntity(
    const std::string& et, const std::string& eid)
{
    auto* d = db(); if (!d) return {};
    auto rows = d->query(
        "SELECT * FROM f77_tasks"
        " WHERE target_entity_type=? AND target_entity_id=?"
        " ORDER BY created_at ASC;",
        {BindParam::text(et), BindParam::text(eid)});
    std::vector<std::shared_ptr<F77Task>> v;
    for (auto& r : rows) { auto t = std::make_shared<F77Task>(); t->fromRow(r); v.push_back(t); }
    return v;
}

// ── Factory ──────────────────────────────────────────────────
std::shared_ptr<F77Task> F77Task::create(
    const std::string& workflowId,
    const std::string& operationId,
    const std::string& title,
    const std::string& targetEntityType,
    const std::string& targetEntityId,
    const std::string& targetAction,
    const std::string& filePath,
    const std::string& fileName,
    const std::string& assignedTo)
{
    auto t = std::make_shared<F77Task>();
    t->taskId           = genId("F77T");
    t->workflowId       = workflowId;
    t->operationId      = operationId;
    t->title            = title;
    t->targetEntityType = targetEntityType;
    t->targetEntityId   = targetEntityId;
    t->targetAction     = targetAction;
    t->filePath         = filePath;
    t->fileName         = fileName;
    t->status           = "open";
    t->assignedTo       = assignedTo;
    t->createdAt        = nowIso();
    t->updatedAt        = t->createdAt;

    if (!opOk(t->save())) {
        LOG_ERROR("[F77Task] create failed: " + title);
        return nullptr;
    }
    LOG_INFO("[F77Task] Created: " + t->taskId + " — " + title);
    return t;
}

// ── F77Task::checkOperationComplete ─────────────────────────────────────
// Pure query — no engine calls. Caller (tick) advances the workflow.
bool F77Task::checkOperationComplete(const std::string& operationId) {
    auto tasks = loadForOperation(operationId);
    if (tasks.empty()) return false;
    for (auto& t : tasks) if (t->isOpen()) return false;
    LOG_INFO("[F77Task] All tasks closed for operation: " + operationId);
    return true;
}

} // namespace Rosenholz

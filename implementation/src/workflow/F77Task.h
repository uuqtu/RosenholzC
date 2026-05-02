#pragma once
// ============================================================
// F77Task.h — Workflow-spawned tasks surfaced in "Meine Aufgaben"
//
// F77Tasks are created by the F77 engine when a WorkflowOperation
// requires a manual decision (e.g. managing an unregistered file).
//
// KEY DESIGN PRINCIPLES:
//   - No parent entity dependency — they are FREE objects
//   - Carry full navigation context (entity type + ID + action hint)
//   - When all F77Tasks for an operation are closed → operation auto-completes
//   - Business logic lives HERE, not in the CLI
//   - CLI only calls F77Task methods and displays results
// ============================================================
#include "../core/OperationResult.h"
#include "../core/Database.h"
#include <string>
#include <vector>
#include <memory>

namespace Rosenholz {

struct F77Task {
    std::string taskId;           ///< XV/F77T/0001/26
    std::string workflowId;       ///< source F77W
    std::string operationId;      ///< source F77W_Operation stepId
    std::string title;            ///< Human-readable task name

    // Navigation context — enough to reach the object needing action directly
    std::string targetEntityType; ///< f16|f22|f18|akt
    std::string targetEntityId;   ///< ID of the entity requiring action
    std::string targetAction;     ///< hint: nacherfassen|review|approve

    // File context (for document-handling tasks)
    std::string filePath;         ///< MFS path of unregistered file (if any)
    std::string fileName;         ///< display name of the file

    // Lifecycle
    std::string status;           ///< open|completed|skipped|cancelled
    std::string assignedTo;       ///< Person-ID (optional)
    std::string createdAt;
    std::string updatedAt;
    std::string completedAt;
    std::string completionNote;   ///< What was decided/done

    bool isOpen()     const { return status == "open"; }
    std::string statusLabel() const; ///< German: offen/erledigt/...
    bool isClosed()   const { return !isOpen(); }

    // ── CRUD ─────────────────────────────────────────────────
    OperationResult save()   const;
    OperationResult update() const;
    OperationResult remove() const;
    void fromRow(const Row& r);

    // ── Complete/skip a task ─────────────────────────────────
    /// Mark task as completed with an optional note.
    OperationResult complete(const std::string& note = "");
    /// Skip this task (won't block the workflow operation).
    OperationResult skip(const std::string& reason = "");
    /// Cancel (admin use, marks operation as cancelled too if desired).
    OperationResult cancel();

    // ── Queries ──────────────────────────────────────────────
    static std::shared_ptr<F77Task> loadById(const std::string& id);
    static std::vector<std::shared_ptr<F77Task>> loadOpen();
    static std::vector<std::shared_ptr<F77Task>> loadAll(int limit = 100);
    static std::vector<std::shared_ptr<F77Task>> loadForWorkflow(const std::string& workflowId);
    static std::vector<std::shared_ptr<F77Task>> loadForOperation(const std::string& operationId);
    static std::vector<std::shared_ptr<F77Task>> loadForEntity(
        const std::string& entityType, const std::string& entityId);

    // ── Factory ──────────────────────────────────────────────
    /// Create and persist a new F77Task.
    static std::shared_ptr<F77Task> create(
        const std::string& workflowId,
        const std::string& operationId,
        const std::string& title,
        const std::string& targetEntityType,
        const std::string& targetEntityId,
        const std::string& targetAction  = "",
        const std::string& filePath      = "",
        const std::string& fileName      = "",
        const std::string& assignedTo    = "");

    /// Check if all open tasks for a given operation are closed.
    /// If so, auto-complete the F77W_Operation.
    static bool checkOperationComplete(const std::string& operationId);

private:
    static Database* db();
};

} // namespace Rosenholz

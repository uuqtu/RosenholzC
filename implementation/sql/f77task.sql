-- ── F77_Task ─────────────────────────────────────────────────────────────
-- Workflow-spawned tasks that appear in the "Meine Aufgaben" main menu.
-- Created by F77_WorkflowOperation when manual decisions are needed.
-- These are FREE objects — no parent entity dependency — but carry enough
-- context to navigate directly to the object requiring action.
--
-- Naming: F77_Task (workflow task) ≠ F22 (project task / Vorgangsaufgabe)
-- DB pool: f77task
-- ─────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS f77_tasks (
    task_id         TEXT    PRIMARY KEY,        -- XV/F77T/0001/26
    workflow_id     TEXT    NOT NULL,           -- source F77_Workflow
    operation_id    TEXT,                       -- source F77_WorkflowOperation (step)
    title           TEXT    NOT NULL,           -- e.g. "Nicht abgelegtes Dokument <Name> verwalten?"

    -- Navigation context: where the user needs to go to complete the task
    target_entity_type  TEXT NOT NULL           -- 'f16'|'f22'|'f18'|'akt'
                             CHECK(target_entity_type IN ('f16','f22','f18','akt')),
    target_entity_id    TEXT NOT NULL,          -- ID of the entity requiring action
    target_action       TEXT,                   -- hint: 'nacherfassen'|'review'|'approve'|...

    -- File reference (for document-handling tasks):
    file_path       TEXT,                       -- MFS path of the unregistered file
    file_name       TEXT,                       -- display name of the file

    -- Lifecycle
    status          TEXT NOT NULL DEFAULT 'open'
                         CHECK(status IN ('open','completed','skipped','cancelled')),
    assigned_to     TEXT,                       -- Person-ID (optional)
    created_at      TEXT NOT NULL,
    updated_at      TEXT NOT NULL,
    completed_at    TEXT,
    completion_note TEXT                        -- what was decided / done
);

CREATE INDEX IF NOT EXISTS idx_f77tasks_workflow  ON f77_tasks(workflow_id);
CREATE INDEX IF NOT EXISTS idx_f77tasks_operation ON f77_tasks(operation_id);
CREATE INDEX IF NOT EXISTS idx_f77tasks_status    ON f77_tasks(status);
CREATE INDEX IF NOT EXISTS idx_f77tasks_entity    ON f77_tasks(target_entity_type, target_entity_id);

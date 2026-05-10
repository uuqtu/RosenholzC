-- f77s.sql  —  F77 workflow step instances (F77S)
-- Separated from f77.sql so step instances have their own database file.
-- Each step belongs to one workflow instance (f77_workflows in f77.db).

CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT NOT NULL,
    version     INTEGER NOT NULL DEFAULT 0,
    applied_at  TEXT NOT NULL,
    note        TEXT,
    PRIMARY KEY (db_name)
);

CREATE TABLE IF NOT EXISTS f77_workflow_steps (
    step_id             TEXT PRIMARY KEY,
    workflow_id         TEXT NOT NULL REFERENCES f77_workflows(workflow_id),
    tpl_step_id         TEXT,           -- soft ref to template step (snapshot source)
    title               TEXT NOT NULL,
    sequence_order      INTEGER NOT NULL DEFAULT 0,
    is_initialize       INTEGER NOT NULL DEFAULT 0,
    is_final            INTEGER NOT NULL DEFAULT 0,
    execution_mode      TEXT NOT NULL DEFAULT 'sequential'
                             CHECK(execution_mode IN ('sequential','parallel')),
    predecessor_step_ids TEXT,          -- comma-sep f77_workflow_steps.step_id
    -- Status:
    status              TEXT NOT NULL DEFAULT 'pending'
                             CHECK(status IN ('pending','in_progress','approved','rejected','skipped','cancelled')),
    auto_approve        INTEGER NOT NULL DEFAULT 0,
    is_system           INTEGER NOT NULL DEFAULT 0,
    system_action       INTEGER NOT NULL DEFAULT 0, -- SystemAction enum value
    requires_comment    INTEGER NOT NULL DEFAULT 0,
    requires_document   INTEGER NOT NULL DEFAULT 0,
    completed_date      TEXT,
    created_at          TEXT NOT NULL,
    updated_at          TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_f77s_workflow ON f77_workflow_steps(workflow_id, sequence_order);
CREATE INDEX IF NOT EXISTS idx_f77s_status   ON f77_workflow_steps(status);

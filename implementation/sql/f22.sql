-- ============================================================
-- f22.db — F22 Aufgaben (Tasks)
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT    NOT NULL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS tasks (
    task_id                 TEXT PRIMARY KEY,
    release_workflow_id     TEXT,
    reg_number              TEXT,
    project_id              TEXT NOT NULL,
    parent_task_id          TEXT,
    assignee_id             TEXT,
    assigned_by             TEXT,
    task_code               TEXT,
    title                   TEXT NOT NULL,
    description             TEXT,
    task_type               TEXT DEFAULT 'task',
    status                  TEXT NOT NULL DEFAULT 'in_work',
    priority                TEXT DEFAULT 'medium',
    effort_planned_hrs      REAL DEFAULT 0,
    effort_actual_hrs       REAL DEFAULT 0,
    effort_remaining_hrs    REAL DEFAULT 0,
    cost_planned            REAL DEFAULT 0,
    cost_actual             REAL DEFAULT 0,
    start_date_planned      TEXT,
    start_date_actual       TEXT,
    due_date_planned        TEXT,
    due_date_actual         TEXT,
    schedule_variance_days  INTEGER DEFAULT 0,
    percent_complete        INTEGER DEFAULT 0,
    quality_criteria        TEXT,
    acceptance_criteria     TEXT,
    milestones              TEXT,
    wbs_code                TEXT,
    sprint_or_phase         TEXT,
    links                   TEXT,
    notes                   TEXT DEFAULT '{}',
    created_at              TEXT NOT NULL,
    updated_at              TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_tasks_project  ON tasks(project_id);
CREATE INDEX IF NOT EXISTS idx_tasks_parent   ON tasks(parent_task_id);
CREATE INDEX IF NOT EXISTS idx_tasks_assignee ON tasks(assignee_id);
CREATE INDEX IF NOT EXISTS idx_tasks_status   ON tasks(status);

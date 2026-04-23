-- ============================================================
-- f16.db — F16 Projekte
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS projects (
    project_id                  TEXT PRIMARY KEY,
    workflow_instance_id        TEXT,
    workflow_status             TEXT,
    workflow_current_state      TEXT,
    release_workflow_id         TEXT,
    reg_number                  TEXT,
    reg_dept                    TEXT,
    reg_sequence                INTEGER DEFAULT 0,
    reg_year                    INTEGER DEFAULT 0,
    owner_team_id               TEXT,
    lead_id                     TEXT,
    sponsor_id                  TEXT,
    codename                    TEXT,
    title                       TEXT NOT NULL,
    project_type                TEXT,
    size_class                  TEXT,
    status                      TEXT DEFAULT 'in_work',
    phase                       TEXT,
    start_date_planned          TEXT,
    start_date_actual           TEXT,
    end_date_planned            TEXT,
    end_date_actual             TEXT,
    duration_planned_days       INTEGER DEFAULT 0,
    duration_actual_days        INTEGER DEFAULT 0,
    schedule_variance_days      INTEGER DEFAULT 0,
    budget_planned              REAL DEFAULT 0,
    budget_approved             REAL DEFAULT 0,
    budget_committed            REAL DEFAULT 0,
    budget_actual               REAL DEFAULT 0,
    cost_variance               REAL DEFAULT 0,
    cost_performance_index      REAL DEFAULT 1,
    schedule_performance_index  REAL DEFAULT 1,
    earned_value                REAL DEFAULT 0,
    planned_value               REAL DEFAULT 0,
    actual_cost                 REAL DEFAULT 0,
    estimate_at_completion      REAL DEFAULT 0,
    estimate_to_complete        REAL DEFAULT 0,
    variance_at_completion      REAL DEFAULT 0,
    communication_plan_id       TEXT,
    currency                    TEXT DEFAULT 'EUR',
    scope_statement             TEXT,
    scope_version               TEXT,
    scope_last_changed          TEXT,
    scope_change_reason         TEXT,
    scope_change_count          INTEGER DEFAULT 0,
    priority                    TEXT,
    complexity                  TEXT,
    strategic_alignment         TEXT,
    quality_gate_id             TEXT,
    methodology                 TEXT,
    classification              TEXT,
    external_ref                TEXT,
    links                       TEXT,
    milestones                  TEXT,
    notes                       TEXT DEFAULT '{}',
    created_at                  TEXT,
    updated_at                  TEXT
);

-- QTCS dimension links for F16
CREATE TABLE IF NOT EXISTS project_quality (project_id TEXT NOT NULL, quality_id TEXT NOT NULL, PRIMARY KEY(project_id, quality_id));
CREATE TABLE IF NOT EXISTS project_time    (project_id TEXT NOT NULL, time_id    TEXT NOT NULL, PRIMARY KEY(project_id, time_id));
CREATE TABLE IF NOT EXISTS project_cost    (project_id TEXT NOT NULL, cost_id    TEXT NOT NULL, PRIMARY KEY(project_id, cost_id));
CREATE TABLE IF NOT EXISTS project_scope   (project_id TEXT NOT NULL, scope_id   TEXT NOT NULL, PRIMARY KEY(project_id, scope_id));


CREATE INDEX IF NOT EXISTS idx_projects_status ON projects(status);

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
    -- release_workflow_id removed in v5: F16 has no lifecycle workflow
    archived                    INTEGER DEFAULT 0,   -- 0=active 1=soft-deleted
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
    currency                    TEXT DEFAULT 'EUR',
    scope_statement             TEXT,
    scope_version               TEXT,
    scope_last_changed          TEXT,
    scope_change_reason         TEXT,
    scope_change_count          INTEGER DEFAULT 0,
    priority                    TEXT,
    complexity                  TEXT,
    strategic_alignment         TEXT,
    methodology                 TEXT,
    classification              TEXT,
    external_ref                TEXT,
    links                       TEXT,
    milestones                  TEXT,
    notes                       TEXT DEFAULT '{}',
    created_at                  TEXT,
    updated_at                  TEXT
);



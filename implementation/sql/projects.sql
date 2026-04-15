-- ============================================================
-- projects.db  --  F16 Projects, F22 Tasks, F18 Incidents,
--                  Milestones, Meetings, Dependencies,
--                  CommunicationPlan (one per F16/F22)
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

-- ── Projects (F16) ───────────────────────────────────────────
CREATE TABLE IF NOT EXISTS projects (
    project_id                  TEXT PRIMARY KEY,
    workflow_instance_id        TEXT,
    workflow_status             TEXT,
    workflow_current_state      TEXT,
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
    status                      TEXT DEFAULT 'draft',
    phase                       TEXT,
    start_date_planned          TEXT,
    start_date_actual           TEXT,
    end_date_planned            TEXT,
    end_date_actual             TEXT,
    duration_planned_days       INTEGER DEFAULT 0,
    duration_actual_days        INTEGER DEFAULT 0,
    schedule_variance_days      INTEGER DEFAULT 0,
    budget_planned              REAL    DEFAULT 0,
    budget_approved             REAL    DEFAULT 0,
    budget_committed            REAL    DEFAULT 0,
    budget_actual               REAL    DEFAULT 0,
    cost_variance               REAL    DEFAULT 0,
    cost_performance_index              REAL    DEFAULT 1,
    schedule_performance_index          REAL    DEFAULT 1,
    estimate_at_completion              REAL    DEFAULT 0,
    estimate_to_complete                REAL    DEFAULT 0,
    variance_at_completion              REAL    DEFAULT 0,
    communication_plan_id               TEXT,
    cpi                         REAL    DEFAULT 1,
    spi                         REAL    DEFAULT 1,
    earned_value                REAL    DEFAULT 0,
    planned_value               REAL    DEFAULT 0,
    actual_cost                 REAL    DEFAULT 0,
    eac                         REAL    DEFAULT 0,
    etc                         REAL    DEFAULT 0,
    vac                         REAL    DEFAULT 0,
    currency                    TEXT    DEFAULT 'EUR',
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
    notes                       TEXT    DEFAULT '{}',
    created_at                  TEXT,
    updated_at                  TEXT
);

-- ── Tasks (F22) ───────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS tasks (
    task_id                     TEXT PRIMARY KEY,
    workflow_instance_id        TEXT,
    workflow_status             TEXT,
    workflow_current_state      TEXT,
    reg_number                  TEXT,
    reg_dept                    TEXT,
    reg_sequence                INTEGER DEFAULT 0,
    reg_year                    INTEGER DEFAULT 0,
    project_id                  TEXT NOT NULL,
    parent_task_id              TEXT,
    assignee_id                 TEXT,
    assigned_by                 TEXT,
    task_code                   TEXT,
    title                       TEXT NOT NULL,
    description                 TEXT,
    task_type                   TEXT,
    status                      TEXT DEFAULT 'draft',
    priority                    TEXT,
    start_date_planned          TEXT,
    start_date_actual           TEXT,
    due_date_planned            TEXT,
    due_date_actual             TEXT,
    schedule_variance_days      INTEGER DEFAULT 0,
    effort_planned_hrs          REAL    DEFAULT 0,
    effort_actual_hrs           REAL    DEFAULT 0,
    effort_remaining_hrs        REAL    DEFAULT 0,
    percent_complete            INTEGER DEFAULT 0,
    cost_planned                REAL    DEFAULT 0,
    cost_actual                 REAL    DEFAULT 0,
    wbs_code                    TEXT,
    sprint_or_phase             TEXT,
    is_milestone                INTEGER DEFAULT 0,
    quality_criteria            TEXT,
    acceptance_criteria         TEXT,
    external_ref                TEXT,
    links                       TEXT,
    notes                       TEXT    DEFAULT '{}',
    created_at                  TEXT,
    updated_at                  TEXT
);

-- ── Incidents (F18) ───────────────────────────────────────────
CREATE TABLE IF NOT EXISTS incidents (
    incident_id                 TEXT PRIMARY KEY,
    workflow_instance_id        TEXT,
    workflow_status             TEXT,
    workflow_current_state      TEXT,
    reg_number                  TEXT,
    reg_dept                    TEXT,
    reg_sequence                INTEGER DEFAULT 0,
    reg_year                    INTEGER DEFAULT 0,
    project_id                  TEXT NOT NULL,
    task_id                     TEXT,
    risk_id                     TEXT,
    reported_by                 TEXT,
    owner_id                    TEXT,
    title                       TEXT NOT NULL,
    description                 TEXT,
    incident_type               TEXT,
    category                    TEXT,
    severity                    TEXT    DEFAULT 'medium',
    impact_area                 TEXT,
    status                      TEXT    DEFAULT 'open',
    occurred_date               TEXT,
    reported_date               TEXT,
    resolved_date               TEXT,
    root_cause                  TEXT,
    immediate_action            TEXT,
    corrective_action           TEXT,
    resolution                  TEXT,
    cost_impact                 REAL    DEFAULT 0,
    schedule_impact_days        INTEGER DEFAULT 0,
    scope_impact                TEXT,
    quality_impact              TEXT,
    escalated                   INTEGER DEFAULT 0,
    escalated_to                TEXT,
    links                       TEXT,
    notes                       TEXT    DEFAULT '{}',
    created_at                  TEXT,
    updated_at                  TEXT
);

-- ── Milestones ────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS milestones (
    milestone_id            TEXT PRIMARY KEY,
    workflow_instance_id    TEXT,
    workflow_status         TEXT,
    workflow_current_state  TEXT,
    project_id              TEXT NOT NULL,
    task_id                 TEXT,
    owner_id                TEXT,
    title                   TEXT NOT NULL,
    description             TEXT,
    milestone_type          TEXT DEFAULT 'internal',
    planned_date            TEXT,
    actual_date             TEXT,
    variance_days           INTEGER DEFAULT 0,
    status                  TEXT DEFAULT 'pending',
    criteria                TEXT,
    contractual             INTEGER DEFAULT 0,
    payment_trigger         INTEGER DEFAULT 0,
    links                   TEXT,
    notes                   TEXT DEFAULT '{}',
    created_at              TEXT
);

-- ── Meetings ─────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS meetings (
    meeting_id              TEXT PRIMARY KEY,
    workflow_instance_id    TEXT,
    workflow_status         TEXT,
    workflow_current_state  TEXT,
    task_id                 TEXT NOT NULL,
    project_id              TEXT,
    organiser_id            TEXT,
    title                   TEXT NOT NULL,
    meeting_type            TEXT,
    status                  TEXT DEFAULT 'scheduled',
    scheduled_date          TEXT,
    actual_date             TEXT,
    duration_mins           INTEGER DEFAULT 0,
    location                TEXT,
    channel                 TEXT,
    agenda                  TEXT,
    decisions               TEXT,
    actions                 TEXT,
    next_meeting_id         TEXT,
    links                   TEXT,
    notes                   TEXT DEFAULT '{}',
    created_at              TEXT
);

-- ── Communication Plan (ONE per project OR task) ──────────────
-- Enforced at model level: at most one plan per entity
CREATE TABLE IF NOT EXISTS communication_plans (
    plan_id         TEXT PRIMARY KEY,
    entity_type     TEXT NOT NULL CHECK(entity_type IN ('project','task')),
    entity_id       TEXT NOT NULL,
    title           TEXT NOT NULL DEFAULT 'Communication Plan',
    objectives      TEXT,
    audience        TEXT,
    frequency       TEXT,
    channels        TEXT,
    responsible_id  TEXT,
    status          TEXT DEFAULT 'active',
    review_date     TEXT,
    notes           TEXT DEFAULT '{}',
    created_at      TEXT,
    UNIQUE(entity_type, entity_id)
);

-- ── Communication Items (entries in the plan) ─────────────────
CREATE TABLE IF NOT EXISTS communication_items (
    item_id         TEXT PRIMARY KEY,
    plan_id         TEXT NOT NULL,
    title           TEXT NOT NULL,
    audience        TEXT,
    format          TEXT,
    frequency       TEXT,
    responsible_id  TEXT,
    due_date        TEXT,
    status          TEXT DEFAULT 'planned',
    notes           TEXT,
    created_at      TEXT
);

-- ── Project dependencies ──────────────────────────────────────
CREATE TABLE IF NOT EXISTS dependencies (
    dep_id            TEXT PRIMARY KEY,
    source_project_id TEXT,
    target_project_id TEXT,
    source_task_id    TEXT,
    target_task_id    TEXT,
    dep_type          TEXT,
    description       TEXT,
    created_at        TEXT
);

-- ── QTCS dimension links ──────────────────────────────────────
-- QTCS dimension links: 2-column format matching legacy model code
-- INSERT OR IGNORE INTO project_quality VALUES (project_id, quality_id)
CREATE TABLE IF NOT EXISTS project_quality (
    project_id TEXT NOT NULL, quality_id TEXT NOT NULL,
    PRIMARY KEY(project_id, quality_id));
CREATE TABLE IF NOT EXISTS project_time (
    project_id TEXT NOT NULL, time_id TEXT NOT NULL,
    PRIMARY KEY(project_id, time_id));
CREATE TABLE IF NOT EXISTS project_cost (
    project_id TEXT NOT NULL, cost_id TEXT NOT NULL,
    PRIMARY KEY(project_id, cost_id));
CREATE TABLE IF NOT EXISTS project_scope (
    project_id TEXT NOT NULL, scope_id TEXT NOT NULL,
    PRIMARY KEY(project_id, scope_id));
CREATE TABLE IF NOT EXISTS task_quality (
    task_id TEXT NOT NULL, quality_id TEXT NOT NULL,
    PRIMARY KEY(task_id, quality_id));
CREATE TABLE IF NOT EXISTS task_time (
    task_id TEXT NOT NULL, time_id TEXT NOT NULL,
    PRIMARY KEY(task_id, time_id));
CREATE TABLE IF NOT EXISTS task_cost (
    task_id TEXT NOT NULL, cost_id TEXT NOT NULL,
    PRIMARY KEY(task_id, cost_id));
CREATE TABLE IF NOT EXISTS task_scope (
    task_id TEXT NOT NULL, scope_id TEXT NOT NULL,
    PRIMARY KEY(task_id, scope_id));
CREATE TABLE IF NOT EXISTS incident_quality (
    incident_id TEXT NOT NULL, quality_id TEXT NOT NULL,
    PRIMARY KEY(incident_id, quality_id));
CREATE TABLE IF NOT EXISTS incident_time (
    incident_id TEXT NOT NULL, time_id TEXT NOT NULL,
    PRIMARY KEY(incident_id, time_id));
CREATE TABLE IF NOT EXISTS incident_cost (
    incident_id TEXT NOT NULL, cost_id TEXT NOT NULL,
    PRIMARY KEY(incident_id, cost_id));
CREATE TABLE IF NOT EXISTS incident_scope (
    incident_id TEXT NOT NULL, scope_id TEXT NOT NULL,
    PRIMARY KEY(incident_id, scope_id));

CREATE TABLE IF NOT EXISTS archived_projects (
    archive_id  TEXT PRIMARY KEY,
    project_id  TEXT NOT NULL,
    archived_at TEXT,
    reason      TEXT
);

CREATE INDEX IF NOT EXISTS idx_tasks_project    ON tasks(project_id);
CREATE INDEX IF NOT EXISTS idx_tasks_parent     ON tasks(parent_task_id);
CREATE INDEX IF NOT EXISTS idx_tasks_assignee   ON tasks(assignee_id);
CREATE INDEX IF NOT EXISTS idx_incidents_proj   ON incidents(project_id);
CREATE INDEX IF NOT EXISTS idx_milestones_proj  ON milestones(project_id);
CREATE INDEX IF NOT EXISTS idx_meetings_task    ON meetings(task_id);

-- ============================================================
-- f18.db  —  F18Operation + Communication
-- Note: F24 steps are stored in f24.db (see f24.sql)
--
-- Replaces:
--   - reporting.db (risks, measures, quality_gates,
--     lessons_learned_*, decision_log_*, assumption_constraints)
--   - tracking.db ChangeRequest + ChangeObject tables
--   - projects.db meetings table
--
-- Design:
--   One table f18_operations stores ALL workflow types.
--   The vorgangType column acts as a discriminator.
--   Type-specific fields are NULL when not applicable.
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

-- ── F18 Workflow (unified entity table) ──────────────────────
CREATE TABLE IF NOT EXISTS f18_operations (
    operation_id          TEXT PRIMARY KEY,
    operation_type        TEXT NOT NULL DEFAULT 'generic',
    task_id             TEXT NOT NULL,  -- → F22 (required: F18 always belongs to a task)
    parent_operation_id   TEXT,           -- → F18 (CO only, → ChangeRequest)
    release_workflow_id    TEXT,           -- WFI ID of controlling Main Workflow
    wf_locked      INTEGER DEFAULT 0, -- True while F77W is ACTIVE (mutations blocked)
    title               TEXT NOT NULL,
    description         TEXT,
    status              TEXT NOT NULL DEFAULT 'draft',
    owner_id            TEXT,
    priority            TEXT DEFAULT 'medium',

    -- incident fields
    incident_type       TEXT,
    severity            TEXT,
    occurred_date       TEXT,
    resolved_date       TEXT,
    root_cause          TEXT,
    immediate_action    TEXT,
    resolution          TEXT,
    cost_impact         REAL DEFAULT 0,
    schedule_impact_days INTEGER DEFAULT 0,
    scope_impact        TEXT,
    quality_impact      TEXT,

    -- risk fields
    risk_level          TEXT DEFAULT 'medium',
    probability_score   INTEGER DEFAULT 0,
    impact_score_time   INTEGER DEFAULT 0,
    impact_score_cost   INTEGER DEFAULT 0,
    impact_score_quality INTEGER DEFAULT 0,
    impact_score_scope  INTEGER DEFAULT 0,
    overall_risk_score  INTEGER DEFAULT 0,
    response_strategy   TEXT,
    contingency_plan    TEXT,
    trigger_condition   TEXT,
    residual_risk_level TEXT,
    cost_reserve        REAL DEFAULT 0,
    schedule_reserve_days INTEGER DEFAULT 0,

    -- measure fields
    measure_category    TEXT,
    planned_date        TEXT,
    actual_date         TEXT,
    effectiveness       TEXT,
    verification_method TEXT,
    verified_date       TEXT,
    verified_by         TEXT,

    -- quality gate fields
    phase               TEXT,
    criteria            TEXT,
    acceptance_criteria TEXT,
    findings            TEXT,
    gate_result         TEXT,
    gate_decision       TEXT,

    -- assumption/constraint fields
    ac_type             TEXT,           -- assumption|constraint
    validated_date      TEXT,
    validated_by        TEXT,
    impact              TEXT,

    -- communication plan fields
    audience            TEXT,
    frequency           TEXT,
    channel             TEXT,
    responsible         TEXT,

    -- lessons learned fields
    lesson_type         TEXT,
    recommendation      TEXT,
    applicable_phases   TEXT,

    -- decision log fields
    decision_type       TEXT,
    rationale           TEXT,
    decision_date       TEXT,
    decision_by         TEXT,
    alternatives_considered TEXT,

    -- change request fields
    change_type         TEXT,
    justification       TEXT,
    cr_impact           TEXT,
    raised_date         TEXT,
    cr_decision_date    TEXT,
    cr_decision_rationale TEXT,
    cr_schedule_impact_days INTEGER DEFAULT 0,

    -- change object fields
    executed_by         TEXT,
    execution_date      TEXT,

    -- common tail
    notes               TEXT DEFAULT '{}',
    links               TEXT,
    created_at          TEXT,
    updated_at          TEXT
);

CREATE INDEX IF NOT EXISTS idx_f18_task       ON f18_operations(task_id, operation_type);
CREATE INDEX IF NOT EXISTS idx_f18_parent     ON f18_operations(parent_operation_id);
CREATE INDEX IF NOT EXISTS idx_f18_status     ON f18_operations(status);
CREATE INDEX IF NOT EXISTS idx_f18_type       ON f18_operations(operation_type);

-- ── Communication (replaces meetings) ────────────────────────
CREATE TABLE IF NOT EXISTS communications (
    communication_id TEXT PRIMARY KEY,
    owner_id        TEXT NOT NULL,
    owner_type      TEXT NOT NULL
                         CHECK(owner_type IN ('f16','f22','f18','f24')),
    comm_type       TEXT DEFAULT 'meeting'
                         CHECK(comm_type IN ('message','call','meeting','email','report')),
    title           TEXT NOT NULL,
    agenda          TEXT,
    scheduled_date  TEXT,
    actual_date     TEXT,
    duration_mins   INTEGER DEFAULT 0,
    channel         TEXT,
    location        TEXT,
    organiser_id    TEXT,
    participants    TEXT DEFAULT '[]',
    decisions       TEXT,
    actions         TEXT DEFAULT '[]',
    notes           TEXT,
    status          TEXT DEFAULT 'scheduled'
                         CHECK(status IN ('scheduled','completed','cancelled')),
    created_at      TEXT,
    updated_at      TEXT
);

CREATE INDEX IF NOT EXISTS idx_comm_owner ON communications(owner_id, owner_type);
CREATE INDEX IF NOT EXISTS idx_comm_type  ON communications(comm_type);

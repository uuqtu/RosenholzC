-- ============================================================
-- f18.db  —  F18Operation + F18OperationStep + Communication
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

-- ── F18 Workflow Steps ────────────────────────────────────────
CREATE TABLE IF NOT EXISTS f18_operation_steps (
    step_id             TEXT PRIMARY KEY,
    operation_id          TEXT NOT NULL REFERENCES f18_operations(vorgang_id),
    tpl_step_id         TEXT,           -- soft ref to template step
    title               TEXT NOT NULL,
    description         TEXT,
    step_type           TEXT DEFAULT 'task',
    sequence_order      INTEGER DEFAULT 0,
    predecessor_step_ids TEXT,          -- comma-separated step IDs

    -- bookends
    is_initialize       INTEGER DEFAULT 0,
    is_final            INTEGER DEFAULT 0,

    -- assignment
    assigned_to         TEXT,
    required_role       TEXT,
    due_date            TEXT,
    started_date        TEXT,
    completed_date      TEXT,
    sla_hours           INTEGER DEFAULT 0,
    sla_breached        INTEGER DEFAULT 0,

    -- status & result
    status              TEXT DEFAULT 'pending'
                             CHECK(status IN ('pending','in_progress','waiting',
                                              'blocked','skipped','done')),

    -- is_free: free steps have no predecessor dependencies and can
    -- transition to any status at any time regardless of other steps.
    is_free             INTEGER DEFAULT 0,
    auto_approve        INTEGER DEFAULT 0,
    requires_comment    INTEGER DEFAULT 0,
    requires_document   INTEGER DEFAULT 0,
    decision            TEXT,
    decision_by         TEXT,
    decision_date       TEXT,
    comment             TEXT,

    -- Tracking — auto-computed from dates; not manually set
    -- tracking_status: planned|focused|due|in_work|archived (computed, stored for query)
    tracking_status     TEXT DEFAULT 'planned'
                             CHECK(tracking_status IN ('planned','focused','due','in_work','archived')),
    focus_date          TEXT,    -- must be before due_date; when passed → focused
    -- due_date already exists above
    priority            TEXT DEFAULT 'medium',
    assigned_to_group   TEXT,
    progress_note       TEXT,
    percent_complete    INTEGER DEFAULT 0,
    in_work_since       TEXT,    -- set when step is marked in_work

    notes               TEXT DEFAULT '{}',
    created_at          TEXT,
    updated_at          TEXT
);

CREATE INDEX IF NOT EXISTS idx_f18step_vorgang  ON f18_operation_steps(operation_id, sequence_order);
CREATE INDEX IF NOT EXISTS idx_f18step_status   ON f18_operation_steps(status);
CREATE INDEX IF NOT EXISTS idx_f18step_assigned ON f18_operation_steps(assigned_to);

-- ── Communication (replaces meetings) ────────────────────────
CREATE TABLE IF NOT EXISTS communications (
    communication_id TEXT PRIMARY KEY,
    owner_id        TEXT NOT NULL,
    owner_type      TEXT NOT NULL
                         CHECK(owner_type IN ('f16','f22','f18','f18step')),
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

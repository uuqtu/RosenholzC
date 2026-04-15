-- ============================================================
-- reporting.db  --  Risks, Measures, QualityGates,
--                   LessonsLearned, DecisionLog, 
--                   AssumptionConstraint (all with header+entries)
-- Version managed by schema_version table.
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name         TEXT PRIMARY KEY,
    version         INTEGER NOT NULL DEFAULT 1,
    applied_at      TEXT    NOT NULL,
    description     TEXT
);

-- ── Risks ────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS risks (
    risk_id                TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT NOT NULL,
    task_id                TEXT,
    owner_id               TEXT,
    identified_by          TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    category               TEXT,
    subcategory            TEXT,
    risk_type              TEXT,
    status                 TEXT DEFAULT 'open',
    risk_level             TEXT,
    probability            INTEGER DEFAULT 0,
    impact                 INTEGER DEFAULT 0,
    overall_risk_score     INTEGER DEFAULT 0,
    response_strategy      TEXT,
    contingency_plan       TEXT,
    trigger_condition      TEXT,
    early_warning          TEXT,
    residual_risk_level    TEXT,
    review_date            TEXT,
    closed_date            TEXT,
    escalated              INTEGER DEFAULT 0,
    escalated_to           TEXT,
    identified_date        TEXT,
    probability_score      INTEGER DEFAULT 0,
    impact_score_time      INTEGER DEFAULT 0,
    impact_score_cost      INTEGER DEFAULT 0,
    impact_score_quality   INTEGER DEFAULT 0,
    impact_score_scope     INTEGER DEFAULT 0,
    cost_reserve           REAL    DEFAULT 0,
    schedule_reserve_days  INTEGER DEFAULT 0,
    links                  TEXT,
    notes                  TEXT DEFAULT '{}',
    created_at             TEXT
);

-- ── Measures ─────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS measures (
    measure_id             TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    risk_id                TEXT,
    incident_id            TEXT,
    project_id             TEXT,
    task_id                TEXT,
    owner_id               TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    measure_type           TEXT,
    measure_category       TEXT,
    status                 TEXT DEFAULT 'planned',
    planned_date           TEXT,
    actual_date            TEXT,
    cost_planned           REAL  DEFAULT 0,
    cost_actual            REAL  DEFAULT 0,
    effort_hrs             REAL  DEFAULT 0,
    effectiveness          TEXT,
    verification_method    TEXT,
    verified_date          TEXT,
    verified_by            TEXT,
    outcome                TEXT,
    links                  TEXT,
    notes                  TEXT DEFAULT '{}',
    created_at             TEXT
);

-- ── Quality Gates ─────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS quality_gates (
    gate_id                TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    reviewer_id            TEXT,
    title                  TEXT NOT NULL,
    phase                  TEXT,
    planned_date           TEXT,
    actual_date            TEXT,
    criteria               TEXT,
    standards_applied      TEXT,
    quality_objectives     TEXT,
    acceptance_criteria    TEXT,
    audit_schedule         TEXT,
    tools_methods          TEXT,
    result                 TEXT DEFAULT 'pending',
    decision               TEXT,
    findings               TEXT,
    links                  TEXT,
    notes                  TEXT DEFAULT '{}',
    created_at             TEXT
);

-- ── Lessons Learned Header (one per F16 or F22) ───────────────
-- A project or task has at most ONE lessons-learned register.
CREATE TABLE IF NOT EXISTS lessons_learned_header (
    ll_header_id    TEXT PRIMARY KEY,
    entity_type     TEXT NOT NULL CHECK(entity_type IN ('project','task')),
    entity_id       TEXT NOT NULL,
    title           TEXT NOT NULL DEFAULT 'Lessons Learned Register',
    status          TEXT DEFAULT 'active',
    created_by      TEXT,
    created_at      TEXT,
    UNIQUE(entity_type, entity_id)
);

-- ── Lessons Learned Entries (many per header) ─────────────────
CREATE TABLE IF NOT EXISTS lessons_learned_entries (
    entry_id        TEXT PRIMARY KEY,
    ll_header_id    TEXT NOT NULL,
    title           TEXT NOT NULL,
    description     TEXT,
    category        TEXT,
    dimension       TEXT,
    identified_date TEXT,
    reviewed_date   TEXT,
    status          TEXT DEFAULT 'draft',
    impact          TEXT,
    recommendation  TEXT,
    action_taken    TEXT,
    submitted_by    TEXT,
    reviewed_by     TEXT,
    added_to_kb     INTEGER DEFAULT 0,
    tags            TEXT,
    links           TEXT,
    notes           TEXT DEFAULT '{}',
    created_at      TEXT
);

-- ── Decision Log Header (one per F16 or F22) ─────────────────
CREATE TABLE IF NOT EXISTS decision_log_header (
    dl_header_id    TEXT PRIMARY KEY,
    entity_type     TEXT NOT NULL CHECK(entity_type IN ('project','task')),
    entity_id       TEXT NOT NULL,
    title           TEXT NOT NULL DEFAULT 'Decision Log',
    status          TEXT DEFAULT 'active',
    created_by      TEXT,
    created_at      TEXT,
    UNIQUE(entity_type, entity_id)
);

-- ── Decision Log Entries (many per header) ────────────────────
CREATE TABLE IF NOT EXISTS decision_log_entries (
    entry_id            TEXT PRIMARY KEY,
    dl_header_id        TEXT NOT NULL,
    title               TEXT NOT NULL,
    description         TEXT,
    decision_type       TEXT,
    status              TEXT DEFAULT 'open',
    decided_by          TEXT,
    meeting_id          TEXT,
    decision_date       TEXT,
    review_date         TEXT,
    options_considered  TEXT,
    rationale           TEXT,
    impact_cost         TEXT,
    impact_schedule     TEXT,
    impact_scope        TEXT,
    impact_quality      TEXT,
    assumptions_made    TEXT,
    links               TEXT,
    notes               TEXT DEFAULT '{}',
    created_at          TEXT
);

-- ── Assumption/Constraint Header (one per F16 or F22) ────────
CREATE TABLE IF NOT EXISTS assumption_constraint_header (
    ac_header_id    TEXT PRIMARY KEY,
    entity_type     TEXT NOT NULL CHECK(entity_type IN ('project','task')),
    entity_id       TEXT NOT NULL,
    title           TEXT NOT NULL DEFAULT 'Assumptions & Constraints Register',
    status          TEXT DEFAULT 'active',
    created_by      TEXT,
    created_at      TEXT,
    UNIQUE(entity_type, entity_id)
);

-- ── Assumption/Constraint Entries (many per header) ──────────
CREATE TABLE IF NOT EXISTS assumption_constraint_entries (
    entry_id            TEXT PRIMARY KEY,
    ac_header_id        TEXT NOT NULL,
    title               TEXT NOT NULL,
    description         TEXT,
    ac_type             TEXT NOT NULL DEFAULT 'assumption',
    category            TEXT,
    dimension           TEXT,
    status              TEXT DEFAULT 'active',
    identified_date     TEXT,
    review_date         TEXT,
    validation_method   TEXT,
    validated_date      TEXT,
    validated_by        TEXT,
    breached            INTEGER DEFAULT 0,
    breached_date       TEXT,
    impact_if_wrong     TEXT,
    mitigation          TEXT,
    owner_id            TEXT,
    links               TEXT,
    notes               TEXT DEFAULT '{}',
    created_at          TEXT
);


CREATE TABLE IF NOT EXISTS decision_log (
    decision_id            TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    meeting_id             TEXT,
    decided_by             TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    decision_type          TEXT,
    status                 TEXT DEFAULT 'open',
    decision_date          TEXT,
    review_date            TEXT,
    options_considered     TEXT,
    rationale              TEXT,
    impact_cost            TEXT,
    impact_schedule        TEXT,
    impact_scope           TEXT,
    impact_quality         TEXT,
    assumptions_made       TEXT,
    links                  TEXT,
    notes                  TEXT DEFAULT '{}',
    created_at             TEXT
);

CREATE TABLE IF NOT EXISTS assumption_constraints (
    ac_id                  TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    task_id                TEXT,
    owner_id               TEXT,
    title                  TEXT NOT NULL,
    description            TEXT,
    ac_type                TEXT DEFAULT 'assumption',
    category               TEXT,
    dimension              TEXT,
    status                 TEXT DEFAULT 'active',
    identified_date        TEXT,
    review_date            TEXT,
    validation_method      TEXT,
    validated_date         TEXT,
    validated_by           TEXT,
    breached               INTEGER DEFAULT 0,
    breached_date          TEXT,
    impact_if_wrong        TEXT,
    mitigation             TEXT,
    links                  TEXT,
    notes                  TEXT DEFAULT '{}',
    created_at             TEXT
);

CREATE INDEX IF NOT EXISTS idx_risks_project  ON risks(project_id);
CREATE INDEX IF NOT EXISTS idx_measures_risk  ON measures(risk_id);
CREATE INDEX IF NOT EXISTS idx_measures_proj  ON measures(project_id);
CREATE INDEX IF NOT EXISTS idx_qg_project     ON quality_gates(project_id);
CREATE INDEX IF NOT EXISTS idx_ll_entries     ON lessons_learned_entries(ll_header_id);
CREATE INDEX IF NOT EXISTS idx_dl_entries     ON decision_log_entries(dl_header_id);

CREATE INDEX IF NOT EXISTS idx_ac_entries     ON assumption_constraint_entries(ac_header_id);

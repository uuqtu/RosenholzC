-- f24.sql  —  F24 step storage (XV/F24/NNNNNN/YY)
-- Separated from f18.sql so F24 has its own database file.
-- Relationship: each F24 step belongs to one F18 operation via operation_id.

CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT NOT NULL,
    version     INTEGER NOT NULL DEFAULT 0,
    applied_at  TEXT NOT NULL,
    note        TEXT,
    PRIMARY KEY (db_name)
);

CREATE TABLE IF NOT EXISTS f24_steps (
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
    start_date_planned  TEXT,    -- planned start date (for tracking)
    end_date_planned    TEXT,    -- planned end date (= planned due date)
    focus_date          TEXT,    -- auto-computed: midpoint of planned range
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

CREATE INDEX IF NOT EXISTS idx_f24_operation ON f24_steps(operation_id, sequence_order);
CREATE INDEX IF NOT EXISTS idx_f24_status    ON f24_steps(status);
CREATE INDEX IF NOT EXISTS idx_f24_assigned  ON f24_steps(assigned_to);

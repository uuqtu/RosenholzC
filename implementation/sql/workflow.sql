-- ============================================================
-- workflow.db  --  WorkflowEngine: Definitions, Templates,
--                  Instances, Actions (Steps), Participants,
--                  SLA, Notifications
--
-- Design principles:
--   - A Workflow Instance attaches to ANY entity (project/task/doc/...)
--   - Each Instance has ordered Actions (steps), execution type:
--       sequential | parallel | free
--   - Every Instance starts automatically with an 'initialize' action
--       (auto-approved on first engine tick)
--   - An Instance completes when all non-skipped actions are done
--   - Actions can hold documents (entity_documents table in documents.db)
--   - Actions are tracked like TrackableItems (status, dueDate, etc.)
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name         TEXT PRIMARY KEY,
    version         INTEGER NOT NULL DEFAULT 1,
    applied_at      TEXT    NOT NULL,
    description     TEXT
);

-- ── Workflow Template (reusable definition) ───────────────────
CREATE TABLE IF NOT EXISTS workflow_templates (
    template_id     TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    version         TEXT NOT NULL DEFAULT '1.0',
    description     TEXT,
    entity_types    TEXT,           -- comma-sep: 'project,task,document'
    execution_type  TEXT NOT NULL DEFAULT 'sequential'
                         CHECK(execution_type IN ('sequential','parallel','free')),
    sla_enforced    INTEGER DEFAULT 0,
    default_sla_hours INTEGER DEFAULT 0,
    status          TEXT DEFAULT 'active',
    created_by      TEXT,
    created_at      TEXT,
    updated_at      TEXT
);

-- ── Template Action Steps ─────────────────────────────────────
CREATE TABLE IF NOT EXISTS workflow_template_actions (
    tpl_action_id   TEXT PRIMARY KEY,
    template_id     TEXT NOT NULL,
    title           TEXT NOT NULL,
    description     TEXT,
    sequence_order  INTEGER DEFAULT 0,     -- for sequential
    execution_type  TEXT DEFAULT 'sequential'
                         CHECK(execution_type IN ('sequential','parallel','free')),
    predecessor_ids TEXT,                  -- comma-sep tpl_action_ids for sequential
    required_role   TEXT,
    sla_hours       INTEGER DEFAULT 0,
    auto_approve    INTEGER DEFAULT 0,     -- 1 = engine auto-approves (like initialize)
    is_initialize   INTEGER DEFAULT 0,     -- 1 = special auto-start step
    is_final        INTEGER DEFAULT 0,     -- 1 = last step (auto-close workflow)
    requires_decision_log_entry INTEGER DEFAULT 0,
    requires_lesson_learned_entry INTEGER DEFAULT 0,
    requires_comment INTEGER DEFAULT 0,
    notes           TEXT
);

-- ── Workflow Instance ─────────────────────────────────────────
CREATE TABLE IF NOT EXISTS workflow_instances (
    instance_id     TEXT PRIMARY KEY,
    template_id     TEXT,               -- soft ref, NULL if ad-hoc
    name            TEXT NOT NULL,      -- human label for this instance
    entity_type     TEXT NOT NULL,      -- 'project'|'task'|'document'|'incident'|'risk'|...
    entity_id       TEXT NOT NULL,
    execution_type  TEXT NOT NULL DEFAULT 'sequential'
                         CHECK(execution_type IN ('sequential','parallel','free')),
    status          TEXT NOT NULL DEFAULT 'active'
                         CHECK(status IN ('active','completed','cancelled','on_hold')),
    initiated_by    TEXT,
    initiated_date  TEXT,
    due_date        TEXT,
    completed_date  TEXT,
    sla_hours       INTEGER DEFAULT 0,
    sla_breached    INTEGER DEFAULT 0,
    sla_breach_date TEXT,
    escalated_to    TEXT,
    escalated_date  TEXT,
    priority        TEXT DEFAULT 'medium',
    outcome         TEXT,
    notes           TEXT DEFAULT '{}',
    created_at      TEXT,
    updated_at      TEXT
);

-- ── Workflow Actions (Steps) ──────────────────────────────────
-- This is the core: each step is both a workflow gate AND trackable
CREATE TABLE IF NOT EXISTS workflow_actions (
    action_id           TEXT PRIMARY KEY,
    instance_id         TEXT NOT NULL,
    tpl_action_id       TEXT,               -- soft ref to template action
    title               TEXT NOT NULL,
    description         TEXT,
    sequence_order      INTEGER DEFAULT 0,
    execution_type      TEXT DEFAULT 'sequential'
                             CHECK(execution_type IN ('sequential','parallel','free')),
    predecessor_action_ids TEXT,            -- comma-sep action_ids

    -- Status lifecycle: pending → in_progress → approved | rejected | skipped
    status              TEXT NOT NULL DEFAULT 'pending'
                             CHECK(status IN ('pending','in_progress','approved',
                                              'rejected','skipped','cancelled')),
    is_initialize       INTEGER DEFAULT 0,  -- auto-approved on engine tick
    is_final            INTEGER DEFAULT 0,  -- completion step

    -- Assignment & timing
    assigned_to         TEXT,               -- person_id
    required_role       TEXT,
    due_date            TEXT,
    started_date        TEXT,
    completed_date      TEXT,
    sla_hours           INTEGER DEFAULT 0,
    sla_breached        INTEGER DEFAULT 0,

    -- Decision / result
    decision            TEXT,               -- 'approved'|'rejected'|'info'|...
    decision_by         TEXT,
    decision_date       TEXT,
    comment             TEXT,

    -- Workflow step flags
    requires_comment    INTEGER DEFAULT 0,
    requires_document   INTEGER DEFAULT 0,
    auto_approve        INTEGER DEFAULT 0,
    requires_decision_log_entry  INTEGER DEFAULT 0,
    requires_lesson_learned_entry INTEGER DEFAULT 0,

    -- ise-cobra tracking state (replaces TrackableItem)
    tracking_status     TEXT DEFAULT 'planned',
    planned_date        TEXT,
    focus_date          TEXT,
    archived_date       TEXT,
    priority            TEXT DEFAULT 'medium',
    assigned_to_group   TEXT,
    progress_note       TEXT,
    percent_complete    INTEGER DEFAULT 0,

    notes               TEXT DEFAULT '{}',
    created_at          TEXT,
    updated_at          TEXT
);

-- ── Participants on an Instance ───────────────────────────────
CREATE TABLE IF NOT EXISTS workflow_participants (
    participant_id      TEXT PRIMARY KEY,
    instance_id         TEXT NOT NULL,
    person_id           TEXT NOT NULL,
    role                TEXT NOT NULL,   -- initiator|approver|reviewer|watcher|informed|delegate|escalation-target
    active              INTEGER DEFAULT 1,
    delegation_from     TEXT,            -- participant_id if delegated
    added_at            TEXT
);

-- ── SLA Log ───────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS workflow_sla_log (
    sla_log_id          TEXT PRIMARY KEY,
    instance_id         TEXT NOT NULL,
    action_id           TEXT,
    entered_date        TEXT,
    exited_date         TEXT,
    sla_hours_allowed   INTEGER DEFAULT 0,
    breached            INTEGER DEFAULT 0,
    breach_reason       TEXT
);

-- ── Notifications ─────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS workflow_notifications (
    notification_id     TEXT PRIMARY KEY,
    instance_id         TEXT NOT NULL,
    action_id           TEXT,
    recipient_id        TEXT,
    notification_type   TEXT,
    channel             TEXT DEFAULT 'console',
    subject             TEXT,
    body                TEXT,
    scheduled_date      TEXT,
    sent                INTEGER DEFAULT 0,
    sent_date           TEXT,
    acknowledged        INTEGER DEFAULT 0,
    acknowledged_date   TEXT
);

CREATE INDEX IF NOT EXISTS idx_wi_entity       ON workflow_instances(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_wi_status       ON workflow_instances(status);
CREATE INDEX IF NOT EXISTS idx_wa_instance     ON workflow_actions(instance_id);
CREATE INDEX IF NOT EXISTS idx_wa_status       ON workflow_actions(status);
CREATE INDEX IF NOT EXISTS idx_wp_instance     ON workflow_participants(instance_id);
CREATE INDEX IF NOT EXISTS idx_wn_instance     ON workflow_notifications(instance_id);
CREATE INDEX IF NOT EXISTS idx_wsl_instance    ON workflow_sla_log(instance_id);

CREATE INDEX IF NOT EXISTS idx_tpl_action      ON workflow_template_actions(template_id);

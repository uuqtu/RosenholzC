-- ============================================================
-- f77.db — F77 Freigabe-Workflow Engine
--
-- Design:
--   F77_WorkflowTemplate   : declarative template (admin-time)
--   F77_WorkflowTemplateStep: steps in the template (declarative)
--   F77_Workflow           : running instance (snapshot of template at start)
--   F77_WorkflowStep       : running steps — each backed by an F18_Operation
--
-- Key principles:
--   - Templates are declarative; changes do not affect running workflows.
--   - On start, the template is snapshotted into F77_Workflow + F77_WorkflowStep.
--   - Each F77_WorkflowStep spawns one F18_Operation of type 'f77_step'.
--   - Steps can be sequential or parallel with predecessor wait conditions.
--   - Init step is auto-approved. End step fires automatically when all mid-steps done.
--   - Target state (pre_released/released/locked/closed/in_work) set on the template
--     and applied to the entity when End fires.
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name         TEXT PRIMARY KEY,
    version         INTEGER NOT NULL DEFAULT 1,
    applied_at      TEXT    NOT NULL,
    description     TEXT
);

-- ── F77_WorkflowTemplate ─────────────────────────────────────
-- Declarative; edited only at admin time, not at runtime.
CREATE TABLE IF NOT EXISTS f77_workflow_templates (
    template_id     TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    version         TEXT NOT NULL DEFAULT '1.0',
    description     TEXT,
    entity_types    TEXT,           -- comma-sep: 'f16,f22,f18,akt'
    target_state    TEXT NOT NULL DEFAULT 'released'
                         CHECK(target_state IN ('in_work','pre_released','released','locked','closed')),
    status          TEXT NOT NULL DEFAULT 'active'
                         CHECK(status IN ('active','inactive')),
    created_by      TEXT,
    created_at      TEXT NOT NULL,
    updated_at      TEXT NOT NULL
);

-- ── F77_WorkflowTemplateStep ──────────────────────────────────
-- Declarative steps. Each becomes one F77_WorkflowStep (and one F18_Operation) at runtime.
CREATE TABLE IF NOT EXISTS f77_workflow_template_steps (
    tpl_step_id         TEXT PRIMARY KEY,
    template_id         TEXT NOT NULL REFERENCES f77_workflow_templates(template_id),
    title               TEXT NOT NULL,
    description         TEXT,
    sequence_order      INTEGER NOT NULL DEFAULT 0,
    is_initialize       INTEGER NOT NULL DEFAULT 0,  -- 1 = auto-approved Init bookend
    is_final            INTEGER NOT NULL DEFAULT 0,  -- 1 = End bookend
    execution_mode      TEXT NOT NULL DEFAULT 'sequential'
                             CHECK(execution_mode IN ('sequential','parallel')),
    predecessor_tpl_step_ids TEXT,   -- comma-sep tpl_step_ids (for parallel/conditional)
    -- Step assignment / SLA
    required_role       TEXT,
    sla_hours           INTEGER DEFAULT 0,
    auto_approve        INTEGER NOT NULL DEFAULT 0,  -- 1 = engine auto-approves
    requires_comment    INTEGER NOT NULL DEFAULT 0,
    requires_document   INTEGER NOT NULL DEFAULT 0,
    created_at          TEXT NOT NULL,
    updated_at          TEXT NOT NULL
);

-- ── F77_Workflow ──────────────────────────────────────────────
-- Running instance — snapshot of the template at start time.
-- Template changes do not affect this record.
CREATE TABLE IF NOT EXISTS f77_workflows (
    workflow_id         TEXT PRIMARY KEY,
    template_id         TEXT,           -- reference only; template may change
    template_name       TEXT NOT NULL,  -- snapshot of template name at start
    entity_type         TEXT NOT NULL   -- 'f16','f22','f18','akt'
                             CHECK(entity_type IN ('f16','f22','f18','akt')),
    entity_id           TEXT NOT NULL,
    target_state        TEXT NOT NULL DEFAULT 'released'
                             CHECK(target_state IN ('in_work','pre_released','released','locked','closed')),
    status              TEXT NOT NULL DEFAULT 'active'
                             CHECK(status IN ('active','completed','cancelled','locked')),
    initiated_by        TEXT,           -- Person-ID or 'system'
    initiated_date      TEXT NOT NULL,
    completed_date      TEXT,
    notes               TEXT DEFAULT '{}',
    created_at          TEXT NOT NULL,
    updated_at          TEXT NOT NULL
);

-- ── F77_WorkflowStep ─────────────────────────────────────────
-- Running step — snapshot of a template step, linked to one F18_Operation.
-- The F18_Operation IS the step; its status drives the F77 step status.
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




-- ── Change Request Header ─────────────────────────────────────
-- A CR aggregates related changes, can attach F22s, Docs, Measures, Incidents
-- Gets a Workflow Instance and when approved, spawns a CO
CREATE TABLE IF NOT EXISTS change_requests (
    cr_id                   TEXT PRIMARY KEY,
    workflow_instance_id    TEXT,
    workflow_status         TEXT,
    workflow_current_state  TEXT,
    project_id              TEXT,
    task_id                 TEXT,
    raised_by               TEXT,
    title                   TEXT NOT NULL,
    description             TEXT,
    change_type             TEXT DEFAULT 'general',
    status                  TEXT DEFAULT 'draft'
                                 CHECK(status IN ('draft','submitted','under-review',
                                                  'approved','rejected','implemented',
                                                  'withdrawn')),
    priority                TEXT DEFAULT 'medium',
    raised_date             TEXT,
    decision_date           TEXT,
    implemented_date        TEXT,
    cost_impact             REAL DEFAULT 0,
    schedule_impact_days    INTEGER DEFAULT 0,
    scope_impact            TEXT,
    quality_impact          TEXT,
    justification           TEXT,
    decision_rationale      TEXT,
    co_id                   TEXT,   -- Change Object (execution workflow)
    links                   TEXT,
    notes                   TEXT DEFAULT '{}',
    created_at              TEXT
);

-- ── CR Attachments (links CR to other entities) ───────────────
CREATE TABLE IF NOT EXISTS cr_attachments (
    attachment_id   TEXT PRIMARY KEY,
    cr_id           TEXT NOT NULL,
    entity_type     TEXT NOT NULL,   -- 'task'|'document'|'measure'|'incident'
    entity_id       TEXT NOT NULL,
    relationship    TEXT DEFAULT 'affected',
    notes           TEXT,
    added_at        TEXT
);

-- ── Change Object (CO = execution workflow for approved CR) ───
-- A CO IS essentially a Workflow Instance, but we record the link here
CREATE TABLE IF NOT EXISTS change_objects (
    co_id               TEXT PRIMARY KEY,
    cr_id               TEXT NOT NULL,
    workflow_instance_id TEXT,       -- the actual execution workflow
    title               TEXT NOT NULL,
    status              TEXT DEFAULT 'planned',
    planned_date        TEXT,
    actual_date         TEXT,
    executed_by         TEXT,
    notes               TEXT DEFAULT '{}',
    created_at          TEXT
);

-- ── QTCS Dimension links ──────────────────────────────────────
CREATE TABLE IF NOT EXISTS quality_dimensions (
    dim_id      TEXT PRIMARY KEY,
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    title       TEXT,
    description TEXT,
    created_at  TEXT
);
CREATE TABLE IF NOT EXISTS time_dimensions (
    dim_id      TEXT PRIMARY KEY,
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    title       TEXT,
    description TEXT,
    created_at  TEXT
);
CREATE TABLE IF NOT EXISTS cost_dimensions (
    dim_id      TEXT PRIMARY KEY,
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    title       TEXT,
    description TEXT,
    created_at  TEXT
);
CREATE TABLE IF NOT EXISTS scope_dimensions (
    dim_id      TEXT PRIMARY KEY,
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    title       TEXT,
    description TEXT,
    created_at  TEXT
);

CREATE INDEX IF NOT EXISTS idx_cr_project    ON change_requests(project_id);
CREATE INDEX IF NOT EXISTS idx_cr_attach     ON cr_attachments(cr_id);


CREATE INDEX IF NOT EXISTS idx_co_cr         ON change_objects(cr_id);

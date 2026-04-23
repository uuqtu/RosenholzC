CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS documents (
    document_id            TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    release_workflow_id       TEXT,
    project_id             TEXT,
    task_id                TEXT,
    f18_operation_id       TEXT,   -- F18 Operation reference
    f18_step_id            TEXT,   -- F18 Operation Step reference
    author_id              TEXT,
    approved_by            TEXT,
    doc_type               TEXT,
    doc_category           TEXT,
    title                  TEXT NOT NULL,
    version                TEXT DEFAULT '1.0',
    date_created           TEXT,
    date_modified          TEXT,
    date_approved          TEXT,
    date_expires           TEXT,
    status                 TEXT DEFAULT 'draft',
    classification         TEXT,
    volume_number          INTEGER DEFAULT 1,
    page_count             INTEGER,
    language               TEXT DEFAULT 'EN',
    format                 TEXT,
    file_path              TEXT,
    file_size              INTEGER DEFAULT 0,
    file_hash              TEXT,
    file_url               TEXT,
    external_ref           TEXT,
    storage_system         TEXT,
    tags                   TEXT,
    summary                TEXT,
    links                  TEXT,
    notes                  TEXT,   -- JSON
    created_at             TEXT DEFAULT (datetime('now')),
    updated_at             TEXT DEFAULT (datetime('now'))
);

-- Polymorphic attachment: any entity can reference any document
CREATE TABLE IF NOT EXISTS entity_documents (
    link_id     TEXT    PRIMARY KEY,
    entity_type TEXT    NOT NULL,
    entity_id   TEXT    NOT NULL,
    document_id TEXT    NOT NULL,
    relationship TEXT   DEFAULT 'attached',
    notes       TEXT,
    linked_at   TEXT
);

CREATE INDEX IF NOT EXISTS idx_docs_project  ON documents(project_id);
CREATE INDEX IF NOT EXISTS idx_docs_task     ON documents(task_id);
CREATE INDEX IF NOT EXISTS idx_ent_docs_key  ON entity_documents(entity_type, entity_id);

-- ── Dokument-Versionen ─────────────────────────────────────────
-- Jede gespeicherte Version eines Dokuments (vor Änderung)


-- ── Document Revisions ───────────────────────────────────────
-- Each document has N revisions identified by (document_id, rev).
-- Exactly one revision per document_id holds superseded = 0 (false).
-- That revision is the "current active revision".
--
-- State machine:
--   in_work → pre_released, locked, closed
--   pre_released → released, locked, closed, in_work
--   released → locked, closed   (immutable)
--   locked → pre_released (newest only), closed
--   closed → [terminal]
CREATE TABLE IF NOT EXISTS document_revisions (
    document_id     TEXT    NOT NULL,
    rev             INTEGER NOT NULL DEFAULT 1,
    parent_rev      INTEGER DEFAULT 0,
    rev_state       TEXT    NOT NULL DEFAULT 'in_work'
                            CHECK(rev_state IN ('in_work','pre_released','released','locked','closed')),
    superseded      INTEGER NOT NULL DEFAULT 1,  -- 0 = current active revision
    content_hash    TEXT,           -- SHA-256 of chunk in LMDB (empty until committed)
    content_size    INTEGER DEFAULT 0,
    created_by      TEXT,
    change_note     TEXT,
    created_at      TEXT,
    updated_at      TEXT,
    PRIMARY KEY (document_id, rev)
);

CREATE INDEX IF NOT EXISTS idx_docrev_current
    ON document_revisions(document_id, superseded);
CREATE INDEX IF NOT EXISTS idx_docrev_state
    ON document_revisions(document_id, rev_state);
-- ── Dokument-Datei-Metadaten ────────────────────────────────────
-- Ergänzt documents-Tabelle um Datei-spezifische Informationen

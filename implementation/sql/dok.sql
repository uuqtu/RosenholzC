CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS documents (
    -- Core identity
    document_id            TEXT PRIMARY KEY,
    release_workflow_id    TEXT,               -- F77 Main-Workflow instance
    project_id             TEXT,               -- Mandatory: filing parent
    task_id                TEXT,               -- Optional: task-scoped

    -- Context links
    f18_operation_id       TEXT,               -- F18 Operation reference
    f18_step_id            TEXT,               -- F18 Step reference

    -- Authorship
    author_id              TEXT,
    approved_by            TEXT,

    -- Classification
    doc_type               TEXT,               -- report|specification|contract|...
    doc_category           TEXT,
    title                  TEXT NOT NULL,
    version                TEXT DEFAULT '1.0',

    -- Dates
    date_created           TEXT,
    date_modified          TEXT,
    date_approved          TEXT,
    date_expires           TEXT,

    -- Properties (status removed — computed from DocumentRevision.revState)
    classification         TEXT,
    volume_number          INTEGER DEFAULT 1,
    page_count             INTEGER,
    language               TEXT DEFAULT 'EN',
    format                 TEXT,

    -- File tracking
    file_path              TEXT,
    file_size              INTEGER DEFAULT 0,
    file_hash              TEXT,
    file_url               TEXT,
    external_ref           TEXT,

    -- Metadata
    tags                   TEXT,
    summary                TEXT,
    links                  TEXT,
    notes                  TEXT,               -- JSON
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

-- ── Document Objects ─────────────────────────────────────────────────────
-- A DocumentRevision is a container for one or more physical files (objects).
-- Each object belongs to exactly one (document_id, rev) pair.
-- Objects in in_work state reside in MFS; on workflow execution the
-- "Create DB Objects" step commits them to LMDB.
--
-- Naming convention:  {docRegNr}_{revNNN}_{originalFilename}
-- Example:            XV_DOK_0018_2026_r001_tests-example.xls
CREATE TABLE IF NOT EXISTS document_objects (
    -- PK: documentId + ":" + 5-char objectId  e.g. "XV/DOK/0018/2026:A1B2C"
    object_id       TEXT    PRIMARY KEY,
    document_id     TEXT    NOT NULL,       -- XV/DOK/0018/2026
    rev             INTEGER NOT NULL,       -- revision number (1,2,3,...)
    original_name   TEXT    NOT NULL,       -- original filename as uploaded
    mfs_filename    TEXT,                   -- {docRegNr}_{objectId}_r{revNr}.{ext}
    mfs_path        TEXT,                   -- full MFS path (in_work only)
    content_hash    TEXT,                   -- SHA-256 in LMDB (when committed)
    content_size    INTEGER DEFAULT 0,
    format          TEXT,                   -- file extension without dot
    committed       INTEGER NOT NULL DEFAULT 0,  -- 1 = in LMDB, 0 = MFS only
    created_at      TEXT DEFAULT (datetime('now')),
    updated_at      TEXT DEFAULT (datetime('now')),
    FOREIGN KEY (document_id, rev) REFERENCES document_revisions(document_id, rev)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_doc_objects_docrev
    ON document_objects (document_id, rev);

CREATE TABLE IF NOT EXISTS document_objects (
    -- Primary key: documentId + ":" + 5-char alphanumeric objectId
    -- e.g. "XV/DOK/0018/2026:A1B2C"
    -- objectId is unique per document, not globally. Same ID can exist in different documents.
    object_id       TEXT    PRIMARY KEY,

    document_id     TEXT    NOT NULL,       -- XV/DOK/0018/2026
    rev             INTEGER NOT NULL,       -- revision number in which this object was created
    original_name   TEXT    NOT NULL,       -- original filename as uploaded (for display/decryption)
    mfs_filename    TEXT,                   -- MFS filename: {docRegNr}_{objectId}_r{revNr}.{ext}
    mfs_path        TEXT,                   -- full MFS path (valid in in_work state)
    content_hash    TEXT,                   -- SHA-256 in LMDB after commit (committed=1)
    content_size    INTEGER DEFAULT 0,
    format          TEXT,                   -- file extension without dot (e.g. "xls")
    committed       INTEGER NOT NULL DEFAULT 0,  -- 0=MFS only, 1=in LMDB
    created_at      TEXT DEFAULT (datetime('now')),
    updated_at      TEXT DEFAULT (datetime('now')),
    FOREIGN KEY (document_id, rev) REFERENCES document_revisions(document_id, rev)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS document_object_sequences (
    -- Tracks the next objectId sequence number per document.
    -- objectId is generated as base-36 zero-padded to 5 chars.
    document_id     TEXT    PRIMARY KEY,
    next_seq        INTEGER NOT NULL DEFAULT 1
);
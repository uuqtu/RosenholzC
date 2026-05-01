CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS folders (
    -- Core identity
    folder_id              TEXT PRIMARY KEY,
    workflow_id            TEXT,               -- F77W controlling this folder lifecycle
    task_id                TEXT,               -- Filing parent (F22)

    -- Context links
    f18_operation_id       TEXT,               -- F18 Operation reference
    f18_step_id            TEXT,               -- F18 Step reference

    -- Authorship
    author_id              TEXT,
    approved_by            TEXT,

    -- Classification
    doc_type               TEXT,               -- report|specification|contract|general|...
    doc_category           TEXT,
    title                  TEXT NOT NULL,
    version                TEXT DEFAULT '1.0',

    -- Dates
    date_created           TEXT,
    date_modified          TEXT,
    date_approved          TEXT,
    date_expires           TEXT,

    -- Properties
    classification         TEXT,
    volume_number          INTEGER DEFAULT 1,
    page_count             INTEGER DEFAULT 0,
    language               TEXT DEFAULT 'EN',
    format                 TEXT,

    -- File tracking
    file_path              TEXT,
    file_size              INTEGER DEFAULT 0,
    file_hash              TEXT,
    external_ref           TEXT,

    -- Metadata
    tags                   TEXT,
    summary                TEXT,
    links                  TEXT,
    notes                  TEXT,               -- JSON
    created_at             TEXT DEFAULT (datetime('now')),
    updated_at             TEXT DEFAULT (datetime('now'))
);

-- Polymorphic attachment: any entity can reference any folder
CREATE TABLE IF NOT EXISTS entity_folders (
    link_id      TEXT    PRIMARY KEY,
    entity_type  TEXT    NOT NULL,
    entity_id    TEXT    NOT NULL,
    folder_id    TEXT    NOT NULL,
    relationship TEXT    DEFAULT 'attached',
    notes        TEXT,
    linked_at    TEXT
);

CREATE INDEX IF NOT EXISTS idx_folders_task     ON folders(task_id);
CREATE INDEX IF NOT EXISTS idx_entity_folders   ON entity_folders(entity_type, entity_id);

-- ── Folder Revisions ─────────────────────────────────────────
CREATE TABLE IF NOT EXISTS folder_revisions (
    folder_id       TEXT    NOT NULL,
    rev             INTEGER NOT NULL DEFAULT 1,
    parent_rev      INTEGER DEFAULT 0,
    rev_state       TEXT    NOT NULL DEFAULT 'in_work'
                            CHECK(rev_state IN ('in_work','pre_released','released','locked','closed')),
    superseded      INTEGER NOT NULL DEFAULT 1,
    content_hash    TEXT,
    content_size    INTEGER DEFAULT 0,
    created_by      TEXT,
    change_note     TEXT,
    created_at      TEXT,
    updated_at      TEXT,
    PRIMARY KEY (folder_id, rev)
);

CREATE INDEX IF NOT EXISTS idx_folderrev_current ON folder_revisions(folder_id, superseded);
CREATE INDEX IF NOT EXISTS idx_folderrev_state   ON folder_revisions(folder_id, rev_state);

-- ── Folder Objects ───────────────────────────────────────────
CREATE TABLE IF NOT EXISTS folder_objects (
    object_id       TEXT    PRIMARY KEY,
    folder_id       TEXT    NOT NULL,
    rev             INTEGER NOT NULL,
    original_name   TEXT    NOT NULL,
    stored_file_name TEXT,
    file_path       TEXT,
    content_hash    TEXT,
    content_size    INTEGER DEFAULT 0,
    format          TEXT,
    source_url      TEXT,
    display_name    TEXT,
    description     TEXT,
    committed       INTEGER NOT NULL DEFAULT 0,
    created_at      TEXT DEFAULT (datetime('now')),
    updated_at      TEXT DEFAULT (datetime('now')),
    FOREIGN KEY (folder_id, rev) REFERENCES folder_revisions(folder_id, rev)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_folder_objects_folderrev ON folder_objects (folder_id, rev);

CREATE TABLE IF NOT EXISTS folder_sequences (
    folder_id   TEXT    PRIMARY KEY,
    next_seq    INTEGER NOT NULL DEFAULT 1
);

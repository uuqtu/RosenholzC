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
    project_id             TEXT,
    task_id                TEXT,
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

CREATE TABLE IF NOT EXISTS communication_plans (
    plan_id                TEXT PRIMARY KEY,
    workflow_instance_id   TEXT,
    workflow_status        TEXT,
    workflow_current_state TEXT,
    project_id             TEXT,
    owner_id               TEXT,
    version                TEXT DEFAULT '1.0',
    created_date           TEXT,
    approved_date          TEXT,
    objectives             TEXT,
    escalation_path        TEXT,
    links                  TEXT,
    notes                  TEXT
);

CREATE TABLE IF NOT EXISTS communication_items (
    item_id              TEXT PRIMARY KEY,
    plan_id              TEXT NOT NULL,
    sender_id            TEXT,
    audience             TEXT,
    title                TEXT,
    content_description  TEXT,
    format               TEXT,
    channel              TEXT,
    frequency            TEXT,
    next_due_date        TEXT,
    last_sent_date       TEXT,
    status               TEXT DEFAULT 'active',
    links                TEXT,
    notes                TEXT
);

CREATE INDEX IF NOT EXISTS idx_docs_project  ON documents(project_id);
CREATE INDEX IF NOT EXISTS idx_docs_task     ON documents(task_id);
CREATE INDEX IF NOT EXISTS idx_ent_docs_key  ON entity_documents(entity_type, entity_id);

-- ── Dokument-Versionen ─────────────────────────────────────────
-- Jede gespeicherte Version eines Dokuments (vor Änderung)
CREATE TABLE IF NOT EXISTS document_versions (
    version_id      TEXT PRIMARY KEY,
    document_id     TEXT NOT NULL REFERENCES documents(document_id),
    version_number  TEXT NOT NULL,          -- z.B. "1.0", "1.1", "2.0"
    file_path       TEXT,                   -- Pfad zur versionierten Datei
    file_size       INTEGER DEFAULT 0,
    file_hash       TEXT,                   -- SHA-256 Prüfsumme
    created_by      TEXT,
    change_note     TEXT,
    created_at      TEXT
);

CREATE INDEX IF NOT EXISTS idx_doc_versions ON document_versions(document_id, version_number);

-- ── Dokument-Datei-Metadaten ────────────────────────────────────
-- Ergänzt documents-Tabelle um Datei-spezifische Informationen

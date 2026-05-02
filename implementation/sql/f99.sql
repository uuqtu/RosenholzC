-- ============================================================
-- f99.sql  —  F99 Notizen fuer alle Entitaetstypen
-- ============================================================

CREATE TABLE IF NOT EXISTS f99_entries (
    f99_id          TEXT PRIMARY KEY,     -- XV/F99/000001/26  (6-digit)
    entity_type     TEXT NOT NULL,        -- f16|f22|f18|akt|f77|per
    entity_id       TEXT NOT NULL,        -- the entity full ID
    created_at      TEXT NOT NULL,        -- ISO-8601 timestamp
    author          TEXT,                 -- person_id or free name (optional)
    body            TEXT NOT NULL         -- note content (plain text)
);

CREATE INDEX IF NOT EXISTS idx_f99_entity
    ON f99_entries (entity_type, entity_id, created_at);

CREATE INDEX IF NOT EXISTS idx_f99_body
    ON f99_entries (body);

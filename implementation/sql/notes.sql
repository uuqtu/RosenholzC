-- ============================================================
-- notes.sql  —  Structured notes for all entity types
-- ============================================================

-- ── note_entries ──────────────────────────────────────────────
-- One row per note. entity_type + entity_id form the owner ref.
-- author is optional (person ID or free text name).
CREATE TABLE IF NOT EXISTS note_entries (
    note_id         TEXT PRIMARY KEY,     -- XV/NOTE/0001/26
    entity_type     TEXT NOT NULL,        -- f16|f22|f18|akt|f77|per
    entity_id       TEXT NOT NULL,        -- the entity's ID
    created_at      TEXT NOT NULL,        -- ISO-8601 timestamp
    author          TEXT,                 -- person_id or free name
    body            TEXT NOT NULL         -- note content (plain text)
);

CREATE INDEX IF NOT EXISTS idx_notes_entity
    ON note_entries (entity_type, entity_id, created_at);

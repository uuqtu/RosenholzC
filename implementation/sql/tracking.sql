-- ============================================================
-- tracking.db — Reserviert fuer zukuenftige Nutzung
-- ============================================================

CREATE TABLE IF NOT EXISTS schema_version (
    db_name     TEXT PRIMARY KEY,
    version     INTEGER NOT NULL DEFAULT 1,
    applied_at  TEXT NOT NULL,
    description TEXT
);

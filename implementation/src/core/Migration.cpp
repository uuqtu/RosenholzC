// ============================================================
// Migration.cpp  —  Schema migration engine implementation
// ============================================================
#include "Migration.h"
#include "Logger.h"
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace Rosenholz {

namespace {
std::string nowIsoMig() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream o;
    o << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return o.str();
}
} // anonymous

// ------------------------------
// targetVersionMap
//
// Returns the canonical target schema version for each database.
// Used by runAll() to determine which deltas need to run.
//
// Increment a version here AND add the corresponding delta
// in registry() when changing a database schema.
// ------------------------------
std::map<std::string, int> MigrationEngine::targetVersionMap() {
    return {
        {"core",      SchemaVersions::core},
        {"projects",  SchemaVersions::projects},
        {"workflow",  SchemaVersions::workflow},
        {"documents", SchemaVersions::documents},
        {"tracking",  SchemaVersions::tracking},
        {"reporting", SchemaVersions::reporting},
    };
}


// ------------------------------
// registry
//
// Returns all migration deltas in chronological order.
//
// Each Migration has:
//   dbName      : the target database ("core", "workflow", …)
//   toVersion   : the version this delta produces
//   description : human-readable change summary
//   sql         : one or more SQL statements to execute
//
// Rules:
//   - Deltas are applied inside a transaction; error → rollback
//   - Only deltas with toVersion > currentVersion are applied
//   - For fresh databases the schema files cover all tables;
//     deltas handle ALTER TABLE for existing databases only
// ------------------------------
std::vector<Migration> MigrationEngine::registry() {
    return {
        // ── core v1 (initial) ────────────────────────────────
        // No delta needed — schema applied fresh via .sql files

        // ── workflow v2 ──────────────────────────────────────
        // Added workflow_template_actions table (was missing in v1)
        {
            "workflow", 2,
            "Add workflow_template_actions table",
            R"SQL(
                CREATE TABLE IF NOT EXISTS workflow_template_actions (
                    tpl_action_id   TEXT PRIMARY KEY,
                    template_id     TEXT NOT NULL,
                    title           TEXT NOT NULL,
                    description     TEXT,
                    sequence_order  INTEGER DEFAULT 0,
                    execution_type  TEXT DEFAULT 'sequential',
                    predecessor_ids TEXT,
                    required_role   TEXT,
                    sla_hours       INTEGER DEFAULT 0,
                    auto_approve    INTEGER DEFAULT 0,
                    is_initialize   INTEGER DEFAULT 0,
                    is_final        INTEGER DEFAULT 0,
                    requires_decision_log_entry INTEGER DEFAULT 0,
                    requires_lesson_learned_entry INTEGER DEFAULT 0,
                    requires_comment INTEGER DEFAULT 0,
                    notes           TEXT
                );
            )SQL"
        },

    
        // ── documents v2 ────────────────────────────────────
        // Added document_versions table + file_size/file_hash columns
        {
            "documents", 2,
            "Add document_versions table and file metadata",
            R"SQL(
                CREATE TABLE IF NOT EXISTS document_versions (
                    version_id      TEXT PRIMARY KEY,
                    document_id     TEXT NOT NULL,
                    version_number  TEXT NOT NULL,
                    file_path       TEXT,
                    file_size       INTEGER DEFAULT 0,
                    file_hash       TEXT,
                    created_by      TEXT,
                    change_note     TEXT,
                    created_at      TEXT
                );
                CREATE INDEX IF NOT EXISTS idx_doc_versions
                    ON document_versions(document_id, version_number);
                -- Add columns to documents (SQLite ignores if already exists)
                ALTER TABLE documents ADD COLUMN file_size INTEGER DEFAULT 0;
                ALTER TABLE documents ADD COLUMN file_hash TEXT;
            )SQL"
        },
        // ── workflow v3 ──────────────────────────────────────
        // Added ise-cobra tracking fields to workflow_actions
        {
            "workflow", 3,
            "Add ise-cobra tracking state to workflow_actions",
            R"SQL(
                ALTER TABLE workflow_actions ADD COLUMN tracking_status TEXT DEFAULT 'planned';
                ALTER TABLE workflow_actions ADD COLUMN planned_date TEXT;
                ALTER TABLE workflow_actions ADD COLUMN focus_date TEXT;
                ALTER TABLE workflow_actions ADD COLUMN archived_date TEXT;
                ALTER TABLE workflow_actions ADD COLUMN priority TEXT DEFAULT 'medium';
                ALTER TABLE workflow_actions ADD COLUMN assigned_to_group TEXT;
                ALTER TABLE workflow_actions ADD COLUMN progress_note TEXT;
                ALTER TABLE workflow_actions ADD COLUMN percent_complete INTEGER DEFAULT 0;
            )SQL"
        },
    }; // end registry
}

bool MigrationEngine::ensureSchemaVersionTable(Database* db, const std::string& dbName) {
    bool ok = db->exec(R"SQL(
        CREATE TABLE IF NOT EXISTS schema_version (
            db_name     TEXT PRIMARY KEY,
            version     INTEGER NOT NULL DEFAULT 0,
            applied_at  TEXT NOT NULL,
            description TEXT
        );
    )SQL");
    if (!ok) return false;

    // Insert initial row if not present
    std::string cur = db->queryScalar(
        "SELECT version FROM schema_version WHERE db_name = ?;",
        {BindParam::text(dbName)});

    if (cur.empty()) {
        db->exec(
            "INSERT INTO schema_version(db_name, version, applied_at, description) "
            "VALUES(?, 1, ?, 'initial schema');",
            {BindParam::text(dbName), BindParam::text(nowIsoMig())});
    }
    return true;
}

int MigrationEngine::currentVersion(const std::string& dbName) {
    auto* db = DatabasePool::instance().get(dbName);
    if (!db) return 0;
    std::string v = db->queryScalar(
        "SELECT version FROM schema_version WHERE db_name = ?;",
        {BindParam::text(dbName)});
    return v.empty() ? 0 : std::stoi(v);
}

int MigrationEngine::targetVersion(const std::string& dbName) {
    auto m = targetVersionMap();
    auto it = m.find(dbName);
    return it != m.end() ? it->second : 1;
}

bool MigrationEngine::applyMigration(Database* db, const Migration& m) {
    LOG_INFO("[migration] Applying migration to " + m.dbName +
             " v" + std::to_string(m.targetVersion) + ": " + m.description);

    // Run delta SQL in a transaction
    bool ok = db->exec("BEGIN;");
    ok &= db->executeBatch(m.sql);
    if (!ok) {
        db->exec("ROLLBACK;");
        LOG_ERROR("[migration] FAILED: " + m.description + " — rolled back");
        return false;
    }

    // Update schema_version
    ok &= db->exec(
        "INSERT OR REPLACE INTO schema_version(db_name, version, applied_at, description) "
        "VALUES(?, ?, ?, ?);",
        {BindParam::text(m.dbName),
         BindParam::int64(m.targetVersion),
         BindParam::text(nowIsoMig()),
         BindParam::text(m.description)});

    ok &= db->exec("COMMIT;") ? ok : false;

    if (ok)
        LOG_INFO("[migration] Applied: " + m.dbName + " -> v" +
                 std::to_string(m.targetVersion));
    else
        LOG_ERROR("[migration] Failed to commit: " + m.description);
    return ok;
}

bool MigrationEngine::runFor(const std::string& dbName) {
    auto* db = DatabasePool::instance().get(dbName);
    if (!db) {
        LOG_ERROR("[migration] DB not available: " + dbName);
        return false;
    }

    if (!ensureSchemaVersionTable(db, dbName)) {
        LOG_ERROR("[migration] Could not ensure schema_version in " + dbName);
        return false;
    }

    int cur    = currentVersion(dbName);
    int target = targetVersion(dbName);

    if (cur >= target) {
        LOG_DEBUG("[migration] " + dbName + " is current at v" + std::to_string(cur));
        return true;
    }

    LOG_INFO("[migration] " + dbName + " needs migration: v" +
             std::to_string(cur) + " -> v" + std::to_string(target));

    bool ok = true;
    for (auto& m : registry()) {
        if (m.dbName != dbName) continue;
        if (m.targetVersion <= cur) continue;   // already applied
        if (m.targetVersion > target) continue; // future migration

        ok &= applyMigration(db, m);
        if (!ok) break;
    }
    return ok;
}

bool MigrationEngine::runAll() {
    LOG_INFO("[migration] Running all pending migrations...");
    bool ok = true;
    for (auto& [dbName, _] : targetVersionMap()) {
        ok &= runFor(dbName);
    }
    LOG_INFO("[migration] Migration run complete. OK=" + std::string(ok ? "yes" : "NO"));
    return ok;
}

} // namespace Rosenholz

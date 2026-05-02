// ============================================================
// Migration.cpp  —  Schema migration engine implementation
// ============================================================
#include "Migration.h"
#include "../model/Utils.h"
#include "Logger.h"
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace Rosenholz {


// ------------------------------
// targetVersionMap
//
// Returns the canonical target (= minimum required) schema version
// per database. runAll() applies any registry() deltas that bring
// currentVersion up to targetVersion.
//
// v4 baseline: all databases start here on a fresh install.
// To evolve the schema: (1) bump the version in SchemaVersions,
// (2) add a delta in registry(), (3) update the SQL file.
// ------------------------------
std::map<std::string, int> MigrationEngine::targetVersionMap() {
    return {
        {"core",      SchemaVersions::core},
        {"f16",  SchemaVersions::f16},
        {"f22",  SchemaVersions::f22},
        {"f77",  SchemaVersions::f77},
        {"akt",  SchemaVersions::akt},
        {"f18",       SchemaVersions::f18},
        {"f99",       1},  // f99 notes
    }; // note: f99 is registered via Database.cpp pool
}


// ------------------------------
// registry
//
// Returns all migration deltas in chronological order.
//
// Each Migration has:
//   dbName      : the target database ("core", "f77", …)
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
    // ── v4 Baseline: no historical deltas ─────────────────────
    //
    // v4 is the clean starting point. No backwards compatibility.
    // Changes from v3: F18 removed from F16, F18 exclusively owned by F22.
    // Add future deltas here as the schema evolves beyond v4.
    //
    // v7: fresh start — no migration from v6 possible, no deltas.
    return {}; // end registry
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
        // Fresh database — stamp it at the current target version directly.
        // No deltas need to run: the schema file already reflects v2 baseline.
        int tv = targetVersion(dbName);
        db->exec(
            "INSERT INTO schema_version(db_name, version, applied_at, description) "
            "VALUES(?, ?, ?, 'v4 baseline — initial install');",
            {BindParam::text(dbName),
             BindParam::int64(tv),
             BindParam::text(Rosenholz::nowIso())});
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
         BindParam::text(Rosenholz::nowIso()),
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

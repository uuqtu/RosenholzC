#pragma once
// ============================================================
// Migration.h  —  Schema migration engine
//
// Each database has a schema_version table. On startup, the
// migration engine compares the current version against the
// known target version and runs delta scripts if needed.
//
// Adding a migration:
//   1. Create a delta SQL function in the relevant schema section
//   2. Add an entry to the migrations registry with the DB name,
//      version number, and SQL to execute
//   3. Bump SCHEMA_VERSION_<DB> constants below
// ============================================================
#include "Database.h"
#include "Logger.h"
#include <string>
#include <vector>
#include <functional>
#include <map>

namespace Rosenholz {

/// Current schema versions — bump these when adding migrations
// ── Schema baseline v4 ────────────────────────────────────────
// v4: F18 exclusively owned by F22 (removed F16→F18 relationship).
// No backwards compatibility with v2/v3 databases.
// Fresh databases start at v4 directly via SQL files.
struct SchemaVersions {
    static constexpr int core      = 5;
    static constexpr int f24       = 1;  // v1: initial (f24_steps table)
    static constexpr int f77s      = 1;  // v1: initial (f77_workflow_steps table)  // v5: users/roles tables added
    static constexpr int f16       = 5;  // v5: wf_locked flag
    static constexpr int f22       = 5;  // v5: wf_locked flag
    static constexpr int f77       = 5;  // v5: is_system + system_action added;  // v4 baseline
    static constexpr int akt       = 8;  // v8: wf_locked flag
    static constexpr int f77task   = 1;  // v1: initial schema  // v7: file_url removed from Akte (URL belongs to objects)
    static constexpr int f18       = 5;  // v5: wf_locked flag
};

/// A single migration step
struct Migration {
    std::string dbName;
    int         targetVersion;
    std::string description;
    std::string sql;         // DDL to execute
};

class MigrationEngine {
public:
    /// Run all pending migrations on all databases.
    /// Returns true if all succeeded (or were already current).
    static bool runAll();

    /// Run migrations for a specific database only.
    static bool runFor(const std::string& dbName);

    /// Get the current persisted version for a database.
    static int currentVersion(const std::string& dbName);

    /// Get the target (code-defined) version for a database.
    static int targetVersion(const std::string& dbName);

private:
    static bool ensureSchemaVersionTable(Database* db, const std::string& dbName);
    static bool applyMigration(Database* db, const Migration& m);
    static std::vector<Migration> registry();
    static std::map<std::string, int> targetVersionMap();
};

} // namespace Rosenholz

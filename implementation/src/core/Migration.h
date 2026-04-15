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
struct SchemaVersions {
    static constexpr int core      = 1;
    static constexpr int projects  = 1;
    static constexpr int workflow  = 2;  // bumped: added template_actions
    static constexpr int documents = 1;
    static constexpr int tracking  = 1;
    static constexpr int reporting = 1;
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

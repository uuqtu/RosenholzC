#pragma once
// ============================================================
// Database.h  —  SQLite connection wrapper and pool
//
// DatabasePool is a singleton managing 6 SQLite databases.
// Each DB opened once at startup; shared across the app.
// Thread safety: single-threaded CLI; no mutex needed.
// ============================================================
// ============================================================
// Database.h  —  Thin SQLite abstraction layer
//
// Multiple database files are used for performance isolation:
//   core.db      — config, persons, teams, reg numbers
//   projects.db  — F16, F22, F18, milestones, dependencies
//   workflow.db  — workflow engine tables
//   documents.db — documents, file archives
//   tracking.db  — trackable items, notes, reminders
//   reporting.db — KPIs, quality gates, lessons, decisions
//
// All databases are opened with WAL mode for concurrent readers
// (important for OneDrive / shared network access).
//
// Use DatabasePool to obtain a handle by logical name.
// ============================================================

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

// Forward-declare sqlite3 to avoid pulling in sqlite3.h everywhere
struct sqlite3;
struct sqlite3_stmt;

namespace Rosenholz {

// ── Row type —  column name -> string value ──────────────────
using Row = std::map<std::string, std::string>;
using ResultSet = std::vector<Row>;

// ── Bind parameter (positional, 1-based) ─────────────────────
struct BindParam {
    enum class Type { TEXT, INT64, DOUBLE, NULL_ } type;
    std::string  sval;
    int64_t      ival { 0 };
    double       dval { 0.0 };
    static BindParam text  (const std::string& v) { return {Type::TEXT,   v, 0,   0.0 }; }
    static BindParam int64 (int64_t v)             { return {Type::INT64,  "", v,  0.0 }; }
    static BindParam real  (double  v)             { return {Type::DOUBLE, "", 0,  v   }; }
    static BindParam null  ()                      { return {Type::NULL_,  "", 0,  0.0 }; }
    // Convenience: null if empty, text otherwise
    static BindParam nullOrText(const std::string& v) {
        return v.empty() ? null() : text(v);
    }
};

// ── Single database connection ────────────────────────────────
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    bool isOpen() const;
    const std::string& path() const { return m_path; }

    // ── DDL helpers ────────────────────────────────────────
    bool execute(const std::string& sql);
    bool executeBatch(const std::string& sql); ///< Multiple statements separated by ;

    // ── DML ────────────────────────────────────────────────
    bool exec(const std::string& sql, const std::vector<BindParam>& params = {});

    /// Returns last inserted rowid
    int64_t insert(const std::string& sql, const std::vector<BindParam>& params = {});

    /// Returns result rows
    ResultSet query(const std::string& sql, const std::vector<BindParam>& params = {});

    /// Returns single scalar value (first column of first row)
    std::string queryScalar(const std::string& sql, const std::vector<BindParam>& params = {});

    // ── Transactions ───────────────────────────────────────
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // ── Utility ────────────────────────────────────────────
    bool tableExists(const std::string& tableName);
    int64_t lastInsertRowId();
    int64_t rowCount(const std::string& tableName);
    std::string lastError() const;

    // ── WAL / performance ──────────────────────────────────
    void applyPerformanceSettings(bool walMode, int cacheSize);

    // ── Schema versioning ──────────────────────────────────
    int  schemaVersion();
    void setSchemaVersion(int v);

private:
    std::string  m_path;
    sqlite3*     m_db     { nullptr };
    std::string  m_lastError;
    mutable std::mutex m_mutex;

    bool bindParams(sqlite3_stmt* stmt, const std::vector<BindParam>& params);
    void logSqlError(const std::string& context);
};

// ── Database pool — access by logical name ───────────────────
class DatabasePool {
public:
    static DatabasePool& instance();

    DatabasePool(const DatabasePool&)            = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;

    /// Initialise all databases under basePath/db/
    bool initAll(const std::string& basePath, bool walMode, int cacheSize);

    /// Get a database by logical name ("core","f16","f77",
    ///   "dok","tracking","reporting")
    Database* get(const std::string& name);

    /// Close all connections gracefully
    void closeAll();

    /// Run pending schema migrations on all databases

private:
    DatabasePool() = default;

    std::map<std::string, std::unique_ptr<Database>> m_dbs;
    std::mutex m_mutex;

    bool applySchema(Database* db, const std::string& schemaName);
};

// ── Schema definitions (one per DB) ──────────────────────────
namespace Schema {
    std::string coreSchema();
    std::string projectsSchema();
    std::string workflowSchema();
    std::string documentsSchema();
    std::string trackingSchema();
    std::string reportingSchema();
} // namespace Schema

} // namespace Rosenholz

// ============================================================
// Database.cpp  —  SQLite abstraction layer implementation
// ============================================================

#include "Database.h"
#include "SchemaFiles.h"
#include "Logger.h"
#include "FileOps.h"
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace Rosenholz {

// ── Database ─────────────────────────────────────────────────
Database::Database(const std::string& path) : m_path(path) {
    LOG_DEBUG("Opening database: " + path);
    FileOps::makeDirs(FileOps::dirName(path));

    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Failed to open DB " + path + ": " + m_lastError);
        sqlite3_close(m_db);
        m_db = nullptr;
    } else {
        LOG_INFO("Database opened: " + path);
    }
}

Database::~Database() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
        LOG_DEBUG("Database closed: " + m_path);
    }
}

bool Database::isOpen() const { return m_db != nullptr; }

std::string Database::lastError() const { return m_lastError; }

// ── Performance settings ─────────────────────────────────────
void Database::applyPerformanceSettings(bool walMode, int cacheSize) {
    if (!m_db) return;
    if (walMode)    execute("PRAGMA journal_mode=WAL;");
    execute("PRAGMA synchronous=NORMAL;");   // safe with WAL
    execute("PRAGMA cache_size=" + std::to_string(cacheSize) + ";");
    execute("PRAGMA temp_store=MEMORY;");
    execute("PRAGMA mmap_size=268435456;");  // 256 MB mmap
    LOG_DEBUG("Performance settings applied to: " + m_path);
}

// ── Schema version ───────────────────────────────────────────
int Database::schemaVersion() {
    auto v = queryScalar("PRAGMA user_version;");
    return v.empty() ? 0 : std::stoi(v);
}

void Database::setSchemaVersion(int v) {
    execute("PRAGMA user_version = " + std::to_string(v) + ";");
}

// ── DDL ──────────────────────────────────────────────────────
bool Database::execute(const std::string& sql) {
    if (!m_db) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        m_lastError = err ? err : "unknown";
        LOG_ERROR("SQL execute error [" + m_path + "]: " + m_lastError + " | SQL: " + sql.substr(0,120));
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::executeBatch(const std::string& sql) {
    // Split on semicolons and run each statement individually.
    // This allows partial success (e.g. ALTER TABLE IF NOT EXISTS equivalent)
    // and avoids SQLite DDL transaction issues.
    bool allOk = true;
    std::string stmt;
    std::istringstream ss(sql);
    std::string line;
    while (std::getline(ss, line)) {
        // Strip comments and whitespace
        auto pos = line.find("--");
        if (pos != std::string::npos) line = line.substr(0, pos);
        // Trim
        size_t s = line.find_first_not_of(" \t\r\n");
        size_t e = line.find_last_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        line = line.substr(s, e - s + 1);
        if (line.empty()) continue;
        stmt += " " + line;
        if (!stmt.empty() && stmt.back() == ';') {
            std::string toRun = stmt;
            stmt.clear();
            // Ignore "duplicate column name" (ALTER TABLE on fresh schema)
            char* err = nullptr;
            int rc = sqlite3_exec(m_db, toRun.c_str(), nullptr, nullptr, &err);
            if (rc != SQLITE_OK) {
                std::string errStr = err ? err : "unknown";
                sqlite3_free(err);
                // Tolerate duplicate column (fresh DB already has the column)
                if (errStr.find("duplicate column name") != std::string::npos) {
                    LOG_DEBUG("[migration] Column already exists (fresh schema) — OK");
                    continue;
                }
                m_lastError = errStr;
                LOG_ERROR("executeBatch error [" + m_path + "]: " + errStr +
                          " | SQL: " + toRun.substr(0, 80));
                allOk = false;
            }
        }
    }
    return allOk;
}

// ── DML helpers ──────────────────────────────────────────────
bool Database::bindParams(sqlite3_stmt* stmt, const std::vector<BindParam>& params) {
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        const auto& p = params[i];
        int idx = i + 1;
        int rc  = SQLITE_OK;
        switch (p.type) {
            case BindParam::Type::TEXT:
                rc = sqlite3_bind_text(stmt, idx, p.sval.c_str(), -1, SQLITE_TRANSIENT);
                break;
            case BindParam::Type::INT64:
                rc = sqlite3_bind_int64(stmt, idx, p.ival);
                break;
            case BindParam::Type::DOUBLE:
                rc = sqlite3_bind_double(stmt, idx, p.dval);
                break;
            case BindParam::Type::NULL_:
                rc = sqlite3_bind_null(stmt, idx);
                break;
        }
        if (rc != SQLITE_OK) {
            m_lastError = sqlite3_errmsg(m_db);
            LOG_ERROR("Bind param " + std::to_string(idx) + " failed: " + m_lastError);
            return false;
        }
    }
    return true;
}

bool Database::exec(const std::string& sql, const std::vector<BindParam>& params) {
    if (!m_db) return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Prepare failed [" + m_path + "]: " + m_lastError);
        return false;
    }
    bindParams(stmt, params);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("exec step failed: " + m_lastError + " | SQL: " + sql.substr(0,120));
        return false;
    }
    return true;
}

int64_t Database::insert(const std::string& sql, const std::vector<BindParam>& params) {
    if (!exec(sql, params)) return -1;
    return sqlite3_last_insert_rowid(m_db);
}

ResultSet Database::query(const std::string& sql, const std::vector<BindParam>& params) {
    ResultSet result;
    if (!m_db) return result;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Query prepare failed: " + m_lastError);
        return result;
    }
    bindParams(stmt, params);

    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row;
        for (int c = 0; c < cols; ++c) {
            const char* name = sqlite3_column_name(stmt, c);
            const char* val  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
            row[name ? name : ""] = val ? val : "";
        }
        result.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    LOG_DEBUG("Query returned " + std::to_string(result.size()) + " rows");
    return result;
}

std::string Database::queryScalar(const std::string& sql, const std::vector<BindParam>& params) {
    auto rs = query(sql, params);
    if (rs.empty() || rs[0].empty()) return "";
    return rs[0].begin()->second;
}

// ── Transactions ─────────────────────────────────────────────
bool Database::beginTransaction()    { return execute("BEGIN TRANSACTION;"); }
bool Database::commitTransaction()   { return execute("COMMIT;"); }
bool Database::rollbackTransaction() { return execute("ROLLBACK;"); }

// ── Utility ──────────────────────────────────────────────────
bool Database::tableExists(const std::string& tableName) {
    auto v = queryScalar(
        "SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?;",
        {BindParam::text(tableName)});
    return v == "1";
}

int64_t Database::lastInsertRowId() {
    return m_db ? sqlite3_last_insert_rowid(m_db) : -1;
}

int64_t Database::rowCount(const std::string& tableName) {
    auto v = queryScalar("SELECT count(*) FROM " + tableName + ";");
    return v.empty() ? 0 : std::stoll(v);
}

// ── DatabasePool ─────────────────────────────────────────────
DatabasePool& DatabasePool::instance() {
    static DatabasePool inst;
    return inst;
}

// ------------------------------
// initAll
//
// Parameters:
//   basePath  : application base directory (db/ subdirectory used)
//   walMode   : true = enable WAL journal for better concurrency
//   cacheSize : SQLite page cache size in KB
//
// Behavior:
//   Opens all 6 databases under basePath/db/.
//   Applies the embedded SQL schema for each DB.
//   Returns false (and logs error) if any DB fails to open.
// ------------------------------
bool DatabasePool::initAll(const std::string& basePath, bool walMode, int cacheSize) {
    std::lock_guard<std::mutex> lk(m_mutex);
    LOG_INFO("Initialising database pool under: " + basePath);

    struct DBDef { std::string name; std::string file; };
    const std::vector<DBDef> defs = {
        {"core",      "core.db"},
        {"f16",       "f16.db"},
        {"f22",       "f22.db"},
        {"f77",       "f77.db"},
        {"dok",       "dok.db"},
        {"f18",       "f18.db"},
    };

    bool ok = true;
    for (auto& def : defs) {
        std::string path = FileOps::joinPath(basePath, "db", def.file);
        auto db = std::make_unique<Database>(path);
        if (!db->isOpen()) { ok = false; continue; }
        db->applyPerformanceSettings(walMode, cacheSize);
        ok &= applySchema(db.get(), def.name);
        m_dbs[def.name] = std::move(db);
    }
    LOG_INFO("Database pool ready. All OK: " + std::string(ok ? "yes" : "NO"));
    return ok;
}

Database* DatabasePool::get(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_dbs.find(name);
    if (it == m_dbs.end()) {
        LOG_ERROR("DatabasePool::get — unknown db name: " + name);
        return nullptr;
    }
    return it->second.get();
}

void DatabasePool::closeAll() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_dbs.clear();
    LOG_INFO("All databases closed.");
}

bool DatabasePool::applySchema(Database* db, const std::string& schemaName) {
    // Load SQL from embedded schema files (generated at build time from sql/*.sql)
    std::string sql = SchemaFiles::get(schemaName);
    if (sql.empty()) {
        LOG_WARN("Schema not found in embedded SQL files: " + schemaName);
        return false;
    }
    bool ok = db->executeBatch(sql);
    LOG_INFO("Schema '" + schemaName + "' applied: " + std::string(ok ? "OK" : "FAILED"));
    return ok;
}

} // namespace Rosenholz

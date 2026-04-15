#pragma once
// ============================================================
// Repository.h  —  Generic CRUD template for all model classes
//
// Usage:
//   auto repo = Repository<Person>::on("core", "persons", "person_id");
//   auto p    = repo.loadById("XV/PER/0001/2026");
//   auto all  = repo.loadAll();
//   auto byProj = repo.loadWhere("project_id = ?", {BindParam::text(id)});
//
// Eliminates the duplicate loadById/loadAll/loadWhere boilerplate
// that was copy-pasted into every model class.
// ============================================================
#include "../core/Database.h"
#include "../core/Logger.h"
#include <memory>
#include <vector>
#include <functional>
#include <string>

namespace Rosenholz {

/// Utility: safely get a string value from a Row by column name.
/// Returns "" if column doesn't exist — avoids at() exceptions.
inline std::string rowGet(const Row& r, const std::string& col) {
    auto it = r.find(col);
    return it != r.end() ? it->second : "";
}

/// Utility: rowGet with a default fallback
inline std::string rowGetOr(const Row& r, const std::string& col, const std::string& def) {
    auto it = r.find(col);
    return (it != r.end() && !it->second.empty()) ? it->second : def;
}

/// Utility: rowGet as int
inline int rowGetInt(const Row& r, const std::string& col, int def = 0) {
    auto it = r.find(col);
    if (it == r.end() || it->second.empty()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

/// Utility: rowGet as double
inline double rowGetDbl(const Row& r, const std::string& col, double def = 0.0) {
    auto it = r.find(col);
    if (it == r.end() || it->second.empty()) return def;
    try { return std::stod(it->second); } catch (...) { return def; }
}

/// Utility: rowGet as bool (SQLite stores as 0/1 integer)
inline bool rowGetBool(const Row& r, const std::string& col) {
    return rowGetInt(r, col) == 1;
}

// ─────────────────────────────────────────────────────────────
/// Generic Repository<T>
///
/// T must be default-constructible and provide:
///   void fromRow(const Row& r)   — populate from DB row
///   bool save() const            — persist to DB
///
/// Obtain via: Repository<T>::on(dbName, tableName, pkColumn)
// ─────────────────────────────────────────────────────────────
template<typename T>
class Repository {
public:
    using Ptr     = std::shared_ptr<T>;
    using PtrVec  = std::vector<Ptr>;

    /// Factory: create a repository bound to a specific DB/table
    static Repository<T> on(const std::string& dbName,
                             const std::string& table,
                             const std::string& pkColumn) {
        return Repository<T>{dbName, table, pkColumn};
    }

    /// Load a single record by primary key value.
    Ptr loadById(const std::string& id) const {
        auto* db = db_();
        if (!db) return nullptr;
        auto rows = db->query(
            "SELECT * FROM " + table_ + " WHERE " + pk_ + " = ?;",
            {BindParam::text(id)});
        if (rows.empty()) {
            LOG_DEBUG("Repository::loadById not found: " + id + " in " + table_);
            return nullptr;
        }
        auto obj = std::make_shared<T>();
        obj->fromRow(rows[0]);
        return obj;
    }

    /// Load all records, optional ORDER BY clause.
    PtrVec loadAll(const std::string& orderBy = "") const {
        auto* db = db_();
        PtrVec result;
        if (!db) return result;
        std::string sql = "SELECT * FROM " + table_;
        if (!orderBy.empty()) sql += " ORDER BY " + orderBy;
        sql += ";";
        for (auto& r : db->query(sql)) {
            auto obj = std::make_shared<T>();
            obj->fromRow(r);
            result.push_back(std::move(obj));
        }
        return result;
    }

    /// Load with a WHERE clause and bind parameters.
    /// where: e.g. "project_id = ? AND status = ?"
    PtrVec loadWhere(const std::string& where,
                     const std::vector<BindParam>& params = {},
                     const std::string& orderBy = "") const {
        auto* db = db_();
        PtrVec result;
        if (!db) return result;
        std::string sql = "SELECT * FROM " + table_ + " WHERE " + where;
        if (!orderBy.empty()) sql += " ORDER BY " + orderBy;
        sql += ";";
        for (auto& r : db->query(sql, params)) {
            auto obj = std::make_shared<T>();
            obj->fromRow(r);
            result.push_back(std::move(obj));
        }
        return result;
    }

    /// Load a single record with WHERE clause (returns first match).
    Ptr loadOneWhere(const std::string& where,
                     const std::vector<BindParam>& params = {}) const {
        auto v = loadWhere(where, params);
        return v.empty() ? nullptr : v[0];
    }

    /// Count records matching WHERE clause.
    int count(const std::string& where = "",
              const std::vector<BindParam>& params = {}) const {
        auto* db = db_();
        if (!db) return 0;
        std::string sql = "SELECT COUNT(*) FROM " + table_;
        if (!where.empty()) sql += " WHERE " + where;
        sql += ";";
        std::string val = db->queryScalar(sql, params);
        return val.empty() ? 0 : std::stoi(val);
    }

    /// Delete record by primary key.
    bool remove(const std::string& id) const {
        auto* db = db_();
        if (!db) return false;
        return db->exec("DELETE FROM " + table_ + " WHERE " + pk_ + " = ?;",
                        {BindParam::text(id)});
    }

    /// Check if a record exists.
    bool exists(const std::string& id) const {
        auto* db = db_();
        if (!db) return false;
        auto val = db->queryScalar(
            "SELECT COUNT(*) FROM " + table_ + " WHERE " + pk_ + " = ?;",
            {BindParam::text(id)});
        return val == "1";
    }

    const std::string& dbName()  const { return db_;    }
    const std::string& table()   const { return table_; }
    const std::string& pkCol()   const { return pk_;    }

private:
    Repository(const std::string& db, const std::string& table, const std::string& pk)
        : dbName_(db), table_(table), pk_(pk) {}

    Database* db_() const {
        return DatabasePool::instance().get(dbName_);
    }

    std::string dbName_, table_, pk_;
};

} // namespace Rosenholz

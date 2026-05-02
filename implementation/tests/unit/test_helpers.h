#pragma once
// ============================================================
// tests/unit/test_helpers.h  —  Shared Catch2 fixtures & utilities
//
// OVERVIEW
// ════════
// This header is included by every test file in tests/unit/.
// It provides:
//
//   TempDB          — RAII isolated database per TEST_CASE.
//   makeF16/F22/F18/Person — one-line entity factories.
//   rowCount        — COUNT(*) on a named DB pool member.
//   colValue        — single column value from a row.
//   colExists       — PRAGMA table_info membership check.
//
// ADDING A NEW TEST FILE
// ══════════════════════
// 1. Create  tests/unit/<object>/test_<object>.cpp
// 2. #include "../test_helpers.h"
// 3. Write TEST_CASEs with tags "[<object>][<aspect>]"
//    Recommended aspects: [model] [sql] [filesystem] [mfs]
//                         [lifecycle] [query] [fields]
// 4. CMakeLists uses GLOB_RECURSE — no CMake edit needed.
//
// RUNNING SELECTED TESTS
// ══════════════════════
//   ./build/rosenholz_unit                     # all tests
//   ./build/rosenholz_unit "[f22]"             # all F22 tests
//   ./build/rosenholz_unit "[sql]"             # SQL-verification tests
//   ./build/rosenholz_unit "[f18][lifecycle]"  # F18 lifecycle only
//   ./build/rosenholz_unit --list-tests        # enumerate
// ============================================================

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <algorithm>

#include "../../src/core/Config.h"
#include "../../src/core/Database.h"
#include "../../src/core/Migration.h"
#include "../../src/core/Logger.h"
#include "../../src/core/FileOps.h"
#include "../../src/model/f16/F16.h"
#include "../../src/model/f22/F22.h"
#include "../../src/model/f18/F18Operation.h"
#include "../../src/model/f18/F18OperationStep.h"
#include "../../src/model/f18/Communication.h"
#include "../../src/model/akt/Folder.h"
#include "../../src/model/akt/FolderRevision.h"
#include "../../src/model/akt/FolderObject.h"
#include "../../src/model/person/Person.h"
#include "../../src/model/team/Team.h"
#include "../../src/workflow/F77Workflow.h"
#include "../../src/workflow/F77Task.h"
#include "../../src/model/Note.h"

namespace RhTest {

// ── TempDB ────────────────────────────────────────────────────────────────────
// RAII wrapper: every TEST_CASE gets its own isolated temp directory
// with freshly-initialised databases.
// The singleton DatabasePool and Config are re-pointed at each new directory.
// Connections from the previous TempDB are closed at the START of construction
// (not only at destruction) to avoid cross-test interference.
class TempDB {
public:
    explicit TempDB(const std::string& tag = "test") {
        Rosenholz::DatabasePool::instance().closeAll();

        static std::atomic<int> counter{0};
        dir_ = std::filesystem::temp_directory_path()
                   / ("rh_unit_" + tag + "_" + std::to_string(++counter));
        std::filesystem::create_directories(dir_);
        std::filesystem::create_directories(dir_ / "db");
        std::filesystem::create_directories(dir_ / "mfs");

        auto& cfg = Rosenholz::Config::instance();
        cfg.setBasePath(dir_.string());
        Rosenholz::Logger::instance().setLevel(Rosenholz::LogLevel::ERR);
        Rosenholz::DatabasePool::instance().initAll(dir_.string(), false, 1024);
    }

    ~TempDB() {
        Rosenholz::DatabasePool::instance().closeAll();
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }

    std::string path()    const { return dir_.string(); }
    std::string mfsPath() const { return (dir_ / "mfs").string(); }

    TempDB(const TempDB&)            = delete;
    TempDB& operator=(const TempDB&) = delete;

private:
    std::filesystem::path dir_;
};

// ── Entity factories ──────────────────────────────────────────────────────────
// Each factory saves to the DB and REQUIREs success.

inline std::shared_ptr<Rosenholz::F16> makeF16(
    const std::string& title = "Test-F16",
    const std::string& type  = "OV")
{
    auto p = Rosenholz::F16::create(title, type);
    REQUIRE(p != nullptr);
    REQUIRE(Rosenholz::opOk(p->save()));
    return p;
}

inline std::shared_ptr<Rosenholz::F22> makeF22(
    const std::string& projectId,
    const std::string& title = "Test-F22")
{
    auto t = Rosenholz::F22::create(projectId, title);
    REQUIRE(t != nullptr);
    REQUIRE(Rosenholz::opOk(t->save()));   // F22::create does NOT auto-save
    return t;
}

inline std::shared_ptr<Rosenholz::F18Operation> makeF18(
    const std::string& taskId,
    const std::string& title = "Test-F18",
    const std::string& type  = "risk")
{
    auto v = Rosenholz::F18Operation::create(taskId, title, type);
    REQUIRE(v != nullptr);
    return v;  // F18Operation::create auto-saves
}

inline std::shared_ptr<Rosenholz::Person> makePerson(
    const std::string& first = "Test",
    const std::string& last  = "Person",
    const std::string& email = "")
{
    std::string em = email.empty() ? (first + "." + last + "@test.de") : email;
    auto p = Rosenholz::Person::create(first, last, em, "internal");
    REQUIRE(p != nullptr);
    REQUIRE(Rosenholz::opOk(p->save()));
    return p;
}

// ── SQL helpers ───────────────────────────────────────────────────────────────

/// COUNT(*) with optional WHERE.  Returns -1 when the DB is unavailable.
inline int rowCount(const std::string& dbName, const std::string& table,
                    const std::string& where = "") {
    auto* db = Rosenholz::DatabasePool::instance().get(dbName);
    if (!db) return -1;
    std::string sql = "SELECT COUNT(*) AS n FROM " + table;
    if (!where.empty()) sql += " WHERE " + where;
    sql += ";";
    auto rows = db->query(sql, {});
    if (rows.empty()) return 0;
    try { return std::stoi(rows[0].at("n")); } catch (...) { return 0; }
}

/// Fetch a single column value from the first matching row.  "" = not found.
inline std::string colValue(const std::string& dbName, const std::string& table,
                             const std::string& col, const std::string& where) {
    auto* db = Rosenholz::DatabasePool::instance().get(dbName);
    if (!db) return "";
    auto rows = db->query("SELECT " + col + " FROM " + table
                          + " WHERE " + where + " LIMIT 1;", {});
    if (rows.empty()) return "";
    auto it = rows[0].find(col);
    return it != rows[0].end() ? it->second : "";
}

/// Returns true when 'colName' appears in PRAGMA table_info(<table>).
inline bool colExists(const std::string& dbName, const std::string& table,
                      const std::string& colName) {
    auto* db = Rosenholz::DatabasePool::instance().get(dbName);
    if (!db) return false;
    auto rows = db->query("PRAGMA table_info(" + table + ");", {});
    for (auto& row : rows) {
        auto it = row.find("name");
        if (it != row.end() && it->second == colName) return true;
    }
    return false;
}

} // namespace RhTest

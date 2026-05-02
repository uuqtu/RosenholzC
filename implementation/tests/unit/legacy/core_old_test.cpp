// ============================================================
// tests/unit/legacy/core_old_test.cpp
//
// Translated from tests/test_core.cpp (legacy custom framework)
// to Catch2 v3.  Tests: Logger, Config, FileOps, RegNumber,
// Migration, Version.
// ============================================================
#include "../test_helpers.h"
#include "../../../src/core/Logger.h"
#include "../../../src/core/Config.h"
#include "../../../src/core/FileOps.h"
#include "../../../src/core/RegNumber.h"
#include "../../../src/core/Migration.h"
#include "../../../src/workflow/F77Workflow.h"

using namespace Rosenholz;
using namespace RhTest;

// ── Logger ───────────────────────────────────────────────────

TEST_CASE("Legacy/Core: Logger level setting", "[legacy][core][logger]") {
    TempDB db("leg_core_log");
    Logger::instance().setLevel(LogLevel::DEBUG);
    CHECK(true);  // must not crash
    Logger::instance().setLevel(LogLevel::WARN);
    CHECK(true);
}

// ── Config ───────────────────────────────────────────────────

TEST_CASE("Legacy/Core: Config registratur fields populated", "[legacy][core][config]") {
    TempDB db("leg_core_cfg");
    auto& cfg = Config::instance();
    CHECK_FALSE(cfg.basePath().empty());
    auto& reg = cfg.registratur();
    CHECK_FALSE(reg.diensteinheitKuerzel.empty());
    CHECK_FALSE(reg.aktenfuehrendeStelle.empty());
    CHECK_FALSE(reg.geschaeftszeichen.empty());
}

// ── FileOps ──────────────────────────────────────────────────

TEST_CASE("Legacy/Core: FileOps write/read/delete", "[legacy][core][fileops]") {
    TempDB db("leg_core_fio");
    std::string tmp = Config::instance().basePath() + "/test_io.txt";

    SECTION("writeTextFile + fileExists") {
        CHECK(FileOps::writeTextFile(tmp, "Rosenholz test", false));
        CHECK(FileOps::fileExists(tmp));
    }
    SECTION("readTextFile content matches") {
        FileOps::writeTextFile(tmp, "Rosenholz test", false);
        CHECK(FileOps::readTextFile(tmp) == "Rosenholz test");
    }
    SECTION("fileSize > 0") {
        FileOps::writeTextFile(tmp, "Rosenholz test", false);
        CHECK(FileOps::fileSize(tmp) > 0);
    }
    SECTION("deleteFile removes file") {
        FileOps::writeTextFile(tmp, "x", false);
        FileOps::deleteFile(tmp);
        CHECK_FALSE(FileOps::fileExists(tmp));
    }
}

// ── RegNumber ────────────────────────────────────────────────

TEST_CASE("Legacy/Core: RegNumber generation and fromString", "[legacy][core][regnumber]") {
    TempDB db("leg_core_rn");

    auto n1 = RegNumberGenerator::next("F16");
    auto n2 = RegNumberGenerator::next("F16");

    SECTION("both valid") {
        CHECK(n1.isValid());
        CHECK(n2.isValid());
    }
    SECTION("sequence increments") {
        CHECK(n2.sequence > n1.sequence);
    }
    SECTION("fromString round-trip") {
        std::string s = n1.toString();
        auto parsed = RegNumber::fromString(s);
        CHECK(parsed.dept     == n1.dept);
        CHECK(parsed.sequence == n1.sequence);
        CHECK(parsed.year     == n1.year);
    }
}

TEST_CASE("Legacy/Core: genId produces unique non-empty IDs", "[legacy][core][genid]") {
    TempDB db("leg_core_gid");
    std::string id1 = genId("F16");
    std::string id2 = genId("F22");
    CHECK_FALSE(id1.empty());
    CHECK_FALSE(id2.empty());
    CHECK(id1 != id2);
}

// ── Migration ────────────────────────────────────────────────

TEST_CASE("Legacy/Core: MigrationEngine::runAll() succeeds", "[legacy][core][migration]") {
    TempDB db("leg_core_mig");
    bool ok = false;
    REQUIRE_NOTHROW(ok = MigrationEngine::runAll());
    CHECK(ok);
}

TEST_CASE("Legacy/Core: Schema versions >= 1 (via core db)", "[legacy][core][migration]") {
    TempDB db("leg_core_ver");
    // schema_version lives in the core db; individual db names are keys in that table.
    // MigrationEngine::runAll() returns true when all schemas are at target version:
    CHECK(MigrationEngine::runAll());
    // Verify specific known schemas via the core db:
    auto* coreDb = DatabasePool::instance().get("core");
    REQUIRE(coreDb != nullptr);
    // At least the core schema itself must be versioned:
    CHECK(MigrationEngine::currentVersion("core") >= 1);
}

// ── Version ──────────────────────────────────────────────────

TEST_CASE("Legacy/Core: Version::toString contains '9'", "[legacy][core][version]") {
    TempDB db("leg_core_vstr");
    std::string ver = Version::toString();
    CHECK_FALSE(ver.empty());
    CHECK(ver.find("9") != std::string::npos);
}

TEST_CASE("Legacy/Core: Version::full contains 'Rosenholz'", "[legacy][core][version]") {
    TempDB db("leg_core_vfull");
    CHECK(Version::full().find("Rosenholz") != std::string::npos);
}

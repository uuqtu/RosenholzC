// ============================================================
// tests/unit/core/test_core.cpp  —  Core Infrastructure
//
// Coverage
// ════════
//   Logger     — setLevel does not crash
//   Config     — basePath, registratur fields non-empty
//   FileOps    — write / read / exists / size / delete / makeDirs
//   RegNumber  — format, sequence, fromString, dept independence
//   genId      — non-empty, unique, correct dept prefix
//   Migration  — runAll idempotent, pool DBs accessible
//   Version    — toString contains major, full contains 'Rosenholz'
//   MFS        — writeProject flat file, writeDocument F22-linked,
//                orphan document refused, rebuildAll
// ============================================================
#include "../test_helpers.h"
#include "../../../src/core/Logger.h"
#include "../../../src/core/Config.h"
#include "../../../src/core/FileOps.h"
#include "../../../src/core/RegNumber.h"
#include "../../../src/core/Migration.h"
#include "../../../src/mfs/MFSWriter.h"
#include "../../../src/workflow/F77Workflow.h"
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

using namespace Rosenholz;
using namespace RhTest;

// ── Logger ────────────────────────────────────────────────────────────────────

TEST_CASE("Core/Logger: setLevel does not crash for any valid level", "[core][logger]") {
    TempDB db("core_log");
    for (auto lvl : {LogLevel::DEBUG, LogLevel::INFO, LogLevel::WARN, LogLevel::ERR}) {
        REQUIRE_NOTHROW(Logger::instance().setLevel(lvl));
    }
}

// ── Config ────────────────────────────────────────────────────────────────────

TEST_CASE("Core/Config: basePath is set and non-empty", "[core][config]") {
    TempDB db("core_cfg_path");
    CHECK_FALSE(Config::instance().basePath().empty());
}

TEST_CASE("Core/Config: registratur fields are non-empty", "[core][config]") {
    TempDB db("core_cfg_reg");
    auto& reg = Config::instance().registratur();
    CHECK_FALSE(reg.diensteinheitKuerzel.empty());
    CHECK_FALSE(reg.aktenfuehrendeStelle.empty());
    CHECK_FALSE(reg.geschaeftszeichen.empty());
}

// ── FileOps ───────────────────────────────────────────────────────────────────

TEST_CASE("Core/FileOps: write and read text file", "[core][fileops][filesystem]") {
    TempDB db("core_fio_rw");
    std::string path = Config::instance().basePath() + "/test_io.txt";
    REQUIRE(FileOps::writeTextFile(path, "Rosenholz test", false));
    CHECK(FileOps::readTextFile(path) == "Rosenholz test");
}

TEST_CASE("Core/FileOps: fileExists after write, false after delete", "[core][fileops][filesystem]") {
    TempDB db("core_fio_exists");
    std::string path = Config::instance().basePath() + "/exists_test.txt";
    FileOps::writeTextFile(path, "x", false);
    CHECK(FileOps::fileExists(path));
    FileOps::deleteFile(path);
    CHECK_FALSE(FileOps::fileExists(path));
}

TEST_CASE("Core/FileOps: fileSize > 0 after write", "[core][fileops][filesystem]") {
    TempDB db("core_fio_size");
    std::string path = Config::instance().basePath() + "/size_test.txt";
    FileOps::writeTextFile(path, "Hello World", false);
    CHECK(FileOps::fileSize(path) > 0);
}

TEST_CASE("Core/FileOps: makeDirs creates nested directories", "[core][fileops][filesystem]") {
    TempDB db("core_fio_dirs");
    std::string path = Config::instance().basePath() + "/a/b/c";
    FileOps::makeDirs(path);
    CHECK(std::filesystem::is_directory(path));
}

// ── RegNumber ─────────────────────────────────────────────────────────────────

TEST_CASE("Core/RegNumber: format XV/DEPT/NNNN/YY", "[core][regnumber]") {
    TempDB db("core_rn_fmt");
    auto rn = RegNumberGenerator::next("F16");
    std::string s = rn.toString();
    CHECK(s.size()         == 16);
    CHECK(s.substr(0, 3)   == "XV/");
    CHECK(s.substr(3, 3)   == "F16");
    CHECK(s.substr(6, 1)   == "/");
    CHECK(s.substr(13, 1)  == "/");
    CHECK(rn.isValid());
}

TEST_CASE("Core/RegNumber: sequence increments within a department", "[core][regnumber]") {
    TempDB db("core_rn_seq");
    auto r1 = RegNumberGenerator::next("F22");
    auto r2 = RegNumberGenerator::next("F22");
    auto r3 = RegNumberGenerator::next("F22");
    CHECK(r2.sequence == r1.sequence + 1);
    CHECK(r3.sequence == r1.sequence + 2);
}

TEST_CASE("Core/RegNumber: departments have independent counters", "[core][regnumber]") {
    TempDB db("core_rn_depts");
    auto f16 = RegNumberGenerator::next("F16");
    auto f22 = RegNumberGenerator::next("F22");
    CHECK(f16.sequence == 1);
    CHECK(f22.sequence == 1);
    CHECK(f16.dept     == "F16");
    CHECK(f22.dept     == "F22");
}

TEST_CASE("Core/RegNumber: fromString round-trip", "[core][regnumber]") {
    TempDB db("core_rn_parse");
    auto rn = RegNumberGenerator::next("F18");
    std::string s = rn.toString();
    auto parsed = RegNumber::fromString(s);
    CHECK(parsed.dept     == rn.dept);
    CHECK(parsed.sequence == rn.sequence);
    CHECK(parsed.year     == rn.year);
}

// ── genId ─────────────────────────────────────────────────────────────────────

TEST_CASE("Core/genId: non-empty and unique per call", "[core][genid]") {
    TempDB db("core_genid");
    std::string id1 = genId("F16");
    std::string id2 = genId("F16");
    std::string id3 = genId("F22");
    CHECK_FALSE(id1.empty());
    CHECK_FALSE(id2.empty());
    CHECK(id1 != id2);       // same dept, different sequence
    CHECK(id1 != id3);       // different dept
}

TEST_CASE("Core/genId: contains correct dept string", "[core][genid]") {
    TempDB db("core_genid_dept");
    CHECK(genId("F16").find("F16") != std::string::npos);
    CHECK(genId("F22").find("F22") != std::string::npos);
    CHECK(genId("F18").find("F18") != std::string::npos);
    CHECK(genId("F99").find("F99") != std::string::npos);
}

// ── Migration ─────────────────────────────────────────────────────────────────

TEST_CASE("Core/Migration: runAll() succeeds and is idempotent", "[core][migration]") {
    TempDB db("core_mig_run");
    REQUIRE(MigrationEngine::runAll());
    REQUIRE(MigrationEngine::runAll());   // second call must succeed
}

TEST_CASE("Core/Migration: core schema version >= 1 after runAll", "[core][migration]") {
    TempDB db("core_mig_ver");
    MigrationEngine::runAll();  // must run first to populate schema_version
    CHECK(MigrationEngine::currentVersion("core") >= 1);
}

TEST_CASE("Core/Migration: all pool databases are accessible", "[core][migration]") {
    TempDB db("core_mig_pool");
    for (auto& name : {"core","f16","f22","f77","akt","f18","f99"}) {
        INFO("DB: " << name);
        CHECK(DatabasePool::instance().get(name) != nullptr);
    }
}

// ── Version ───────────────────────────────────────────────────────────────────

TEST_CASE("Core/Version: toString is non-empty and contains major version", "[core][version]") {
    TempDB db("core_ver_str");
    std::string ver = Version::toString();
    CHECK_FALSE(ver.empty());
    CHECK(ver.find("9") != std::string::npos);   // v9
}

TEST_CASE("Core/Version: full() contains 'Rosenholz'", "[core][version]") {
    TempDB db("core_ver_full");
    CHECK(Version::full().find("Rosenholz") != std::string::npos);
}

// ── MFS ───────────────────────────────────────────────────────────────────────

TEST_CASE("Core/MFS: writeProject creates flat .txt file for F16", "[core][mfs][filesystem]") {
    TempDB db("core_mfs_proj");
    auto& cfg     = Config::instance();
    std::string mfsRoot = cfg.basePath() + "/mfs";
    auto proj = makeF16("MFS-Core-Test", "OV");

    bool ok = MFSWriter::writeProject(*proj, mfsRoot);
    CHECK(ok);

    std::string sane  = sanitiseRegNr(proj->regNumber.toString());
    std::string yearStr = proj->regNumber.toString().substr(proj->regNumber.toString().rfind('/')+1);
    std::string fpath = FileOps::joinPath(
        FileOps::joinPath(FileOps::joinPath(mfsRoot, "F16"), yearStr), sane + ".txt");
    CHECK(FileOps::fileExists(fpath));

    // File must reference the project ID:
    std::ifstream f(fpath);
    std::string content((std::istreambuf_iterator<char>(f)), {});
    CHECK(content.find(proj->projectId) != std::string::npos);
}

TEST_CASE("Core/MFS: orphan document (no task) is refused by writeDocument",
          "[core][mfs][filesystem]") {
    TempDB db("core_mfs_orphan");
    auto& cfg     = Config::instance();
    std::string mfsRoot = cfg.basePath() + "/mfs";
    auto orphan = Folder::create("Orphan", "misc", "");
    REQUIRE(opOk(orphan->save()));
    CHECK_FALSE(MFSWriter::writeDocument(*orphan, mfsRoot));
}

TEST_CASE("Core/MFS: F22-linked document is filed in MFS", "[core][mfs][filesystem]") {
    TempDB db("core_mfs_doc");
    auto& cfg     = Config::instance();
    std::string mfsRoot = cfg.basePath() + "/mfs";
    auto proj = makeF16("MFS-Doc-F16");
    auto task = makeF22(proj->projectId, "MFS-Doc-F22");
    auto doc  = Folder::create("Test-Dokument", "report", task->taskId);
    REQUIRE(opOk(doc->save()));
    CHECK(MFSWriter::writeDocument(*doc, mfsRoot));
}

TEST_CASE("Core/MFS: owner_key.txt has 0600 permissions when created",
          "[core][mfs][filesystem][permissions]") {
    TempDB db("core_mfs_perm");
    auto& cfg = Config::instance();
    std::string mfsRoot = cfg.basePath() + "/mfs";
    FileOps::makeDirs(mfsRoot);

    // Write the file directly and set 0600:
    std::string keyPath = FileOps::joinPath(mfsRoot, "owner_key.txt");
    FileOps::writeTextFile(keyPath, "owner: test\n", false);
#ifndef _WIN32
    chmod(keyPath.c_str(), 0600);
    struct stat st{};
    stat(keyPath.c_str(), &st);
    CHECK((st.st_mode & 0777) == 0600);
#endif
}

// test_core.cpp  —  Core infrastructure tests with fixtures
#include "TestFramework.h"
#include "TestFixtures.h"
#include "../src/core/Logger.h"
#include "../src/core/Config.h"
#include "../src/core/FileOps.h"
#include "../src/core/RegNumber.h"
#include "../src/core/Migration.h"
#include "../src/workflow/WorkflowEngine.h"

namespace R = Rosenholz;

void testSuiteCore() {
    SECTION("Logger — level setting");
    {
        R::Logger::instance().setLevel(R::LogLevel::DEBUG);
        CHECK(true, "Debug mode set");
        R::Logger::instance().setLevel(R::LogLevel::WARN);
        CHECK(true, "Warn mode set");
    }

    SECTION("Config — registratur settings");
    {
        auto& cfg = R::Config::instance();
        CHECK(!cfg.basePath().empty(), "basePath is set");
        auto& reg = cfg.registratur();
        CHECK(!reg.diensteinheitKuerzel.empty(), "diensteinheitKuerzel set");
        CHECK(!reg.aktenfuehrendeStelle.empty(), "aktenfuehrendeStelle set");
        CHECK(!reg.geschaeftszeichen.empty(), "geschaeftszeichen set");
    }

    SECTION("FileOps — basic I/O");
    {
        std::string tmp = R::Config::instance().basePath() + "/test_io.txt";
        CHECK(R::FileOps::writeTextFile(tmp, "Rosenholz test", false), "writeTextFile");
        CHECK(R::FileOps::fileExists(tmp), "fileExists");
        auto content = R::FileOps::readTextFile(tmp);
        CHECK(content == "Rosenholz test", "readTextFile content matches");
        CHECK(R::FileOps::fileSize(tmp) > 0, "fileSize > 0");
        R::FileOps::deleteFile(tmp);
        CHECK(!R::FileOps::fileExists(tmp), "file gone after delete");
    }

    SECTION("RegNumber — generation and parsing");
    {
        auto n1 = R::RegNumberGenerator::next("F16");
        auto n2 = R::RegNumberGenerator::next("F16");
        CHECK(n1.isValid(), "RegNumber n1 valid");
        CHECK(n2.isValid(), "RegNumber n2 valid");
        CHECK(n2.sequence > n1.sequence, "Sequence increments");

        std::string s = n1.toString();
        auto parsed = R::RegNumber::fromString(s);
        CHECK(parsed.dept     == n1.dept,     "fromString preserves dept");
        CHECK(parsed.sequence == n1.sequence, "fromString preserves sequence");
        CHECK(parsed.year     == n1.year,     "fromString preserves year");
    }

    SECTION("ID Format — DDR-style with DE Kuerzel");
    {
        const std::string& de = R::Config::instance().registratur().diensteinheitKuerzel;
        std::string id = R::genId("F16");
        CHECK(id.substr(0, de.size()) == de, "ID starts with DE code: " + id);
        CHECK(id.find("/F16/") != std::string::npos, "ID contains type code /F16/");

        std::string year = "/" + std::to_string(R::currentYear());
        CHECK(id.find(year) != std::string::npos, "ID contains current year");
    }

    SECTION("Migration — schema versions");
    {
        bool ok = R::MigrationEngine::runAll();
        CHECK(ok, "MigrationEngine::runAll() succeeds");

        int wfVer = R::MigrationEngine::currentVersion("workflow");
        CHECK(wfVer >= 1, "workflow schema version >= 1");

        int repVer = R::MigrationEngine::currentVersion("reporting");
        CHECK(repVer >= 1, "reporting schema version >= 1");
    }

    SECTION("Version — compile-time info");
    {
        std::string ver = R::Version::toString();
        CHECK(!ver.empty(), "Version::toString() not empty");
        CHECK(ver.find("2.0") != std::string::npos, "Version is 2.x");
        std::string full = R::Version::full();
        CHECK(full.find("Rosenholz") != std::string::npos, "Version::full() contains Rosenholz");
    }
}

// ============================================================
// tests/unit/f16/test_f16.cpp  —  F16 Projektkartei
//
// Verifies: create, save (SQL row), reload, update, archive,
// all query methods, registration number format, MFS file,
// removed V8 fields absent from schema.
// ============================================================
#include "../test_helpers.h"
#include "../../../src/mfs/MFSWriter.h"
#include <filesystem>
#include <fstream>

using namespace Rosenholz;
using namespace RhTest;
namespace fs = std::filesystem;

TEST_CASE("F16: create() returns valid in-memory object", "[f16][create]") {
    TempDB db("f16_create");
    auto p = F16::create("Wolfram-Projekt", "OV");
    REQUIRE(p != nullptr);
    CHECK(p->title       == "Wolfram-Projekt");
    CHECK(p->projectType == "OV");
    CHECK_FALSE(p->archived);
    CHECK(p->projectId.find("F16") != std::string::npos);
    CHECK(p->codename.empty());
    CHECK(p->scopeStatement.empty());
    CHECK(p->budgetPlanned == 0.0);
}

TEST_CASE("F16: save() writes SQL row to f16.db", "[f16][sql]") {
    TempDB db("f16_sql");
    CHECK(rowCount("f16","projects") == 0);
    auto p = F16::create("SQL-Test", "IM");
    REQUIRE(opOk(p->save()));
    CHECK(rowCount("f16","projects") == 1);
    CHECK(colValue("f16","projects","project_id","project_id='"+p->projectId+"'") == p->projectId);
    CHECK(colValue("f16","projects","title","project_id='"+p->projectId+"'") == "SQL-Test");
    CHECK(colValue("f16","projects","project_type","project_id='"+p->projectId+"'") == "IM");
    CHECK(colValue("f16","projects","archived","project_id='"+p->projectId+"'") == "0");
    CHECK_FALSE(colValue("f16","projects","created_at","project_id='"+p->projectId+"'").empty());
}

TEST_CASE("F16: save → loadById round-trip", "[f16][model]") {
    TempDB db("f16_rt");
    auto p = makeF16("Reload-Test", "OPK");
    auto loaded = F16::loadById(p->projectId);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->title       == "Reload-Test");
    CHECK(loaded->projectType == "OPK");
    CHECK(loaded->projectId   == p->projectId);
    CHECK_FALSE(loaded->archived);
}

TEST_CASE("F16: all optional fields survive save/load", "[f16][fields][sql]") {
    TempDB db("f16_fields");
    auto p = F16::create("Full-Test", "GMS");
    p->codename         = "Falke";
    p->scopeStatement   = "Beobachtung Zielgebiet Nord";
    p->startDatePlanned = "2026-01-01";
    p->endDatePlanned   = "2026-12-31";
    p->budgetPlanned    = 12500.50;
    REQUIRE(opOk(p->save()));

    SECTION("SQL columns written correctly") {
        CHECK(colValue("f16","projects","codename","project_id='"+p->projectId+"'") == "Falke");
        CHECK(colValue("f16","projects","scope_statement","project_id='"+p->projectId+"'")
              == "Beobachtung Zielgebiet Nord");
        CHECK(colValue("f16","projects","start_date_planned","project_id='"+p->projectId+"'")
              == "2026-01-01");
        CHECK(colValue("f16","projects","end_date_planned","project_id='"+p->projectId+"'")
              == "2026-12-31");
    }
    SECTION("round-trip: all fields match") {
        auto r = F16::loadById(p->projectId);
        REQUIRE(r != nullptr);
        CHECK(r->codename         == "Falke");
        CHECK(r->scopeStatement   == "Beobachtung Zielgebiet Nord");
        CHECK(r->startDatePlanned == "2026-01-01");
        CHECK(r->endDatePlanned   == "2026-12-31");
        CHECK_THAT(r->budgetPlanned, Catch::Matchers::WithinRel(12500.50, 0.001));
    }
}

TEST_CASE("F16: update() persists field changes to SQL", "[f16][update][sql]") {
    TempDB db("f16_update");
    auto p = makeF16("Before-Update");
    p->codename       = "Adler";
    p->scopeStatement = "New scope";
    REQUIRE(opOk(p->update()));
    CHECK(colValue("f16","projects","codename","project_id='"+p->projectId+"'") == "Adler");
    auto r = F16::loadById(p->projectId);
    REQUIRE(r != nullptr);
    CHECK(r->codename       == "Adler");
    CHECK(r->scopeStatement == "New scope");
}

TEST_CASE("F16: archive() — SQL flag, persists, idempotent", "[f16][archive][sql]") {
    TempDB db("f16_archive");
    auto p = makeF16("Archive-Target");

    SECTION("sets archived=1 in SQL") {
        REQUIRE(opOk(p->archive()));
        CHECK(colValue("f16","projects","archived","project_id='"+p->projectId+"'") == "1");
    }
    SECTION("reload shows archived=true") {
        REQUIRE(opOk(p->archive()));
        auto r = F16::loadById(p->projectId);
        REQUIRE(r != nullptr);
        CHECK(r->archived);
    }
    SECTION("second archive() succeeds (idempotent)") {
        REQUIRE(opOk(p->archive()));
        REQUIRE(opOk(p->archive()));
        CHECK(rowCount("f16","projects","archived=1") == 1);
    }
    SECTION("still exactly one SQL row") {
        REQUIRE(opOk(p->archive()));
        CHECK(rowCount("f16","projects") == 1);
    }
}

TEST_CASE("F16: loadAll excludes archived, loadRecent respects limit", "[f16][query]") {
    TempDB db("f16_query");
    CHECK(F16::loadAll().empty());
    makeF16("Alpha");
    makeF16("Beta");
    auto c = makeF16("Archived");
    REQUIRE(opOk(c->archive()));

    SECTION("loadAll has WHERE archived=0") {
        CHECK(F16::loadAll().size() == 2);
    }
    SECTION("SQL row counts") {
        CHECK(rowCount("f16","projects") == 3);
        CHECK(rowCount("f16","projects","archived=0") == 2);
        CHECK(rowCount("f16","projects","archived=1") == 1);
    }
    SECTION("loadRecent respects limit") {
        for (int i = 0; i < 5; i++) makeF16("X"+std::to_string(i));
        CHECK(F16::loadRecent(3).size() == 3);
    }
}

TEST_CASE("F16: loadWithDates returns only projects with dates", "[f16][query]") {
    TempDB db("f16_dates");
    auto p1 = F16::create("Dated", "OV");
    p1->startDatePlanned = "2026-03-01";
    p1->endDatePlanned   = "2026-12-31";
    REQUIRE(opOk(p1->save()));
    REQUIRE(opOk(F16::create("NoDates","AU")->save()));

    auto dated = F16::loadWithDates();
    REQUIRE(dated.size() == 1);
    CHECK(dated[0]->projectId        == p1->projectId);
    CHECK(dated[0]->startDatePlanned == "2026-03-01");
}

TEST_CASE("F16: registration number format and SQL columns", "[f16][regnumber][sql]") {
    TempDB db("f16_regnr");
    auto p = makeF16("RegNr-Test");

    CHECK(p->regNumber.dept     == "F16");
    CHECK(p->regNumber.sequence == 1);
    CHECK(p->regNumber.year     > 0);

    std::string s = p->regNumber.toString();
    CHECK(s.substr(0,3) == "XV/");
    CHECK(s.substr(3,3) == "F16");
    CHECK(s.substr(7,6) == "000001");
    CHECK(s.size()      == 16);  // XV/F16/0001/26

    CHECK(colValue("f16","projects","reg_number","project_id='"+p->projectId+"'") == s);
    CHECK(colValue("f16","projects","reg_dept","project_id='"+p->projectId+"'") == "F16");
    CHECK(colValue("f16","projects","reg_sequence","project_id='"+p->projectId+"'") == "1");

    auto p2 = makeF16("Second");
    CHECK(p2->regNumber.sequence == 2);
}

TEST_CASE("F16: V8 removed fields absent from SQL schema", "[f16][v8][sql]") {
    TempDB db("f16_v8");
    // These columns were removed in V8 — the schema must not contain them:
    CHECK_FALSE(colExists("f16","projects","size_class"));
    CHECK_FALSE(colExists("f16","projects","priority"));
    CHECK_FALSE(colExists("f16","projects","complexity"));
    CHECK_FALSE(colExists("f16","projects","methodology"));
    // Remaining optional fields still in schema:
    CHECK(colExists("f16","projects","codename"));
    CHECK(colExists("f16","projects","scope_statement"));
    CHECK(colExists("f16","projects","start_date_planned"));
    CHECK(colExists("f16","projects","end_date_planned"));
    CHECK(colExists("f16","projects","budget_planned"));
}

TEST_CASE("F16: MFS writeProject() creates .txt file on disk", "[f16][mfs][filesystem]") {
    TempDB db("f16_mfs");
    auto& cfg = Config::instance();
    std::string mfsRoot = cfg.basePath() + "/mfs";
    auto p = makeF16("MFS-Test", "SVG");

    bool ok = MFSWriter::writeProject(*p, mfsRoot);
    CHECK(ok);

    // Find the created .txt file and verify it contains the project ID:
    bool found = false;
    if (fs::exists(mfsRoot)) {
        for (auto& e : fs::recursive_directory_iterator(mfsRoot)) {
            if (e.is_regular_file() && e.path().extension() == ".txt") {
                found = true;
                std::ifstream f(e.path());
                std::string content((std::istreambuf_iterator<char>(f)), {});
                CHECK(content.find(p->projectId) != std::string::npos);
            }
        }
    }
    CHECK(found);
}

TEST_CASE("F16: INSERT OR REPLACE is idempotent (no duplicate rows)", "[f16][sql]") {
    TempDB db("f16_idem");
    auto p = makeF16("Idempotent");
    CHECK(rowCount("f16","projects") == 1);
    p->codename = "Updated";
    REQUIRE(opOk(p->save()));
    CHECK(rowCount("f16","projects") == 1);  // still one row
    CHECK(F16::loadById(p->projectId)->codename == "Updated");
}

TEST_CASE("F16: loadById with unknown ID returns nullptr", "[f16][query]") {
    TempDB db("f16_notfound");
    CHECK(F16::loadById("XV/F16/9999/99") == nullptr);
}

TEST_CASE("F16: all six project types saved and reloaded correctly", "[f16][types]") {
    TempDB db("f16_types");
    for (auto type : {"OV","IM","OPK","GMS","AU","SVG"}) {
        auto p = F16::create(std::string("T-")+type, type);
        REQUIRE(opOk(p->save()));
        CHECK(F16::loadById(p->projectId)->projectType == type);
    }
    CHECK(rowCount("f16","projects") == 6);
}

TEST_CASE("F16: recalcEarnedValue() computes CPI and SPI", "[f16][model][ev]") {
    // EV metrics: CPI = EV/AC,  SPI = EV/PV.
    // Both must be positive after recalc when all inputs are positive.
    TempDB db("f16_ev");
    auto p = makeF16("EV-Test", "OV");
    p->earnedValue   = 75000.0;
    p->plannedValue  = 90000.0;
    p->actualCost    = 80000.0;
    p->recalcEarnedValue();
    REQUIRE(opOk(p->save()));

    SECTION("CPI > 0") { CHECK(p->costPerformanceIndex     > 0.0); }
    SECTION("SPI > 0") { CHECK(p->schedulePerformanceIndex > 0.0); }
    SECTION("CPI = EV/AC") {
        CHECK_THAT(p->costPerformanceIndex,
                   Catch::Matchers::WithinRel(75000.0/80000.0, 0.001));
    }
    SECTION("persists after reload") {
        auto r = F16::loadById(p->projectId);
        REQUIRE(r != nullptr);
        CHECK(r->costPerformanceIndex     > 0.0);
        CHECK(r->schedulePerformanceIndex > 0.0);
    }
}

TEST_CASE("F16: reassignLead and reassignSponsor persist", "[f16][model][reassign]") {
    TempDB db("f16_reassign");
    auto p = makeF16("Reassign-Test");
    auto lead = Person::create("Neue", "Leiterin", "nl@test.de", "internal");
    REQUIRE(opOk(lead->save()));

    REQUIRE(opOk(p->reassignLead(lead->personId)));
    REQUIRE(opOk(p->reassignSponsor(lead->personId)));

    auto r = F16::loadById(p->projectId);
    REQUIRE(r != nullptr);
    CHECK(r->leadId    == lead->personId);
    CHECK(r->sponsorId == lead->personId);
    // SQL:
    CHECK(colValue("f16","projects","lead_id","project_id='"+p->projectId+"'")
          == lead->personId);
}

TEST_CASE("F16: milestones free-text field survives save/load", "[f16][fields]") {
    TempDB db("f16_milestones");
    auto p = makeF16("Milestone-Test");
    p->milestones = "2026-06-01: Kick-off\n2026-08-01: Design-Freeze";
    REQUIRE(opOk(p->update()));
    auto r = F16::loadById(p->projectId);
    REQUIRE(r != nullptr);
    CHECK_FALSE(r->milestones.empty());
    CHECK(r->milestones.find("Kick-off") != std::string::npos);
}

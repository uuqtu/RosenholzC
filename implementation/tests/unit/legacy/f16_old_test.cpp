// ============================================================
// tests/unit/legacy/f16_old_test.cpp
//
// Translated from tests/test_model.cpp (F16 sections) to Catch2 v3.
// Tests: create, save, EV metrics, reassignLead/Sponsor, loadAll,
// loadByStatus, milestones field.
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/person/Person.h"
#include "../../../src/workflow/F77Workflow.h"

using namespace Rosenholz;
using namespace RhTest;

// ── Helpers ──────────────────────────────────────────────────

static std::shared_ptr<Person> makePerson(const std::string& first = "Test",
                                           const std::string& last  = "Person") {
    auto p = Person::create(first, last, first+"."+last+"@test.de", "internal");
    REQUIRE(p != nullptr);
    REQUIRE(opOk(p->save()));
    return p;
}

// ── Tests ────────────────────────────────────────────────────

TEST_CASE("Legacy/F16: create and save", "[legacy][f16][model]") {
    TempDB db("leg_f16_create");
    auto p = makeF16("Test-Vorgang Alpha", "OV");

    SECTION("ID contains F16") {
        CHECK(p->projectId.find("F16") != std::string::npos);
    }
    SECTION("title matches") {
        CHECK(p->title == "Test-Vorgang Alpha");
    }
    SECTION("loadById finds it") {
        auto loaded = F16::loadById(p->projectId);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->title == "Test-Vorgang Alpha");
    }
}

TEST_CASE("Legacy/F16: EV metrics calculated after save", "[legacy][f16][ev]") {
    TempDB db("leg_f16_ev");
    auto p = makeF16("EV-Test", "OV");
    p->earnedValue   = 75000.0;
    p->plannedValue  = 90000.0;
    p->actualCost    = 80000.0;
    p->recalcEarnedValue();
    REQUIRE(opOk(p->save()));
    CHECK(p->costPerformanceIndex     > 0.0);
    CHECK(p->schedulePerformanceIndex > 0.0);
}

TEST_CASE("Legacy/F16: reassignLead and reassignSponsor", "[legacy][f16][model]") {
    TempDB db("leg_f16_reassign");
    auto p = makeF16("Reassign-Test");
    auto lead = makePerson("Neue", "Leiterin");
    CHECK(opOk(p->reassignLead(lead->personId)));
    CHECK(opOk(p->reassignSponsor(lead->personId)));
}

TEST_CASE("Legacy/F16: loadAll and loadByStatus return results", "[legacy][f16][query]") {
    TempDB db("leg_f16_load");
    makeF16("Alpha");
    makeF16("Beta");
    CHECK_FALSE(F16::loadAll().empty());
    CHECK_FALSE(F16::loadByStatus("in_work").empty());
}

TEST_CASE("Legacy/F16: milestones free-text field persists", "[legacy][f16][fields]") {
    TempDB db("leg_f16_ms");
    auto p = makeF16("Milestone-Test");
    p->milestones = "2026-06-01: Kick-off\n2026-08-01: Design-Freeze";
    REQUIRE(opOk(p->update()));
    auto r = F16::loadById(p->projectId);
    REQUIRE(r != nullptr);
    CHECK_FALSE(r->milestones.empty());
    CHECK(r->milestones.find("Kick-off") != std::string::npos);
}

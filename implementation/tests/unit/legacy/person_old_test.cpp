// ============================================================
// tests/unit/legacy/person_old_test.cpp
//
// Translated from tests/test_model.cpp  (Person + Team sections)
// to Catch2 v3.
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/person/Person.h"
#include "../../../src/model/team/Team.h"

using namespace Rosenholz;
using namespace RhTest;

// ── Person ───────────────────────────────────────────────────

TEST_CASE("Legacy/Person: create, save, loadById", "[legacy][person]") {
    TempDB db("leg_per_basic");
    auto p = Person::create("Heinrich", "Schmitt", "hs@test.de", "internal");
    REQUIRE(p != nullptr);
    p->roleTitle  = "Test Role";
    p->department = "Test Dept";
    p->dayRate    = 800.0;
    REQUIRE(opOk(p->save()));

    SECTION("ID contains /PER/") {
        CHECK(p->personId.find("/PER/") != std::string::npos);
    }
    SECTION("loadById returns same fields") {
        auto loaded = Person::loadById(p->personId);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->firstName == "Heinrich");
        CHECK(loaded->lastName  == "Schmitt");
        CHECK(loaded->email     == "hs@test.de");
    }
}

TEST_CASE("Legacy/Person: search by lastName", "[legacy][person]") {
    TempDB db("leg_per_search");
    auto p = Person::create("Heinrich", "Schmitt", "hs2@test.de", "internal");
    REQUIRE(opOk(p->save()));
    auto found = Person::search("Schmitt");
    bool has = false;
    for (auto& x : found) if (x->personId == p->personId) has = true;
    CHECK(has);
}

TEST_CASE("Legacy/Person: reassignManager", "[legacy][person]") {
    TempDB db("leg_per_mgr");
    auto p = Person::create("Max", "Muster", "mm@test.de", "internal");
    REQUIRE(opOk(p->save()));
    p->reassignManager(p->personId);   // self-manage (just tests the call)
    CHECK_FALSE(p->managerId.empty());
}

TEST_CASE("Legacy/Person: setStatus persists", "[legacy][person]") {
    TempDB db("leg_per_status");
    auto p = Person::create("Anna", "Test", "at@test.de", "internal");
    REQUIRE(opOk(p->save()));
    p->setStatus("on-leave");
    auto chk = Person::loadById(p->personId);
    REQUIRE(chk != nullptr);
    CHECK(chk->status == "on-leave");
}

// ── Team ─────────────────────────────────────────────────────

TEST_CASE("Legacy/Team: hierarchy and parent linkage", "[legacy][team]") {
    TempDB db("leg_team_hier");
    auto lead   = Person::create("Parent", "Lead", "pl@test.de", "internal");
    REQUIRE(opOk(lead->save()));
    auto parent = Team::create("Parent Diensteinheit", "division");
    parent->leadId = lead->personId;
    REQUIRE(opOk(parent->save()));
    auto child  = Team::create("Child Diensteinheit", "squad");
    REQUIRE(opOk(child->save()));
    child->parentTeamId = parent->teamId;
    REQUIRE(opOk(child->update()));
    auto loaded = Team::loadById(child->teamId);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->parentTeamId == parent->teamId);
}

TEST_CASE("Legacy/Team: addMember and loadMembers", "[legacy][team]") {
    TempDB db("leg_team_mbr");
    auto lead = Person::create("Team", "Lead", "tl@test.de", "internal");
    REQUIRE(opOk(lead->save()));
    auto team = Team::create("Test Team", "engineering");
    team->leadId = lead->personId;
    REQUIRE(opOk(team->save()));

    auto p1 = Person::create("Klaus", "Bauer", "kb@test.de", "internal");
    REQUIRE(opOk(p1->save()));
    auto member = team->addMember(p1->personId, "developer");
    REQUIRE(member != nullptr);
    member->isLead          = true;
    member->allocationPct   = 80.0;
    member->roleCategory    = "technical";
    member->memberType      = "core";
    REQUIRE(opOk(member->save()));

    team->loadMembers();
    REQUIRE_FALSE(team->members.empty());
    CHECK(team->members[0]->isLead);
}

TEST_CASE("Legacy/Team: reassignLead persists", "[legacy][team]") {
    TempDB db("leg_team_lead");
    auto lead = Person::create("Old", "Lead", "ol@test.de", "internal");
    REQUIRE(opOk(lead->save()));
    auto newLead = Person::create("New", "Lead", "nl@test.de", "internal");
    REQUIRE(opOk(newLead->save()));
    auto team = Team::create("Lead-Test Team", "squad");
    team->leadId = lead->personId;
    REQUIRE(opOk(team->save()));
    team->reassignLead(newLead->personId);
    auto chk = Team::loadById(team->teamId);
    REQUIRE(chk != nullptr);
    CHECK(chk->leadId == newLead->personId);
}

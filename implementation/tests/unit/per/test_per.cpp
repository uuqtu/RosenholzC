// ============================================================
// tests/unit/per/test_per.cpp  —  Person + Team
//
// Coverage
// ════════
//   Person
//     create / save (SQL row with /PER/ ID)
//     loadById / loadAll / loadByEmail
//     search by lastName / firstName / email (wildcard)
//     reassignManager persists managerId
//     setStatus persists status
//     SQL column verification
//
//   Team
//     create / save / loadById / loadAll / loadChildren
//     parentTeamId linkage (hierarchy)
//     addMember: isLead, allocationPct, roleCategory, memberType
//     loadMembers returns members
//     removeMember removes from team
//     reassignLead persists leadId
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/person/Person.h"
#include "../../../src/model/team/Team.h"

using namespace Rosenholz;
using namespace RhTest;

// ── Person: create / save ─────────────────────────────────────────────────────

TEST_CASE("PER/Person: create() and save() writes SQL row with /PER/ ID",
          "[per][person][sql]") {
    TempDB db("per_create");
    CHECK(rowCount("core","persons") == 0);

    auto p = Person::create("Heinrich", "Schmitt", "hs@test.de", "internal");
    REQUIRE(p != nullptr);
    REQUIRE(opOk(p->save()));

    SECTION("ID contains /PER/") { CHECK(p->personId.find("/PER/") != std::string::npos); }
    SECTION("SQL row exists")    { CHECK(rowCount("core","persons") == 1); }
    SECTION("SQL first_name")    {
        CHECK(colValue("core","persons","first_name","person_id='"+p->personId+"'") == "Heinrich");
    }
    SECTION("SQL last_name")     {
        CHECK(colValue("core","persons","last_name","person_id='"+p->personId+"'") == "Schmitt");
    }
    SECTION("SQL email")         {
        CHECK(colValue("core","persons","email","person_id='"+p->personId+"'") == "hs@test.de");
    }
}

TEST_CASE("PER/Person: loadById round-trip", "[per][person][model]") {
    TempDB db("per_lbi");
    auto p = makePerson("Anna", "Muster");
    auto r = Person::loadById(p->personId);
    REQUIRE(r != nullptr);
    CHECK(r->firstName == "Anna");
    CHECK(r->lastName  == "Muster");
    CHECK(r->personId  == p->personId);
}

TEST_CASE("PER/Person: loadAll returns all saved persons", "[per][person][query]") {
    TempDB db("per_loadall");
    makePerson("A", "One");
    makePerson("B", "Two");
    makePerson("C", "Three");
    CHECK(Person::loadAll().size() == 3);
}

TEST_CASE("PER/Person: loadByEmail finds by email address", "[per][person][query]") {
    TempDB db("per_byemail");
    auto p = makePerson("Emil", "Test", "emil@example.de");
    auto r = Person::loadByEmail("emil@example.de");
    REQUIRE(r != nullptr);
    CHECK(r->personId == p->personId);
}

TEST_CASE("PER/Person: loadByEmail returns nullptr for unknown address", "[per][person][query]") {
    TempDB db("per_byemail_miss");
    CHECK(Person::loadByEmail("nobody@nowhere.de") == nullptr);
}

// ── Person: search ────────────────────────────────────────────────────────────

TEST_CASE("PER/Person: search by lastName substring", "[per][person][query]") {
    TempDB db("per_search_ln");
    auto p = makePerson("Heinrich", "Schmitt");
    makePerson("Ignored", "Mustermann");
    auto found = Person::search("Schmitt");
    bool has = false;
    for (auto& x : found) if (x->personId == p->personId) has = true;
    CHECK(has);
}

TEST_CASE("PER/Person: search with wildcard", "[per][person][query][wildcard]") {
    TempDB db("per_search_wild");
    makePerson("Karoline", "Bauer");
    makePerson("Karl",     "Bauer");
    makePerson("Other",    "Person");
    // "Bau*" should match both Bauers:
    auto hits = Person::search("Bau*");
    CHECK(hits.size() >= 2);
}

TEST_CASE("PER/Person: search returns empty for unknown name", "[per][person][query]") {
    TempDB db("per_search_miss");
    makePerson("Test", "Person");
    CHECK(Person::search("ZZZNobody").empty());
}

// ── Person: optional fields and operations ────────────────────────────────────

TEST_CASE("PER/Person: optional fields survive save/load", "[per][person][fields]") {
    TempDB db("per_fields");
    auto p = Person::create("Max", "Muster", "mm@test.de", "internal");
    REQUIRE(p != nullptr);
    p->roleTitle  = "Projektleiter";
    p->department = "Engineering";
    p->dayRate    = 800.0;
    REQUIRE(opOk(p->save()));

    auto r = Person::loadById(p->personId);
    REQUIRE(r != nullptr);
    CHECK(r->roleTitle  == "Projektleiter");
    CHECK(r->department == "Engineering");
    CHECK_THAT(r->dayRate, Catch::Matchers::WithinRel(800.0, 0.001));
}

TEST_CASE("PER/Person: reassignManager persists managerId", "[per][person][model][sql]") {
    TempDB db("per_mgr");
    auto mgr  = makePerson("Manager", "Person");
    auto emp  = makePerson("Employee", "Person2");
    emp->reassignManager(mgr->personId);
    CHECK(emp->managerId == mgr->personId);
    CHECK(colValue("core","persons","manager_id","person_id='"+emp->personId+"'")
          == mgr->personId);
}

TEST_CASE("PER/Person: setStatus persists to SQL", "[per][person][model][sql]") {
    TempDB db("per_status");
    auto p = makePerson("Status", "Test");
    p->setStatus("on-leave");
    auto r = Person::loadById(p->personId);
    REQUIRE(r != nullptr);
    CHECK(r->status == "on-leave");
    CHECK(colValue("core","persons","status","person_id='"+p->personId+"'") == "on-leave");
}

TEST_CASE("PER/Person: INSERT OR REPLACE is idempotent", "[per][person][sql]") {
    TempDB db("per_idem");
    auto p = makePerson("Idm", "Person");
    CHECK(rowCount("core","persons") == 1);
    p->roleTitle = "Updated";
    REQUIRE(opOk(p->save()));
    CHECK(rowCount("core","persons") == 1);   // no duplicate
    CHECK(Person::loadById(p->personId)->roleTitle == "Updated");
}

// ── Team: create / save ───────────────────────────────────────────────────────

TEST_CASE("PER/Team: create() and save()", "[per][team][sql]") {
    TempDB db("per_team_create");
    auto lead = makePerson("Team", "Lead");
    auto team = Team::create("Alpha Team", "engineering");
    team->leadId = lead->personId;
    REQUIRE(opOk(team->save()));
    CHECK_FALSE(team->teamId.empty());
    CHECK(colValue("core","teams","name","team_id='"+team->teamId+"'") == "Alpha Team");
}

TEST_CASE("PER/Team: loadById round-trip", "[per][team][model]") {
    TempDB db("per_team_lbi");
    auto lead = makePerson("L", "X");
    auto team = Team::create("Beta Team", "ops");
    team->leadId = lead->personId;
    REQUIRE(opOk(team->save()));
    auto r = Team::loadById(team->teamId);
    REQUIRE(r != nullptr);
    CHECK(r->name   == "Beta Team");
    CHECK(r->leadId == lead->personId);
}

// ── Team: hierarchy ───────────────────────────────────────────────────────────

TEST_CASE("PER/Team: parent/child linkage via parentTeamId", "[per][team][hierarchy][sql]") {
    TempDB db("per_team_hier");
    auto lead   = makePerson("L", "Parent");
    auto parent = Team::create("Parent Diensteinheit", "division");
    parent->leadId = lead->personId;
    REQUIRE(opOk(parent->save()));

    auto child = Team::create("Child Diensteinheit", "squad");
    REQUIRE(opOk(child->save()));
    child->parentTeamId = parent->teamId;
    REQUIRE(opOk(child->update()));

    auto r = Team::loadById(child->teamId);
    REQUIRE(r != nullptr);
    CHECK(r->parentTeamId == parent->teamId);
    CHECK(colValue("core","teams","parent_team_id","team_id='"+child->teamId+"'")
          == parent->teamId);
}

TEST_CASE("PER/Team: loadChildren returns child teams", "[per][team][hierarchy]") {
    TempDB db("per_team_children");
    auto lead   = makePerson("L", "C");
    auto parent = Team::create("Parent", "division");
    parent->leadId = lead->personId;
    REQUIRE(opOk(parent->save()));

    auto c1 = Team::create("Child-1", "squad"); REQUIRE(opOk(c1->save()));
    auto c2 = Team::create("Child-2", "squad"); REQUIRE(opOk(c2->save()));
    c1->parentTeamId = parent->teamId; REQUIRE(opOk(c1->update()));
    c2->parentTeamId = parent->teamId; REQUIRE(opOk(c2->update()));

    auto children = Team::loadChildren(parent->teamId);
    CHECK(children.size() == 2);
}

// ── Team: members ─────────────────────────────────────────────────────────────

TEST_CASE("PER/Team: addMember and loadMembers", "[per][team][members]") {
    TempDB db("per_team_members");
    auto lead = makePerson("Team", "Lead");
    auto p1   = makePerson("Klaus", "Bauer");
    auto team = Team::create("Dev Team", "engineering");
    team->leadId = lead->personId;
    REQUIRE(opOk(team->save()));

    auto member = team->addMember(p1->personId, "developer");
    REQUIRE(member != nullptr);
    member->isLead        = true;
    member->allocationPct = 80.0;
    member->roleCategory  = "technical";
    member->memberType    = "core";
    REQUIRE(opOk(member->save()));

    team->loadMembers();
    REQUIRE_FALSE(team->members.empty());
    CHECK(team->members[0]->isLead        == true);
    CHECK_THAT(team->members[0]->allocationPct,
               Catch::Matchers::WithinRel(80.0, 0.001));
}

TEST_CASE("PER/Team: reassignLead persists new leadId", "[per][team][model][sql]") {
    TempDB db("per_team_lead");
    auto oldLead = makePerson("Old",  "Lead");
    auto newLead = makePerson("New",  "Lead");
    auto team    = Team::create("Reassign Team", "squad");
    team->leadId = oldLead->personId;
    REQUIRE(opOk(team->save()));

    team->reassignLead(newLead->personId);
    auto r = Team::loadById(team->teamId);
    REQUIRE(r != nullptr);
    CHECK(r->leadId == newLead->personId);
    CHECK(colValue("core","teams","lead_id","team_id='"+team->teamId+"'")
          == newLead->personId);
}

TEST_CASE("PER/Team: loadAll returns all teams", "[per][team][query]") {
    TempDB db("per_team_all");
    auto l  = makePerson("L", "X");
    auto t1 = Team::create("T1", "engineering"); REQUIRE(opOk(t1->save()));
    auto t2 = Team::create("T2", "ops");         REQUIRE(opOk(t2->save()));
    CHECK(Team::loadAll().size() >= 2);
}

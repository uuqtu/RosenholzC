// ============================================================
// tests/unit/legacy/f22_old_test.cpp
//
// Translated from tests/test_model.cpp  (F22 section) to Catch2 v3.
// Tests: create/save, hierarchy (parent/child), reassignTo,
// reassignParent, loadForProject, loadChildren.
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/person/Person.h"

using namespace Rosenholz;
using namespace RhTest;

TEST_CASE("Legacy/F22: create and save", "[legacy][f22][model]") {
    TempDB db("leg_f22_create");
    auto proj = makeF16("F22-Parent-F16");
    auto task = makeF22(proj->projectId, "Haupt-Aufgabe");

    SECTION("ID contains /F22/") {
        CHECK(task->taskId.find("/F22/") != std::string::npos);
    }
    SECTION("loadById returns it") {
        auto loaded = F22::loadById(task->taskId);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->title == "Haupt-Aufgabe");
    }
}

TEST_CASE("Legacy/F22: child task linked to parent", "[legacy][f22][hierarchy]") {
    TempDB db("leg_f22_child");
    auto proj  = makeF16("Hier-F16");
    auto parent = makeF22(proj->projectId, "Haupt-Aufgabe");
    auto child  = F22::create(proj->projectId, "Kind-Aufgabe", "", parent->taskId);
    child->wbsCode = "1.1.1";
    REQUIRE(opOk(child->save()));

    auto children = F22::loadChildren(parent->taskId);
    REQUIRE_FALSE(children.empty());
    CHECK(children[0]->parentTaskId == parent->taskId);
}

TEST_CASE("Legacy/F22: update percentComplete and effortActualHrs", "[legacy][f22][fields]") {
    TempDB db("leg_f22_update");
    auto proj = makeF16("Update-F16");
    auto task = makeF22(proj->projectId, "Update-Task");
    task->percentComplete  = 25;
    task->effortActualHrs  = 20.0;
    REQUIRE(opOk(task->save()));
    auto loaded = F22::loadById(task->taskId);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->percentComplete == 25);
    CHECK_THAT(loaded->effortActualHrs, Catch::Matchers::WithinRel(20.0, 0.001));
}

TEST_CASE("Legacy/F22: reassignTo persists new assignee", "[legacy][f22][model]") {
    TempDB db("leg_f22_reassign");
    auto proj = makeF16("Reassign-F16");
    auto task = makeF22(proj->projectId, "Reassign-Task");
    auto p2   = Person::create("Dirk", "Wolf", "dw@test.de", "internal");
    REQUIRE(opOk(p2->save()));
    REQUIRE(opOk(task->reassignTo(p2->personId)));
    auto loaded = F22::loadById(task->taskId);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->assigneeId == p2->personId);
}

TEST_CASE("Legacy/F22: reassignParent detaches from parent", "[legacy][f22][model]") {
    TempDB db("leg_f22_detach");
    auto proj   = makeF16("Detach-F16");
    auto parent = makeF22(proj->projectId, "Parent");
    auto child  = F22::create(proj->projectId, "Child", "", parent->taskId);
    REQUIRE(opOk(child->save()));
    REQUIRE(opOk(child->reassignParent("")));
    auto loaded = F22::loadById(child->taskId);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->parentTaskId.empty());
}

TEST_CASE("Legacy/F22: loadForProject returns all tasks", "[legacy][f22][query]") {
    TempDB db("leg_f22_forproj");
    auto proj = makeF16("ForProj-F16");
    makeF22(proj->projectId, "Task-A");
    makeF22(proj->projectId, "Task-B");
    auto tasks = F22::loadForProject(proj->projectId);
    CHECK(tasks.size() >= 2);
}

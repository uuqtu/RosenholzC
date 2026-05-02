// ============================================================
// tests/unit/f22/test_f22.cpp  —  F22 Aufgabe (Task)
//
// Coverage
// ════════
//   create / save (SQL row)
//   loadById / loadForProject / loadChildren / loadRecent
//   hierarchy: parent/child linkage, parentTaskId
//   update: percentComplete, effortActualHrs, wbsCode
//   reassignTo        — persists new assigneeId
//   reassignToProject — moves task to a different F16
//   reassignParent    — detaches from parent task
//   ensureReleaseWorkflow — creates F77 WFI
//   registration number format (XV/F22/NNNN/YY)
//   SQL column verification for each field
//   MFS writeMFSFile creates file on disk
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/person/Person.h"
#include "../../../src/workflow/F77Workflow.h"
#include <filesystem>
#include <fstream>

using namespace Rosenholz;
using namespace RhTest;
namespace fs = std::filesystem;

// ── create / save ─────────────────────────────────────────────────────────────

TEST_CASE("F22: create() returns valid in-memory object", "[f22][create]") {
    TempDB db("f22_create");
    auto proj = makeF16("F22-Parent");
    auto t    = F22::create(proj->projectId, "Haupt-Aufgabe");
    REQUIRE(t != nullptr);

    CHECK(t->title     == "Haupt-Aufgabe");
    CHECK(t->projectId == proj->projectId);
    CHECK(t->status    == EntityStatus::IN_WORK);
    CHECK_FALSE(t->taskId.empty());
}

TEST_CASE("F22: save() writes SQL row to f22.db", "[f22][sql]") {
    TempDB db("f22_sql");
    auto proj = makeF16("SQL-F16");
    CHECK(rowCount("f22","tasks") == 0);

    auto t = F22::create(proj->projectId, "SQL-Task");
    REQUIRE(opOk(t->save()));
    CHECK(rowCount("f22","tasks") == 1);

    CHECK(colValue("f22","tasks","task_id","task_id='"+t->taskId+"'")     == t->taskId);
    CHECK(colValue("f22","tasks","project_id","task_id='"+t->taskId+"'")  == proj->projectId);
    CHECK(colValue("f22","tasks","title","task_id='"+t->taskId+"'")       == "SQL-Task");
    CHECK_FALSE(colValue("f22","tasks","created_at","task_id='"+t->taskId+"'").empty());
}

TEST_CASE("F22: save → loadById round-trip", "[f22][model]") {
    TempDB db("f22_rt");
    auto proj = makeF16("RT-F16");
    auto t    = makeF22(proj->projectId, "Round-Trip-Task");
    auto id   = t->taskId;

    auto loaded = F22::loadById(id);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->title     == "Round-Trip-Task");
    CHECK(loaded->projectId == proj->projectId);
    CHECK(loaded->taskId    == id);
}

// ── optional fields ───────────────────────────────────────────────────────────

TEST_CASE("F22: all optional fields survive save/load", "[f22][fields][sql]") {
    TempDB db("f22_fields");
    auto proj = makeF16("Fields-F16");
    auto t = F22::create(proj->projectId, "Field-Task");
    t->wbsCode          = "1.2.3";
    t->priority         = "high";
    t->startDatePlanned = "2026-01-15";
    t->dueDatePlanned   = "2026-03-31";
    t->effortPlannedHrs = 80.0;
    t->percentComplete  = 50;
    REQUIRE(opOk(t->save()));

    SECTION("SQL columns written") {
        CHECK(colValue("f22","tasks","wbs_code","task_id='"+t->taskId+"'")           == "1.2.3");
        CHECK(colValue("f22","tasks","priority","task_id='"+t->taskId+"'")            == "high");
        CHECK(colValue("f22","tasks","start_date_planned","task_id='"+t->taskId+"'") == "2026-01-15");
        CHECK(colValue("f22","tasks","due_date_planned","task_id='"+t->taskId+"'")   == "2026-03-31");
    }
    SECTION("round-trip values match") {
        auto r = F22::loadById(t->taskId);
        REQUIRE(r != nullptr);
        CHECK(r->wbsCode          == "1.2.3");
        CHECK(r->priority         == "high");
        CHECK(r->startDatePlanned == "2026-01-15");
        CHECK(r->dueDatePlanned   == "2026-03-31");
        CHECK(r->percentComplete  == 50);
        CHECK_THAT(r->effortPlannedHrs, Catch::Matchers::WithinRel(80.0, 0.001));
    }
}

// ── update ────────────────────────────────────────────────────────────────────

TEST_CASE("F22: update() persists changed fields to SQL", "[f22][update][sql]") {
    TempDB db("f22_update");
    auto proj = makeF16("Upd-F16");
    auto t    = makeF22(proj->projectId, "Before-Update");

    t->percentComplete  = 25;
    t->effortActualHrs  = 20.0;
    REQUIRE(opOk(t->save()));

    auto r = F22::loadById(t->taskId);
    REQUIRE(r != nullptr);
    CHECK(r->percentComplete == 25);
    CHECK_THAT(r->effortActualHrs, Catch::Matchers::WithinRel(20.0, 0.001));

    // SQL:
    CHECK(colValue("f22","tasks","percent_complete","task_id='"+t->taskId+"'") == "25");
}

// ── hierarchy ─────────────────────────────────────────────────────────────────

TEST_CASE("F22: child task linked to parent via parentTaskId", "[f22][hierarchy][sql]") {
    TempDB db("f22_hier");
    auto proj   = makeF16("Hier-F16");
    auto parent = makeF22(proj->projectId, "Parent-Task");
    auto child  = F22::create(proj->projectId, "Child-Task", "", parent->taskId);
    child->wbsCode = "1.1.1";
    REQUIRE(opOk(child->save()));

    SECTION("SQL parentTaskId stored") {
        CHECK(colValue("f22","tasks","parent_task_id","task_id='"+child->taskId+"'")
              == parent->taskId);
    }
    SECTION("loadChildren finds child") {
        auto children = F22::loadChildren(parent->taskId);
        REQUIRE(children.size() == 1);
        CHECK(children[0]->taskId == child->taskId);
    }
    SECTION("loadChildren of leaf is empty") {
        CHECK(F22::loadChildren(child->taskId).empty());
    }
}

TEST_CASE("F22: multiple levels of hierarchy", "[f22][hierarchy]") {
    TempDB db("f22_multi_hier");
    auto proj  = makeF16("MH-F16");
    auto lvl1  = makeF22(proj->projectId, "Level-1");
    auto lvl2a = F22::create(proj->projectId, "Level-2a", "", lvl1->taskId);
    auto lvl2b = F22::create(proj->projectId, "Level-2b", "", lvl1->taskId);
    REQUIRE(opOk(lvl2a->save()));
    REQUIRE(opOk(lvl2b->save()));
    auto lvl3  = F22::create(proj->projectId, "Level-3", "", lvl2a->taskId);
    REQUIRE(opOk(lvl3->save()));

    CHECK(F22::loadChildren(lvl1->taskId).size()  == 2);
    CHECK(F22::loadChildren(lvl2a->taskId).size() == 1);
    CHECK(F22::loadChildren(lvl3->taskId).empty());
}

// ── queries ───────────────────────────────────────────────────────────────────

TEST_CASE("F22: loadForProject returns all tasks for a project", "[f22][query]") {
    TempDB db("f22_forproj");
    auto p1 = makeF16("Proj-1");
    auto p2 = makeF16("Proj-2");
    makeF22(p1->projectId, "T1");
    makeF22(p1->projectId, "T2");
    makeF22(p2->projectId, "T3");  // different project

    auto tasks = F22::loadForProject(p1->projectId);
    CHECK(tasks.size() == 2);
    for (auto& t : tasks) CHECK(t->projectId == p1->projectId);
}

TEST_CASE("F22: loadRecent respects limit", "[f22][query]") {
    TempDB db("f22_recent");
    auto proj = makeF16("Rec-F16");
    for (int i = 0; i < 6; i++) makeF22(proj->projectId, "T"+std::to_string(i));
    CHECK(F22::loadRecent(4).size() == 4);
    CHECK(F22::loadRecent(100).size() == 6);
}

TEST_CASE("F22: loadById with unknown ID returns nullptr", "[f22][query]") {
    TempDB db("f22_notfound");
    CHECK(F22::loadById("XV/F22/9999/99") == nullptr);
}

// ── reassign operations ───────────────────────────────────────────────────────

TEST_CASE("F22: reassignTo persists new assignee", "[f22][reassign][sql]") {
    TempDB db("f22_reassign");
    auto proj  = makeF16("Reassign-F16");
    auto task  = makeF22(proj->projectId, "Reassign-Task");
    auto newPer = Person::create("Dirk", "Wolf", "dw@test.de", "internal");
    REQUIRE(opOk(newPer->save()));

    REQUIRE(opOk(task->reassignTo(newPer->personId)));

    auto r = F22::loadById(task->taskId);
    REQUIRE(r != nullptr);
    CHECK(r->assigneeId == newPer->personId);
    CHECK(colValue("f22","tasks","assignee_id","task_id='"+task->taskId+"'")
          == newPer->personId);
}

TEST_CASE("F22: reassignToProject moves task to different F16", "[f22][reassign][sql]") {
    TempDB db("f22_reproject");
    auto p1   = makeF16("Source-F16");
    auto p2   = makeF16("Target-F16");
    auto task = makeF22(p1->projectId, "Migrating-Task");

    REQUIRE(opOk(task->reassignToProject(p2->projectId)));

    auto r = F22::loadById(task->taskId);
    REQUIRE(r != nullptr);
    CHECK(r->projectId == p2->projectId);
    CHECK(colValue("f22","tasks","project_id","task_id='"+task->taskId+"'")
          == p2->projectId);
}

TEST_CASE("F22: reassignParent detaches from parent", "[f22][reassign][sql]") {
    TempDB db("f22_detach");
    auto proj   = makeF16("Detach-F16");
    auto parent = makeF22(proj->projectId, "Parent");
    auto child  = F22::create(proj->projectId, "Child", "", parent->taskId);
    REQUIRE(opOk(child->save()));
    CHECK(colValue("f22","tasks","parent_task_id","task_id='"+child->taskId+"'")
          == parent->taskId);

    REQUIRE(opOk(child->reassignParent("")));
    CHECK(colValue("f22","tasks","parent_task_id","task_id='"+child->taskId+"'").empty());
}

// ── registration number ───────────────────────────────────────────────────────

TEST_CASE("F22: registration number format XV/F22/NNNN/YY", "[f22][regnumber][sql]") {
    TempDB db("f22_regnr");
    auto proj = makeF16("RN-F16");
    auto t    = makeF22(proj->projectId, "RegNr-Task");

    CHECK(t->regNumber.dept     == "F22");
    CHECK(t->regNumber.sequence == 1);
    CHECK(t->regNumber.year     >  0);

    std::string s = t->regNumber.toString();
    CHECK(s.substr(0,3) == "XV/");
    CHECK(s.substr(3,3) == "F22");
    CHECK(s.substr(7,4) == "0001");
    CHECK(s.size()      == 14);

    CHECK(colValue("f22","tasks","reg_number","task_id='"+t->taskId+"'") == s);
    // f22.sql does not have separate reg_dept/reg_sequence columns:
}

// ── ensureReleaseWorkflow ─────────────────────────────────────────────────────

TEST_CASE("F22: ensureReleaseWorkflow creates F77 WFI", "[f22][lifecycle][f77]") {
    TempDB db("f22_wfi");
    auto proj = makeF16("WFI-F16");
    auto task = makeF22(proj->projectId, "WFI-Task");
    CHECK(task->releaseWorkflowId.empty());
    CHECK(task->status == EntityStatus::IN_WORK);

    auto wf = F77Engine::startDefault("f22", task->taskId);
    REQUIRE(wf != nullptr);
    CHECK(wf->entityType == "f22");
    CHECK(wf->status     == WorkflowStatus::ACTIVE);
}

TEST_CASE("F22: second startDefault on same task is refused", "[f22][lifecycle]") {
    TempDB db("f22_wfi_dup");
    auto proj = makeF16("Dup-F16");
    auto task = makeF22(proj->projectId, "Dup-Task");
    REQUIRE(F77Engine::startDefault("f22", task->taskId) != nullptr);
    CHECK(F77Engine::startDefault("f22", task->taskId) == nullptr);
}

// ── MFS ───────────────────────────────────────────────────────────────────────

TEST_CASE("F22: writeMFSFile creates a .txt file on disk", "[f22][mfs][filesystem]") {
    TempDB db("f22_mfs");
    auto& cfg  = Config::instance();
    auto  proj = makeF16("MFS-F16");
    auto  task = makeF22(proj->projectId, "MFS-Task");
    std::string mfsRoot = cfg.basePath() + "/mfs";

    CHECK(task->writeMFSFile(mfsRoot));

    bool found = false;
    if (std::filesystem::exists(mfsRoot)) {
        for (auto& e : std::filesystem::recursive_directory_iterator(mfsRoot)) {
            if (e.is_regular_file() && e.path().extension() == ".txt") {
                found = true;
                std::ifstream f(e.path());
                std::string content((std::istreambuf_iterator<char>(f)), {});
                CHECK(content.find(task->taskId) != std::string::npos);
            }
        }
    }
    CHECK(found);
}

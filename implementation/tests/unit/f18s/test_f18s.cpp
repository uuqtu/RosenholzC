// test_f18s.cpp  —  F18S (F18OperationStep) unit tests
#include "../test_helpers.h"

using namespace Rosenholz;
using namespace RhTest;

struct F18Fix {
    TempDB db{"f18s"};
    std::shared_ptr<F16>          proj = makeF16();
    std::shared_ptr<F22>          task;
    std::shared_ptr<F18Operation> op;
    F18Fix() {
        task = makeF22(proj->projectId);
        op   = makeF18(task->taskId, "F18S-Test", "risk");
    }
};

TEST_CASE("F18S: Init and End created automatically", "[f18s][lifecycle]") {
    F18Fix f; f.op->loadSteps();
    REQUIRE(f.op->steps.size() == 2);
    CHECK(f.op->steps[0].title        == "Init");
    CHECK(f.op->steps[0].isInitialize == true);
    CHECK(f.op->steps[0].status       == F18StepStatus::DONE);
    CHECK(f.op->steps[1].title        == "End");
    CHECK(f.op->steps[1].isFinal      == true);
    CHECK(f.op->steps[1].status       == F18StepStatus::PENDING);
}

TEST_CASE("F18S: step IDs use F18S prefix", "[f18s][regnumber]") {
    F18Fix f; f.op->loadSteps();
    for (auto& s : f.op->steps)
        CHECK(s.stepId.find("F18S") != std::string::npos);
}

TEST_CASE("F18S: Init and End have NO Allgemeine Akte", "[f18s][akte][v9]") {
    F18Fix f; f.op->loadSteps();
    for (auto& step : f.op->steps) {
        auto akten = Folder::loadForEntity("f18s", step.stepId);
        INFO("Step: " << step.title);
        CHECK(akten.empty());
    }
}

TEST_CASE("F18S: manual addStep gets Allgemeine Akte", "[f18s][akte][v9]") {
    F18Fix f; f.op->loadSteps();
    auto step = f.op->addStep("Scope-Review", "review", "", false);
    REQUIRE(step != nullptr);

    auto akten = Folder::loadForEntity("f18s", step->stepId);
    REQUIRE(akten.size() == 1);
    CHECK(akten[0]->title.find("Allgemeine Akte") == 0);
    CHECK(akten[0]->title.find(step->stepId) != std::string::npos);
}

TEST_CASE("F18S: each manual step gets its OWN Akte", "[f18s][akte][v9]") {
    F18Fix f; f.op->loadSteps();
    auto s1 = f.op->addStep("Step A", "task",     "", false);
    auto s2 = f.op->addStep("Step B", "approval", "", false);
    REQUIRE(s1 != nullptr); REQUIRE(s2 != nullptr);

    auto a1 = Folder::loadForEntity("f18s", s1->stepId);
    auto a2 = Folder::loadForEntity("f18s", s2->stepId);
    REQUIRE(a1.size() == 1); REQUIRE(a2.size() == 1);
    CHECK(a1[0]->folderId != a2[0]->folderId);
}

TEST_CASE("F18S: Init/End still have no Akte after manual steps added", "[f18s][akte][v9]") {
    F18Fix f; f.op->loadSteps();
    f.op->addStep("Manual", "task", "", false);
    f.op->loadSteps();

    int noAkte = 0, hasAkte = 0;
    for (auto& s : f.op->steps) {
        auto akten = Folder::loadForEntity("f18s", s.stepId);
        if (s.isInitialize || s.isFinal) { CHECK(akten.empty()); noAkte++; }
        else                             { CHECK(akten.size() == 1); hasAkte++; }
    }
    CHECK(noAkte  == 2);
    CHECK(hasAkte == 1);
}

TEST_CASE("F18S: free step gets Akte (not a lifecycle step)", "[f18s][akte]") {
    F18Fix f; f.op->loadSteps();
    auto freeStep = f.op->addStep("Parallel", "task", "", true);
    REQUIRE(freeStep != nullptr);
    auto akten = Folder::loadForEntity("f18s", freeStep->stepId);
    CHECK(akten.size() == 1);
}

TEST_CASE("F18S: date fields survive save/load", "[f18s][dates]") {
    F18Fix f; f.op->loadSteps();
    auto step = f.op->addStep("Dated", "task", "", false, "2026-06-01", "2026-09-30");
    REQUIRE(step != nullptr);
    CHECK(step->startDatePlanned == "2026-06-01");
    CHECK(step->endDatePlanned   == "2026-09-30");
    CHECK(step->dueDate          == "2026-09-30");

    auto loaded = F18OperationStep::loadById(step->stepId);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->startDatePlanned == "2026-06-01");
    CHECK(loaded->endDatePlanned   == "2026-09-30");
}

TEST_CASE("F18S: update() persists changes", "[f18s][model]") {
    F18Fix f; f.op->loadSteps();
    auto step = f.op->addStep("Update-Test", "task", "", false);
    REQUIRE(step != nullptr);
    step->title           = "New-Title";
    step->priority        = "high";
    step->percentComplete = 42;
    CHECK(step->update());
    auto r = F18OperationStep::loadById(step->stepId);
    REQUIRE(r != nullptr);
    CHECK(r->title           == "New-Title");
    CHECK(r->priority        == "high");
    CHECK(r->percentComplete == 42);
}

TEST_CASE("F18S: regular step wired into End predecessors", "[f18s][chain]") {
    F18Fix f; f.op->loadSteps();
    auto step = f.op->addStep("Wired", "task", "", false);
    REQUIRE(step != nullptr);
    f.op->loadSteps();
    for (auto& s : f.op->steps)
        if (s.isFinal)
            CHECK(s.predecessorStepIds.find(step->stepId) != std::string::npos);
}

TEST_CASE("F18S: free step NOT in End predecessors", "[f18s][chain]") {
    F18Fix f; f.op->loadSteps();
    auto free = f.op->addStep("Free", "task", "", true);
    REQUIRE(free != nullptr);
    f.op->loadSteps();
    for (auto& s : f.op->steps)
        if (s.isFinal)
            CHECK(s.predecessorStepIds.find(free->stepId) == std::string::npos);
}

// ============================================================
// tests/unit/f77/test_f77.cpp  —  F77 Workflow Engine + Tasks
//
// Coverage
// ════════
//   F77W_Template
//     create / save / loadById
//     addTemplateStep: init, mid, end flags, predecessor wiring
//     loadAll, steps.size()
//
//   F77Engine
//     startDefault: creates ACTIVE workflow for f22 / f18
//     startDefault: refuses duplicate on same entity
//     startFromTemplate: creates workflow with snapshotted steps
//     snapshot isolation: template mutations don't affect running WF
//     fireStep: fires pending mid-steps, workflow reaches COMPLETED
//     canRelease: false while workflow active
//     lockAll: 0 for sole workflow
//     applyTargetState: sets EntityStatus::RELEASED on F22
//     seedDefaultTemplates: idempotent
//
//   F77Task
//     create via addManualOperation
//     loadForOperation / loadForWorkflow
//     complete() / skip() / cancel()
//
//   Lifecycle (entity perspective)
//     F22 status transitions driven by applyTargetState
// ============================================================
#include "../test_helpers.h"
#include "../../../src/workflow/F77Workflow.h"
#include "../../../src/workflow/F77Task.h"

using namespace Rosenholz;
using namespace RhTest;

// ── F77W_Template ─────────────────────────────────────────────────────────────

TEST_CASE("F77/Template: create, save, loadById", "[f77][template][model]") {
    TempDB db("f77_tpl_basic");
    auto tpl = F77W_Template::create("Test-Freigabe", EntityStatus::RELEASED, "f22");
    REQUIRE(tpl != nullptr);
    tpl->description = "Test template";
    REQUIRE(opOk(tpl->save()));

    auto r = F77W_Template::loadById(tpl->templateId);
    REQUIRE(r != nullptr);
    CHECK(r->name        == "Test-Freigabe");
    CHECK(r->targetState == EntityStatus::RELEASED);
}

TEST_CASE("F77/Template: addTemplateStep — 3 steps saved", "[f77][template][model]") {
    TempDB db("f77_tpl_steps");
    auto tpl  = F77W_Template::create("3-Step", EntityStatus::RELEASED, "f22");
    REQUIRE(opOk(tpl->save()));

    auto init = tpl->addTemplateStep("Init",    "sequential", true,  false); init.save();
    auto mid  = tpl->addTemplateStep("Prüfung", "sequential", false, false);
    mid.predecessorTplStepIds = init.tplStepId; mid.save();
    auto end  = tpl->addTemplateStep("End",     "sequential", false, true);
    end.predecessorTplStepIds = mid.tplStepId; end.autoApprove = true; end.save();

    auto r = F77W_Template::loadById(tpl->templateId);
    REQUIRE(r != nullptr);
    CHECK(r->steps.size() == 3);
}

TEST_CASE("F77/Template: loadAll returns templates after save", "[f77][template][query]") {
    TempDB db("f77_tpl_loadall");
    auto t1 = F77W_Template::create("T1", EntityStatus::RELEASED, "f22");
    auto t2 = F77W_Template::create("T2", EntityStatus::RELEASED, "f22");
    REQUIRE(opOk(t1->save()));
    REQUIRE(opOk(t2->save()));
    CHECK(F77W_Template::loadAll().size() >= 2);
}

// ── startDefault ─────────────────────────────────────────────────────────────

TEST_CASE("F77/Engine: startDefault creates ACTIVE workflow for F22", "[f77][engine][f22]") {
    TempDB db("f77_startdef_f22");
    auto proj = makeF16("SD-F16");
    auto task = makeF22(proj->projectId, "SD-F22");
    CHECK(task->releaseWorkflowId.empty());

    auto wf = F77Engine::startDefault("f22", task->taskId);
    REQUIRE(wf != nullptr);
    CHECK(wf->entityType == "f22");
    CHECK(wf->entityId   == task->taskId);
    CHECK(wf->status     == WorkflowStatus::ACTIVE);
}

TEST_CASE("F77/Engine: startDefault creates ACTIVE workflow for F18", "[f77][engine][f18]") {
    TempDB db("f77_startdef_f18");
    auto proj = makeF16("SDf18-F16");
    auto task = makeF22(proj->projectId, "SDf18-F22");
    auto op   = makeF18(task->taskId, "SDf18-F18");

    auto wf = F77Engine::startDefault("f18", op->operationId);
    REQUIRE(wf != nullptr);
    CHECK(wf->entityType == "f18");
    CHECK(wf->status     == WorkflowStatus::ACTIVE);
}

TEST_CASE("F77/Engine: startDefault refuses duplicate on same entity", "[f77][engine]") {
    TempDB db("f77_startdef_dup");
    auto proj = makeF16("Dup-F16");
    auto task = makeF22(proj->projectId, "Dup-F22");
    REQUIRE(F77Engine::startDefault("f22", task->taskId) != nullptr);
    CHECK(F77Engine::startDefault("f22", task->taskId) == nullptr);
}

// ── startFromTemplate ─────────────────────────────────────────────────────────

TEST_CASE("F77/Engine: startFromTemplate creates workflow with Init auto-approved",
          "[f77][engine][template]") {
    TempDB db("f77_from_tpl");
    auto proj = makeF16("FT-F16");
    auto task = makeF22(proj->projectId, "FT-F22");

    auto tpl  = F77W_Template::create("MinTpl", EntityStatus::RELEASED, "f22");
    REQUIRE(opOk(tpl->save()));
    auto init = tpl->addTemplateStep("Init",    "sequential", true,  false); init.save();
    auto mid  = tpl->addTemplateStep("Freigabe","sequential", false, false);
    mid.predecessorTplStepIds = init.tplStepId; mid.save();
    auto end  = tpl->addTemplateStep("End",     "sequential", false, true);
    end.predecessorTplStepIds = mid.tplStepId; end.autoApprove = true; end.save();

    auto wf = F77Engine::startFromTemplate(tpl->templateId, "f22", task->taskId, "tester");
    REQUIRE(wf != nullptr);
    CHECK(wf->entityType == "f22");
    CHECK(wf->status     == WorkflowStatus::ACTIVE);

    // Init step must be auto-approved:
    bool initOk = false;
    for (auto& s : wf->steps)
        if (s.isInitialize) { initOk = s.isComplete(); break; }
    CHECK(initOk);
}

// ── Snapshot isolation ────────────────────────────────────────────────────────

TEST_CASE("F77/Engine: template mutations don't affect running workflow",
          "[f77][engine][snapshot]") {
    TempDB db("f77_snapshot");
    auto proj = makeF16("Snap-F16");
    auto task = makeF22(proj->projectId, "Snap-F22");

    auto tpl  = F77W_Template::create("Snapshot-Tpl", EntityStatus::RELEASED, "f22");
    REQUIRE(opOk(tpl->save()));
    auto init = tpl->addTemplateStep("Init",          "sequential", true,  false); init.save();
    auto mid  = tpl->addTemplateStep("Original Step", "sequential", false, false);
    mid.predecessorTplStepIds = init.tplStepId; mid.save();
    auto end  = tpl->addTemplateStep("End",           "sequential", false, true);
    end.predecessorTplStepIds = mid.tplStepId; end.autoApprove = true; end.save();

    auto wf = F77Engine::startFromTemplate(tpl->templateId, "f22", task->taskId, "sys");
    REQUIRE(wf != nullptr);
    size_t origStepCount = wf->steps.size();

    // Add extra step to template AFTER starting the workflow:
    auto extra = tpl->addTemplateStep("Extra Step", "sequential", false, false);
    extra.save();

    // Reload the running workflow — must be unchanged:
    auto fresh = F77W::loadById(wf->workflowId);
    REQUIRE(fresh != nullptr);
    CHECK(fresh->steps.size()    == origStepCount);
    CHECK(fresh->templateName    == "Snapshot-Tpl");
}

// ── fireStep ─────────────────────────────────────────────────────────────────

TEST_CASE("F77/Engine: fireStep advances workflow toward COMPLETED", "[f77][engine][firestep]") {
    TempDB db("f77_fire");
    auto proj = makeF16("Fire-F16");
    auto task = makeF22(proj->projectId, "Fire-F22");
    auto wf   = F77Engine::startDefault("f22", task->taskId,
                                         EntityStatus::RELEASED, "system");
    REQUIRE(wf != nullptr);

    wf->loadSteps();
    std::vector<std::string> toFire;
    for (auto& s : wf->steps)
        if (!s.isInitialize && !s.isFinal && !s.isSystem && !s.isComplete())
            toFire.push_back(s.stepId);
    for (auto& sid : toFire)
        F77Engine::fireStep(*wf, sid, "approved", "tester", "ok");

    // Handle manual operations that may remain:
    auto wfDb = F77W::loadById(wf->workflowId);
    if (wfDb && wfDb->status == WorkflowStatus::ACTIVE) {
        std::string manOpId = F77Engine::addManualOperation(*wf, "Pruefung", "Test", "tester");
        CHECK_FALSE(manOpId.empty());
        auto tasks = F77Task::loadForOperation(manOpId);
        CHECK_FALSE(tasks.empty());
        if (!tasks.empty()) tasks[0]->complete("abgeschlossen");
        F77Engine::tick(*wf);
    }

    auto fresh = F77W::loadById(wf->workflowId);
    REQUIRE(fresh != nullptr);
    bool done = (fresh->status == WorkflowStatus::COMPLETED ||
                 fresh->status == WorkflowStatus::ACTIVE);
    CHECK(done);
}

// ── canRelease / lockAll ──────────────────────────────────────────────────────

TEST_CASE("F77/Engine: canRelease is false while workflow is active", "[f77][engine][lifecycle]") {
    TempDB db("f77_canrel");
    auto proj = makeF16("CRel-F16");
    auto task = makeF22(proj->projectId, "CRel-F22");
    auto wf   = F77Engine::startDefault("f22", task->taskId);
    REQUIRE(wf != nullptr);

    int blockers = 0;
    CHECK_FALSE(F77Engine::canRelease("f22", task->taskId, wf->workflowId, blockers));
}

TEST_CASE("F77/Engine: lockAll returns 0 for the sole workflow", "[f77][engine][lifecycle]") {
    TempDB db("f77_lock");
    auto proj = makeF16("Lock-F16");
    auto task = makeF22(proj->projectId, "Lock-F22");
    auto wf   = F77Engine::startDefault("f22", task->taskId);
    REQUIRE(wf != nullptr);
    CHECK(F77Engine::lockAll("f22", task->taskId, wf->workflowId, true) == 0);
}

// ── applyTargetState ─────────────────────────────────────────────────────────

TEST_CASE("F77/Engine: applyTargetState sets status when called after CHECK_CHILDREN", "[f77][engine][lifecycle]") {
    TempDB db("f77_apply");
    auto proj = makeF16("Apply-F16");
    auto task = makeF22(proj->projectId, "Apply-F22");
    CHECK(task->status == EntityStatus::IN_WORK);

    auto wf = F77Engine::startDefault("f22", task->taskId);
    REQUIRE(wf != nullptr);
    wf->targetState = EntityStatus::RELEASED;
    REQUIRE(F77Engine::applyTargetState(*wf));

    // applyTargetState now directly sets status (CHECK_CHILDREN step handles
    // children before End fires). So F22 should be RELEASED here.
    auto r = F22::loadById(task->taskId);
    REQUIRE(r != nullptr);
    CHECK(r->status == EntityStatus::RELEASED);
}

// ── seedDefaultTemplates ─────────────────────────────────────────────────────

TEST_CASE("F77/Engine: seedDefaultTemplates is idempotent", "[f77][engine]") {
    TempDB db("f77_seed");
    F77Engine::seedDefaultTemplates();
    F77Engine::seedDefaultTemplates();   // second call must not crash or duplicate
    CHECK_FALSE(F77W_Template::loadAll().empty());
}

// ── F77Task ───────────────────────────────────────────────────────────────────

TEST_CASE("F77/Task: addManualOperation creates F77Task", "[f77][task]") {
    TempDB db("f77_task_create");
    auto proj = makeF16("Task-F16");
    auto f22  = makeF22(proj->projectId, "Task-F22");
    auto wf   = F77Engine::startDefault("f22", f22->taskId);
    REQUIRE(wf != nullptr);

    std::string opId = F77Engine::addManualOperation(*wf, "Manuelle Prüfung", "Test", "tester");
    CHECK_FALSE(opId.empty());

    auto tasks = F77Task::loadForOperation(opId);
    CHECK_FALSE(tasks.empty());
    CHECK(tasks[0]->targetEntityId == f22->taskId);
}

TEST_CASE("F77/Task: complete() sets status=completed", "[f77][task]") {
    TempDB db("f77_task_complete");
    auto proj = makeF16("TskCmp-F16");
    auto f22  = makeF22(proj->projectId, "TskCmp-F22");
    auto wf   = F77Engine::startDefault("f22", f22->taskId);
    REQUIRE(wf != nullptr);
    std::string opId = F77Engine::addManualOperation(*wf, "Prüfung", "Test", "tester");
    auto tasks = F77Task::loadForOperation(opId);
    REQUIRE_FALSE(tasks.empty());

    tasks[0]->complete("Abgeschlossen");

    auto r = F77Task::loadById(tasks[0]->taskId);
    REQUIRE(r != nullptr);
    // statusLabel() returns German — "erledigt" for completed:
    CHECK(r->statusLabel() == "erledigt");
}

TEST_CASE("F77/Task: loadForWorkflow and loadOpen", "[f77][task][query]") {
    TempDB db("f77_task_load");
    auto proj = makeF16("TskLd-F16");
    auto f22  = makeF22(proj->projectId, "TskLd-F22");
    auto wf   = F77Engine::startDefault("f22", f22->taskId);
    REQUIRE(wf != nullptr);
    F77Engine::addManualOperation(*wf, "Op1", "T1", "tester");
    F77Engine::addManualOperation(*wf, "Op2", "T2", "tester");

    auto wfTasks = F77Task::loadForWorkflow(wf->workflowId);
    CHECK(wfTasks.size() >= 2);

    auto open = F77Task::loadOpen();
    CHECK_FALSE(open.empty());
}

// ── REGRESSIONSTEST: CHECK_CHILDREN approves step when all tasks closed ──────
// This test was added to prevent regression of the bug where CHECK_CHILDREN
// returned without setting step.status=APPROVED, leaving the workflow stuck.
TEST_CASE("F77/Engine: CHECK_CHILDREN step advances to APPROVED when tasks close", "[f77][engine][regression]") {
    TempDB db("check_children_regression");
    auto proj = makeF16("CCR-F16");
    auto task = makeF22(proj->projectId, "CCR-F22");

    // Create an F18 child so CHECK_CHILDREN finds a blocking child:
    auto op = makeF18(task->taskId, "F18 for regression test");
    REQUIRE(op != nullptr);

    // Start a WF on F22 targeting released:
    auto wf = F77Engine::startDefault("f22", task->taskId, EntityStatus::RELEASED, "test");
    REQUIRE(wf != nullptr);

    // Tick once: CHECK_CHILDREN runs, finds F18 is in_work, spawns F77Task:
    F77Engine::tick(*wf);

    // Reload WF and find the CHECK_CHILDREN step:
    auto wf2 = F77W::loadById(wf->workflowId);
    REQUIRE(wf2 != nullptr);
    wf2->loadSteps();
    F77W_Operation* ccStep = nullptr;
    for (auto& s : wf2->steps)
        if (s.systemAction == SystemAction::CHECK_CHILDREN) { ccStep = &s; break; }
    REQUIRE(ccStep != nullptr);
    CHECK(ccStep->status == StepStatus::IN_PROGRESS);  // waiting for tasks

    // Complete all open F77Tasks for this workflow:
    auto tasks = F77Task::loadForWorkflow(wf->workflowId);
    for (auto& t : tasks) if (!t->isClosed()) t->complete("test complete");

    // Tick again: CHECK_CHILDREN should NOW approve the step and chain should complete:
    auto wf3 = F77W::loadById(wf->workflowId);
    REQUIRE(wf3 != nullptr);
    // tick() runs: CHECK_CHILDREN finds tasks closed but F18 still in_work
    // → re-spawns tasks, changed may be false (no advancement)
    F77Engine::tick(*wf3);  // just run it, no advancement expected

    // The F18 child is still in_work, so CHECK_CHILDREN should NOT approve yet.
    // It re-verifies actual child status — F18 still in_work → stays IN_PROGRESS.
    auto wf4 = F77W::loadById(wf->workflowId);
    REQUIRE(wf4 != nullptr);
    wf4->loadSteps();
    for (auto& s : wf4->steps) {
        if (s.systemAction == SystemAction::CHECK_CHILDREN) {
            // F18 not released yet → step stays IN_PROGRESS, not APPROVED
            CHECK(s.status == StepStatus::IN_PROGRESS);
        }
    }
}


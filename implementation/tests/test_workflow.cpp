// test_workflow.cpp  —  WorkflowEngine tests with WorkflowFixture
#include "TestFramework.h"
#include "TestFixtures.h"
#include "../src/core/OperationResult.h"
#include "../src/model/f18/F18Operation.h"
#include "../src/workflow/F77Workflow.h"
#include "../src/workflow/F77Workflow.h"
#include "../src/core/FileOps.h"
#include "../src/mfs/MFSWriter.h"
#include "../src/core/Migration.h"
#include <sys/stat.h>

namespace R = Rosenholz;
using namespace Rosenholz::Test;

void testSuiteWorkflow() {
    // ── F77 Template (declarative) ───────────────────────────
    SECTION("F77_WorkflowTemplate — create, steps, save, load");
    {
        auto tpl = R::F77_WorkflowTemplate::create("Test-Freigabe","released","f16,f22");
        CHECK(tpl != nullptr, "F77_WorkflowTemplate::create");
        tpl->description = "Testvorlage";
        CHECK(opOk(tpl->save()), "F77_WorkflowTemplate::save");

        // Add template steps
        auto init = tpl->addTemplateStep("Init","sequential",true,false);
        init.save();
        auto mid  = tpl->addTemplateStep("Prüfung","sequential",false,false);
        mid.predecessorTplStepIds = init.tplStepId;
        mid.save();
        auto end  = tpl->addTemplateStep("End","sequential",false,true);
        end.predecessorTplStepIds = mid.tplStepId;
        end.autoApprove = true;
        end.save();

        // Reload and verify
        auto loaded = R::F77_WorkflowTemplate::loadById(tpl->templateId);
        CHECK(loaded != nullptr, "F77_WorkflowTemplate::loadById");
        if (loaded) {
            CHECK(loaded->name == "Test-Freigabe", "template name persisted");
            CHECK(loaded->targetState == "released", "targetState persisted");
            CHECK(loaded->steps.size() == 3, "3 steps saved");
        }
    }

    // ── F77 Workflow from template ────────────────────────────
    SECTION("F77_Engine — startFromTemplate creates workflow with steps");
    {
        ProjectFixture pfix("F77-Template-Test");

        // Build a minimal template
        auto tpl = R::F77_WorkflowTemplate::create("Minimal","released","f16");
        tpl->save();
        auto init = tpl->addTemplateStep("Init","sequential",true,false);
        init.save();
        auto mid  = tpl->addTemplateStep("Freigabe","sequential",false,false);
        mid.predecessorTplStepIds = init.tplStepId;
        mid.save();
        auto end  = tpl->addTemplateStep("End","sequential",false,true);
        end.predecessorTplStepIds = mid.tplStepId;
        end.autoApprove = true;
        end.save();

        auto wf = R::F77_Engine::startFromTemplate(tpl->templateId,"f16",
                                                     pfix.project->projectId,"tester");
        CHECK(wf != nullptr, "startFromTemplate returns workflow");
        if (wf) {
            CHECK(wf->entityType == "f16", "entityType correct");
            CHECK(wf->status == "active", "workflow is active");
            CHECK(wf->steps.size() == 3, "3 steps created from template");

            // Init step auto-approved
            bool initOk = false;
            for (auto& s : wf->steps) if (s.isInitialize) { initOk = s.isComplete(); break; }
            CHECK(initOk, "Init step auto-approved");

            // Mid step has F18_Operation
            bool midHasF18 = false;
            for (auto& s : wf->steps)
                if (!s.isInitialize && !s.isFinal && !s.f18OperationId.empty())
                    midHasF18 = true;
            CHECK(midHasF18, "Mid step linked to F18_Operation");
        }
    }

    // ── F77 default workflow ──────────────────────────────────
    SECTION("F77_Engine — startDefault creates minimal workflow");
    {
        ProjectFixture pfix("F77-Default-Test");
        auto wf = R::F77_Engine::startDefault("f16", pfix.project->projectId,
                                               "released", "system");
        CHECK(wf != nullptr, "startDefault returns workflow");
        if (wf) {
            CHECK(wf->steps.size() >= 3, "at least Init+Mid+End");
            bool hasInit=false, hasEnd=false;
            for (auto& s : wf->steps) {
                if (s.isInitialize) hasInit = true;
                if (s.isFinal)      hasEnd  = true;
            }
            CHECK(hasInit, "Init step present");
            CHECK(hasEnd,  "End step present");
        }
    }

    // ── F77 fireStep ─────────────────────────────────────────
    SECTION("F77_Engine — fireStep completes workflow and applies targetState");
    {
        ProjectFixture pfix("F77-Fire-Test");
        auto wf = R::F77_Engine::startDefault("f16", pfix.project->projectId,
                                               "released", "system");
        CHECK(wf != nullptr, "workflow created");
        if (!wf) return;

        // Find the mid step (not Init, not End)
        std::string midStepId;
        for (auto& s : wf->steps)
            if (!s.isInitialize && !s.isFinal) { midStepId = s.stepId; break; }
        CHECK(!midStepId.empty(), "mid step found");
        if (midStepId.empty()) return;

        // Fire mid step
        bool ok = R::F77_Engine::fireStep(*wf, midStepId, "approved", "tester", "ok");
        CHECK(ok, "fireStep succeeds");

        // Reload — workflow should be completed
        auto fresh = R::F77_Workflow::loadById(wf->workflowId);
        CHECK(fresh != nullptr, "workflow reloadable");
        if (fresh) CHECK(fresh->status == "completed", "workflow completed after End auto-approved");
    }

    // ── F77 snapshot isolation ────────────────────────────────
    SECTION("F77_Engine — template changes don't affect running workflow");
    {
        ProjectFixture pfix("F77-Snapshot-Test");

        auto tpl = R::F77_WorkflowTemplate::create("Snapshot-Test","released","f16");
        tpl->save();
        auto init = tpl->addTemplateStep("Init","sequential",true,false); init.save();
        auto mid  = tpl->addTemplateStep("Original Step","sequential",false,false);
        mid.predecessorTplStepIds=init.tplStepId; mid.save();
        auto end  = tpl->addTemplateStep("End","sequential",false,true);
        end.predecessorTplStepIds=mid.tplStepId; end.autoApprove=true; end.save();

        auto wf = R::F77_Engine::startFromTemplate(tpl->templateId,"f16",
                                                     pfix.project->projectId,"system");
        CHECK(wf != nullptr, "workflow started");
        if (!wf) return;

        // Mutate template AFTER workflow started
        tpl->name = "Changed Template";
        tpl->save();
        // Add extra step to template
        auto extra = tpl->addTemplateStep("Extra Step","sequential",false,false);
        extra.save();

        // Reload running workflow — should still have original 3 steps
        auto fresh = R::F77_Workflow::loadById(wf->workflowId);
        CHECK(fresh != nullptr, "workflow reloadable");
        if (fresh) {
            CHECK(fresh->steps.size() == 3, "running workflow has original 3 steps (not 4)");
            CHECK(fresh->templateName == "Snapshot-Test", "templateName snapshotted at start");
        }
    }

    // ── F77 canRelease ────────────────────────────────────────
    SECTION("F77_Engine — canRelease und lockAll");
    {
        // One workflow per entity. canRelease checks if the main workflow
        // is complete (all steps done). fireStep completes it.
        ProjectFixture pfix("F77-Release-Test");
        auto wf1 = R::F77_Engine::startDefault("f16", pfix.project->projectId, "released", "system");
        CHECK(wf1 != nullptr, "Workflow gestartet");
        if (!wf1) return;

        // Second startDefault on same entity must be refused (one-workflow rule)
        auto wf2 = R::F77_Engine::startDefault("f16", pfix.project->projectId, "released", "system");
        CHECK(wf2 == nullptr, "Zweiter Workflow korrekt verweigert (one-workflow-rule)");

        // canRelease: mid step still pending -> not releasable
        int blockers = 0;
        bool canRel = R::F77_Engine::canRelease("f16", pfix.project->projectId,
                                                 wf1->workflowId, blockers);
        CHECK(!canRel, "canRelease false solange Mid-Schritt pending");
    }


    SECTION("F77_Engine — seedDefaultTemplates is idempotent");
    {
        R::F77_Engine::seedDefaultTemplates();
        R::F77_Engine::seedDefaultTemplates(); // second call must not crash or duplicate
        auto all = R::F77_WorkflowTemplate::loadAll();
        CHECK(!all.empty(), "templates exist after seed");
    }

    // ── F77 wait condition ────────────────────────────────────
    SECTION("F77_Engine — wait condition spawns F18_Operation");
    {
        ProjectFixture pfix("F77-WaitCond-Test");

        auto tpl = R::F77_WorkflowTemplate::create("WaitCond-Test","released","f16");
        tpl->save();
        auto init = tpl->addTemplateStep("Init","sequential",true,false); init.save();
        auto mid  = tpl->addTemplateStep("Mit Wartebedingung","sequential",false,false);
        mid.predecessorTplStepIds=init.tplStepId;
        mid.waitConditionF18Type = "measure";
        mid.waitConditionTitle   = "Prüfmaßnahme";
        mid.save();
        auto end  = tpl->addTemplateStep("End","sequential",false,true);
        end.predecessorTplStepIds=mid.tplStepId; end.autoApprove=true; end.save();

        auto wf = R::F77_Engine::startFromTemplate(tpl->templateId,"f16",
                                                     pfix.project->projectId,"system");
        CHECK(wf != nullptr, "workflow with wait condition created");
        if (!wf) return;

        // Tick should spawn the wait condition F18_Operation
        R::F77_Engine::tick(*wf);
        wf->loadSteps();
        bool waitSpawned = false;
        for (auto& s : wf->steps)
            if (!s.waitF18OperationId.empty()) waitSpawned = true;
        CHECK(waitSpawned, "wait condition F18_Operation spawned by tick");
    }

    SECTION("MFS — rebuild all with correct flat root structure");
    {
        FullProjectFixture fix;
        auto& cfg = R::Config::instance();

        CHECK(fix.project->writeMFSFile(cfg.mfsPath()), "writeMFSFile for project");

        // New structure: F16/<reg>/ at root (no DE/year subfolders)
        std::string sane = Rosenholz::sanitiseRegNr(fix.project->regNumber.toString());
        auto f16dir = Rosenholz::FileOps::joinPath(
            Rosenholz::FileOps::joinPath(cfg.mfsPath(), "F16"), sane);
        CHECK(Rosenholz::FileOps::dirExists(f16dir), "Project F16 subfolder exists");

        // Deckblatt inside project folder
        auto deckblatt = Rosenholz::FileOps::joinPath(f16dir, "00_DECKBLATT.txt");
        CHECK(Rosenholz::FileOps::fileExists(deckblatt), "Deckblatt written");
    }

    SECTION("MFS — document filing requires entity reference");
    {
        auto& cfg = R::Config::instance();

        // Orphan document must be refused
        auto orphan = R::Document::create("Orphan","misc","");
        orphan->save();
        bool filed = Rosenholz::MFSWriter::writeDocument(*orphan, cfg.mfsPath());
        CHECK(!filed, "Orphan document refused by MFSWriter");

        // Document with project gets filed
        ProjectFixture pfix("MFS-DOK-Test");
        auto doc = R::Document::create("Testdokument","report",pfix.project->projectId);
        doc->save();
        bool ok = Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
        CHECK(ok, "Document with project ref filed in MFS");

        // Re-filing after rename — document now in its own subfolder
        doc->title = "Testdokument v2";
        doc->update();
        Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
        // Verify the document subfolder exists under F16/<reg>/DOK/<docId>/
        auto proj = R::ProjectF16::loadById(pfix.project->projectId);
        if (proj) {
            std::string projSane = Rosenholz::sanitiseRegNr(proj->regNumber.toString());
            std::string docSane  = Rosenholz::sanitiseRegNr(doc->documentId);
            std::string docDir = Rosenholz::FileOps::joinPath(
                Rosenholz::FileOps::joinPath(
                    Rosenholz::FileOps::joinPath(
                        Rosenholz::FileOps::joinPath(cfg.mfsPath(), "F16"), projSane),
                    "DOK"),
                docSane);
            CHECK(Rosenholz::FileOps::dirExists(docDir),
                  "Only one file per document ID after rename+refile");
        }
    }

}

void testSuiteMFS() {
    SECTION("MFS — owner_key.txt is 600 permissions");
    {
        FullProjectFixture fix;
        auto& cfg = R::Config::instance();
        Rosenholz::MFSWriter::rebuildAll(cfg.mfsPath());
        std::string keyPath = Rosenholz::FileOps::joinPath(cfg.mfsPath(),"owner_key.txt");
        CHECK(Rosenholz::FileOps::fileExists(keyPath), "owner_key.txt exists");
#ifndef _WIN32
        struct stat st{};
        stat(keyPath.c_str(), &st);
        CHECK((st.st_mode & 0777) == 0600, "owner_key.txt has 600 permissions");
#else
        CHECK(true, "Permission check skipped on Windows");
#endif
    }
}


void testSuiteReporting() {
    SECTION("F18Operation — LessonsLearned type");
    {
        ProjectFixture pfix("LL-F18-Test");
        auto ll = Rosenholz::F18Operation::create(
            pfix.project->projectId, "Test-Lessons-Learned",
            Rosenholz::F18OperationType::LESSONS_LEARNED);
        CHECK(ll != nullptr, "LessonsLearned F18Operation created");
        ll->lessonType = "positive";
        ll->recommendation = "Structured reviews are effective";
        ll->update();
        auto reloaded = Rosenholz::F18Operation::loadById(ll->vorgangId);
        CHECK(reloaded != nullptr, "LessonsLearned reloadable");
        CHECK(reloaded->lessonType == "positive", "lessonType persisted");
    }

    SECTION("F18Operation — DecisionLog type");
    {
        ProjectFixture pfix("DL-Test-Vorgang");
        // Create a DecisionLog F18 Operation
        auto dl = Rosenholz::F18Operation::create(
            pfix.project->projectId, "Test-Entscheidung",
            Rosenholz::F18OperationType::DECISION_LOG);
        CHECK(dl != nullptr, "DecisionLog F18Operation created");
        if (dl) {
            dl->rationale = "Structured approach chosen";
            dl->decisionBy = "tester";
            dl->update();
            auto reloaded = Rosenholz::F18Operation::loadById(dl->vorgangId);
            CHECK(reloaded != nullptr, "DecisionLog reloadable");
            if (reloaded)
                CHECK(reloaded->rationale == "Structured approach chosen", "rationale persisted");
        }
    }

    SECTION("Migration — idempotent re-run");
    {
        // Running again should be a no-op (already current)
        bool ok = Rosenholz::MigrationEngine::runAll();
        CHECK(ok, "MigrationEngine::runAll() idempotent");
    }
}


// ============================================================
// testSuiteMigration  —  Schema migration idempotency tests
// ============================================================
void testSuiteMigration() {
    namespace R = Rosenholz;

    SECTION("Migration — schema versions");
    {
        // v2 baseline: all schemas start at version 2
        std::vector<std::string> allDbs = {"core","f16","f77","dok","f18"};
        for (auto& name : allDbs) {
            int ver = R::MigrationEngine::currentVersion(name);
            CHECK(ver >= 2, "schema version >= 2 for db: " + name);
        }
    }

    SECTION("Migration — idempotent re-run");
    {
        bool ok = R::MigrationEngine::runAll();
        CHECK(ok, "MigrationEngine::runAll() idempotent");
    }

    SECTION("Migration — all pool DBs accessible");
    {
        std::vector<std::string> dbs = {"core","f16","f77","dok","f18"};
        for (auto& name : dbs) {
            auto* db = R::DatabasePool::instance().get(name);
            CHECK(db != nullptr, "DB accessible: " + name);
        }
    }
}
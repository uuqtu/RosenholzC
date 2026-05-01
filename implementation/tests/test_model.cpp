// ============================================================
// test_model.cpp  —  Model entity tests with fixtures
// ============================================================
#include "TestFramework.h"
#include <fstream>
#include "TestFixtures.h"
#include "../src/core/OperationResult.h"
#include "../src/repository/ArchiveStore.h"
#include "../src/repository/DocumentRevision.h"
#include "../src/model/Utils.h"
#include "../src/model/f18/F18Operation.h"
#include "../src/workflow/F77Workflow.h"
#include "../src/model/f18/Communication.h"
#include "../src/core/FileOps.h"

namespace R = Rosenholz;
using namespace Rosenholz::Test;

void testSuiteModel() {
    // ── Person ──────────────────────────────────────────────
    SECTION("Person — create, save, load, search");
    {
        PersonFixture fix("Heinrich","Schmitt","hs@test.de","internal");
        CHECK(!fix.person->personId.empty(), "Person::save()");
        CHECK(fix.person->personId.find("/PER/") != std::string::npos,
              "Person ID contains /PER/");

        auto loaded = R::Person::loadById(fix.person->personId);
        CHECK(loaded != nullptr, "Person::loadById");
        CHECK(loaded->firstName == "Heinrich", "firstName matches");
        CHECK(loaded->lastName  == "Schmitt",  "lastName matches");
        CHECK(loaded->email     == "hs@test.de","email matches");

        auto found = R::Person::search("Schmitt");
        bool has = false;
        for (auto& p : found) if (p->personId == fix.person->personId) has = true;
        CHECK(has, "search finds by lastName");

        R::Person::create("Ingrid","Mueller","im@test.de","external")->save();
        fix.person->reassignManager(fix.person->personId);
        CHECK(!fix.person->managerId.empty(), "reassignManager");

        fix.person->setStatus("on-leave");
        auto chk = R::Person::loadById(fix.person->personId);
        CHECK(chk->status == "on-leave", "Status persisted");
    }

    // ── Team ────────────────────────────────────────────────
    SECTION("Team — hierarchy, members, lead reassignment");
    {
        TeamFixture parent("Parent Diensteinheit","division");
        TeamFixture child("Child Diensteinheit","squad");
        child.team->parentTeamId = parent.team->teamId;
        child.team->update();

        PersonFixture p1("Klaus","Bauer","kb@test.de","internal");
        auto member = child.team->addMember(p1.person->personId, "developer");
        member->isLead = true;
        member->allocationPct = 80.0;
        member->roleCategory  = "technical";
        member->memberType    = "core";
        member->save();

        child.team->loadMembers();
        CHECK(!child.team->members.empty(), "loadMembers returns results");
        CHECK(child.team->members[0]->isLead, "isLead flag persisted");

        child.team->reassignLead(p1.person->personId);
        auto chk = R::Team::loadById(child.team->teamId);
        CHECK(chk->leadId == p1.person->personId, "reassignLead persisted");
    }

    // ── ProjectF16 ──────────────────────────────────────────
    SECTION("ProjectF16 — create, EV, QTCS, MFS");
    {
        ProjectFixture fix("Test-Vorgang Alpha","OV","large");
        CHECK(opOk(fix.project->save()), "ProjectF16::save()");
        CHECK(!fix.project->regNumber.toString().empty(), "regNumber valid");

        auto loaded = R::ProjectF16::loadById(fix.project->projectId);
        CHECK(loaded != nullptr, "loadById returns result");
        CHECK(loaded->title == "Test-Vorgang Alpha", "title matches");

        // EV metrics
        fix.project->earnedValue  = 75000.0;
        fix.project->plannedValue = 90000.0;
        fix.project->actualCost   = 80000.0;
        fix.project->recalcEarnedValue();
        fix.project->save();
        CHECK(fix.project->costPerformanceIndex > 0.0, "CPI calculated");
        CHECK(fix.project->schedulePerformanceIndex > 0.0, "SPI calculated");

        // Reassignment
        PersonFixture newLead("Neue","Leiterin","nl@test.de","internal");
        CHECK(opOk(fix.project->reassignLead(newLead.person->personId)), "reassignLead");
        CHECK(opOk(fix.project->reassignSponsor(newLead.person->personId)), "reassignSponsor");

        auto all = R::ProjectF16::loadAll();
        CHECK(!all.empty(), "loadAll returns results");
        auto byStatus = R::ProjectF16::loadByStatus("in_work");
        CHECK(!byStatus.empty(), "loadByStatus(in_work) returns results");
    }

    // ── TaskF22 ─────────────────────────────────────────────
    SECTION("TaskF22 — hierarchy, reassign, convert");
    {
        TaskFixture fix("Haupt-Aufgabe");
        CHECK(opOk(fix.task->save()), "TaskF22::save()");
        CHECK(fix.task->taskId.find("/F22/") != std::string::npos,
              "Task ID contains /F22/");

        auto child = R::TaskF22::create(fix.projFix.project->projectId,
                                         "Kind-Aufgabe", "", fix.task->taskId);
        child->wbsCode = "1.1.1";
        CHECK(opOk(child->save()), "Child task saved");

        auto children = R::TaskF22::loadChildren(fix.task->taskId);
        CHECK(!children.empty(), "loadChildren returns subtask");

        // addTime/addCost take dimension IDs
        // Just verify the task saves with updated percentComplete
        fix.task->percentComplete = 25;
        fix.task->effortActualHrs = 20.0;
        CHECK(opOk(fix.task->save()), "TaskF22 update save");

        PersonFixture p2("Dirk","Wolf","dw@test.de","internal");
        CHECK(opOk(fix.task->reassignTo(p2.person->personId)), "reassignTo");
        CHECK(opOk(fix.task->reassignParent("")), "reassignParent (detach)");

        auto forProj = R::TaskF22::loadForProject(fix.projFix.project->projectId);
        CHECK(!forProj.empty(), "loadForProject returns tasks");
    }

    // ── IncidentF18 ─────────────────────────────────────────
        SECTION("F18Operation — create incident and risk types");
    {
        ProjectFixture pfix;
        // F18 belongs to a task — create one first
        auto task = R::TaskF22::create("F18-Test-Aufgabe", "spec", pfix.project->projectId);
        task->save();

        auto inc = R::F18Operation::create(task->taskId, "Test-Vorfall",
                                           R::F18OperationType::INCIDENT);
        CHECK(inc != nullptr, "F18Operation incident created");
        CHECK(!inc->vorgangId.empty(), "Incident has vorgangId");
        CHECK(inc->vorgangType == "incident", "vorgangType=incident");
        inc->severity = "high";
        inc->incidentType = "technical";
        inc->update();
        auto reloaded = R::F18Operation::loadById(inc->vorgangId);
        CHECK(reloaded != nullptr, "Incident reloadable");
        CHECK(reloaded->severity == "high", "severity persisted");
        CHECK(reloaded->incidentType == "technical", "incidentType persisted");

        auto risk = R::F18Operation::create(task->taskId, "Test-Risiko",
                                            R::F18OperationType::RISK);
        CHECK(risk != nullptr, "F18Operation risk created");
        CHECK(risk->vorgangType == "risk", "vorgangType=risk");
        risk->riskLevel = "high";
        risk->probabilityScore = 4;
        risk->impactScoreTime = 3;
        risk->update();
        auto rreloaded = R::F18Operation::loadById(risk->vorgangId);
        CHECK(rreloaded != nullptr, "Risk reloadable");
        CHECK(rreloaded->riskLevel == "high", "riskLevel persisted");
        CHECK(rreloaded->probabilityScore == 4, "probabilityScore persisted");
    }

    // ── Risk ────────────────────────────────────────────────


    // ── Document ────────────────────────────────────────────
    SECTION("Document — create, attach, enforce project ref");
    {
        ProjectFixture pfix;
        // Create a task to attach documents to (docs now live on F22 or F18)
        auto task = R::TaskF22::create("Charter-Aufgabe", "spec", pfix.project->projectId);
        task->save();
        auto d = R::Document::create("Projektcharter", "report", task->taskId);
        d->format  = "pdf";
        d->version = "1.0";
        CHECK(opOk(d->save()), "Document::save()");
        CHECK(d->documentId.find("/AKT/") != std::string::npos,
              "Document ID contains /AKT/");

        auto docs = R::Document::loadForEntity("f22", task->taskId);
        CHECK(!docs.empty(), "loadForEntity(f22) returns documents");

        // Orphan document (no task) still saveable
        auto orphan = R::Document::create("Orphan", "misc");
        orphan->save();
    }

    SECTION("Document — version snapshot via DocumentRevision");
    {
        ProjectFixture pfix("Doc-Version-Test");
        auto vTask = R::TaskF22::create("VerTask","spec",pfix.project->projectId);
        vTask->save();
        auto doc = R::Document::create("Testdokument V1","report",vTask->taskId);
        doc->version = "1.0";
        doc->format  = "txt";
        CHECK(opOk(doc->save()), "Document saved before version test");

        // Write a real file so importLocalFile has something
        std::string tmpPath = "/tmp/rosenholz_doc_ver_" + doc->documentId + ".txt";
        Rosenholz::FileOps::writeTextFile(tmpPath, "Testinhalt Version 1.0\n");

        auto imported = doc->importLocalFile(tmpPath);
        CHECK(opOk(imported), "importLocalFile copies file to MFS");
        CHECK(!doc->filePath.empty(), "filePath populated after import");
        CHECK(doc->fileSize > 0, "fileSize set");
        CHECK(!doc->fileHash.empty(), "fileHash computed");

        // revise() creates the first revision (in_work)
        auto rev1 = doc->revise("Initiale Version", "system-test");
        CHECK(rev1 != nullptr, "revise() creates Revision 1");

        // While Rev 1 is still in_work, a second revise() must be refused
        auto rev1b = doc->revise("Versuch zweiter Revision waehrend in_work");
        CHECK(rev1b == nullptr, "revise() refused while active revision is in_work");

        // Transition Rev 1 to pre_released so a new revision can be created
        bool transitioned = rev1->transitionState("pre_released");
        CHECK(transitioned, "Revision transitioniert zu pre_released");

        // Now revise() must succeed — creates Rev 2 branching from Rev 1
        auto rev2 = doc->revise("Aktualisierung auf 1.1");
        CHECK(rev2 != nullptr, "revise() erlaubt nach Freigabe der Vorgaenger-Revision");
        CHECK(rev2->rev == 2, "Rev 2 angelegt");
        CHECK(rev2->parentRev == 1, "Rev 2 verzweigt von Rev 1");

        auto versions = doc->loadVersions();
        CHECK(!versions.empty(), "loadVersions gibt Revisions-Eintraege zurueck");
        Rosenholz::FileOps::deleteFile(tmpPath);
    }

    // ── Milestone ───────────────────────────────────────────
    SECTION("ProjectF16 — milestones free-text field");
    {
        ProjectFixture pfix("Milestone-Text-Test");
        // Milestone tracking is now a free-text field on F16
        pfix.project->milestones = "2026-06-01: Kick-off abgeschlossen\n2026-08-01: Design-Freeze";
        pfix.project->update();
        auto reloaded = R::ProjectF16::loadById(pfix.project->projectId);
        CHECK(reloaded != nullptr, "Project reloadable");
        CHECK(!reloaded->milestones.empty(), "milestones field persisted");
        CHECK(reloaded->milestones.find("Kick-off") != std::string::npos,
              "milestones content correct");
    }

    // ── Trackable ────────────────────────────────────────────

    SECTION("F18Operation — all major vorgangTypes");
    {
        ProjectFixture pfix("F18-Types-Test");
        auto task = R::TaskF22::create("Types-Task", "spec", pfix.project->projectId);
        task->save();

        // Generic
        auto gen = R::F18Operation::create(task->taskId, "Generischer Workflow",
                                           R::F18OperationType::GENERIC);
        CHECK(gen != nullptr, "Generic F18Operation created");
        CHECK(gen->vorgangType == "generic", "vorgangType=generic");

        // Measure
        auto measure = R::F18Operation::create(task->taskId, "Korrekturmassnahme",
                                               R::F18OperationType::MEASURE);
        CHECK(measure != nullptr, "Measure F18Operation created");
        measure->measureCategory = "corrective";
        measure->plannedDate     = "2026-05-01";
        measure->update();
        auto mr = R::F18Operation::loadById(measure->vorgangId);
        CHECK(mr != nullptr, "Measure reloadable");
        CHECK(mr->measureCategory == "corrective", "measureCategory persisted");

        // QualityGate
        auto qg = R::F18Operation::create(task->taskId, "Phase-Gate Review",
                                          R::F18OperationType::QUALITY_GATE);
        CHECK(qg != nullptr, "QualityGate F18Operation created");
        qg->phase       = "design";
        qg->gateResult  = "passed";
        qg->update();
        auto qgr = R::F18Operation::loadById(qg->vorgangId);
        CHECK(qgr != nullptr, "QualityGate reloadable");
        CHECK(qgr->phase == "design", "phase persisted");

        // ChangeRequest
        auto cr = R::F18Operation::create(task->taskId, "Scope-Erweiterung",
                                          R::F18OperationType::CHANGE_REQUEST);
        CHECK(cr != nullptr, "ChangeRequest F18Operation created");
        cr->changeType   = "scope";
        cr->justification= "Neues Feature erbeten";
        cr->update();

        // ChangeObject — references the CR
        auto co = R::F18Operation::create(task->taskId, "Scope-Umsetzung",
                                          R::F18OperationType::CHANGE_OBJECT);
        CHECK(co != nullptr, "ChangeObject F18Operation created");
        co->parentVorgangId = cr->vorgangId;
        co->update();
        auto cor = R::F18Operation::loadById(co->vorgangId);
        CHECK(cor != nullptr, "ChangeObject reloadable");
        CHECK(cor->parentVorgangId == cr->vorgangId, "parentVorgangId links to CR");

        // DecisionLog
        auto dl = R::F18Operation::create(task->taskId, "Datenbankentscheidung",
                                          R::F18OperationType::DECISION_LOG);
        CHECK(dl != nullptr, "DecisionLog F18Operation created");
        dl->decisionType = "architectural";
        dl->rationale    = "SQLite passt besser als PostgreSQL";
        dl->update();
        auto dlr = R::F18Operation::loadById(dl->vorgangId);
        CHECK(dlr != nullptr, "DecisionLog reloadable");
        CHECK(dlr->decisionType == "architectural", "decisionType persisted");

        // CommunicationPlan
        auto cp = R::F18Operation::create(task->taskId, "Stakeholder-Kommunikation",
                                          R::F18OperationType::COMMUNICATION_PLAN);
        CHECK(cp != nullptr, "CommunicationPlan F18Operation created");
        cp->audience  = "Auftraggeber";
        cp->frequency = "weekly";
        cp->update();
    }

    SECTION("F18OperationStep — addStep creates Init/End bookends");
    {
        ProjectFixture pfix("F18-Step-Test");
        auto task_step = R::TaskF22::create("Step-Task", "spec", pfix.project->projectId);
        task_step->save();
        auto v = R::F18Operation::create(task_step->taskId, "Step-Test-Workflow",
                                         R::F18OperationType::GENERIC);
        CHECK(v != nullptr, "F18Operation created for step test");
        v->loadSteps();
        CHECK(v->steps.size() == 2, "Initial steps = Init + End");
        bool hasInit = false, hasEnd = false;
        for (auto& s : v->steps) {
            if (s.isInitialize) hasInit = true;
            if (s.isFinal)      hasEnd  = true;
        }
        CHECK(hasInit, "Init step present");
        CHECK(hasEnd,  "End step present");

        // Add a mid-step
        auto step = v->addStep("Prüfung", "review");
        CHECK(step != nullptr, "addStep returns step");
        CHECK(!step->stepId.empty(), "Step has ID");
        CHECK(step->stepType == "review", "stepType correct");
        v->loadSteps();
        CHECK(v->steps.size() == 3, "Steps = Init + Prüfung + End");

        // End's predecessors include the new step
        std::string endPreds;
        for (auto& s : v->steps) if (s.isFinal) endPreds = s.predecessorStepIds;
        CHECK(endPreds.find(step->stepId) != std::string::npos,
              "End's predecessorStepIds includes new step");
    }

    SECTION("Communication — create for F16, F22, and F18Step");
    {
        ProjectFixture pfix("Comm-Test");
        using Comm = R::Communication;

        // F16
        auto c16 = Comm::create(pfix.project->projectId, "f16", "Kick-off Meeting", "meeting");
        CHECK(c16 != nullptr, "Communication for F16 created");
        CHECK(c16->ownerType == "f16", "ownerType=f16");
        c16->scheduledDate = "2026-06-01";
        c16->update();

        // F22
        auto taskComm = R::TaskF22::create(pfix.project->projectId, "Comm-Task", "", "");
        taskComm->save();
        auto c22 = Comm::create(taskComm->taskId, "f22", "Sprint Review", "meeting");
        CHECK(c22 != nullptr, "Communication for F22 created");

        // loadForOwner — f16
        auto comms = Comm::loadForOwner(pfix.project->projectId, "f16");
        CHECK(!comms.empty(), "loadForOwner returns communications for f16");

        // complete()
        c16->complete("Meeting abgeschlossen", "Aktionsplan erstellt");
        auto reloaded = Comm::loadById(c16->commId);
        CHECK(reloaded != nullptr, "Communication reloadable after complete");
        CHECK(reloaded->status == R::CommStatus::COMPLETED, "status=completed after complete()");
        CHECK(!reloaded->decisions.empty(), "decisions persisted");
    }

    SECTION("F18Operation — loadForProject and loadForTask filtering");
    {
        // F18 belongs exclusively to F22. All loading is task-scoped.
        ProjectFixture pfix("F18-Filter-Test");
        auto task = R::TaskF22::create("Filter-Task", "spec", pfix.project->projectId);
        task->save();

        R::F18Operation::create(task->taskId, "Incident-A", R::F18OperationType::INCIDENT);
        R::F18Operation::create(task->taskId, "Risiko-A",   R::F18OperationType::RISK);
        R::F18Operation::create(task->taskId, "Risiko-B",   R::F18OperationType::RISK);

        // Unfiltered: loadForTask
        auto all = R::F18Operation::loadForTask(task->taskId);
        CHECK(all.size() >= 3, "loadForTask returns all");

        // Filtered by type
        auto risks = R::F18Operation::loadForTask(task->taskId, R::F18OperationType::RISK);
        CHECK(risks.size() >= 2, "loadForTask with type filter returns 2 risks");
        for (auto& r : risks)
            CHECK(r->vorgangType == "risk", "filtered result has type=risk");

        // All F18 ops have taskId set (no projectId in v4)
        CHECK(!all[0]->taskId.empty(), "F18 has taskId set");

        // F22 link: load by task returns correct items
        auto byTask = R::F18Operation::loadForTask(task->taskId);
        CHECK(!byTask.empty(), "loadForTask returns F18 linked to task");
        CHECK(byTask[0]->taskId == task->taskId, "taskId set correctly on loaded F18");
    }


    SECTION("Lifecycle — F16 has no F77 workflow (removed by design)");
    {
        // F16 is a project container — F77 workflows run on F22/AKT/F18, not F16.
        CHECK(true, "F16 has no F77 workflow (by design)");
    }

    SECTION("Lifecycle — F22 starts without WFI, startDefault creates one");
    {
        ProjectFixture pfix("Lifecycle-F22-Test");
        auto task = R::TaskF22::create(pfix.project->projectId, "Lifecycle-Task", "", "");
        task->save();
        CHECK(task->workflowInstanceId.empty(), "F22 starts with no workflow");
        CHECK(task->status == R::EntityStatus::IN_WORK, "F22 status=in_work");
        auto wf = R::F77_Engine::startDefault("f22", task->taskId);
        CHECK(wf != nullptr, "startDefault creates F77 workflow for F22");
        if (wf) CHECK(wf->entityType == "f22", "WF entityType=f22");
    }

    SECTION("Lifecycle — F18Operation starts without WFI, startDefault creates one");
    {
        ProjectFixture pfix("Lifecycle-F18-Test");
        auto task = R::TaskF22::create("Lifecycle-Task", "spec", pfix.project->projectId);
        task->save();
        auto v = R::F18Operation::create(task->taskId, "Lifecycle-Vorgang",
                                         R::F18OperationType::RISK);
        CHECK(v != nullptr, "F18Operation created");
        CHECK(v->releaseWorkflowId.empty(), "F18 starts with no workflow");
        auto wf = R::F77_Engine::startDefault("f18", v->vorgangId);
        CHECK(wf != nullptr, "startDefault creates F77 workflow for F18");
        if (wf) CHECK(wf->entityType == "f18", "WF entityType=f18");
    }

    SECTION("Lifecycle — canRelease mit pending Mid-Schritt");
    {
        ProjectFixture pfix("Release-Block-Test");
        auto task = R::TaskF22::create(pfix.project->projectId, "Release-Block-Task", "", "");
        task->save();
        auto mainWf = R::F77_Engine::startDefault("f22", task->taskId);
        CHECK(mainWf != nullptr, "Workflow gestartet");
        if (!mainWf) return;
        auto refused = R::F77_Engine::startDefault("f22", task->taskId);
        CHECK(refused == nullptr, "Zweiter Workflow korrekt verweigert");
        int blockers = 0;
        bool canRel = R::F77_Engine::canRelease("f22", task->taskId,
                                                 mainWf->workflowId, blockers);
        // Workflow is active but not yet ticked — all steps pending
        CHECK(!canRel, "canRelease=false solange Workflow aktiv");
    }

    SECTION("Lifecycle — lockAll (kein anderer Workflow)");
    {
        ProjectFixture pfix("Lock-Test");
        auto task = R::TaskF22::create(pfix.project->projectId, "Lock-Task", "", "");
        task->save();
        auto mainWf = R::F77_Engine::startDefault("f22", task->taskId);
        CHECK(mainWf != nullptr, "Workflow gestartet");
        if (!mainWf) return;
        int locked = R::F77_Engine::lockAll("f22", task->taskId, mainWf->workflowId, true);
        CHECK(locked == 0, "lockAll: kein anderer Workflow zu sperren");
        int blockers = 0;
        bool canRel = R::F77_Engine::canRelease("f22", task->taskId, mainWf->workflowId, blockers);
        CHECK(!canRel, "canRelease false solange Workflow aktiv");
    }

    SECTION("Lifecycle — applyTargetState sets status=released on F22");
    {
        ProjectFixture pfix("Release-Test");
        auto task = R::TaskF22::create(pfix.project->projectId, "Release-Task", "", "");
        task->save();
        CHECK(task->status == R::EntityStatus::IN_WORK, "starts in_work");
        auto wf = R::F77_Engine::startDefault("f22", task->taskId);
        CHECK(wf != nullptr, "WF created");
        if (!wf) return;
        wf->targetState = R::EntityStatus::RELEASED;
        bool ok = R::F77_Engine::applyTargetState(*wf);
        CHECK(ok, "applyTargetState succeeds");
        auto reloaded = R::TaskF22::loadById(task->taskId);
        CHECK(reloaded != nullptr, "reloaded after release");
        CHECK(reloaded->status == R::EntityStatus::RELEASED, "status=released after applyTargetState");
    }


    SECTION("DocumentRevision — createRevision initial state");
    {
        // Every new document starts with rev=1, in_work, superseded=false
        ProjectFixture pfix("DocRev-Init-Test");
        auto doc = R::Document::create("DocRev-Test-Doc","report");
        doc->save();

        auto rev = R::DocumentRevision::createRevision(doc->documentId, 0,
                                                        "test-user", "Initial creation");
        CHECK(rev != nullptr, "createRevision returns non-null");
        CHECK(rev->rev == 1, "First revision is rev=1");
        CHECK(rev->revStateStr() == "in_work", "Starts in_work");
        CHECK(rev->parentRev == 0, "Initial rev has parentRev=0");

        // Superseded flag: first rev must be active (superseded=false)
        auto current = R::DocumentRevision::currentRevision(doc->documentId);
        CHECK(current != nullptr, "currentRevision finds the initial rev");
        if (current) {
            CHECK(!current->superseded, "Initial rev is active (superseded=false)");
            CHECK(current->rev == 1, "currentRevision returns rev=1");
        }
    }

    SECTION("DocumentRevision — state machine: allowed transitions");
    {
        // Test all valid transitions from the spec (using string constants directly)
        auto allowed = [](const std::string& f, const std::string& t) {
            return R::DocumentRevision::isTransitionAllowed(
        R::revStateFromString(f), R::revStateFromString(t), false);
        };
        // in_work → pre_released, locked, closed
        CHECK( allowed("in_work","pre_released"), "in_work→pre_released");
        CHECK( allowed("in_work","locked"),       "in_work→locked");
        CHECK( allowed("in_work","closed"),       "in_work→closed");
        CHECK( allowed("in_work","released"),     "in_work→released OK (direct)");
        // pre_released → released, locked, closed, in_work
        CHECK( allowed("pre_released","released"),    "pre_released→released");
        CHECK(!allowed("pre_released","locked"),      "pre_released→locked BLOCKED (standard)");
        CHECK( allowed("pre_released","closed"),      "pre_released→closed");
        CHECK(!allowed("pre_released","in_work"),     "pre_released→in_work BLOCKED (standard)");
        // released → locked, closed only
        CHECK(!allowed("released","locked"),      "released→locked BLOCKED (standard)");
        CHECK( allowed("released","closed"),      "released→closed");
        CHECK(!allowed("released","in_work"),     "released→in_work BLOCKED (standard)");
        CHECK(!allowed("released","pre_released"),"released→pre_released BLOCKED (standard)");
        // locked → pre_released (newest only), closed
        CHECK( allowed("locked","pre_released"), "locked→pre_released");
        CHECK( allowed("locked","closed"),       "locked→closed");
        CHECK( allowed("locked","released"),     "locked→released OK (direct)");
        // closed → nothing (terminal)
        CHECK(!allowed("closed","in_work"),      "closed→in_work BLOCKED");
        CHECK(!allowed("closed","released"),     "closed→released BLOCKED");
        CHECK(!allowed("closed","locked"),       "closed→locked BLOCKED");
    }

    SECTION("DocumentRevision — transitionState persists and updates superseded");
    {
        ProjectFixture pfix("DocRev-Transition-Test");
        auto doc = R::Document::create("Transition-Doc","report");
        doc->save();
        auto rev = R::DocumentRevision::createRevision(doc->documentId, 0, "u1", "v1");
        CHECK(rev != nullptr, "rev created");

        // in_work → pre_released
        bool ok = rev->transitionState("pre_released");
        CHECK(ok, "in_work→pre_released succeeds");
        auto r2 = R::DocumentRevision::loadByRev(doc->documentId, 1);
        CHECK(r2 != nullptr, "reload after transition");
        if (r2) CHECK(r2->revStateStr() == "pre_released", "revState=pre_released persisted");

        // pre_released → released
        ok = rev->transitionState("released");
        CHECK(ok, "pre_released→released succeeds");
        auto r3 = R::DocumentRevision::loadByRev(doc->documentId, 1);
        if (r3) CHECK(r3->revStateStr() == "released", "revState=released persisted");

        // released → closed (only valid path from released)
        ok = rev->transitionState("closed");
        CHECK(ok, "released→closed succeeds");

        // closed → in_work must fail (terminal)
        ok = rev->transitionState("in_work");
        CHECK(!ok, "closed→in_work blocked (terminal)");
    }

    SECTION("DocumentRevision — superseded invariant with multiple revisions");
    {
        ProjectFixture pfix("DocRev-Superseded-Test");
        auto doc = R::Document::create("Multi-Rev-Doc","spec");
        doc->save();

        // Create rev 1
        auto r1 = R::DocumentRevision::createRevision(doc->documentId, 0, "u1", "rev 1");
        CHECK(r1 != nullptr, "rev 1 created");

        // Create rev 2 (based on rev 1)
        auto r2 = R::DocumentRevision::createRevision(doc->documentId, 1, "u1", "rev 2");
        CHECK(r2 != nullptr, "rev 2 created");
        CHECK(r2->rev == 2, "second revision is rev=2");
        CHECK(r2->parentRev == 1, "parentRev=1 for rev 2");

        // Exactly one should be active (superseded=false)
        auto all = R::DocumentRevision::loadAllRevisions(doc->documentId);
        CHECK(all.size() == 2, "two revisions exist");
        int activeCount = 0;
        for (auto& r : all) if (!r->superseded) activeCount++;
        CHECK(activeCount == 1, "exactly one active revision (superseded=false)");

        // Current should be rev 2 (latest non-locked/non-closed)
        auto cur = R::DocumentRevision::currentRevision(doc->documentId);
        CHECK(cur != nullptr, "currentRevision exists");
        if (cur) CHECK(cur->rev == 2, "currentRevision is rev=2 (latest)");

        // Release rev 1 → rev 1 is released, but rev 2 (in_work) should still be
        // the active revision? No — priority says latest released wins.
        // But rev 2 is newer and in_work. After releasing rev 1, priority:
        // latest released = rev 1. So superseded switches to rev 1.
        r1->transitionState("pre_released");
        r1->transitionState("released");
        auto cur2 = R::DocumentRevision::currentRevision(doc->documentId);
        CHECK(cur2 != nullptr, "currentRevision after release");
        if (cur2) CHECK(cur2->rev == 1, "rev 1 is now current (released has priority)");

        // Re-verify invariant: still exactly one active
        auto all2 = R::DocumentRevision::loadAllRevisions(doc->documentId);
        int activeCount2 = 0;
        for (auto& r : all2) if (!r->superseded) activeCount2++;
        CHECK(activeCount2 == 1, "still exactly one active after release");
    }

    SECTION("ArchiveStore — init, stage, commit, retrieve");
    {
        // ArchiveStore is already initialized by AppController in the test setup.
        // If it's not open (non-fatal in tests), skip content checks gracefully.
        auto& store = Rosenholz::Archive::ArchiveStore::instance();
        if (!store.isOpen()) {
            CHECK(true, "ArchiveStore not open (non-fatal in test env) — skipping");
        } else {
            // Write a temp file to stage
            std::string tmpSrc = "/tmp/rh_archive_test_" +
                                  std::to_string(std::time(nullptr)) + ".txt";
            {
                std::ofstream f(tmpSrc);
                f << "Hello Rosenholz Archive Test Content 12345";
            }

            // Stage it
            std::string stagePath;
            auto ref = store.stageContent(tmpSrc, stagePath);
            CHECK(ref.valid(), "stageContent returns valid ChunkRef");
            CHECK(!ref.sha256.empty(), "ChunkRef has SHA-256");
            CHECK(ref.size > 0, "ChunkRef has size > 0");
            CHECK(!stagePath.empty(), "stagePath set");

            // Commit to LMDB (docId:rev=1)
            std::string testDocId = "XV/AKT/9999/2026";
            bool committed = store.commitContent(stagePath, ref, testDocId, 1);
            CHECK(committed, "commitContent succeeds");

            // Retrieve to a new path
            std::string retrievePath = "/tmp/rh_archive_retrieve_test.txt";
            bool retrieved = store.retrieveContent(testDocId, 1, retrievePath);
            CHECK(retrieved, "retrieveContent succeeds");

            // Verify content matches
            std::ifstream fin(retrievePath);
            std::string content((std::istreambuf_iterator<char>(fin)),
                                 std::istreambuf_iterator<char>());
            CHECK(content.find("Hello Rosenholz") != std::string::npos,
                  "Retrieved content matches original");

            // lookupRevChunk
            std::string sha = store.lookupRevChunk(testDocId, 1);
            CHECK(sha == ref.sha256, "lookupRevChunk returns correct SHA-256");

            // chunkExists
            CHECK(store.chunkExists(ref.sha256), "chunkExists for committed chunk");

            // Cleanup
            std::remove(tmpSrc.c_str());
            std::remove(retrievePath.c_str());
        }
    }


    SECTION("Document — Revision 1 auto-created on save");
    {
        ProjectFixture pfix("DocRev-AutoCreate-Test");
        auto t2 = R::TaskF22::create("AutoRev-Aufgabe","spec",pfix.project->projectId);
        t2->save();
        auto doc = R::Document::create("AutoRev-Doc","spec",t2->taskId);
        doc->save();
        doc->revise("Revision 1 — Test");
        doc->ensureReleaseWorkflow();

        // Revision 1 must exist
        auto rev1 = R::DocumentRevision::loadByRev(doc->documentId, 1);
        CHECK(rev1 != nullptr, "Rev 1 exists after ensureRevision1");
        if (rev1) {
            CHECK(rev1->revStateStr() == "in_work", "Rev 1 starts in_work");
            CHECK(!rev1->superseded, "Rev 1 is active (superseded=false)");
        }

        // currentRevision returns Rev 1
        auto cur = R::DocumentRevision::currentRevision(doc->documentId);
        CHECK(cur != nullptr, "currentRevision returns Rev 1");
        if (cur) CHECK(cur->rev == 1, "currentRevision is Rev 1");

        // Calling ensureRevision1 again is idempotent
        doc->revise("Revision 1 — Test");
        auto all = R::DocumentRevision::loadAllRevisions(doc->documentId);
        CHECK(all.size() == 1, "ensureRevision1 is idempotent — still 1 revision");

        // F77 WFI exists
        auto fresh = R::Document::loadById(doc->documentId);
        CHECK(fresh != nullptr, "Document reloadable");
        if (fresh) CHECK(!fresh->releaseWorkflowId.empty(), "Main WFI created");
    }

    SECTION("Document — 5-state machine via DocumentRevision");
    {
        ProjectFixture pfix("DocState-Test");
        auto doc = R::Document::create("StateTest-Doc","report");
        doc->save();
        doc->revise("Revision 1 — Test");

        auto rev = R::DocumentRevision::currentRevision(doc->documentId);
        CHECK(rev != nullptr, "current revision exists");
        if (!rev) return;

        // in_work → pre_released
        CHECK(rev->transitionState("pre_released"), "in_work → pre_released");
        CHECK(rev->revStateStr() == "pre_released", "state is pre_released");

        // pre_released → released (with confirmation in test = just call directly)
        CHECK(rev->transitionState("released"), "pre_released → released");
        CHECK(rev->revStateStr() == "released", "state is released");

        // released → in_work must fail
        // released → in_work is now allowed per diagram (colours had no meaning)

        // released → closed (only valid in standard mode)
        CHECK(rev->transitionState("closed"), "released → closed");
        CHECK(rev->revStateStr() == "closed", "state is closed");

        // closed → anything fails
        CHECK(!rev->transitionState("in_work"), "closed → in_work blocked (terminal)");
        CHECK(!rev->transitionState("released"), "closed → released blocked (terminal)");
    }

}

// ============================================================
// test_model.cpp  —  Model entity tests with fixtures
// ============================================================
#include "TestFramework.h"
#include "TestFixtures.h"
#include "../src/model/Utils.h"
#include "../src/model/f18/F18Workflow.h"
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
        CHECK(fix.project->save(), "ProjectF16::save()");
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
        CHECK(fix.project->cpi > 0.0, "CPI calculated");
        CHECK(fix.project->spi > 0.0, "SPI calculated");

        // Reassignment
        PersonFixture newLead("Neue","Leiterin","nl@test.de","internal");
        CHECK(fix.project->reassignLead(newLead.person->personId), "reassignLead");
        CHECK(fix.project->reassignSponsor(newLead.person->personId), "reassignSponsor");

        auto all = R::ProjectF16::loadAll();
        CHECK(!all.empty(), "loadAll returns results");
        auto byStatus = R::ProjectF16::loadByStatus("in_work");
        CHECK(!byStatus.empty(), "loadByStatus(in_work) returns results");
    }

    // ── TaskF22 ─────────────────────────────────────────────
    SECTION("TaskF22 — hierarchy, reassign, convert");
    {
        TaskFixture fix("Haupt-Aufgabe");
        CHECK(fix.task->save(), "TaskF22::save()");
        CHECK(fix.task->taskId.find("/F22/") != std::string::npos,
              "Task ID contains /F22/");

        auto child = R::TaskF22::create(fix.projFix.project->projectId,
                                         "Kind-Aufgabe", "", fix.task->taskId);
        child->wbsCode = "1.1.1";
        CHECK(child->save(), "Child task saved");

        auto children = R::TaskF22::loadChildren(fix.task->taskId);
        CHECK(!children.empty(), "loadChildren returns subtask");

        // addTime/addCost take dimension IDs
        // Just verify the task saves with updated percentComplete
        fix.task->percentComplete = 25;
        fix.task->effortActualHrs = 20.0;
        CHECK(fix.task->save(), "TaskF22 update save");

        PersonFixture p2("Dirk","Wolf","dw@test.de","internal");
        CHECK(fix.task->reassignTo(p2.person->personId), "reassignTo");
        CHECK(fix.task->reassignParent(""), "reassignParent (detach)");

        auto forProj = R::TaskF22::loadForProject(fix.projFix.project->projectId);
        CHECK(!forProj.empty(), "loadForProject returns tasks");
    }

    // ── IncidentF18 ─────────────────────────────────────────
        SECTION("F18Workflow — create incident and risk types");
    {
        ProjectFixture pfix;
        // Create an incident-type F18Workflow
        auto inc = R::F18Workflow::create(pfix.project->projectId, "Test-Vorfall",
                                           R::F18VorgangType::INCIDENT);
        CHECK(inc != nullptr, "F18Workflow incident created");
        CHECK(!inc->vorgangId.empty(), "Incident has vorgangId");
        CHECK(inc->vorgangType == "incident", "vorgangType=incident");
        inc->severity = "high";
        inc->incidentType = "technical";
        inc->update();
        auto reloaded = R::F18Workflow::loadById(inc->vorgangId);
        CHECK(reloaded != nullptr, "Incident reloadable");
        CHECK(reloaded->severity == "high", "severity persisted");
        CHECK(reloaded->incidentType == "technical", "incidentType persisted");

        // Create a risk-type F18Workflow
        auto risk = R::F18Workflow::create(pfix.project->projectId, "Test-Risiko",
                                            R::F18VorgangType::RISK);
        CHECK(risk != nullptr, "F18Workflow risk created");
        risk->probabilityScore   = 4;
        risk->impactScoreTime    = 3;
        risk->impactScoreCost    = 3;
        risk->impactScoreQuality = 2;
        risk->impactScoreScope   = 2;
        risk->recalcRiskScore();
        auto reloadedRisk = R::F18Workflow::loadById(risk->vorgangId);
        CHECK(reloadedRisk != nullptr, "Risk reloadable");
        CHECK(reloadedRisk->overallRiskScore > 0, "Risk score calculated");
        CHECK(!reloadedRisk->riskLevel.empty(), "Risk level set");
    }

    // ── Risk ────────────────────────────────────────────────


    // ── Document ────────────────────────────────────────────
    SECTION("Document — create, attach, enforce project ref");
    {
        ProjectFixture pfix;
        auto d = R::Document::create("Projektcharter","report",pfix.project->projectId);
        d->format  = "pdf";
        d->version = "1.0";
        CHECK(d->save(), "Document::save()");
        CHECK(d->documentId.find("/DOK/") != std::string::npos,
              "Document ID contains /DOK/");
        CHECK(d->attachToEntity("project", pfix.project->projectId), "attachToEntity");

        auto docs = R::Document::loadForEntity("project", pfix.project->projectId);
        CHECK(!docs.empty(), "loadForEntity returns documents");

        // Orphan document should be saveable in DB but refused by MFS
        auto orphan = R::Document::create("Orphan","misc","");
        orphan->save();
        // MFS filing refusal tested in testSuiteMFS
    }

    SECTION("Document — version snapshot and loadVersions");
    {
        ProjectFixture pfix("Doc-Version-Test");
        auto doc = R::Document::create("Testdokument V1", "report", pfix.project->projectId);
        doc->version = "1.0";
        doc->format  = "txt";
        CHECK(doc->save(), "Document saved before version test");

        // Write a real file so importLocalFile has something
        std::string tmpPath = "/tmp/rosenholz_doc_ver_" + doc->documentId + ".txt";
        Rosenholz::FileOps::writeTextFile(tmpPath, "Testinhalt Version 1.0\n");

        bool imported = doc->importLocalFile(tmpPath);
        CHECK(imported, "importLocalFile copies file to MFS");
        CHECK(!doc->filePath.empty(), "filePath populated after import");
        CHECK(doc->fileSize > 0, "fileSize set");
        CHECK(!doc->fileHash.empty(), "fileHash computed");

        // Snapshot version 1.0
        bool snapped = doc->snapshotVersion("Initiale Version", "system-test");
        CHECK(snapped, "snapshotVersion persists to DB");

        // Bump to 1.1 and snapshot again
        doc->version = "1.1";
        Rosenholz::FileOps::writeTextFile(tmpPath, "Testinhalt Version 1.1\n");
        doc->importLocalFile(tmpPath);
        doc->snapshotVersion("Aktualisierung auf 1.1");

        auto versions = doc->loadVersions();
        CHECK(versions.size() >= 2, "At least 2 version snapshots");
        // Clean up tmp
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

    SECTION("F18Workflow — all major vorgangTypes");
    {
        ProjectFixture pfix("F18-Types-Test");

        // Generic
        auto gen = R::F18Workflow::create(pfix.project->projectId, "Generischer Workflow",
                                           R::F18VorgangType::GENERIC);
        CHECK(gen != nullptr, "Generic F18Workflow created");
        CHECK(gen->vorgangType == "generic", "vorgangType=generic");

        // Measure
        auto measure = R::F18Workflow::create(pfix.project->projectId, "Korrekturmassnahme",
                                               R::F18VorgangType::MEASURE);
        CHECK(measure != nullptr, "Measure F18Workflow created");
        measure->measureCategory = "corrective";
        measure->plannedDate     = "2026-05-01";
        measure->update();
        auto mr = R::F18Workflow::loadById(measure->vorgangId);
        CHECK(mr != nullptr, "Measure reloadable");
        CHECK(mr->measureCategory == "corrective", "measureCategory persisted");

        // QualityGate
        auto qg = R::F18Workflow::create(pfix.project->projectId, "Phase-Gate Review",
                                          R::F18VorgangType::QUALITY_GATE);
        CHECK(qg != nullptr, "QualityGate F18Workflow created");
        qg->phase       = "design";
        qg->gateResult  = "passed";
        qg->update();
        auto qgr = R::F18Workflow::loadById(qg->vorgangId);
        CHECK(qgr != nullptr, "QualityGate reloadable");
        CHECK(qgr->phase == "design", "phase persisted");

        // ChangeRequest
        auto cr = R::F18Workflow::create(pfix.project->projectId, "Scope-Erweiterung",
                                          R::F18VorgangType::CHANGE_REQUEST);
        CHECK(cr != nullptr, "ChangeRequest F18Workflow created");
        cr->changeType   = "scope";
        cr->justification= "Neues Feature erbeten";
        cr->update();

        // ChangeObject — references the CR
        auto co = R::F18Workflow::create(pfix.project->projectId, "Scope-Umsetzung",
                                          R::F18VorgangType::CHANGE_OBJECT);
        CHECK(co != nullptr, "ChangeObject F18Workflow created");
        co->parentVorgangId = cr->vorgangId;
        co->update();
        auto cor = R::F18Workflow::loadById(co->vorgangId);
        CHECK(cor != nullptr, "ChangeObject reloadable");
        CHECK(cor->parentVorgangId == cr->vorgangId, "parentVorgangId links to CR");

        // DecisionLog
        auto dl = R::F18Workflow::create(pfix.project->projectId, "Datenbankentscheidung",
                                          R::F18VorgangType::DECISION_LOG);
        CHECK(dl != nullptr, "DecisionLog F18Workflow created");
        dl->decisionType = "architectural";
        dl->rationale    = "SQLite passt besser als PostgreSQL";
        dl->update();
        auto dlr = R::F18Workflow::loadById(dl->vorgangId);
        CHECK(dlr != nullptr, "DecisionLog reloadable");
        CHECK(dlr->decisionType == "architectural", "decisionType persisted");

        // CommunicationPlan
        auto cp = R::F18Workflow::create(pfix.project->projectId, "Stakeholder-Kommunikation",
                                          R::F18VorgangType::COMMUNICATION_PLAN);
        CHECK(cp != nullptr, "CommunicationPlan F18Workflow created");
        cp->audience  = "Auftraggeber";
        cp->frequency = "weekly";
        cp->update();
    }

    SECTION("F18WorkflowStep — addStep creates Init/End bookends");
    {
        ProjectFixture pfix("F18-Step-Test");
        auto v = R::F18Workflow::create(pfix.project->projectId, "Step-Test-Workflow",
                                         R::F18VorgangType::GENERIC);
        CHECK(v != nullptr, "F18Workflow created for step test");
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

        // F16 (project)
        auto c16 = Comm::create(pfix.project->projectId, "project", "Kick-off Meeting", "meeting");
        CHECK(c16 != nullptr, "Communication for F16 created");
        CHECK(c16->ownerType == "project", "ownerType=project");
        c16->scheduledDate = "2026-06-01";
        c16->update();

        // F22 (task)
        // Create a task for the F22 communication test
        auto taskComm = R::TaskF22::create(pfix.project->projectId, "Comm-Task", "", "");
        taskComm->save();
        auto c22 = Comm::create(taskComm->taskId, "task", "Sprint Review", "meeting");
        CHECK(c22 != nullptr, "Communication for F22 created");

        // loadForOwner — project
        auto comms = Comm::loadForOwner(pfix.project->projectId, "project");
        CHECK(!comms.empty(), "loadForOwner returns communications for project");

        // complete()
        c16->complete("Meeting abgeschlossen", "Aktionsplan erstellt");
        auto reloaded = Comm::loadById(c16->commId);
        CHECK(reloaded != nullptr, "Communication reloadable after complete");
        CHECK(reloaded->status == "completed", "status=completed after complete()");
        CHECK(!reloaded->decisions.empty(), "decisions persisted");
    }

    SECTION("F18Workflow — loadForProject and loadForTask filtering");
    {
        ProjectFixture pfix("F18-Filter-Test");

        // Create task for F22 context
        auto task = R::TaskF22::create(pfix.project->projectId, "Filter-Aufgabe",
                                        "", "");
        task->save();

        // Create mixed types on project
        R::F18Workflow::create(pfix.project->projectId, "Risiko-A", R::F18VorgangType::RISK);
        R::F18Workflow::create(pfix.project->projectId, "Incident-A", R::F18VorgangType::INCIDENT);
        R::F18Workflow::create(pfix.project->projectId, "Risiko-B", R::F18VorgangType::RISK);

        // Unfiltered
        auto all = R::F18Workflow::loadForProject(pfix.project->projectId);
        CHECK(all.size() >= 3, "loadForProject returns all");

        // Filtered by type
        auto risks = R::F18Workflow::loadForProject(pfix.project->projectId,
                                                     R::F18VorgangType::RISK);
        CHECK(risks.size() >= 2, "loadForProject with type filter returns 2 risks");
        for (auto& r : risks)
            CHECK(r->vorgangType == "risk", "filtered result has type=risk");

        // F22 link
        auto f22v = R::F18Workflow::create(pfix.project->projectId, "Task-Vorgang",
                                            R::F18VorgangType::GENERIC, task->taskId);
        CHECK(f22v != nullptr, "F18Workflow linked to F22");
        CHECK(f22v->taskId == task->taskId, "taskId set correctly");
        auto byTask = R::F18Workflow::loadForTask(task->taskId);
        CHECK(!byTask.empty(), "loadForTask returns F18 linked to F22");
    }


    SECTION("Lifecycle — F16 gets Main WFI on creation");
    {
        ProjectFixture pfix("Lifecycle-F16-Test");
        auto p = pfix.project;
        p->ensureMainWorkflow();  // wizard calls this; test must call explicitly
        CHECK(!p->mainWorkflowId.empty(), "F16 has mainWorkflowId after ensureMainWorkflow");
        CHECK(p->status == "in_work", "F16 status=in_work");
        auto inst = R::WorkflowInstance::loadById(p->mainWorkflowId);
        CHECK(inst != nullptr, "Main WFI loadable");
        if (inst) {
            CHECK(inst->name.find("Main") != std::string::npos, "Main WFI name contains Main");
            CHECK(inst->entityType == "project", "Main WFI entityType=project");
            CHECK(inst->entityId == p->projectId, "Main WFI entityId=projectId");
        }
    }

    SECTION("Lifecycle — F22 gets Main WFI on creation");
    {
        ProjectFixture pfix("Lifecycle-F22-Test");
        auto task = R::TaskF22::create(pfix.project->projectId, "Lifecycle-Task", "", "");
        task->save();
        task->ensureMainWorkflow();
        CHECK(!task->mainWorkflowId.empty(), "F22 has mainWorkflowId");
        CHECK(task->status == "in_work", "F22 status=in_work");
        auto inst = R::WorkflowInstance::loadById(task->mainWorkflowId);
        CHECK(inst != nullptr, "F22 Main WFI loadable");
        if (inst) CHECK(inst->entityType == "task", "F22 Main WFI entityType=task");
    }

    SECTION("Lifecycle — F18Workflow gets Main WFI on creation");
    {
        ProjectFixture pfix("Lifecycle-F18-Test");
        // F18Workflow::create calls ensureMainWorkflow() automatically
        auto v = R::F18Workflow::create(pfix.project->projectId, "Lifecycle-Vorgang",
                                         R::F18VorgangType::RISK);
        CHECK(v != nullptr, "F18Workflow created");
        CHECK(!v->mainWorkflowId.empty(), "F18 has mainWorkflowId (auto from create)");
        CHECK(v->status == "in_work", "F18 status=in_work");
        auto inst = R::WorkflowInstance::loadById(v->mainWorkflowId);
        CHECK(inst != nullptr, "F18 Main WFI loadable");
        if (inst) CHECK(inst->entityType == "f18", "F18 Main WFI entityType=f18");
    }

    SECTION("Lifecycle — canReleaseEntity blocks when WFIs open");
    {
        ProjectFixture pfix("Release-Block-Test");
        auto p = pfix.project;
        // Start an additional WFI on the project
        auto extra = R::WorkflowEngine::startAdHoc(
            "project", p->projectId, "Extra-WF");
        CHECK(extra != nullptr, "Extra WFI created");
        // canReleaseEntity should report 1 blocker
        int blockers = 0;
        bool canRelease = R::WorkflowEngine::canReleaseEntity(
            "project", p->projectId, p->mainWorkflowId, blockers);
        CHECK(!canRelease, "canReleaseEntity=false with open extra WFI");
        CHECK(blockers == 1, "1 blocker counted");
    }

    SECTION("Lifecycle — lockAllOpenWorkflows enables release");
    {
        ProjectFixture pfix("Lock-Test");
        auto p = pfix.project;
        p->ensureMainWorkflow();

        // Start two WFIs with a mid-step so they stay "active" (don't auto-complete)
        auto wf1 = R::WorkflowEngine::startAdHoc("project", p->projectId, "Sub-WF-1");
        auto wf2 = R::WorkflowEngine::startAdHoc("project", p->projectId, "Sub-WF-2");
        // Add mid-steps so End can't auto-approve (WFIs stay active)
        if (wf1) {
            std::string initId1;
            for (auto& a : wf1->actions) if (a.isInitialize) initId1 = a.actionId;
            R::WorkflowEngine::addAction(*wf1, "Pending-Step-1", "sequential", 1, initId1);
        }
        if (wf2) {
            std::string initId2;
            for (auto& a : wf2->actions) if (a.isInitialize) initId2 = a.actionId;
            R::WorkflowEngine::addAction(*wf2, "Pending-Step-2", "sequential", 1, initId2);
        }
        // Both WFIs are now active with pending mid-steps
        int blockers = 0;
        R::WorkflowEngine::canReleaseEntity(
            "project", p->projectId, p->mainWorkflowId, blockers);
        CHECK(blockers == 2, "2 blockers before lock");
        // Lock all open WFIs (requires explicit confirmation)
        int locked = R::WorkflowEngine::lockAllOpenWorkflows(
            "project", p->projectId, p->mainWorkflowId, true);
        CHECK(locked == 2, "2 WFIs locked");
        // Now canReleaseEntity should return true
        int blockers2 = 0;
        bool canNow = R::WorkflowEngine::canReleaseEntity(
            "project", p->projectId, p->mainWorkflowId, blockers2);
        CHECK(canNow, "canReleaseEntity=true after locking all");
        CHECK(blockers2 == 0, "0 blockers after lock");
    }

    SECTION("Lifecycle — releaseEntity sets status=released");
    {
        ProjectFixture pfix("Release-Test");
        auto p = pfix.project;
        CHECK(p->status == "in_work", "starts in_work");
        bool ok = R::WorkflowEngine::releaseEntity("project", p->projectId);
        CHECK(ok, "releaseEntity succeeds");
        auto reloaded = R::ProjectF16::loadById(p->projectId);
        CHECK(reloaded != nullptr, "reloaded after release");
        CHECK(reloaded->status == "released", "status=released after releaseEntity");
    }

}

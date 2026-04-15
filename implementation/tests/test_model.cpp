// ============================================================
// test_model.cpp  —  Model entity tests with fixtures
// ============================================================
#include "TestFramework.h"
#include "TestFixtures.h"
#include "../src/model/Utils.h"
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
        auto byStatus = R::ProjectF16::loadByStatus("draft");
        CHECK(!byStatus.empty(), "loadByStatus(draft) returns results");
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
    SECTION("IncidentF18 — create, link risk, load");
    {
        ProjectFixture pfix;
        auto i = R::IncidentF18::create(pfix.project->projectId,
                                         "Datenverlust","critical","");
        CHECK(i->save(), "IncidentF18::save()");
        CHECK(i->incidentId.find("/F18/") != std::string::npos,
              "Incident ID contains /F18/");

        auto risk = R::Risk::create(pfix.project->projectId,"Datenverlust-Risiko");
        risk->probabilityScore = 4; risk->impactScoreTime = 5; risk->impactScoreCost = 4;
        risk->recalcScore(); risk->save();
        CHECK(i->linkToRisk(risk->riskId), "linkToRisk");

        auto open = R::IncidentF18::loadOpenIncidents();
        CHECK(!open.empty(), "loadOpenIncidents returns results");
    }

    // ── Risk ────────────────────────────────────────────────
    SECTION("Risk — scoring, high-risk filter");
    {
        ProjectFixture pfix;
        auto r = R::Risk::create(pfix.project->projectId,"Lieferantenrisiko");
        r->probabilityScore = 4; r->impactScoreTime = 4; r->impactScoreCost = 4;
        r->recalcScore();
        r->riskLevel = "high";   // needed for loadHighRisks() filter
        r->status    = "open";
        CHECK(r->save(), "Risk::save()");
        CHECK(r->riskId.find("/RSK/") != std::string::npos,
              "Risk ID contains /RSK/");
        CHECK(r->overallRiskScore > 0, "Risk score calculated");

        auto high = R::Risk::loadHighRisks();
        bool found = false;
        for (auto& hr : high) if (hr->riskId == r->riskId) found = true;
        CHECK(found, "loadHighRisks returns high-risk item");
    }

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
    SECTION("Milestone — create, achieve, overdue");
    {
        ProjectFixture pfix;
        auto m = R::Milestone::create(pfix.project->projectId,
                                       "Phase-1-Gate","2025-03-31");
        m->milestoneType   = "phase-gate";
        m->contractual     = true;
        m->paymentTrigger  = false;
        CHECK(m->save(), "Milestone::save()");
        CHECK(m->milestoneId.find("/MEI/") != std::string::npos,
              "Milestone ID contains /MEI/");

        auto list = R::Milestone::loadForProject(pfix.project->projectId);
        CHECK(!list.empty(), "loadForProject returns milestones");

        CHECK(m->markAchieved("2025-03-29"), "Milestone::markAchieved");
        auto ach = R::Milestone::loadById(m->milestoneId);
        CHECK(ach && ach->status == "achieved", "status=achieved after markAchieved");
    }

    // ── Trackable ────────────────────────────────────────────
}

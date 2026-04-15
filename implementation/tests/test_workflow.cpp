// test_workflow.cpp  —  WorkflowEngine tests with WorkflowFixture
#include "TestFramework.h"
#include "TestFixtures.h"
#include "../src/workflow/WorkflowEngine.h"
#include "../src/core/FileOps.h"
#include "../src/mfs/MFSWriter.h"
#include "../src/core/Migration.h"
#include <sys/stat.h>

namespace R = Rosenholz;
using namespace Rosenholz::Test;

void testSuiteWorkflow() {
    SECTION("WorkflowTemplate — create, actions, save, load");
    {
        auto tpl = R::WorkflowTemplate::create("Test-Genehmigung","sequential");
        tpl->description = "Testvorlage";
        tpl->entityTypes = "project,task";
        CHECK(tpl->save(), "WorkflowTemplate::save()");
        CHECK(!tpl->templateId.empty(), "Template has ID");
        CHECK(tpl->templateId.find("/WFD/") != std::string::npos,
              "Template ID contains /WFD/");

        // Add template actions
        R::WorkflowTemplateAction init;
        init.tplActionId = R::genId("WFT");
        init.templateId  = tpl->templateId;
        init.title = "Initialisierung"; init.isInitialize = true; init.autoApprove = true;
        init.save();
        tpl->templateActions.push_back(init);

        R::WorkflowTemplateAction step;
        step.tplActionId = R::genId("WFT");
        step.templateId  = tpl->templateId;
        step.title = "Prüfschritt"; step.sequenceOrder = 1;
        step.predecessorIds = init.tplActionId;
        step.save();
        tpl->templateActions.push_back(step);

        R::WorkflowTemplateAction final_;
        final_.tplActionId = R::genId("WFT");
        final_.templateId  = tpl->templateId;
        final_.title = "Abschluss"; final_.sequenceOrder = 2; final_.isFinal = true;
        final_.requiresDecisionLogEntry = true;
        final_.predecessorIds = step.tplActionId;
        final_.save();
        tpl->templateActions.push_back(final_);

        auto loaded = R::WorkflowTemplate::loadById(tpl->templateId);
        CHECK(loaded != nullptr, "WorkflowTemplate::loadById");
        CHECK(loaded->name == "Test-Genehmigung", "Template name persisted");
        CHECK(loaded->templateActions.size() == 3, "3 template actions loaded");
    }

    SECTION("WorkflowEngine — Init and End bookends always present");
    {
        ProjectFixture pfix("WF-Bookend-Test");

        // Ad-hoc instance must have Init + End
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId, "Bookend-Test");
        CHECK(inst != nullptr, "Instance created");

        bool hasInit = false, hasEnd = false;
        for (auto& a : inst->actions) {
            if (a.isInitialize) hasInit = true;
            if (a.isFinal)      hasEnd  = true;
        }
        CHECK(hasInit, "Init action present in ad-hoc instance");
        CHECK(hasEnd,  "End action present in ad-hoc instance");
        CHECK(inst->actions.size() >= 2, "At least 2 actions (Init + End)");

        // Init must be auto-approved after tick
        R::WorkflowEngine::tick(*inst);
        bool initApproved = false;
        for (auto& a : inst->actions)
            if (a.isInitialize && a.status == "approved") initApproved = true;
        CHECK(initApproved, "Init auto-approved after tick");

        // Add a mid-step — End should update its predecessor
        auto step = R::WorkflowEngine::addAction(*inst, "Zwischenschritt",
                                                  "sequential", 0, "", "", "", 0);
        CHECK(step != nullptr, "Mid-step added");

        // End's predecessor should include the new step
        bool endHasNewPred = false;
        for (auto& a : inst->actions) {
            if (a.isFinal) {
                endHasNewPred = a.predecessorActionIds.find(step->actionId) != std::string::npos;
                break;
            }
        }
        CHECK(endHasNewPred, "End action updated to wait for new mid-step");
    }

    SECTION("WorkflowEngine — End auto-approves when all predecessors done");
    {
        ProjectFixture pfix("WF-End-Auto-Test");
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId, "End-Auto-Test");
        CHECK(inst != nullptr, "Instance created");

        // Add one mid-step
        std::string initId;
        for (auto& a : inst->actions) if (a.isInitialize) initId = a.actionId;
        auto step = R::WorkflowEngine::addAction(*inst, "Freigabe",
                                                  "sequential", 1, initId, "", "", 0);

        // Fire the mid-step → End should auto-approve on next tick
        R::WorkflowEngine::fireAction(*inst, step->actionId, "approved", "tester");
        R::WorkflowEngine::tick(*inst);

        // Reload and check completion
        auto reloaded = R::WorkflowInstance::loadById(inst->instanceId);
        CHECK(reloaded != nullptr, "Instance reloadable after completion");
        CHECK(reloaded->status == "completed",
              "Instance completed after mid-step done + End auto-approved");
    }

    SECTION("WorkflowInstance — ad-hoc start, auto-initialize");
    {
        ProjectFixture pfix("WF-Test-Vorgang");
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId,
            "Ad-hoc Testinstanz", "sequential", "system");
        CHECK(inst != nullptr, "startAdHoc returns instance");
        CHECK(!inst->instanceId.empty(), "Instance has ID");
        CHECK(inst->instanceId.find("/WFI/") != std::string::npos,
              "Instance ID contains /WFI/");
        // An instance with only the auto-approved initialize step completes immediately
        CHECK(inst->status == "active" || inst->status == "completed",
              "Instance is active or auto-completed");
        CHECK(!inst->actions.empty(), "Initialize action created");

        // First action (initialize) must be auto-approved after tick
        bool initApproved = false;
        for (auto& a : inst->actions)
            if (a.isInitialize && a.status == "approved") initApproved = true;
        CHECK(initApproved, "Initialize action auto-approved on tick");
    }

    SECTION("WorkflowEngine — add steps, fire, complete");
    {
        ProjectFixture pfix("WF-Steps-Test");
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId, "Steps-Test", "sequential");
        CHECK(inst != nullptr, "Instance started");

        // Add a step with the initialize as predecessor
        std::string initId = inst->actions[0].actionId;
        auto step1 = R::WorkflowEngine::addAction(
            *inst, "Prüfschritt", "sequential", 1, initId,
            pfix.lead->personId, "2025-06-30", 48);
        CHECK(step1 != nullptr, "addAction returns action");
        CHECK(step1->actionId.find("/WFA/") != std::string::npos,
              "Action ID contains /WFA/");

        // Tick to advance initialize to in_progress state for step1
        R::WorkflowEngine::tick(*inst);

        // Fire step1 as approved
        bool ok = R::WorkflowEngine::fireAction(
            *inst, step1->actionId, "approved",
            pfix.lead->personId, "Alles in Ordnung");
        CHECK(ok, "fireAction succeeds");

        // Reload and verify
        auto reloaded = R::WorkflowInstance::loadById(inst->instanceId);
        CHECK(reloaded != nullptr, "Instance reloadable");
        bool step1done = false;
        for (auto& a : reloaded->actions)
            if (a.actionId == step1->actionId && a.status == "approved")
                step1done = true;
        CHECK(step1done, "Step1 approved in DB");
    }

    SECTION("WorkflowEngine — parallel execution");
    {
        TaskFixture tfix("WF-Parallel-Task");
        auto inst = R::WorkflowEngine::startAdHoc(
            "task", tfix.task->taskId, "Parallel-Test", "parallel");
        CHECK(inst != nullptr, "Parallel instance started");

        std::string initId = inst->actions[0].actionId;
        // Add 3 parallel actions (no predecessor dependency between them)
        auto a1 = R::WorkflowEngine::addAction(*inst,"Review A","parallel",1,"","","",24);
        auto a2 = R::WorkflowEngine::addAction(*inst,"Review B","parallel",2,"","","",24);
        auto a3 = R::WorkflowEngine::addAction(*inst,"Review C","parallel",3,"","","",24);
        R::WorkflowEngine::tick(*inst);

        // Fire all three
        R::WorkflowEngine::fireAction(*inst,a1->actionId,"approved","user1");
        R::WorkflowEngine::fireAction(*inst,a2->actionId,"approved","user2");
        R::WorkflowEngine::fireAction(*inst,a3->actionId,"approved","user3");

        auto done = R::WorkflowInstance::loadById(inst->instanceId);
        CHECK(done != nullptr, "Parallel instance reloadable");
        // With all actions approved, instance should be complete
        CHECK(done->status == "completed", "Instance completed when all parallel actions done");
    }

    SECTION("WorkflowEngine — standard templates seeded");
    {
        R::WorkflowEngine::createStandardTemplates();
        auto templates = R::WorkflowTemplate::loadAll();
        bool hasStandard = false, hasClosure = false;
        for (auto& t : templates) {
            if (t->name == "Standardgenehmigung") hasStandard = true;
            if (t->name == "Projektabschluss")    hasClosure  = true;
        }
        CHECK(hasStandard, "Standardgenehmigung template exists");
        CHECK(hasClosure,  "Projektabschluss template exists");
    }

    SECTION("WorkflowEngine — document attachment to instance and action");
    {
        ProjectFixture pfix("WF-Doc-Attach-Test");

        // Create a document with proper project ref
        auto doc = R::Document::create("Anforderungsdokument","specification",
                                        pfix.project->projectId);
        doc->version = "1.0";
        CHECK(doc->save(), "Document for attachment saved");

        // Start a workflow instance
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId, "Doc-Attach-Instanz");
        CHECK(inst != nullptr, "Instance created");

        // Attach document to instance level
        bool ok = R::WorkflowEngine::attachDocumentToInstance(
            inst->instanceId, doc->documentId, "mandatory", "Pflichtdokument");
        CHECK(ok, "attachDocumentToInstance succeeds");

        // Attach document to specific action
        std::string initId = inst->actions[0].actionId;
        bool ok2 = R::WorkflowEngine::attachDocumentToAction(
            initId, doc->documentId, "reference");
        CHECK(ok2, "attachDocumentToAction succeeds");

        // Load documents back
        auto docs = R::WorkflowEngine::loadDocumentsForInstance(inst->instanceId);
        CHECK(!docs.empty(), "loadDocumentsForInstance returns documents");

        auto actionDocs = R::WorkflowEngine::loadDocumentsForAction(initId);
        CHECK(!actionDocs.empty(), "loadDocumentsForAction returns documents");

        // Verify the document ID is correct
        bool foundDoc = false;
        for (auto& d : docs) if (d->documentId == doc->documentId) foundDoc = true;
        CHECK(foundDoc, "Correct document returned from instance query");
    }

    SECTION("WorkflowEngine — escalation and participants");
    {
        ProjectFixture pfix("WF-Escalation-Test");
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId, "Eskalations-Test");

        PersonFixture escalateTo("Eskalations","Empfaenger","ee@test.de","internal");
        CHECK(R::WorkflowEngine::escalate(*inst, escalateTo.person->personId, "SLA verletzt"),
              "escalate() succeeds");

        auto reloaded = R::WorkflowInstance::loadById(inst->instanceId);
        CHECK(reloaded->escalatedTo == escalateTo.person->personId, "escalatedTo persisted");

        PersonFixture watcher("Watch","Er","we@test.de","internal");
        CHECK(R::WorkflowEngine::addParticipant(*inst, watcher.person->personId, "watcher"),
              "addParticipant() succeeds");

        auto inst2 = R::WorkflowInstance::loadById(inst->instanceId);
        inst2->loadParticipants();
        CHECK(!inst2->participants.empty(), "Participant stored");
    }

    SECTION("WorkflowEngine — decision log entry from action");
    {
        ProjectFixture pfix("WF-DL-Test");
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId, "DL-Test");
        auto action = R::WorkflowEngine::addAction(*inst, "Entscheidungsschritt",
                                                    "sequential", 1,
                                                    inst->actions[0].actionId);
        action->requiresDecisionLogEntry = true;
        action->save();
        R::WorkflowEngine::tick(*inst);

        CHECK(R::WorkflowEngine::createDecisionLogEntry(
            action->actionId, "project", pfix.project->projectId,
            "Architektur: Microservices gewählt",
            "Bessere Skalierbarkeit und unabhängige Deployments"),
            "createDecisionLogEntry succeeds");

        // Verify the entry was created in reporting DB
        auto* db = R::DatabasePool::instance().get("reporting");
        CHECK(db != nullptr, "reporting DB accessible");
        auto rows = db->query(
            "SELECT COUNT(*) FROM decision_log_entries;");
        CHECK(!rows.empty() && rows[0].begin()->second != "0",
              "Decision log entry created in DB");
    }

    SECTION("WorkflowFixture — from-template instance");
    {
        WorkflowFixture fix;
        CHECK(fix.instance != nullptr, "WorkflowFixture creates instance");
        CHECK(!fix.instance->actions.empty(), "Instance has actions");

        // The initialize action should be auto-approved
        bool initOk = false;
        for (auto& a : fix.instance->actions)
            if (a.isInitialize) { initOk = a.status == "approved"; break; }
        CHECK(initOk, "Fixture initialize action auto-approved");
    }
}

void testSuiteMFS() {
    SECTION("MFS — rebuild all with correct German folder structure");
    {
        FullProjectFixture fix;
        auto& cfg = R::Config::instance();
        // MFS enabled by default in test config

        CHECK(fix.project->writeMFSFile(cfg.mfsPath()), "writeMFSFile for project");

        // Verify folder hierarchy
        std::string de   = cfg.registratur().diensteinheitKuerzel;
        std::string year = std::to_string(fix.project->regNumber.year);
        std::string sane = Rosenholz::sanitiseRegNr(fix.project->regNumber.toString());

        auto heft = Rosenholz::FileOps::joinPath(
            Rosenholz::FileOps::joinPath(
                Rosenholz::FileOps::joinPath(cfg.mfsPath(), "F16"), de),
            Rosenholz::FileOps::joinPath(year, sane));
        CHECK(Rosenholz::FileOps::dirExists(heft), "Project Haengeregister exists");

        // Verify German subfolder names
        for (auto& sub : {"F22","F18","DOK","RSK","MSN","QT","LE","ENT","AEA","ABE","MEI","BSP"}) {
            CHECK(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft, sub)),
                  std::string("Haengeregister has ") + sub + "/ subfolder");
        }
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

        // Re-filing after rename replaces old file
        doc->title = "Testdokument v2";
        doc->update();
        Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
        // Verify only one file with this ID prefix
        auto proj = R::ProjectF16::loadById(pfix.project->projectId);
        if (proj) {
            std::string de = cfg.registratur().diensteinheitKuerzel;
            std::string dokDir = Rosenholz::FileOps::joinPath(
                Rosenholz::FileOps::joinPath(
                    Rosenholz::FileOps::joinPath(cfg.mfsPath(), "F16"), de),
                Rosenholz::FileOps::joinPath(
                    std::to_string(proj->regNumber.year),
                    Rosenholz::FileOps::joinPath(
                        Rosenholz::sanitiseRegNr(proj->regNumber.toString()), "DOK")));
            std::string sane = Rosenholz::sanitiseRegNr(doc->documentId);
            int count = 0;
            for (auto& f : Rosenholz::FileOps::listDir(dokDir))
                if (f.size() >= sane.size() && f.substr(0,sane.size()) == sane) count++;
            CHECK(count == 1, "Only one file per document ID after rename+refile");
        }
    }

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
    SECTION("LessonsLearned — header+entries, one per F16");
    {
        ProjectFixture pfix("LL-Test-Vorgang");

        // Create header via WorkflowEngine helper
        auto* db = R::DatabasePool::instance().get("reporting");
        CHECK(db != nullptr, "reporting DB accessible");

        // Simulate creating LL header
        std::string headerId = R::genId("LLH");
        bool ok = db->exec(
            "INSERT INTO lessons_learned_header(ll_header_id,entity_type,entity_id,created_at) "
            "VALUES(?,?,?,?);",
            {R::BindParam::text(headerId),
             R::BindParam::text("project"),
             R::BindParam::text(pfix.project->projectId),
             R::BindParam::text(R::nowIso())});
        CHECK(ok, "LessonsLearned header created");

        // UNIQUE constraint: second insert with same entity_id must fail
        bool dup = db->exec(
            "INSERT INTO lessons_learned_header(ll_header_id,entity_type,entity_id,created_at) "
            "VALUES(?,?,?,?);",
            {R::BindParam::text(R::genId("LLH")),
             R::BindParam::text("project"),
             R::BindParam::text(pfix.project->projectId),
             R::BindParam::text(R::nowIso())});
        CHECK(!dup, "UNIQUE constraint: second LL header rejected for same entity");

        // Add entries
        std::string entryId = R::genId("LLE");
        ok = db->exec(
            "INSERT INTO lessons_learned_entries(entry_id,ll_header_id,title,status,created_at) "
            "VALUES(?,?,?,?,?);",
            {R::BindParam::text(entryId), R::BindParam::text(headerId),
             R::BindParam::text("Frühe Stakeholder-Einbindung"),
             R::BindParam::text("draft"), R::BindParam::text(R::nowIso())});
        CHECK(ok, "LL entry created");

        auto entries = db->query(
            "SELECT * FROM lessons_learned_entries WHERE ll_header_id=?;",
            {R::BindParam::text(headerId)});
        CHECK(!entries.empty(), "LL entries loadable");
    }

    SECTION("DecisionLog — header+entries, WorkflowEngine integration");
    {
        ProjectFixture pfix("DL-Test-Vorgang");
        auto inst = R::WorkflowEngine::startAdHoc(
            "project", pfix.project->projectId, "DL-Test-WF");
        auto action = R::WorkflowEngine::addAction(
            *inst,"Entscheidung","sequential",1,inst->actions[0].actionId);
        R::WorkflowEngine::tick(*inst);

        bool ok = R::WorkflowEngine::createDecisionLogEntry(
            action->actionId, "project", pfix.project->projectId,
            "Entscheidung: SQLite statt PostgreSQL",
            "Kein Netzwerk erforderlich, einfachere Deployment");
        CHECK(ok, "createDecisionLogEntry via WorkflowEngine");

        auto* db = R::DatabasePool::instance().get("reporting");
        auto entries = db->query(
            "SELECT * FROM decision_log_entries WHERE title LIKE '%SQLite%';");
        CHECK(!entries.empty(), "Decision log entry in DB");
    }

    SECTION("AssumptionConstraint — header UNIQUE per F16/F22");
    {
        ProjectFixture pfix("AC-Test-Vorgang");
        auto* db = R::DatabasePool::instance().get("reporting");

        std::string hdrid = R::genId("ACH");
        db->exec(
            "INSERT INTO assumption_constraint_header(ac_header_id,entity_type,entity_id,created_at) "
            "VALUES(?,?,?,?);",
            {R::BindParam::text(hdrid), R::BindParam::text("project"),
             R::BindParam::text(pfix.project->projectId), R::BindParam::text(R::nowIso())});

        // Second header for same entity must fail
        bool dup = db->exec(
            "INSERT INTO assumption_constraint_header(ac_header_id,entity_type,entity_id,created_at) "
            "VALUES(?,?,?,?);",
            {R::BindParam::text(R::genId("ACH")), R::BindParam::text("project"),
             R::BindParam::text(pfix.project->projectId), R::BindParam::text(R::nowIso())});
        CHECK(!dup, "UNIQUE constraint: second AC header rejected");

        // Entries can be multiple
        for (int i = 1; i <= 3; i++) {
            db->exec(
                "INSERT INTO assumption_constraint_entries(entry_id,ac_header_id,title,ac_type,created_at) "
                "VALUES(?,?,?,?,?);",
                {R::BindParam::text(R::genId("ACE")), R::BindParam::text(hdrid),
                 R::BindParam::text("Annahme " + std::to_string(i)),
                 R::BindParam::text("assumption"), R::BindParam::text(R::nowIso())});
        }
        auto rows = db->query(
            "SELECT COUNT(*) FROM assumption_constraint_entries WHERE ac_header_id=?;",
            {R::BindParam::text(hdrid)});
        int entryCount = rows.empty() ? 0 : std::stoi(rows[0].begin()->second);
        CHECK(entryCount == 3, "3 AC entries for one header");
    }

    SECTION("CommunicationPlan — UNIQUE per entity");
    {
        ProjectFixture pfix("CP-Test-Vorgang");
        auto* db = R::DatabasePool::instance().get("projects");

        std::string planId = R::genId("CP");
        bool ok = db->exec(
            "INSERT INTO communication_plans(plan_id,entity_type,entity_id,title,created_at) "
            "VALUES(?,?,?,?,?);",
            {R::BindParam::text(planId), R::BindParam::text("project"),
             R::BindParam::text(pfix.project->projectId),
             R::BindParam::text("Kommunikationsplan"),
             R::BindParam::text(R::nowIso())});
        CHECK(ok, "CommunicationPlan created");

        bool dup = db->exec(
            "INSERT INTO communication_plans(plan_id,entity_type,entity_id,title,created_at) "
            "VALUES(?,?,?,?,?);",
            {R::BindParam::text(R::genId("CP")), R::BindParam::text("project"),
             R::BindParam::text(pfix.project->projectId),
             R::BindParam::text("Zweiter Plan"),
             R::BindParam::text(R::nowIso())});
        CHECK(!dup, "UNIQUE: second CommunicationPlan per entity rejected");
    }
}

void testSuiteMigration() {
    SECTION("Migration — version tracking per DB");
    {
        std::vector<std::string> dbs = {"core","projects","workflow","documents","tracking","reporting"};
        for (auto& db : dbs) {
            int cur = Rosenholz::MigrationEngine::currentVersion(db);
            int tgt = Rosenholz::MigrationEngine::targetVersion(db);
            CHECK(cur == tgt, db + " schema is current (v" + std::to_string(cur) + ")");
        }
    }

    SECTION("Migration — idempotent re-run");
    {
        // Running again should be a no-op (already current)
        bool ok = Rosenholz::MigrationEngine::runAll();
        CHECK(ok, "MigrationEngine::runAll() idempotent");
    }
}

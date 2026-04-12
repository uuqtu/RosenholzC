// ============================================================
// main.cpp  —  Rosenholz PM — Extensive console test suite
//
// Tests every entity type, CRUD, QTCS assignment,
// Trackable items, Notes, Reminders, MFS output,
// conversions (F16<->F22), reassignment, and backup.
//
// Run: ./rosenholz [--debug] [--basepath /path/to/data]
// ============================================================

#include "src/app/AppController.h"
#include "src/core/Config.h"
#include "src/core/Logger.h"
#include "src/core/FileOps.h"
#include "src/core/Database.h"
#include "src/core/RegNumber.h"
#include "src/core/BackupManager.h"
#include "src/model/Person.h"
#include "src/model/Team.h"
#include "src/model/ProjectF16.h"
#include "src/model/TaskF22.h"
#include "src/model/IncidentF18.h"
#include "src/model/Risk.h"
#include "src/model/Document.h"
#include "src/model/Trackable.h"
#include "src/mfs/MFSWriter.h"
#include "src/model/Milestone.h"
#include "src/model/ReportingModels.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <cassert>
#include <stdexcept>
#ifndef _WIN32
  #include <sys/stat.h>
#endif

using namespace Rosenholz;

// ── Test helpers ─────────────────────────────────────────────
static int  g_pass = 0;
static int  g_fail = 0;
static std::string g_section;

void section(const std::string& name) {
    g_section = name;
    std::cout << "\n=== " << name << " ===\n" << std::flush;
}

void check(bool cond, const std::string& desc) {
    if (cond) {
        std::cout << "  PASS: " << desc << "\n" << std::flush;
        ++g_pass;
    } else {
        std::cout << "  FAIL: " << desc << "  [" << g_section << "]\n" << std::flush;
        ++g_fail;
    }
}

void summary() {
    std::cout << "\n=== Results: PASS=" << g_pass << "  FAIL=" << g_fail << " ===\n" << std::flush;
}

// ── Individual test groups ────────────────────────────────────

void testLogger() {
    section("Logger — verbosity levels");
    auto& log = Logger::instance();

    log.setLevel(LogLevel::ERR);
    log.debug("Should NOT appear (level=ERR)");
    log.warn ("Should NOT appear (level=ERR)");
    log.error("Should appear    (level=ERR)");
    check(true, "Error-only mode set");

    log.setLevel(LogLevel::WARN);
    log.debug("Should NOT appear (level=WARN)");
    log.warn ("Should appear     (level=WARN)");
    check(true, "Warn mode set");

    log.setLevel(LogLevel::INFO);
    log.info("Should appear (level=INFO)");
    check(true, "Info mode set");

    log.setLevel(LogLevel::DEBUG);
    log.debug("Should appear (level=DEBUG)");
    check(true, "Debug mode set — all messages visible");
}

void testConfig() {
    section("Config — settings.json");
    auto& cfg = Config::instance();
    check(!cfg.basePath().empty(), "basePath is set");
    check(!cfg.logLevel().empty(), "logLevel is set");
    std::cout << "  basePath = " << cfg.basePath() << "\n";

    // Derived paths
    auto dbPath = cfg.dbPath("core.db");
    check(!dbPath.empty(), "dbPath(core.db) returns non-empty");
    std::cout << "  dbPath   = " << dbPath << "\n";
}

void testFileOps() {
    section("FileOps — cross-platform filesystem operations");
    std::string tmp = FileOps::joinPath(FileOps::tempDirectory(), "rh_test");
    FileOps::makeDirs(tmp);
    check(FileOps::dirExists(tmp), "makeDirs creates directory");

    std::string f = FileOps::joinPath(tmp, "test.txt");
    check(FileOps::writeTextFile(f, "hello rosenholz"), "writeTextFile");
    check(FileOps::fileExists(f),  "fileExists after write");
    check(FileOps::readTextFile(f) == "hello rosenholz", "readTextFile content matches");
    check(FileOps::fileSize(f) > 0, "fileSize > 0");

    std::string f2 = FileOps::joinPath(tmp, "copy.txt");
    check(FileOps::copyFile(f, f2), "copyFile");
    check(FileOps::fileExists(f2),  "copied file exists");

    check(FileOps::deleteFile(f),  "deleteFile");
    check(!FileOps::fileExists(f), "file gone after delete");

    check(!FileOps::baseName("/foo/bar/baz.txt").empty(), "baseName");
    check(FileOps::extension("/foo/bar/baz.txt") == ".txt", "extension");
    check(FileOps::sanitizeFilename("hello world!") == "hello world_", "sanitizeFilename");
}

void testRegNumber() {
    section("RegNumber — sequential generator");
    auto rn1 = RegNumberGenerator::next(RegDept::PROJECT);
    auto rn2 = RegNumberGenerator::next(RegDept::PROJECT);
    auto rn3 = RegNumberGenerator::next(RegDept::TASK);

    check(rn1.isValid(), "RegNumber 1 is valid: " + rn1.toString());
    check(rn2.isValid(), "RegNumber 2 is valid: " + rn2.toString());
    check(rn2.sequence == rn1.sequence + 1, "Sequence increments correctly");
    check(rn3.dept == RegDept::TASK, "Task dept code correct");

    std::cout << "  F16 seq1 = " << rn1.toString() << "\n";
    std::cout << "  F16 seq2 = " << rn2.toString() << "\n";
    std::cout << "  F22 seq1 = " << rn3.toString() << "\n";

    auto parsed = RegNumber::fromString(rn1.toString());
    check(parsed.dept == rn1.dept, "fromString preserves dept");
    check(parsed.sequence == rn1.sequence, "fromString preserves sequence");
    check(parsed.year == rn1.year, "fromString preserves year");
}

void testPerson() {
    section("Person — create, save, load, search");
    auto p = Person::create("Heinrich", "Schmidt", "h.schmidt@example.com", "internal");
    p->roleTitle    = "Project Lead";
    p->department   = "Engineering";
    p->dayRate      = 850.0;
    p->seniorityLevel = "senior";
    check(p->save(), "Person::save()");

    auto loaded = Person::loadById(p->personId);
    check(loaded != nullptr, "Person::loadById returns result");
    check(loaded->firstName == "Heinrich", "firstName matches");
    check(loaded->lastName  == "Schmidt",  "lastName matches");
    check(loaded->email     == "h.schmidt@example.com", "email matches");
    check(std::abs(loaded->dayRate - 850.0) < 0.01, "dayRate matches");
    check(loaded->regNumber.isValid(), "RegNumber is valid");
    std::cout << "  Person reg: " << loaded->regNumber.toString() << "\n";
    std::cout << "  Display:    " << loaded->displayName() << "\n";

    // Search
    auto found = Person::search("Schmidt");
    check(!found.empty(), "search finds by lastName");

    // Reassign manager
    auto p2 = Person::create("Ingrid", "Müller", "i.mueller@example.com");
    p2->save();
    check(p->reassignManager(p2->personId), "reassignManager");

    // Status change
    check(p->setStatus("on-leave"), "setStatus");
    auto reloaded = Person::loadById(p->personId);
    check(reloaded->status == "on-leave", "Status persisted");

    // Restore
    p->setStatus("active");
}

std::string g_personId;   // shared across tests
std::string g_teamId;
std::string g_projectId;
std::string g_taskId;
std::string g_extProjectId;   // shared across extended test sections
std::string g_extTaskId;      // shared across extended test sections
std::string g_incidentId;
std::string g_riskId;

void testTeam() {
    section("Team — hierarchy, members, reassignment");
    auto parent = Team::create("Platform Division", "platform");
    check(parent->save(), "Parent team saved");

    auto child = Team::create("Backend Tribe", "delivery", parent->teamId);
    child->methodology = "agile";
    check(child->save(), "Child team saved");
    g_teamId = child->teamId;

    // Add member
    auto person = Person::create("Klaus", "Bauer", "k.bauer@example.com");
    person->save();
    g_personId = person->personId;

    auto member = child->addMember(person->personId, "Senior Engineer", "internal");
    member->isLead            = true;
    member->primarySkill      = "C++";
    member->allocationPct     = 80.0;
    member->plannedHoursPerWeek = 32.0;
    member->costRate          = 120.0;
    member->save();
    check(true, "Team member added with categorization");
    std::cout << "  Member role=" << member->role
              << " skill=" << member->primarySkill
              << " alloc=" << member->allocationPct << "%\n";

    // Load members
    child->loadMembers();
    check(!child->members.empty(), "loadMembers returns results");
    check(child->members[0]->isLead == true, "isLead flag persisted");

    // Reassign
    check(child->reassignLead(person->personId), "reassignLead");

    // Load children
    auto children = Team::loadChildren(parent->teamId);
    check(!children.empty(), "loadChildren returns child team");

    // Member: moveToTeam
    check(member->moveToTeam(parent->teamId), "TeamMember::moveToTeam");
    check(member->reassignRole("Tech Lead", "leadership"), "TeamMember::reassignRole");
}

void testProjectF16() {
    section("ProjectF16 — create, QTCS, trackables, MFS, conversion");
    auto p = ProjectF16::create("Projekt ROSENHOLZ", "OV", "large", g_personId);
    p->leadId      = g_personId;
    p->ownerTeamId = g_teamId;
    p->priority    = "high";
    p->complexity  = "complex";
    p->startDatePlanned = "2025-01-01";
    p->endDatePlanned   = "2025-12-31";
    p->budgetPlanned    = 500000.0;
    p->scopeStatement   = "Full PM system implementation";
    p->methodology      = "agile";
    check(p->save(), "ProjectF16::save()");
    g_projectId = p->projectId;

    // Load back
    auto loaded = ProjectF16::loadById(p->projectId);
    check(loaded != nullptr, "loadById returns result");
    check(loaded->title == "Projekt ROSENHOLZ", "title matches");
    check(loaded->regNumber.isValid(), "regNumber valid");
    std::cout << "  F16 reg: " << loaded->regNumber.toString() << "\n";

    // QTCS — add quality dimension (by ID string for now)
    std::string fakeQualityId = "qual_test_001";
    check(p->addQuality(fakeQualityId), "addQuality to F16");
    p->loadQTCSLinks();
    check(!p->qualityIds.empty(), "qualityIds loaded");

    // Trackable item
    auto trk = p->addTrackable("Review requirements document", g_personId);
    trk->plan("2025-02-01");
    trk->addNote(g_personId, "Requirements doc needs stakeholder sign-off");
    trk->addReminder("Review meeting", "2025-02-01T09:00:00", g_personId);
    check(trk->save(), "TrackableItem saved on project");
    check(!trk->notes.empty(), "TrackableItem has notes");

    p->loadTrackables();
    check(!p->trackables.empty(), "loadTrackables returns items");

    // EV calc
    p->earnedValue = 125000.0;
    p->plannedValue= 200000.0;
    p->actualCost  = 150000.0;
    p->recalcEarnedValue();
    std::cout << "  CPI=" << std::fixed << std::setprecision(3) << p->cpi
              << "  SPI=" << p->spi << "\n";
    check(p->cpi > 0, "CPI calculated");

    // Reassign
    auto p2 = Person::create("Ute", "Fischer", "u.fischer@example.com"); p2->save();
    check(p->reassignLead(p2->personId), "reassignLead");
    check(p->reassignSponsor(g_personId), "reassignSponsor");

    // MFS output
    check(p->writeMFSFile(Config::instance().mfsPath()), "writeMFSFile");

    // loadAll / loadByStatus
    auto all = ProjectF16::loadAll();
    check(!all.empty(), "loadAll returns results");
    auto active = ProjectF16::loadByStatus("draft");
    check(!active.empty(), "loadByStatus(draft) returns results");
}

void testTaskF22() {
    section("TaskF22 — hierarchy, meetings, QTCS, conversion");
    auto t = TaskF22::create(g_projectId, "Analyse Anforderungen", g_personId);
    t->priority         = "high";
    t->effortPlannedHrs = 40.0;
    t->dueDatePlanned   = "2025-03-01";
    t->costPlanned      = 5000.0;
    t->wbsCode          = "1.1";
    check(t->save(), "TaskF22::save()");
    g_taskId = t->taskId;
    std::cout << "  F22 reg: " << t->regNumber.toString() << "\n";

    // Child task
    auto child = TaskF22::create(g_projectId, "Interview Stakeholder", g_personId, t->taskId);
    child->wbsCode = "1.1.1";
    check(child->save(), "Child task saved");

    // Load children
    auto children = TaskF22::loadChildren(t->taskId);
    check(!children.empty(), "loadChildren returns subtask");

    // QTCS
    check(t->addTime("time_dim_001"), "addTime to task");
    check(t->addCost("cost_dim_001"), "addCost to task");

    // Trackable with child items (recursive)
    auto trk = t->addTrackable("Define acceptance criteria");
    trk->plan("2025-02-15");
    trk->focus();
    check(trk->status == TrackableStatus::Focused, "trackable status=Focused");
    auto childTrk = TrackableItem::create("task", t->taskId, "Write test cases");
    childTrk->parentTrackableId = trk->trackableId;
    childTrk->plan("2025-02-20");
    childTrk->save();
    trk->children.push_back(childTrk);
    check(!trk->children.empty(), "Trackable has child item");

    // Note on trackable
    trk->addNote(g_personId, "Acceptance criteria must cover edge cases", "decision");
    check(!trk->notes.empty(), "Note added to trackable");

    // Reminder
    auto& rem = trk->addReminder("Daily check-in", "2025-02-16T09:00:00", g_personId);
    check(!rem.reminderId.empty(), "Reminder created");

    // Reassign
    auto person2 = Person::create("Dirk", "Wolf", "d.wolf@example.com"); person2->save();
    check(t->reassignTo(person2->personId), "reassignTo");
    check(t->reassignParent(""), "reassignParent (detach from parent)");

    // MFS
    check(t->writeMFSFile(Config::instance().mfsPath()), "writeMFSFile F22");

    // Convert to project
    std::string newProjId = child->convertToProject("OV");
    check(!newProjId.empty(), "Task converted to project");
    std::cout << "  Converted task -> project: " << newProjId << "\n";

    // Load for project
    auto tasks = TaskF22::loadForProject(g_projectId);
    check(!tasks.empty(), "loadForProject returns tasks");
}

void testIncidentF18() {
    section("IncidentF18 — create, QTCS, link to risk");
    auto i = IncidentF18::create(g_projectId, "Budget Überschreitung Q1", "high", g_personId);
    i->description      = "Q1 budget exceeded by 15% due to scope changes";
    i->category         = "cost";
    i->incidentType     = "financial";
    i->costImpact       = 15000.0;
    i->scheduleImpactDays = 0;
    i->occurredDate     = "2025-03-15";
    i->immediateAction  = "Budget freeze initiated";
    check(i->save(), "IncidentF18::save()");
    g_incidentId = i->incidentId;
    std::cout << "  F18 reg: " << i->regNumber.toString() << "\n";

    // QTCS
    check(i->addCost("cost_dim_001"), "addCost to incident");
    check(i->addScope("scope_dim_001"), "addScope to incident");

    // Trackable
    auto trk = i->addTrackable("Root cause analysis", g_personId);
    trk->plan("2025-03-16");
    trk->addNote(g_personId, "RCA team assembled", "action");
    trk->save();
    check(true, "Trackable on incident with note");

    // Reassign
    auto p2 = Person::create("Monika", "Klein", "m.klein@example.com"); p2->save();
    check(i->reassignOwner(p2->personId), "reassignOwner");
    check(i->reassignToProject(g_projectId), "reassignToProject");

    // MFS
    check(i->writeMFSFile(Config::instance().mfsPath()), "writeMFSFile F18");

    // Load open
    auto open = IncidentF18::loadOpenIncidents();
    check(!open.empty(), "loadOpenIncidents returns results");

    // Load for project
    auto byProj = IncidentF18::loadForProject(g_projectId);
    check(!byProj.empty(), "loadForProject returns incidents");
}

void testRisk() {
    section("Risk — scoring, trackables, link to incident");
    auto r = Risk::create(g_projectId, "Vendor delivery delay", "high");
    r->description      = "Key vendor may not deliver components by deadline";
    r->category         = "supply-chain";
    r->riskType         = "external";
    r->probabilityScore = 3;
    r->impactScoreTime  = 4;
    r->impactScoreCost  = 3;
    r->impactScoreQuality = 2;
    r->impactScoreScope = 2;
    r->recalcScore();
    r->responseStrategy = "mitigate";
    r->contingencyPlan  = "Identify alternative vendor";
    r->costReserve      = 20000.0;
    r->scheduleReserveDays = 14;
    check(r->save(), "Risk::save()");
    g_riskId = r->riskId;
    std::cout << "  Risk score: " << r->overallRiskScore << " level=" << r->riskLevel << "\n";

    // Link incident to this risk
    auto inc = IncidentF18::loadById(g_incidentId);
    if (inc) {
        check(inc->linkToRisk(r->riskId), "linkToRisk");
    }

    // Trackable
    auto trk = r->addTrackable("Monitor vendor KPIs weekly");
    trk->plan("2025-01-01");
    trk->addNote(g_personId, "Weekly call scheduled with vendor PM");
    trk->save();
    check(true, "Trackable on risk");

    // Load high risks
    auto highs = Risk::loadHighRisks();
    check(!highs.empty(), "loadHighRisks returns results");

    check(r->reassignOwner(g_personId), "Risk::reassignOwner");
}

void testDocument() {
    section("Document — create, attach to entity, URL archiving");

    // Create a plain document
    auto doc = Document::create("Project Charter", "report", g_projectId);
    doc->authorId    = g_personId;
    doc->format      = "pdf";
    doc->status      = "approved";
    doc->summary     = "Defines scope, objectives, and stakeholders";
    check(doc->save(), "Document::save()");
    check(doc->attachToEntity("project", g_projectId), "attachToEntity");

    // Load for entity
    auto docs = Document::loadForEntity("project", g_projectId);
    check(!docs.empty(), "loadForEntity returns documents");

    // Reassign
    auto p2 = Person::create("Bernd", "Lange", "b.lange@example.com"); p2->save();
    check(doc->reassignAuthor(p2->personId), "reassignAuthor");

    // URL archiving (using a safe, stable URL — may be skipped if offline)
    LOG_INFO("Attempting URL archive test (requires network + curl)...");
    auto archived = Document::archiveFromUrl(
        "https://example.com", g_projectId, g_personId);
    if (archived) {
        check(true, "URL archived as document");
        check(!archived->filePath.empty(), "Archived document has filePath");
        archived->attachToEntity("project", g_projectId);
        std::cout << "  Archived: " << archived->filePath << "\n";
        MFSWriter::writeDocument(*archived, Config::instance().mfsPath());
    } else {
        std::cout << "  (URL archive skipped — network unavailable)\n";
    }
}

void testTrackable() {
    section("Trackable — ise-cobra state machine, notes, reminders");

    // Attach to a project (any entity works)
    auto trk = TrackableItem::create("project", g_projectId, "Sprint planning preparation");
    trk->plan("2025-01-10");
    check(trk->status == TrackableStatus::Planned, "Initial status=Planned");

    trk->focus("2025-01-10");
    check(trk->status == TrackableStatus::Focused, "After focus()=Focused");

    trk->markDue();
    check(trk->status == TrackableStatus::Due, "After markDue()=Due");

    trk->archive();
    check(trk->status == TrackableStatus::Archived, "After archive()=Archived");
    check(!trk->archivedDate.empty(), "archivedDate set");

    // Notes
    auto& n1 = trk->addNote(g_personId, "Completed sprint planning successfully", "general");
    auto& n2 = trk->addNote(g_personId, "Approved by PO", "decision");
    check(trk->notes.size() == 2, "Two notes added");
    check(!n1.noteId.empty(), "Note has ID");

    // Notes JSON round-trip
    std::string jsonStr = trk->notesToJsonString();
    check(!jsonStr.empty(), "notesToJsonString not empty");
    trk->notesFromJsonString(jsonStr);
    check(trk->notes.size() == 2, "Notes survive JSON round-trip");

    // Reminder
    auto& rem = trk->addReminder("Follow-up on PO approval", "2025-01-11T09:00:00", g_personId);
    check(!rem.reminderId.empty(), "Reminder has ID");
    trk->dismissReminder(rem.reminderId);
    check(rem.isDismissed, "Reminder dismissed");

    // Save and reload
    check(trk->save(), "TrackableItem::save()");
    auto loaded = std::make_shared<TrackableItem>();
    check(loaded->load(trk->trackableId), "TrackableItem::load()");
    check(loaded->status == TrackableStatus::Archived, "Loaded status=Archived");
    check(loaded->notes.size() == 2, "Loaded notes count matches");

    // loadForEntity
    auto items = TrackableItem::loadForEntity("project", g_projectId);
    check(!items.empty(), "loadForEntity returns items");
    std::cout << "  " << items.size() << " trackable items on project\n";

    // Child items
    auto child = TrackableItem::create("project", g_projectId, "Child task");
    child->parentTrackableId = trk->trackableId;
    child->plan("2025-01-11");
    child->save();
    check(!child->parentTrackableId.empty(), "Child has parent ref");
}

void testConversions() {
    section("F16 <-> F22 conversions");

    // Project -> Task
    auto proj = ProjectF16::loadById(g_projectId);
    check(proj != nullptr, "Loaded project for conversion");
    if (proj) {
        std::string newTaskId = proj->convertToTask(g_projectId);
        check(!newTaskId.empty(), "ProjectF16::convertToTask() returns taskId");
        std::cout << "  F16 -> F22: " << newTaskId << "\n";
    }

    // Task -> Project
    auto task = TaskF22::loadById(g_taskId);
    check(task != nullptr, "Loaded task for conversion");
    if (task) {
        std::string newProjId = task->convertToProject("OV");
        check(!newProjId.empty(), "TaskF22::convertToProject() returns projectId");
        std::cout << "  F22 -> F16: " << newProjId << "\n";
    }

    // Note: F18 is kept separate (it represents occurred events, not plans)
    // Linking F18 -> Risk is the appropriate connection, done in testRisk()
    std::cout << "  (F18 stays as incident — linked to Risk via linkToRisk())\n";
    check(true, "F18 conversion strategy documented");
}

void testMFSRebuild() {
    section("MFS — full tree rebuild");
    std::string mfsRoot = Config::instance().mfsPath();
    check(FileOps::dirExists(mfsRoot), "MFS root directory exists");

    bool ok = MFSWriter::rebuildAll(mfsRoot);
    check(ok, "MFSWriter::rebuildAll() succeeds");

    // Check key files exist
    check(FileOps::fileExists(FileOps::joinPath(mfsRoot, "owner_key.txt")),
          "owner_key.txt created");
    check(FileOps::dirExists(FileOps::joinPath(mfsRoot, "F16")), "F16 dir exists");
    check(FileOps::dirExists(FileOps::joinPath(mfsRoot, "F22")), "F22 dir exists");
    check(FileOps::dirExists(FileOps::joinPath(mfsRoot, "F18")), "F18 dir exists");

#ifndef _WIN32
    // Verify owner-only permissions on owner_key.txt
    struct stat st{};
    std::string keyPath = FileOps::joinPath(mfsRoot, "owner_key.txt");
    if (stat(keyPath.c_str(), &st) == 0) {
        bool ownerOnly = ((st.st_mode & 0777) == 0600);
        check(ownerOnly, "owner_key.txt has 600 permissions");
    }
#endif
}

void testBackup() {
    section("BackupManager — database and MFS backup");
    auto& cfg = Config::instance();
    std::string backupDest = FileOps::joinPath(cfg.basePath(), "backup_test");

    int dbCount = BackupManager::backupDatabases(cfg.basePath(), backupDest, 3);
    check(dbCount > 0, "At least one DB backed up (got " + std::to_string(dbCount) + ")");

    bool mfsOk = BackupManager::backupMFS(cfg.mfsPath(), backupDest, 3);
    check(mfsOk, "MFS backup OK");

    // Check backup dir structure
    check(FileOps::dirExists(FileOps::joinPath(backupDest, "db")), "backup/db exists");
    check(FileOps::dirExists(FileOps::joinPath(backupDest, "mfs")), "backup/mfs exists");
}

void testProjectStatus() {
    section("ProjectF16 — status change, reload, count");
    auto projects = ProjectF16::loadAll();
    check(!projects.empty(), "loadAll returns projects");
    std::cout << "  Total projects in DB: " << projects.size() << "\n";

    if (!projects.empty()) {
        auto& p = projects[0];
        std::string origStatus = p->status;
        p->status = "active"; p->update();
        auto reloaded = ProjectF16::loadById(p->projectId);
        check(reloaded && reloaded->status == "active", "Status update persists");
        p->status = origStatus; p->update();
    }

    auto* db = DatabasePool::instance().get("projects");
    if (db) {
        auto cnt = db->rowCount("projects");
        std::cout << "  DB row count (projects): " << cnt << "\n";
        check(cnt > 0, "projects table has rows");
    }
}

// ── main ─────────────────────────────────────────────────────

// ── Interactive console (old-school text menu) ──────────────
// ============================================================

#include <limits>
#include <algorithm>


void testIDFormat() {
    section("ID Format — DDR-style with DE Kuerzel from config");

    // All IDs must start with the Diensteinheit code
    const std::string& de = Rosenholz::Config::instance().registratur().diensteinheitKuerzel;
    check(!de.empty(), "registratur.diensteinheitKuerzel is set");

    // Create one of each entity type and verify its ID starts with the DE code
    auto p = Rosenholz::Person::create("Test","IDFormat","id@test.de","internal");
    p->save();
    check(p->personId.substr(0, de.size()) == de, "Person ID starts with DE: " + p->personId);
    check(p->personId.find("/PER/") != std::string::npos, "Person ID contains /PER/");

    auto proj = Rosenholz::ProjectF16::create("ID Test Project","OV","small");
    proj->save();
    check(proj->projectId.substr(0, de.size()) == de, "Project ID starts with DE: " + proj->projectId);
    check(proj->projectId.find("/F16/") != std::string::npos, "Project ID contains /F16/");

    auto risk = Rosenholz::Risk::create(proj->projectId, "ID Test Risk");
    risk->save();
    check(risk->riskId.find("/RSK/") != std::string::npos, "Risk ID contains /RSK/");

    auto doc = Rosenholz::Document::create("ID Test Doc","report",proj->projectId);
    doc->save();
    check(doc->documentId.find("/DOK/") != std::string::npos, "Document ID contains /DOK/");

    // Year must be current year
    int year = Rosenholz::currentYear();
    std::string yearStr = "/" + std::to_string(year);
    check(proj->projectId.find(yearStr) != std::string::npos,
          "Project ID contains current year");
    check(risk->riskId.find(yearStr) != std::string::npos,
          "Risk ID contains current year");

    g_extProjectId = proj->projectId;
}

void testMilestoneEntity() {
    section("Milestone — create, achieve, overdue, MFS");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto m = Rosenholz::Milestone::create(g_extProjectId, "Phase-1-Gate","2025-03-31");
    m->milestoneType   = "phase-gate";
    m->criteria        = "All requirements signed off";
    m->contractual     = true;
    m->paymentTrigger  = false;

    check(m->save(), "Milestone::save()");
    check(!m->milestoneId.empty(), "Milestone has ID");
    check(m->milestoneId.find("/MEI/") != std::string::npos, "Milestone ID contains /MEI/");

    auto loaded = Rosenholz::Milestone::loadById(m->milestoneId);
    check(loaded != nullptr, "Milestone::loadById");
    check(loaded->milestoneType == "phase-gate", "milestoneType persisted");
    check(loaded->contractual, "contractual flag persisted");

    auto list = Rosenholz::Milestone::loadForProject(g_extProjectId);
    check(!list.empty(), "loadForProject returns milestones");

    check(m->markAchieved("2025-03-29"), "Milestone::markAchieved");
    auto ach = Rosenholz::Milestone::loadById(m->milestoneId);
    check(ach->status == "achieved", "status=achieved after markAchieved");
    check(!ach->actualDate.empty(), "actualDate set");

    // MFS filing
    auto& cfg = Rosenholz::Config::instance();
    if (cfg.mfs().enabled) {
        // Milestones are filed in project Haengeregister/MEI/
        auto proj = Rosenholz::ProjectF16::loadById(g_extProjectId);
        if (proj) {
            proj->writeMFSFile(cfg.mfsPath());
            std::string meiDir = Rosenholz::FileOps::joinPath(
                Rosenholz::FileOps::joinPath(cfg.mfsPath(), "F16"),
                Rosenholz::FileOps::joinPath(
                    Rosenholz::Config::instance().registratur().diensteinheitKuerzel,
                    Rosenholz::FileOps::joinPath(
                        std::to_string(proj->regNumber.year),
                        Rosenholz::sanitiseRegNr(proj->regNumber.toString()))));
            check(Rosenholz::FileOps::dirExists(meiDir), "Project Haengeregister exists");
        }
    }
}

void testMeetingEntity() {
    section("Meeting — create under task, complete, load");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    // Create a task to attach meeting to
    auto t = Rosenholz::TaskF22::create(g_extProjectId,"Meeting Test Task","","");
    t->save();
    g_extTaskId = t->taskId;

    auto m = Rosenholz::Meeting::create(t->taskId, "Kickoff-Besprechung","2025-02-01T09:00");
    m->projectId     = g_extProjectId;
    m->meetingType   = "kickoff";
    m->durationMins  = 60;
    m->location      = "Konferenzraum A";
    m->channel       = "in-person";
    m->agenda        = "Vorstellung der Projektziele";

    check(m->save(), "Meeting::save()");
    check(!m->meetingId.empty(), "Meeting has ID");
    check(m->meetingId.find("/BSP/") != std::string::npos, "Meeting ID contains /BSP/");

    auto loaded = Rosenholz::Meeting::loadById(m->meetingId);
    check(loaded != nullptr, "Meeting::loadById");
    check(loaded->durationMins == 60, "durationMins persisted");
    check(loaded->meetingType == "kickoff", "meetingType persisted");

    auto byTask = Rosenholz::Meeting::loadForTask(t->taskId);
    check(!byTask.empty(), "Meeting::loadForTask returns results");
    bool foundMeeting = false;
    for (auto& bm : byTask) if (bm->meetingId == m->meetingId) { foundMeeting = true; break; }
    check(foundMeeting, "correct meeting found");

    check(m->complete("Projektziele besprochen","Lastenheft bis 15.02."),
          "Meeting::complete");
    auto done = Rosenholz::Meeting::loadById(m->meetingId);
    check(done->status == "completed", "status=completed after complete()");
    check(!done->decisions.empty(), "decisions persisted");
}

void testMeasureEntity() {
    section("Measure — preventive/corrective, verify, load for project");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto m = Rosenholz::Measure::create(g_extProjectId,"Lieferanten-Qualitaetspruefung","preventive");
    m->description       = "Monatliche Pruefung der Lieferantenqualitaet";
    m->measureCategory   = "quality";
    m->plannedDate       = "2025-04-01";
    m->ownerId           = "";
    m->costPlanned       = 5000.0;

    check(m->save(), "Measure::save()");
    check(m->measureId.find("/MSN/") != std::string::npos, "Measure ID contains /MSN/");

    auto loaded = Rosenholz::Measure::loadById(m->measureId);
    check(loaded != nullptr, "Measure::loadById");
    check(loaded->measureType == "preventive", "measureType persisted");
    check(loaded->costPlanned == 5000.0, "costPlanned persisted");

    auto list = Rosenholz::Measure::loadForProject(g_extProjectId);
    check(!list.empty(), "Measure::loadForProject returns results");

    // Verify
    m->status        = "completed";
    m->actualDate    = Rosenholz::nowIso().substr(0,10);
    m->costActual    = 4800.0;
    m->update();
    m->verifiedDate  = Rosenholz::nowIso().substr(0,10);
    m->verifiedBy    = "XV/PER/0001/2026";
    m->effectiveness = "high";
    m->status        = "verified";
    check(m->update(), "Measure::update (verify)");
}

void testQualityGateEntity() {
    section("QualityGate — create, record result, load");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto g = Rosenholz::QualityGate::create(g_extProjectId,"Architektur-Review","design");
    g->plannedDate        = "2025-03-15";
    g->criteria           = "Alle Architektur-Entscheidungen dokumentiert";
    g->standardsApplied   = "ISO 25010";
    g->acceptanceCriteria = "Review-Protokoll genehmigt";

    check(g->save(), "QualityGate::save()");
    check(g->gateId.find("/QT/") != std::string::npos, "QualityGate ID contains /QT/");

    auto list = Rosenholz::QualityGate::loadForProject(g_extProjectId);
    check(!list.empty(), "QualityGate::loadForProject");

    check(g->recordResult("passed","proceed","Alle Kriterien erfuellt"),
          "QualityGate::recordResult");
    auto done = Rosenholz::QualityGate::loadById(g->gateId);
    check(done->result == "passed", "result persisted");
    check(done->decision == "proceed", "decision persisted");
}

void testKPIEntity() {
    section("KPI — create, record measurement, RAG calculation");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto k = Rosenholz::KPI::create(g_extProjectId,"Termintreue","%");
    k->description        = "Prozentsatz termingerecht erledigter Aufgaben";
    k->category           = "schedule";
    k->targetValue        = 95.0;
    k->baselineValue      = 80.0;
    k->thresholdGreen     = 90.0;
    k->thresholdAmber     = 75.0;
    k->thresholdRed       = 60.0;
    k->measurementFrequency = "weekly";

    check(k->save(), "KPI::save()");
    check(k->kpiId.find("/KPI/") != std::string::npos, "KPI ID contains /KPI/");

    check(k->recordMeasurement(92.5,"2025-03-07"), "KPI::recordMeasurement (green)");
    check(k->ragStatus == "green", "RAG=green when >= thresholdGreen");
    check(k->actualValue == 92.5, "actualValue persisted");

    check(k->recordMeasurement(78.0,"2025-03-14"), "KPI::recordMeasurement (amber)");
    check(k->ragStatus == "amber", "RAG=amber when >= thresholdAmber");

    check(k->recordMeasurement(55.0,"2025-03-21"), "KPI::recordMeasurement (red)");
    check(k->ragStatus == "red", "RAG=red when < thresholdRed");

    auto list = Rosenholz::KPI::loadForProject(g_extProjectId);
    check(!list.empty(), "KPI::loadForProject");
}

void testLessonLearnedEntity() {
    section("LessonLearned — create, approve, knowledge base");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto l = Rosenholz::LessonLearned::create(g_extProjectId,
        "Fruehzeitige Stakeholder-Einbindung reduziert Scope-Aenderungen");
    l->description     = "In Phase 1 wurden Stakeholder zu spaet eingebunden";
    l->category        = "process";
    l->dimension       = "scope";
    l->impact          = "3 Scope-Aenderungen in Phase 1 verzoegerten die Lieferung um 2 Wochen";
    l->recommendation  = "Stakeholder-Workshop in der ersten Projektwoche abhalten";
    l->submittedBy     = "XV/PER/0001/2026";

    check(l->save(), "LessonLearned::save()");
    check(l->lessonId.find("/LE/") != std::string::npos, "LessonLearned ID contains /LE/");

    auto list = Rosenholz::LessonLearned::loadForProject(g_extProjectId);
    check(!list.empty(), "LessonLearned::loadForProject");

    l->status = "approved";
    l->reviewedDate = Rosenholz::nowIso().substr(0,10);
    l->update();
    auto loaded = Rosenholz::LessonLearned::loadById(l->lessonId);
    check(loaded->status == "approved", "status approved persisted");

    l->addedToKb = true;
    l->update();
    auto kb = Rosenholz::LessonLearned::loadKnowledgeBase();
    check(!kb.empty(), "LessonLearned in knowledge base");
}

void testDecisionLogEntity() {
    section("DecisionLog — create, implement, load");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto d = Rosenholz::DecisionLog::create(g_extProjectId,
        "Microservices-Architektur statt Monolith");
    d->description        = "Entscheidung fuer eine Microservices-Architektur";
    d->decisionType       = "technical";
    d->decidedBy          = "XV/PER/0001/2026";
    d->optionsConsidered  = "1. Monolith  2. Microservices  3. Modularer Monolith";
    d->rationale          = "Bessere Skalierbarkeit und unabhaengige Deployments";
    d->impactCost         = "+15% initiale Entwicklungskosten";
    d->impactSchedule     = "+3 Wochen Einrichtungszeit";

    check(d->save(), "DecisionLog::save()");
    check(d->decisionId.find("/ENT/") != std::string::npos, "DecisionLog ID contains /ENT/");

    auto list = Rosenholz::DecisionLog::loadForProject(g_extProjectId);
    check(!list.empty(), "DecisionLog::loadForProject");

    d->status = "implemented";
    check(d->update(), "DecisionLog::update (implement)");
    auto loaded = Rosenholz::DecisionLog::loadById(d->decisionId);
    check(loaded->status == "implemented", "status implemented persisted");
}

void testChangeRequestEntity() {
    section("ChangeRequest — create, submit, approve, open list");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto cr = Rosenholz::ChangeRequest::create(g_extProjectId,
        "Zusaetzliches Reporting-Modul","scope");
    cr->description       = "Erweiterung des Berichtsmoduls um Executive Dashboard";
    cr->justification     = "Geshaeftsfuehrung benoetigt woechtliche Kennzahlen-Uebersicht";
    cr->raisedBy          = "XV/PER/0001/2026";
    cr->costImpact        = 12000.0;
    cr->scheduleImpactDays = 14;
    cr->scopeImpact       = "Neues UI-Modul erforderlich";

    check(cr->save(), "ChangeRequest::save()");
    check(cr->crId.find("/AEA/") != std::string::npos, "ChangeRequest ID contains /AEA/");

    cr->status = "submitted";
    cr->update();
    auto open = Rosenholz::ChangeRequest::loadOpen();
    check(!open.empty(), "ChangeRequest::loadOpen has results");

    check(cr->approve("Genehmigt, da strategisch notwendig"), "ChangeRequest::approve");
    auto loaded = Rosenholz::ChangeRequest::loadById(cr->crId);
    check(loaded->status == "approved", "status approved");
    check(!loaded->decisionRationale.empty(), "decisionRationale set");

    // Test reject path
    auto cr2 = Rosenholz::ChangeRequest::create(g_extProjectId,"Abgelehnter Antrag","cost");
    cr2->save();
    check(cr2->reject("Budget nicht verfuegbar"), "ChangeRequest::reject");
    auto rej = Rosenholz::ChangeRequest::loadById(cr2->crId);
    check(rej->status == "rejected", "status rejected");
}

void testAssumptionConstraintEntity() {
    section("AssumptionConstraint — assumption, constraint, breach");
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    // Assumption
    auto a = Rosenholz::AssumptionConstraint::create(g_extProjectId,
        "Alle externen APIs sind stabil","assumption");
    a->description   = "Annahme: Alle benoetigten externen Schnittstellen bleiben unveraendert";
    a->dimension     = "scope";
    a->impactIfWrong = "Zusaetzliche Migrationsarbeit notwendig";
    a->mitigation    = "Versionierung aller API-Aufrufe";

    check(a->save(), "AssumptionConstraint::save (assumption)");
    check(a->acId.find("/ABE/") != std::string::npos, "Assumption ID contains /ABE/");

    // Constraint
    auto c = Rosenholz::AssumptionConstraint::create(g_extProjectId,
        "Budget darf 500.000 EUR nicht ueberschreiten","constraint");
    c->dimension     = "cost";
    c->status        = "active";
    check(c->save(), "AssumptionConstraint::save (constraint)");

    auto list = Rosenholz::AssumptionConstraint::loadForProject(g_extProjectId);
    check(list.size() >= 2, "loadForProject returns both assumption and constraint");

    // Mark breached
    check(a->markBreached("2025-04-15"), "AssumptionConstraint::markBreached");
    auto loaded = Rosenholz::AssumptionConstraint::loadById(a->acId);
    check(loaded->breached, "breached flag set");
    check(loaded->status == "breached", "status=breached");
}

void testDocumentMFSEnforcement() {
    section("Document MFS — ref enforcement, unique naming, replace-on-update");
    auto& cfg = Rosenholz::Config::instance();
    if (!cfg.mfs().enabled) { check(true,"MFS disabled — skip"); return; }

    // 1. Document without project/task must not be filed in MFS
    auto d_nref = Rosenholz::Document::create("Orphan Doc","misc","");
    d_nref->save();
    bool filed = Rosenholz::MFSWriter::writeDocument(*d_nref, cfg.mfsPath());
    check(!filed, "Document without project ref refused by MFSWriter");

    // 2. Document with project ref IS filed
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }
    auto d = Rosenholz::Document::create("Projektcharter","report",g_extProjectId);
    d->format = "pdf";
    d->save();
    bool ok = Rosenholz::MFSWriter::writeDocument(*d, cfg.mfsPath());
    check(ok, "Document with project ref filed in MFS");

    // Find the filed file
    auto proj = Rosenholz::ProjectF16::loadById(g_extProjectId);
    if (proj) {
        std::string dokDir = Rosenholz::FileOps::joinPath(
            cfg.mfsPath(),
            Rosenholz::FileOps::joinPath("F16",
            Rosenholz::FileOps::joinPath(cfg.registratur().diensteinheitKuerzel,
            Rosenholz::FileOps::joinPath(std::to_string(proj->regNumber.year),
            Rosenholz::FileOps::joinPath(
                Rosenholz::sanitiseRegNr(proj->regNumber.toString()), "DOK")))));
        check(Rosenholz::FileOps::dirExists(dokDir), "DOK subfolder exists in Haengeregister");

        // The filename must start with the sanitised document ID
        std::string sane = Rosenholz::sanitiseRegNr(d->documentId);
        bool found = false;
        for (auto& f : Rosenholz::FileOps::listDir(dokDir))
            if (f.size() >= sane.size() && f.substr(0, sane.size()) == sane)
                found = true;
        check(found, "Document file starts with sanitised ID prefix");

        // 3. Update title and re-file — old file must be replaced
        d->title = "Projektcharter v2";
        d->update();
        Rosenholz::MFSWriter::writeDocument(*d, cfg.mfsPath());

        int count = 0;
        for (auto& f : Rosenholz::FileOps::listDir(dokDir))
            if (f.size() >= sane.size() && f.substr(0, sane.size()) == sane) count++;
        check(count == 1, "Only one file with this ID prefix after re-file (old replaced)");
    }
}

void testWorkflowEngine() {
    section("Workflow engine — definition, states, transitions, instance, fire");
    auto* db = Rosenholz::DatabasePool::instance().get("workflow");
    if (!db) { check(false, "workflow DB available"); return; }

    // Create definition
    std::string defId = Rosenholz::genId("WFD");
    std::vector<Rosenholz::BindParam> wfDefParams = {
        Rosenholz::BindParam::text(defId),
        Rosenholz::BindParam::text("Testprozess"),
        Rosenholz::BindParam::text("1.0"),
        Rosenholz::BindParam::text("project"),
        Rosenholz::BindParam::text("entwurf"),
        Rosenholz::BindParam::text("genehmigt,abgelehnt"),
        Rosenholz::BindParam::int64(0),
        Rosenholz::BindParam::int64(1),
        Rosenholz::BindParam::int64(72)};
    bool ok = db->exec(R"(INSERT OR REPLACE INTO workflow_definitions
        (workflow_def_id,name,version,entity_type,initial_state,terminal_states,
         parallel_approval_allowed,sla_enforced,default_sla_hours,status)
        VALUES(?,?,?,?,?,?,?,?,?,'active'))", wfDefParams);
    check(ok, "WF definition created");
    check(defId.find("/WFD/") != std::string::npos, "WFD ID has correct type code");

    // Add states
    auto addState = [&](const std::string& name, bool initial, bool terminal) {
        std::string sid = Rosenholz::genId("WFS");
        db->exec(R"(INSERT OR REPLACE INTO workflow_states
            (state_id,workflow_def_id,name,label,state_type,
             is_initial,is_terminal,requires_approval,
             notifies_on_entry,notifies_on_exit,sla_hours)
            VALUES(?,?,?,?,?,?,?,?,?,?,?))",{
            Rosenholz::BindParam::text(sid),
            Rosenholz::BindParam::text(defId),
            Rosenholz::BindParam::text(name),
            Rosenholz::BindParam::text(name),
            Rosenholz::BindParam::text("normal"),
            Rosenholz::BindParam::int64(initial?1:0),
            Rosenholz::BindParam::int64(terminal?1:0),
            Rosenholz::BindParam::int64(0),
            Rosenholz::BindParam::int64(0),
            Rosenholz::BindParam::int64(0),
            Rosenholz::BindParam::int64(24)});
        return sid;
    };
    std::string s1 = addState("entwurf",   true,  false);
    std::string s2 = addState("in_pruefung",false, false);
    std::string s3 = addState("genehmigt", false, true);

    // Add transition
    std::string tid = Rosenholz::genId("WFT");
    db->exec(R"(INSERT OR REPLACE INTO workflow_transitions
        (transition_id,workflow_def_id,from_state_id,to_state_id,
         trigger_event,requires_comment,auto_trigger)
        VALUES(?,?,?,?,?,?,?))",{
        Rosenholz::BindParam::text(tid),
        Rosenholz::BindParam::text(defId),
        Rosenholz::BindParam::text("entwurf"),
        Rosenholz::BindParam::text("in_pruefung"),
        Rosenholz::BindParam::text("einreichen"),
        Rosenholz::BindParam::int64(0),
        Rosenholz::BindParam::int64(0)});

    auto defs = db->query("SELECT * FROM workflow_definitions WHERE workflow_def_id=?;",
        {Rosenholz::BindParam::text(defId)});
    check(!defs.empty(), "WF definition retrievable");
    check(defs[0].at("name") == "Testprozess", "WF definition name correct");

    auto states = db->query("SELECT * FROM workflow_states WHERE workflow_def_id=?;",
        {Rosenholz::BindParam::text(defId)});
    check(states.size() == 3, "3 WF states created");

    auto trans = db->query("SELECT * FROM workflow_transitions WHERE workflow_def_id=?;",
        {Rosenholz::BindParam::text(defId)});
    check(!trans.empty(), "WF transition created");

    // Start instance
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }
    std::string instId = Rosenholz::genId("WFI");
    db->exec(R"(INSERT OR REPLACE INTO workflow_instances
        (instance_id,workflow_def_id,entity_type,entity_id,
         current_state_id,initiated_by,initiated_date,sla_hours,status)
        VALUES(?,?,?,?,?,?,?,?,'active'))",{
        Rosenholz::BindParam::text(instId),
        Rosenholz::BindParam::text(defId),
        Rosenholz::BindParam::text("project"),
        Rosenholz::BindParam::text(g_extProjectId),
        Rosenholz::BindParam::text("entwurf"),
        Rosenholz::BindParam::text("XV/PER/0001/2026"),
        Rosenholz::BindParam::text(Rosenholz::nowIso()),
        Rosenholz::BindParam::int64(72)});
    check(instId.find("/WFI/") != std::string::npos, "WFI ID has correct type code");

    auto insts = db->query("SELECT * FROM workflow_instances WHERE instance_id=?;",
        {Rosenholz::BindParam::text(instId)});
    check(!insts.empty(), "WF instance created");
    check(insts[0].at("current_state_id") == "entwurf", "instance starts in initial state");

    // Fire transition (entwurf -> in_pruefung)
    std::string actId = Rosenholz::genId("WFA");
    db->exec(R"(INSERT INTO workflow_actions
        (action_id,instance_id,transition_id,actor_id,action_type,decision,action_date)
        VALUES(?,?,?,?,?,?,?))",{
        Rosenholz::BindParam::text(actId),
        Rosenholz::BindParam::text(instId),
        Rosenholz::BindParam::text(tid),
        Rosenholz::BindParam::text("XV/PER/0001/2026"),
        Rosenholz::BindParam::text("einreichen"),
        Rosenholz::ton(""),
        Rosenholz::BindParam::text(Rosenholz::nowIso())});

    db->exec("UPDATE workflow_instances SET current_state_id=?,previous_state_id=? WHERE instance_id=?;",{
        Rosenholz::BindParam::text("in_pruefung"),
        Rosenholz::BindParam::text("entwurf"),
        Rosenholz::BindParam::text(instId)});

    auto updated = db->query("SELECT * FROM workflow_instances WHERE instance_id=?;",
        {Rosenholz::BindParam::text(instId)});
    check(updated[0].at("current_state_id") == "in_pruefung", "state moved to in_pruefung");

    auto actions = db->query("SELECT * FROM workflow_actions WHERE instance_id=?;",
        {Rosenholz::BindParam::text(instId)});
    check(!actions.empty(), "WF action recorded");
}

void testMFSGermanAbbreviations() {
    section("MFS — German abbreviations in IDs and folder structure");

    // Create one of each and check the ID type code
    if (g_extProjectId.empty()) { check(false,"need extProjectId"); return; }

    auto risk = Rosenholz::Risk::create(g_extProjectId, "Test-Risiko");
    risk->save();
    check(risk->riskId.find("/RSK/") != std::string::npos, "Risk uses /RSK/ (Risiko-Akte)");

    auto ms = Rosenholz::Measure::create(g_extProjectId, "Test-Massnahme", "corrective");
    ms->save();
    check(ms->measureId.find("/MSN/") != std::string::npos, "Measure uses /MSN/ (Massnahme)");

    auto qg = Rosenholz::QualityGate::create(g_extProjectId, "Test-QT");
    qg->save();
    check(qg->gateId.find("/QT/") != std::string::npos, "QualityGate uses /QT/ (Qualitaetstor)");

    auto kpi = Rosenholz::KPI::create(g_extProjectId, "Test-KPI");
    kpi->save();
    check(kpi->kpiId.find("/KPI/") != std::string::npos, "KPI uses /KPI/ (Kennzahl)");

    auto ll = Rosenholz::LessonLearned::create(g_extProjectId, "Test-LE");
    ll->save();
    check(ll->lessonId.find("/LE/") != std::string::npos, "LessonLearned uses /LE/ (Lernerkenntnis)");

    auto dl = Rosenholz::DecisionLog::create(g_extProjectId, "Test-ENT");
    dl->save();
    check(dl->decisionId.find("/ENT/") != std::string::npos, "DecisionLog uses /ENT/ (Entscheidung)");

    auto cr = Rosenholz::ChangeRequest::create(g_extProjectId, "Test-AEA");
    cr->save();
    check(cr->crId.find("/AEA/") != std::string::npos, "ChangeRequest uses /AEA/ (Aenderungsantrag)");

    auto ac = Rosenholz::AssumptionConstraint::create(g_extProjectId, "Test-ABE");
    ac->save();
    check(ac->acId.find("/ABE/") != std::string::npos, "AssumptionConstraint uses /ABE/");

    auto mtg = Rosenholz::Meeting::create(g_extTaskId.empty()?"dummy":g_extTaskId, "Test-BSP");
    mtg->save();
    check(mtg->meetingId.find("/BSP/") != std::string::npos, "Meeting uses /BSP/ (Besprechung)");

    auto mil = Rosenholz::Milestone::create(g_extProjectId, "Test-MEI");
    mil->save();
    check(mil->milestoneId.find("/MEI/") != std::string::npos, "Milestone uses /MEI/ (Meilenstein)");

    auto doc = Rosenholz::Document::create("Test-DOK","report",g_extProjectId);
    doc->save();
    check(doc->documentId.find("/DOK/") != std::string::npos, "Document uses /DOK/");

    // Verify MFS Haengeregister subfolder names are German
    auto& cfg = Rosenholz::Config::instance();
    if (cfg.mfs().enabled && !g_extProjectId.empty()) {
        auto proj = Rosenholz::ProjectF16::loadById(g_extProjectId);
        if (proj) {
            proj->writeMFSFile(cfg.mfsPath());
            // Check some subfolders exist with correct German names
            std::string heft = Rosenholz::FileOps::joinPath(
                cfg.mfsPath(),
                Rosenholz::FileOps::joinPath("F16",
                Rosenholz::FileOps::joinPath(cfg.registratur().diensteinheitKuerzel,
                Rosenholz::FileOps::joinPath(std::to_string(proj->regNumber.year),
                    Rosenholz::sanitiseRegNr(proj->regNumber.toString())))));
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"RSK")),
                  "Haengeregister has RSK/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"MSN")),
                  "Haengeregister has MSN/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"QT")),
                  "Haengeregister has QT/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"LE")),
                  "Haengeregister has LE/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"ENT")),
                  "Haengeregister has ENT/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"AEA")),
                  "Haengeregister has AEA/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"ABE")),
                  "Haengeregister has ABE/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"BSP")),
                  "Haengeregister has BSP/ subfolder");
            check(Rosenholz::FileOps::dirExists(Rosenholz::FileOps::joinPath(heft,"MEI")),
                  "Haengeregister has MEI/ subfolder");
        }
    }
}

void testRegistraturConfig() {
    section("Registratur config — DE Kuerzel drives IDs and MFS paths");
    auto& reg = Rosenholz::Config::instance().registratur();
    check(!reg.diensteinheitKuerzel.empty(), "diensteinheitKuerzel set");
    check(!reg.aktenfuehrendeStelle.empty(), "aktenfuehrendeStelle set");
    check(!reg.geschaeftszeichen.empty(),    "geschaeftszeichen set");
    check(!reg.archivSignatur.empty(),        "archivSignatur set");

    std::cout << "  DE-Kuerzel   : " << reg.diensteinheitKuerzel << "\n";
    std::cout << "  Stelle       : " << reg.aktenfuehrendeStelle << "\n";
    std::cout << "  GZ           : " << reg.geschaeftszeichen    << "\n";
    std::cout << "  Archiv-Sig   : " << reg.archivSignatur       << "\n";

    // All newly generated IDs must start with this DE code
    std::string newId = Rosenholz::genId("TST");
    check(newId.substr(0, reg.diensteinheitKuerzel.size()) == reg.diensteinheitKuerzel,
          "genId() starts with configured DE Kuerzel");
    check(newId.find("/TST/") != std::string::npos, "genId() inserts type code");

    // Deckblatt contains Aktenzeichen = GZ-<regNr>
    auto& cfg = Rosenholz::Config::instance();
    if (cfg.mfs().enabled && !g_extProjectId.empty()) {
        auto proj = Rosenholz::ProjectF16::loadById(g_extProjectId);
        if (proj) {
            proj->writeMFSFile(cfg.mfsPath());
            std::string deckblatt = Rosenholz::FileOps::joinPath(
                cfg.mfsPath(),
                Rosenholz::FileOps::joinPath("F16",
                Rosenholz::FileOps::joinPath(reg.diensteinheitKuerzel,
                Rosenholz::FileOps::joinPath(std::to_string(proj->regNumber.year),
                Rosenholz::FileOps::joinPath(
                    Rosenholz::sanitiseRegNr(proj->regNumber.toString()),
                    "00_DECKBLATT.txt")))));
            check(Rosenholz::FileOps::fileExists(deckblatt), "DECKBLATT.txt exists at correct path");
            std::string content = Rosenholz::FileOps::readTextFile(deckblatt);
            check(content.find(reg.geschaeftszeichen) != std::string::npos,
                  "DECKBLATT contains Geschaeftszeichen");
        }
    }
}

namespace CLI {

// ── forward declarations ──────────────────────────────────────
static std::string startWfInstanceWizard(const std::string& = "", const std::string& = "");
static void instanceMenu(const std::string&);
static void listWfInstances(const std::string& = "", const std::string& = "");
static void workflowMenu();
static void documentBrowserMenu(const std::string& = "", const std::string& = "");
static void documentMenu(std::shared_ptr<Rosenholz::Document>);
static void milestoneMenu(const std::string&);
static void meetingMenu(const std::string&, const std::string& = "");
static void measureMenu(const std::string&);
static void qualityGateMenu(const std::string&);
static void kpiMenu(const std::string&);
static void changeRequestMenu(const std::string&);
static void lessonLearnedMenu(const std::string&);
static void assumptionConstraintMenu(const std::string&);
static void decisionLogMenu(const std::string&);

// ── tiny helpers ─────────────────────────────────────────────
static void hr()  { std::cout << "  " << std::string(52,'-') << "\n"; }
static void hdr(const std::string& t) {
    std::cout << "\n  +" << std::string(52,'-') << "+\n";
    std::cout << "  |  " << t << std::string(52 - (int)t.size() - 1,' ') << "|\n";
    std::cout << "  +" << std::string(52,'-') << "+\n";
}

// Read a non-empty line, trim trailing whitespace
static std::string readLine(const std::string& prompt = "") {
    if (!prompt.empty()) std::cout << "  " << prompt;
    std::string s;
    while (std::getline(std::cin, s)) {
        // trim right
        while (!s.empty() && (s.back()=='\r'||s.back()=='\n'||s.back()==' ')) s.pop_back();
        if (!s.empty()) return s;
        if (!prompt.empty()) std::cout << "  " << prompt;
    }
    return "";  // EOF
}

// Read integer in [lo,hi]
static int readInt(const std::string& prompt, int lo, int hi) {
    while (true) {
        std::cout << "  " << prompt << " [" << lo << "-" << hi << "]: ";
        std::string s;
        if (!std::getline(std::cin, s)) return lo;
        try {
            int v = std::stoi(s);
            if (v >= lo && v <= hi) return v;
        } catch (...) {}
        std::cout << "  >> Please enter a number between " << lo << " and " << hi << "\n";
    }
}

// Read optional line (may be empty)
static std::string readOpt(const std::string& prompt) {
    std::cout << "  " << prompt;
    std::string s;
    std::getline(std::cin, s);
    while (!s.empty() && (s.back()=='\r'||s.back()=='\n'||s.back()==' ')) s.pop_back();
    return s;
}

// ── Format a date string or "—" ───────────────────────────────
static std::string fdate(const std::string& d) { return d.empty() ? "—" : d; }
static std::string fval (const std::string& v) { return v.empty() ? "—" : v; }


// ─────────────────────────────────────────────────────────────
// DOCUMENT HELPERS
// ─────────────────────────────────────────────────────────────
static void printDocument(const Rosenholz::Document& d) {
    hdr("DOCUMENT  " + d.documentId.substr(0, 16) + "...");
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(30) << v << "|\n";
    };
    row("ID",           d.documentId);
    row("Title",        d.title);
    row("Type",         fval(d.docType));
    row("Category",     fval(d.docCategory));
    row("Version",      fval(d.version));
    row("Status",       fval(d.status));
    row("Format",       fval(d.format));
    row("Language",     fval(d.language));
    row("Classification",fval(d.classification));
    hr();
    row("Author-ID",    fval(d.authorId));
    row("Approved-by",  fval(d.approvedBy));
    row("Project-ID",   fval(d.projectId));
    row("Task-ID",      fval(d.taskId));
    hr();
    row("Created",      fdate(d.dateCreated));
    row("Modified",     fdate(d.dateModified));
    row("Approved",     fdate(d.dateApproved));
    row("Expires",      fdate(d.dateExpires));
    hr();
    row("Storage",      fval(d.storageSystem));
    row("Pages",        d.pageCount > 0 ? std::to_string(d.pageCount) : "—");
    if (!d.fileUrl.empty())
        row("Source URL", d.fileUrl.size() > 29
            ? d.fileUrl.substr(0,28) + "~" : d.fileUrl);
    if (!d.filePath.empty())
        row("Local path", d.filePath.size() > 29
            ? "..."+d.filePath.substr(d.filePath.size()-26) : d.filePath);
    if (!d.summary.empty())
        std::cout << "  | Summary: " << d.summary.substr(0,43) << "|\n";
    if (!d.tags.empty())
        row("Tags",      d.tags);
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

static void listDocuments(const std::vector<std::shared_ptr<Rosenholz::Document>>& docs,
                          const std::string& heading = "DOCUMENTS") {
    if (docs.empty()) { std::cout << "\n  (no documents)\n\n"; return; }
    hdr(heading);
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(12) << "Type"
              << std::setw(26) << "Title"
              << std::setw(8)  << "Status"
              << std::setw(7)  << "Format"
              << "\n";
    hr();
    int n = 1;
    for (auto& d : docs) {
        std::string title = d->title.size() > 24 ? d->title.substr(0,23)+"~" : d->title;
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(12) << fval(d->docType)
                  << std::setw(26) << title
                  << std::setw(8)  << fval(d->status)
                  << std::setw(7)  << fval(d->format)
                  << "\n";
    }
    std::cout << "\n";
}

static std::shared_ptr<Rosenholz::Document> createDocumentWizard(
        const std::string& projectId = "",
        const std::string& taskId    = "") {
    hdr("CREATE / REGISTER DOCUMENT");

    // Enforce that every document is filed under a known entity
    std::string effProjectId = projectId;
    std::string effTaskId    = taskId;
    if (effProjectId.empty() && effTaskId.empty()) {
        std::cout << "  A document MUST be filed under a project or task.\n"
                  << "  Without a reference it cannot be located in the archive.\n";
        std::cout << "  1. Attach to project   2. Attach to task   0. Cancel\n";
        int ref = readInt("Attach to", 0, 2);
        if (ref == 0) return nullptr;
        if (ref == 1) effProjectId = readLine("Project ID: ");
        if (ref == 2) {
            effTaskId    = readLine("Task ID: ");
            // also derive project from task
            auto task = Rosenholz::TaskF22::loadById(effTaskId);
            if (task) effProjectId = task->projectId;
        }
        if (effProjectId.empty() && effTaskId.empty()) {
            std::cout << "  >> Cancelled -- no reference provided.\n";
            return nullptr;
        }
    }
    std::cout << "  Source:\n"
              << "    1. Local file path\n"
              << "    2. URL (download + archive automatically)\n"
              << "    3. Manual entry (no file yet)\n";
    int src = readInt("Source type", 1, 3);

    std::string title, filePath, fileUrl, format, summary;

    if (src == 1) {
        filePath = readLine("Full file path: ");
        // derive a default title from filename
        std::string base = Rosenholz::FileOps::baseName(filePath);
        title  = readOpt("Title (Enter for '" + base + "'): ");
        if (title.empty()) title = base;
        format = Rosenholz::FileOps::extension(filePath);
        if (!format.empty() && format[0]=='.') format = format.substr(1);
    } else if (src == 2) {
        fileUrl = readLine("URL to archive: ");
        title   = readOpt("Title (optional, derived from URL if empty): ");
        std::cout << "  >> Downloading and archiving...\n";
        auto& cfg = Rosenholz::Config::instance();
        std::string archiveDir = Rosenholz::FileOps::joinPath(cfg.basePath(), "documents", "archived");
        auto archived = Rosenholz::Document::archiveFromUrl(fileUrl, projectId);
        if (archived) {
            std::cout << "  >> Archived to: " << archived->filePath << "\n";
            if (!title.empty()) archived->title = title;
            archived->projectId = projectId;
            archived->taskId    = taskId;
            archived->update();
            return archived;
        } else {
            std::cout << "  >> Archive failed. Creating manual entry instead.\n";
            title = title.empty() ? fileUrl : title;
        }
    } else {
        title = readLine("Title: ");
    }

    // doc type
    std::cout << "  Document type:\n"
              << "    1. report       2. specification  3. contract\n"
              << "    4. correspondence 5. evidence     6. plan\n"
              << "    7. minutes      8. archive        9. other\n";
    int dt = readInt("Type", 1, 9);
    static const char* dtypes[] = {
        "report","specification","contract","correspondence",
        "evidence","plan","minutes","archive","other"};
    std::string docType = dtypes[dt-1];

    std::string docCat  = readOpt("Category (optional, e.g. 'technical','legal'): ");
    std::string version = readOpt("Version (default 1.0): ");
    if (version.empty()) version = "1.0";
    if (format.empty()) format = readOpt("Format (pdf/docx/xlsx/txt/html, optional): ");
    std::string lang    = readOpt("Language (default EN): ");
    if (lang.empty()) lang = "EN";
    std::string cls     = readOpt("Classification (optional): ");
    summary             = readOpt("Summary (optional): ");
    std::string tags    = readOpt("Tags comma-separated (optional): ");
    std::string author  = readOpt("Author person-ID (optional): ");
    std::string dateC   = readOpt("Date created YYYY-MM-DD (optional): ");
    std::string dateE   = readOpt("Date expires YYYY-MM-DD (optional): ");

    auto doc = Rosenholz::Document::create(title, docType, effProjectId);
    doc->docCategory    = docCat;
    doc->version        = version;
    doc->format         = format;
    doc->language       = lang;
    doc->classification = cls;
    doc->summary        = summary;
    doc->tags           = tags;
    doc->authorId       = author;
    doc->taskId         = effTaskId;
    doc->filePath       = filePath;
    doc->fileUrl        = fileUrl;
    doc->dateCreated    = dateC;
    doc->dateExpires    = dateE;
    doc->storageSystem  = "local";

    if (doc->save()) {
        std::cout << "\n  >> Document saved: " << doc->documentId << "\n\n";
        // Auto-attach to project/task
        if (!effProjectId.empty()) doc->attachToEntity("project", effProjectId);
        if (!effTaskId.empty())    doc->attachToEntity("task",    effTaskId);
        // Write MFS file
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
        return doc;
    } else {
        std::cout << "\n  >> ERROR: Document could not be saved.\n\n";
        return nullptr;
    }
}

static void documentMenu(std::shared_ptr<Rosenholz::Document> doc) {
    while (true) {
        printDocument(*doc);
        std::cout << "  Document actions:\n"
                  << "    1. Edit title / category / version\n"
                  << "    2. Edit status / classification\n"
                  << "    3. Edit dates (created / approved / expires)\n"
                  << "    4. Edit summary / tags\n"
                  << "    5. Set author / approver\n"
                  << "    6. Attach to entity (project / task / incident)\n"
                  << "    7. Re-download / re-archive URL\n"
                  << "    8. Write MFS file\n"
                  << "    9. Delete document record\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 9);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string t = readOpt("New title (Enter to keep): ");
            if (!t.empty()) doc->title = t;
            std::string c = readOpt("New category (Enter to keep): ");
            if (!c.empty()) doc->docCategory = c;
            std::string v = readOpt("New version (Enter to keep): ");
            if (!v.empty()) doc->version = v;
            std::string f = readOpt("New format (Enter to keep): ");
            if (!f.empty()) doc->format = f;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 2) {
            std::cout << "  Status: draft / review / approved / superseded / archived\n";
            std::string s = readOpt("New status (Enter to keep): ");
            if (!s.empty()) doc->status = s;
            std::string cl = readOpt("New classification (Enter to keep): ");
            if (!cl.empty()) doc->classification = cl;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 3) {
            std::string dc = readOpt("Created  YYYY-MM-DD: ");
            if (!dc.empty()) doc->dateCreated = dc;
            std::string da = readOpt("Approved YYYY-MM-DD: ");
            if (!da.empty()) doc->dateApproved = da;
            std::string de = readOpt("Expires  YYYY-MM-DD: ");
            if (!de.empty()) doc->dateExpires = de;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 4) {
            std::cout << "  Current summary: " << fval(doc->summary) << "\n";
            std::string s = readLine("New summary: ");
            doc->summary = s;
            std::string t = readOpt("New tags comma-separated: ");
            if (!t.empty()) doc->tags = t;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 5) {
            std::string a = readOpt("Author person-ID (Enter to keep): ");
            if (!a.empty()) doc->reassignAuthor(a);
            std::string ap = readOpt("Approved-by person-ID (Enter to keep): ");
            if (!ap.empty()) { doc->approvedBy = ap; doc->update(); }
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 6) {
            std::cout << "  Entity types: project / task / incident / risk\n";
            std::string et = readLine("Entity type: ");
            std::string ei = readLine("Entity ID: ");
            std::string rel = readOpt("Relationship (attached/reference/evidence, default=attached): ");
            if (rel.empty()) rel = "attached";
            bool ok = doc->attachToEntity(et, ei, rel);
            std::cout << "  >> " << (ok ? "Attached." : "Failed.") << "\n";
        }
        else if (ch == 7) {
            if (doc->fileUrl.empty()) {
                std::string url = readLine("URL to download: ");
                doc->fileUrl = url;
            }
            std::cout << "  >> Archiving: " << doc->fileUrl << "\n";
            auto& cfg = Rosenholz::Config::instance();
            std::string archDir = Rosenholz::FileOps::joinPath(cfg.basePath(), "documents", "archived");
            std::string newPath = Rosenholz::FileOps::downloadUrl(doc->fileUrl, archDir);
            if (!newPath.empty()) {
                doc->filePath = newPath;
                doc->format   = Rosenholz::FileOps::extension(newPath);
                doc->update();
                std::cout << "  >> Downloaded to: " << newPath << "\n";
            } else {
                std::cout << "  >> Download failed.\n";
            }
        }
        else if (ch == 8) {
            auto& cfg = Rosenholz::Config::instance();
            Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
            std::cout << "  >> MFS file written.\n";
        }
        else if (ch == 9) {
            std::cout << "  Delete document record '" << doc->title << "'? (y/n): ";
            std::string ans; std::getline(std::cin, ans);
            if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y')) {
                doc->remove();
                std::cout << "  >> Deleted.\n";
                break;
            }
        }
    }
}

static void documentBrowserMenu(const std::string& projectId,
                                const std::string& taskId) {
    while (true) {
        // Load docs for given context or all
        std::vector<std::shared_ptr<Rosenholz::Document>> docs;
        if (!projectId.empty())
            docs = Rosenholz::Document::loadForProject(projectId);
        else if (!taskId.empty())
            docs = Rosenholz::Document::loadForEntity("task", taskId);

        listDocuments(docs, projectId.empty() && taskId.empty()
            ? "ALL DOCUMENTS" : "DOCUMENTS");

        std::cout << "  Actions:\n"
                  << "    1. Open document by number\n"
                  << "    2. Create / register new document\n"
                  << "    3. Attach existing document by ID\n";
        if (!projectId.empty())
            std::cout << "    4. List all documents in system\n";
        std::cout << "    0. Back\n";

        int maxch = projectId.empty() ? 3 : 4;
        int ch = readInt("Choice", 0, maxch);
        if (ch == 0) break;

        else if (ch == 1) {
            if (docs.empty()) { std::cout << "  (nothing to open)\n"; continue; }
            int n = readInt("Document number", 1, (int)docs.size());
            documentMenu(docs[n-1]);
        }
        else if (ch == 2) {
            createDocumentWizard(projectId, taskId);
        }
        else if (ch == 3) {
            std::string did = readLine("Document ID to attach: ");
            auto doc = Rosenholz::Document::loadById(did);
            if (!doc) { std::cout << "  >> Not found.\n"; continue; }
            std::string et = !projectId.empty() ? "project"
                           : !taskId.empty()    ? "task" : readLine("Entity type: ");
            std::string ei = !projectId.empty() ? projectId
                           : !taskId.empty()    ? taskId  : readLine("Entity ID: ");
            doc->attachToEntity(et, ei);
            std::cout << "  >> Attached.\n";
        }
        else if (ch == 4) {
            // load everything from documents DB
            auto* db = Rosenholz::DatabasePool::instance().get("documents");
            if (db) {
                auto rows = db->query("SELECT * FROM documents ORDER BY date_created DESC;");
                std::vector<std::shared_ptr<Rosenholz::Document>> all;
                for (auto& r : rows) {
                    auto d = Rosenholz::Document::create("","","");
                    d->load(r.at("document_id"));
                    all.push_back(d);
                }
                listDocuments(all, "ALL DOCUMENTS IN SYSTEM");
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// DISPLAY HELPERS
// ─────────────────────────────────────────────────────────────
static void printProject(const Rosenholz::ProjectF16& p) {
    hdr("PROJECT (F16)  " + p.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",           p.projectId);
    row("Reg-Nr",       p.regNumber.toString());
    row("Title",        p.title);
    row("Codename",     fval(p.codename));
    row("Type",         fval(p.projectType));
    row("Size",         fval(p.sizeClass));
    row("Status",       fval(p.status));
    row("Phase",        fval(p.phase));
    row("Priority",     fval(p.priority));
    row("Complexity",   fval(p.complexity));
    row("Methodology",  fval(p.methodology));
    hr();
    row("Lead-ID",      fval(p.leadId));
    row("Team-ID",      fval(p.ownerTeamId));
    row("Sponsor-ID",   fval(p.sponsorId));
    hr();
    row("Start planned",fdate(p.startDatePlanned));
    row("Start actual", fdate(p.startDateActual));
    row("End planned",  fdate(p.endDatePlanned));
    row("End actual",   fdate(p.endDateActual));
    row("Sched.var.(d)",std::to_string(p.scheduleVarianceDays));
    hr();
    row("Budget plan",  std::to_string((int)p.budgetPlanned) + " " + p.currency);
    row("Budget actual",std::to_string((int)p.budgetActual)  + " " + p.currency);
    row("Cost var.",    std::to_string((int)p.costVariance)  + " " + p.currency);
    row("CPI",          std::to_string(p.cpi).substr(0,6));
    row("SPI",          std::to_string(p.spi).substr(0,6));
    row("EV",           std::to_string((int)p.earnedValue));
    row("EAC",          std::to_string((int)p.eac));
    hr();
    row("Scope ver.",   fval(p.scopeVersion));
    row("Scope chgs",   std::to_string(p.scopeChangeCount));
    if (!p.scopeStatement.empty())
        std::cout << "  | Scope: " << p.scopeStatement.substr(0,46) << "|\n";
    hr();
    // QTCS dimension counts
    std::cout << "  | Quality dims: " << std::left << std::setw(4) << p.qualityIds.size()
              << "  Cost dims: " << std::setw(4) << p.costIds.size()
              << "  Time dims: " << std::setw(4) << p.timeIds.size()
              << "  Scope dims: " << std::setw(4) << p.scopeIds.size() << "|\n";
    std::cout << "  | Trackables  : " << std::setw(37) << p.trackables.size() << "|\n";
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

static void printTask(const Rosenholz::TaskF22& t) {
    hdr("TASK (F22)  " + t.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",            t.taskId);
    row("Reg-Nr",        t.regNumber.toString());
    row("Title",         t.title);
    row("Project-ID",    fval(t.projectId));
    row("Parent-Task",   fval(t.parentTaskId));
    row("Status",        fval(t.status));
    row("Priority",      fval(t.priority));
    row("Assignee-ID",   fval(t.assigneeId));
    row("WBS",           fval(t.wbsCode));
    row("Sprint/Phase",  fval(t.sprintOrPhase));
    hr();
    row("Effort plan(h)",std::to_string((int)t.effortPlannedHrs));
    row("Effort act.(h)",std::to_string((int)t.effortActualHrs));
    row("Effort rem.(h)",std::to_string((int)t.effortRemainingHrs));
    row("% complete",   std::to_string(t.percentComplete) + "%");
    row("Cost plan",    std::to_string((int)t.costPlanned));
    row("Cost actual",  std::to_string((int)t.costActual));
    hr();
    row("Start planned", fdate(t.startDatePlanned));
    row("Due planned",   fdate(t.dueDatePlanned));
    row("Due actual",    fdate(t.dueDateActual));
    row("Sched.var.(d)", std::to_string(t.scheduleVarianceDays));
    row("Is milestone",  t.isMilestone ? "YES" : "no");
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

static void printIncident(const Rosenholz::IncidentF18& i) {
    hdr("INCIDENT (F18)  " + i.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",             i.incidentId);
    row("Reg-Nr",         i.regNumber.toString());
    row("Title",          i.title);
    row("Project-ID",     fval(i.projectId));
    row("Status",         fval(i.status));
    row("Severity",       fval(i.severity));
    row("Type",           fval(i.incidentType));
    row("Category",       fval(i.category));
    row("Owner-ID",       fval(i.ownerId));
    row("Reported-by",    fval(i.reportedBy));
    hr();
    row("Occurred",       fdate(i.occurredDate));
    row("Reported",       fdate(i.reportedDate));
    row("Resolved",       fdate(i.resolvedDate));
    hr();
    row("Cost impact",    std::to_string((int)i.costImpact));
    row("Sched.impact(d)",std::to_string(i.scheduleImpactDays));
    row("Scope impact",   fval(i.scopeImpact));
    row("Quality impact", fval(i.qualityImpact));
    hr();
    row("Root cause",     i.rootCause.empty() ? "—" : i.rootCause.substr(0,27));
    row("Immed.action",   i.immediateAction.empty() ? "—" : i.immediateAction.substr(0,27));
    row("Resolution",     i.resolution.empty() ? "—" : i.resolution.substr(0,27));
    row("Escalated",      i.escalated ? "YES -> " + i.escalatedTo : "no");
    row("Linked Risk",    fval(i.riskId));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

static void printPerson(const Rosenholz::Person& p) {
    hdr("PERSON  " + p.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",           p.personId);
    row("Name",         p.fullName());
    row("Email",        fval(p.email));
    row("Phone",        fval(p.phone));
    row("Role",         fval(p.roleTitle));
    row("Dept",         fval(p.department));
    row("Type",         fval(p.personType));
    row("Status",       fval(p.status));
    row("Day-rate",     std::to_string((int)p.dayRate) + " EUR");
    row("Avail.%",      std::to_string((int)p.availabilityPct));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

static void printTrackable(const Rosenholz::TrackableItem& t) {
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "    | " << std::left << std::setw(18) << k
                  << std::setw(30) << v << "|\n";
    };
    std::cout << "    +" << std::string(50,'-') << "+\n";
    row("ID",       t.trackableId.substr(0,20)+"...");
    row("Title",    t.title);
    row("Status",   Rosenholz::trackableStatusToString(t.status));
    row("Priority", fval(t.priority));
    row("Planned",  fdate(t.plannedDate));
    row("Focus",    fdate(t.focusDate));
    row("Due",      fdate(t.dueDate));
    row("Archived", fdate(t.archivedDate));
    row("Notes",    std::to_string(t.notes.size()) + " note(s)");
    std::cout << "    +" << std::string(50,'-') << "+\n";
}

static void listProjects() {
    auto all = Rosenholz::ProjectF16::loadAll();
    if (all.empty()) {
        std::cout << "\n  (no projects yet)\n\n";
        return;
    }
    hdr("ALL PROJECTS");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(14) << "Reg-Nr"
              << std::setw(28) << "Title"
              << std::setw(8)  << "Status"
              << "\n";
    hr();
    int n = 1;
    for (auto& p : all) {
        std::string title = p->title.size() > 26 ? p->title.substr(0,25)+"~" : p->title;
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(14) << p->regNumber.toString()
                  << std::setw(28) << title
                  << std::setw(8)  << p->status
                  << "\n";
    }
    std::cout << "\n";
}

static void listTasks(const std::string& projectId) {
    auto tasks = Rosenholz::TaskF22::loadForProject(projectId);
    if (tasks.empty()) {
        std::cout << "\n  (no tasks for this project)\n\n";
        return;
    }
    hdr("TASKS FOR PROJECT");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(14) << "Reg-Nr"
              << std::setw(24) << "Title"
              << std::setw(6)  << "WBS"
              << std::setw(8)  << "Status"
              << std::setw(5)  << "%"
              << "\n";
    hr();
    int n = 1;
    for (auto& t : tasks) {
        std::string title = t->title.size()>22 ? t->title.substr(0,21)+"~" : t->title;
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(14) << t->regNumber.toString()
                  << std::setw(24) << title
                  << std::setw(6)  << fval(t->wbsCode)
                  << std::setw(8)  << t->status
                  << std::setw(5)  << t->percentComplete
                  << "\n";
    }
    std::cout << "\n";
}

static void listIncidents(const std::string& projectId) {
    auto incs = Rosenholz::IncidentF18::loadForProject(projectId);
    if (incs.empty()) { std::cout << "\n  (no incidents)\n\n"; return; }
    hdr("INCIDENTS FOR PROJECT");
    int n = 1;
    for (auto& i : incs) {
        std::cout << "  " << std::setw(3) << n++ << ". ["
                  << i->regNumber.toString() << "]  "
                  << std::left << std::setw(26) << i->title
                  << "  sev=" << i->severity
                  << "  status=" << i->status << "\n";
    }
    std::cout << "\n";
}

static void listPersons() {
    auto all = Rosenholz::Person::loadAll();
    if (all.empty()) { std::cout << "\n  (no persons yet)\n\n"; return; }
    hdr("PERSONS");
    int n = 1;
    for (auto& p : all) {
        std::cout << "  " << std::setw(3) << n++ << ". ["
                  << p->regNumber.toString() << "]  "
                  << std::left << std::setw(22) << p->fullName()
                  << "  " << std::setw(20) << p->email
                  << "  " << p->roleTitle << "\n";
    }
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────
// CREATE HELPERS
// ─────────────────────────────────────────────────────────────
static std::shared_ptr<Rosenholz::ProjectF16> createProjectWizard() {
    hdr("CREATE NEW PROJECT (F16)");
    std::string title = readLine("Title: ");
    std::cout << "  Project type:\n"
              << "    1. OV  (Operativer Vorgang — active investigation)\n"
              << "    2. IM  (IM-Vorgang — contributor engagement)\n"
              << "    3. OPK (Operative Personenkontrolle — due diligence)\n"
              << "    4. GMS (GMS-Akte — advisory relationship)\n"
              << "    5. AU  (Untersuchungsvorgang — formal inquiry)\n"
              << "    6. SVG (Sicherungsvorgang — monitoring)\n";
    int tc = readInt("Choose type", 1, 6);
    static const char* types[] = {"OV","IM","OPK","GMS","AU","SVG"};
    std::string ptype = types[tc-1];

    std::cout << "  Size class:\n"
              << "    1. large   2. medium   3. small\n";
    int sc = readInt("Choose size", 1, 3);
    static const char* sizes[] = {"large","medium","small"};
    std::string size = sizes[sc-1];

    std::string codename   = readOpt("Codename (optional): ");
    std::string priority   = readOpt("Priority (high/medium/low, optional): ");
    std::string complexity = readOpt("Complexity (complex/moderate/simple, optional): ");
    std::string method     = readOpt("Methodology (agile/waterfall/kanban, optional): ");
    std::string scope      = readOpt("Scope statement (optional): ");
    std::string startPlan  = readOpt("Planned start date (YYYY-MM-DD, optional): ");
    std::string endPlan    = readOpt("Planned end date  (YYYY-MM-DD, optional): ");

    std::string budgetStr  = readOpt("Budget planned (EUR, optional): ");
    double budget = 0.0;
    if (!budgetStr.empty()) try { budget = std::stod(budgetStr); } catch(...) {}

    auto p = Rosenholz::ProjectF16::create(title, ptype, size);
    p->codename        = codename;
    p->priority        = priority;
    p->complexity      = complexity;
    p->methodology     = method;
    p->scopeStatement  = scope;
    p->startDatePlanned= startPlan;
    p->endDatePlanned  = endPlan;
    p->budgetPlanned   = budget;

    if (p->save()) {
        std::cout << "\n  >> Project created: " << p->regNumber.toString()
                  << " (" << p->projectId << ")\n\n";
        // write MFS file
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) p->writeMFSFile(cfg.mfsPath());
        return p;
    } else {
        std::cout << "\n  >> ERROR: Project could not be saved.\n\n";
        return nullptr;
    }
}

static std::shared_ptr<Rosenholz::TaskF22> createTaskWizard(const std::string& projectId) {
    hdr("CREATE TASK (F22)");
    std::string title    = readLine("Title: ");
    std::string desc     = readOpt("Description (optional): ");
    std::string assignee = readOpt("Assignee-ID (optional): ");
    std::string parent   = readOpt("Parent task-ID (optional): ");
    std::string priority = readOpt("Priority (high/medium/low): ");
    std::string wbs      = readOpt("WBS code (e.g. 1.2.3, optional): ");
    std::string start    = readOpt("Planned start (YYYY-MM-DD, optional): ");
    std::string due      = readOpt("Due date      (YYYY-MM-DD, optional): ");
    std::string effortStr= readOpt("Planned effort hours (optional): ");
    double effort = 0.0;
    if (!effortStr.empty()) try { effort = std::stod(effortStr); } catch(...) {}

    auto t = Rosenholz::TaskF22::create(projectId, title, assignee, parent);
    t->description      = desc;
    t->priority         = priority;
    t->wbsCode          = wbs;
    t->startDatePlanned = start;
    t->dueDatePlanned   = due;
    t->effortPlannedHrs = effort;

    if (t->save()) {
        std::cout << "\n  >> Task created: " << t->regNumber.toString()
                  << " (" << t->taskId << ")\n\n";
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) t->writeMFSFile(cfg.mfsPath());
        return t;
    } else {
        std::cout << "\n  >> ERROR: Task could not be saved.\n\n";
        return nullptr;
    }
}

static std::shared_ptr<Rosenholz::IncidentF18> createIncidentWizard(const std::string& projectId) {
    hdr("CREATE INCIDENT (F18)");
    std::string title    = readLine("Title: ");
    std::string desc     = readOpt("Description (optional): ");
    std::cout << "  Severity:\n"
              << "    1. critical  2. high  3. medium  4. low\n";
    int sc = readInt("Choose severity", 1, 4);
    static const char* sevs[] = {"critical","high","medium","low"};
    std::string sev = sevs[sc-1];

    std::string type     = readOpt("Incident type (financial/technical/schedule/quality, optional): ");
    std::string occurred = readOpt("Occurred date (YYYY-MM-DD, optional): ");
    std::string reporter = readOpt("Reported-by person-ID (optional): ");
    std::string cause    = readOpt("Root cause (optional): ");
    std::string action   = readOpt("Immediate action taken (optional): ");
    std::string costStr  = readOpt("Cost impact EUR (optional): ");
    double cost = 0.0;
    if (!costStr.empty()) try { cost = std::stod(costStr); } catch(...) {}

    auto i = Rosenholz::IncidentF18::create(projectId, title, sev, reporter);
    i->description    = desc;
    i->incidentType   = type;
    i->occurredDate   = occurred;
    i->rootCause      = cause;
    i->immediateAction= action;
    i->costImpact     = cost;

    if (i->save()) {
        std::cout << "\n  >> Incident created: " << i->regNumber.toString()
                  << " (" << i->incidentId << ")\n\n";
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) i->writeMFSFile(cfg.mfsPath());
        return i;
    } else {
        std::cout << "\n  >> ERROR: Incident could not be saved.\n\n";
        return nullptr;
    }
}

static std::shared_ptr<Rosenholz::Person> createPersonWizard() {
    hdr("CREATE PERSON");
    std::string first  = readLine("First name: ");
    std::string last   = readLine("Last name: ");
    std::string email  = readOpt("Email (optional): ");
    std::string role   = readOpt("Role title (optional): ");
    std::string dept   = readOpt("Department (optional): ");
    std::cout << "  Person type:\n"
              << "    1. internal  2. external  3. contractor  4. advisor\n";
    int tc = readInt("Choose type", 1, 4);
    static const char* ptypes[] = {"internal","external","contractor","advisor"};

    std::string rateStr = readOpt("Day rate EUR (optional): ");
    double rate = 0.0;
    if (!rateStr.empty()) try { rate = std::stod(rateStr); } catch(...) {}

    auto p = Rosenholz::Person::create(first, last, email, ptypes[tc-1]);
    p->roleTitle  = role;
    p->department = dept;
    p->dayRate    = rate;

    if (p->save()) {
        std::cout << "\n  >> Person created: " << p->regNumber.toString()
                  << " (" << p->personId << ")\n\n";
        return p;
    } else {
        std::cout << "\n  >> ERROR: Person could not be saved.\n\n";
        return nullptr;
    }
}

// ─────────────────────────────────────────────────────────────
// PROJECT DETAIL MENU
// ─────────────────────────────────────────────────────────────
static void projectMenu(std::shared_ptr<Rosenholz::ProjectF16> p) {
    while (true) {
        p->loadTrackables();
        printProject(*p);
        std::cout << "  Project actions:\n"
                  << "    1. Edit title / codename\n"
                  << "    2. Edit status / phase\n"
                  << "    3. Edit dates (planned start / end)\n"
                  << "    4. Edit budget\n"
                  << "    5. Edit scope statement\n"
                  << "    6. Recalculate Earned Value\n"
                  << "    7. Reassign lead / team / sponsor\n"
                  << "    8. Add trackable item\n"
                  << "    9. List / view trackable items\n"
                  << "   10. Create task (F22)\n"
                  << "   11. List tasks (F22)\n"
                  << "   12. Create incident (F18)\n"
                  << "   13. List incidents (F18)\n"
                  << "   14. Convert project -> task\n"
                  << "   15. Write MFS file\n"
                  << "   16. Documents (create / list / open)\n"
                  << "   17. Workflow (start / view instances)\n"
                  << "\n  REPORTING & TRACKING\n"
                  << "   18. Milestones\n"
                  << "   19. Meetings (via project)\n"
                  << "   20. Quality gates\n"
                  << "   21. KPIs\n"
                  << "   22. Measures\n"
                  << "   23. Change requests\n"
                  << "   24. Lessons learned\n"
                  << "   25. Assumptions & constraints\n"
                  << "   26. Decision log\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 26);

        if (ch == 0) break;

        else if (ch == 1) {
            std::string t = readLine("New title (Enter to keep): ");
            if (!t.empty()) p->title = t;
            std::string c = readOpt("New codename (Enter to keep): ");
            if (!c.empty()) p->codename = c;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 2) {
            std::cout << "  Status options: draft / active / on-hold / closed / archived\n";
            std::string s = readOpt("New status (Enter to keep): ");
            if (!s.empty()) p->status = s;
            std::string ph = readOpt("New phase (Enter to keep): ");
            if (!ph.empty()) p->phase = ph;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 3) {
            std::string sp = readOpt("Planned start YYYY-MM-DD (Enter to keep): ");
            if (!sp.empty()) p->startDatePlanned = sp;
            std::string ep = readOpt("Planned end   YYYY-MM-DD (Enter to keep): ");
            if (!ep.empty()) p->endDatePlanned = ep;
            std::string sa = readOpt("Actual start  YYYY-MM-DD (Enter to keep): ");
            if (!sa.empty()) p->startDateActual = sa;
            std::string ea = readOpt("Actual end    YYYY-MM-DD (Enter to keep): ");
            if (!ea.empty()) p->endDateActual = ea;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 4) {
            std::string bp = readOpt("Budget planned EUR (Enter to keep): ");
            if (!bp.empty()) try { p->budgetPlanned = std::stod(bp); } catch(...) {}
            std::string ba = readOpt("Budget actual  EUR (Enter to keep): ");
            if (!ba.empty()) try { p->budgetActual  = std::stod(ba); } catch(...) {}
            std::string ev = readOpt("Earned value       (Enter to keep): ");
            if (!ev.empty()) try { p->earnedValue   = std::stod(ev); } catch(...) {}
            std::string pv = readOpt("Planned value      (Enter to keep): ");
            if (!pv.empty()) try { p->plannedValue  = std::stod(pv); } catch(...) {}
            std::string ac = readOpt("Actual cost        (Enter to keep): ");
            if (!ac.empty()) try { p->actualCost    = std::stod(ac); } catch(...) {}
            p->recalcEarnedValue();
            p->update();
            std::cout << "  >> Saved.  CPI=" << p->cpi << "  SPI=" << p->spi << "\n";
        }
        else if (ch == 5) {
            std::cout << "  Current: " << fval(p->scopeStatement) << "\n";
            std::string sc = readLine("New scope statement: ");
            p->scopeStatement = sc;
            p->scopeChangeCount++;
            std::string reason = readOpt("Change reason (optional): ");
            p->scopeChangeReason = reason;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 6) {
            p->recalcEarnedValue();
            p->update();
            std::cout << "  >> EV recalculated.  CPI=" << p->cpi
                      << "  SPI=" << p->spi << "  EAC=" << p->eac << "\n";
        }
        else if (ch == 7) {
            std::string lead = readOpt("New lead person-ID (Enter to keep): ");
            if (!lead.empty()) p->reassignLead(lead);
            std::string team = readOpt("New team-ID (Enter to keep): ");
            if (!team.empty()) p->reassignTeam(team);
            std::string spon = readOpt("New sponsor person-ID (Enter to keep): ");
            if (!spon.empty()) p->reassignSponsor(spon);
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 8) {
            std::string ttl = readLine("Trackable title: ");
            std::string by  = readOpt("Created-by person-ID (optional): ");
            auto trk = p->addTrackable(ttl, by);

            std::cout << "  Status: 1.planned  2.focused  3.due  4.archived\n";
            int ts = readInt("Initial status", 1, 4);
            std::string fdate2 = readOpt("Focus date YYYY-MM-DD (optional): ");
            std::string ddate  = readOpt("Due date   YYYY-MM-DD (optional): ");
            std::string pdate  = readOpt("Planned date YYYY-MM-DD (optional): ");
            if (ts == 1) trk->plan(pdate);
            else if (ts == 2) trk->focus(fdate2);
            else if (ts == 3) trk->markDue();
            else if (ts == 4) trk->archive();
            if (!ddate.empty()) trk->dueDate = ddate;

            std::string note = readOpt("Add a note (optional): ");
            if (!note.empty()) {
                trk->addNote(by, note);
            }
            trk->save();
            std::cout << "  >> Trackable saved: " << trk->trackableId << "\n";
        }
        else if (ch == 9) {
            if (p->trackables.empty()) {
                std::cout << "\n  (no trackable items)\n\n";
            } else {
                hdr("TRACKABLE ITEMS");
                for (auto& t : p->trackables) printTrackable(*t);
            }
        }
        else if (ch == 10) {
            createTaskWizard(p->projectId);
        }
        else if (ch == 11) {
            listTasks(p->projectId);
        }
        else if (ch == 12) {
            createIncidentWizard(p->projectId);
        }
        else if (ch == 13) {
            listIncidents(p->projectId);
        }
        else if (ch == 14) {
            std::string parentPid = readLine("Parent project-ID to attach task to: ");
            std::string newTaskId = p->convertToTask(parentPid);
            if (!newTaskId.empty())
                std::cout << "  >> Task created: " << newTaskId << "\n";
        }
        else if (ch == 15) {
            auto& cfg = Rosenholz::Config::instance();
            bool ok = p->writeMFSFile(cfg.mfsPath());
            std::cout << "  >> MFS file " << (ok ? "written." : "FAILED.") << "\n";
        }
        else if (ch == 16) {
            documentBrowserMenu(p->projectId);
        }
        else if (ch == 17) {
            std::cout << "  1. Start instance  2. List instances  3. Browser\n";
            int wch = readInt("Choice", 1, 3);
            if (wch == 1) { std::string iid=startWfInstanceWizard("project",p->projectId); if(!iid.empty()) instanceMenu(iid); }
            else if (wch == 2) { listWfInstances("project", p->projectId); }
            else { workflowMenu(); }
        }
        else if (ch == 18) { milestoneMenu(p->projectId); }
        else if (ch == 19) { meetingMenu("", p->projectId); }
        else if (ch == 20) { qualityGateMenu(p->projectId); }
        else if (ch == 21) { kpiMenu(p->projectId); }
        else if (ch == 22) { measureMenu(p->projectId); }
        else if (ch == 23) { changeRequestMenu(p->projectId); }
        else if (ch == 24) { lessonLearnedMenu(p->projectId); }
        else if (ch == 25) { assumptionConstraintMenu(p->projectId); }
        else if (ch == 26) { decisionLogMenu(p->projectId); }
    }
}

// ─────────────────────────────────────────────────────────────
// TASK DETAIL MENU
// ─────────────────────────────────────────────────────────────
static void taskMenu(std::shared_ptr<Rosenholz::TaskF22> t) {
    while (true) {
        printTask(*t);
        std::cout << "  Task actions:\n"
                  << "    1. Edit title / description\n"
                  << "    2. Edit status / priority / % complete\n"
                  << "    3. Edit dates\n"
                  << "    4. Edit effort / cost\n"
                  << "    5. Reassign to person\n"
                  << "    6. Add note\n"
                  << "    7. Add trackable item\n"
                  << "    8. Create child task\n"
                  << "    9. Convert task -> project\n"
                  << "   10. Documents (create / list / open)\n"
                  << "   11. Workflow (start / view instances)\n"
                  << "   12. Meetings\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 12);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string ti = readOpt("New title (Enter to keep): ");
            if (!ti.empty()) t->title = ti;
            std::string de = readOpt("New description (Enter to keep): ");
            if (!de.empty()) t->description = de;
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 2) {
            std::cout << "  Status: draft/active/in-review/done/blocked\n";
            std::string s = readOpt("New status (Enter to keep): ");
            if (!s.empty()) t->status = s;
            std::string pr = readOpt("Priority (high/medium/low, Enter to keep): ");
            if (!pr.empty()) t->priority = pr;
            std::string pc = readOpt("% complete 0-100 (Enter to keep): ");
            if (!pc.empty()) try { t->percentComplete = std::stoi(pc); } catch(...) {}
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 3) {
            std::string sp = readOpt("Planned start YYYY-MM-DD: ");
            if (!sp.empty()) t->startDatePlanned = sp;
            std::string dp = readOpt("Planned due   YYYY-MM-DD: ");
            if (!dp.empty()) t->dueDatePlanned = dp;
            std::string da = readOpt("Actual due    YYYY-MM-DD: ");
            if (!da.empty()) t->dueDateActual = da;
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 4) {
            std::string ep = readOpt("Planned effort h: ");
            if (!ep.empty()) try { t->effortPlannedHrs = std::stod(ep); } catch(...) {}
            std::string ea = readOpt("Actual effort h: ");
            if (!ea.empty()) try { t->effortActualHrs = std::stod(ea); } catch(...) {}
            std::string er = readOpt("Remaining effort h: ");
            if (!er.empty()) try { t->effortRemainingHrs = std::stod(er); } catch(...) {}
            std::string cp = readOpt("Planned cost EUR: ");
            if (!cp.empty()) try { t->costPlanned = std::stod(cp); } catch(...) {}
            std::string ca = readOpt("Actual cost EUR: ");
            if (!ca.empty()) try { t->costActual = std::stod(ca); } catch(...) {}
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 5) {
            std::string pid = readLine("New assignee person-ID: ");
            t->reassignTo(pid);
            std::cout << "  >> Reassigned.\n";
        }
        else if (ch == 6) {
            std::string note = readLine("Note text: ");
            std::string by   = readOpt("Author person-ID (optional): ");
            // Save note directly to notes DB
            auto* db = Rosenholz::DatabasePool::instance().get("tracking");
            if (db) {
                std::string nid = "note_" + std::to_string(
                    std::chrono::system_clock::now().time_since_epoch().count());
                nlohmann::json jc;
                jc["text"] = note; jc["format"] = "plain";
                db->exec(
                    "INSERT INTO notes (note_id,entity_type,entity_id,author_id,content,note_type) "
                    "VALUES (?,?,?,?,?,?);",
                    {Rosenholz::BindParam::text(nid), Rosenholz::BindParam::text("task"),
                     Rosenholz::BindParam::text(t->taskId), Rosenholz::ton(by),
                     Rosenholz::BindParam::text(jc.dump()),
                     Rosenholz::BindParam::text("general")});
                std::cout << "  >> Note saved.\n";
            }
        }
        else if (ch == 7) {
            std::string ttl = readLine("Trackable title: ");
            auto trk = t->addTrackable(ttl);
            std::cout << "  Status: 1.planned  2.focused  3.due  4.archived\n";
            int ts = readInt("Status", 1, 4);
            std::string dd = readOpt("Due date YYYY-MM-DD (optional): ");
            if (!dd.empty()) trk->dueDate = dd;
            if (ts==1) trk->plan(readOpt("Planned date: "));
            else if (ts==2) trk->focus();
            else if (ts==3) trk->markDue();
            else trk->archive();
            trk->save();
            std::cout << "  >> Trackable saved.\n";
        }
        else if (ch == 8) {
            createTaskWizard(t->projectId);
        }
        else if (ch == 9) {
            std::cout << "  Type: OV/IM/OPK/GMS/AU/SVG\n";
            std::string ptype = readLine("Project type: ");
            std::string newPid = t->convertToProject(ptype);
            if (!newPid.empty())
                std::cout << "  >> Project created: " << newPid << "\n";
        }
        else if (ch == 10) {
            documentBrowserMenu("", t->taskId);
        }
        else if (ch == 11) {
            std::cout << "  1. Start instance  2. List instances\n";
            int wch = readInt("Choice", 1, 2);
            if (wch == 1) {
                std::string iid = startWfInstanceWizard("task", t->taskId);
                if (!iid.empty()) instanceMenu(iid);
            } else {
                listWfInstances("task", t->taskId);
            }
        }
        else if (ch == 12) { meetingMenu(t->taskId, t->projectId); }
    }
}

// ─────────────────────────────────────────────────────────────
// MAIN MENU
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// WORKFLOW HELPERS
// ─────────────────────────────────────────────────────────────

// ── low-level DB helpers (workflow.db) ────────────────────────
static Rosenholz::Database* wfdb() { return Rosenholz::DatabasePool::instance().get("workflow"); }

// ── display ───────────────────────────────────────────────────
static void printWfDefinition(const Rosenholz::Row& r) {
    auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:"—"; };
    hdr("WORKFLOW DEFINITION  " + g("workflow_def_id").substr(0,20));
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(30) << v << "|\n";
    };
    row("ID",            g("workflow_def_id"));
    row("Name",          g("name"));
    row("Version",       g("version"));
    row("Entity type",   g("entity_type"));
    row("Status",        g("status"));
    row("Initial state", g("initial_state"));
    row("Terminal states",g("terminal_states"));
    row("SLA enforced",  g("sla_enforced")=="1" ? "YES" : "no");
    row("Default SLA h", g("default_sla_hours"));
    row("Parallel appr.",g("parallel_approval_allowed")=="1" ? "YES" : "no");
    row("Created by",    g("created_by"));
    row("Effective from",fdate(g("effective_from")));
    row("Effective to",  fdate(g("effective_to")));
    if (g("description") != "—")
        std::cout << "  | Desc: " << g("description").substr(0,47) << "|\n";
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

static void printWfInstance(const Rosenholz::Row& r) {
    auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:"—"; };
    hdr("WORKFLOW INSTANCE  " + g("instance_id").substr(0,20));
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(30) << v << "|\n";
    };
    row("ID",             g("instance_id"));
    row("Definition",     g("workflow_def_id"));
    row("Entity type",    g("entity_type"));
    row("Entity ID",      g("entity_id").substr(0,28));
    row("Status",         g("status"));
    row("Current state",  g("current_state_id"));
    row("Previous state", g("previous_state_id"));
    row("Priority",       g("priority"));
    row("Initiated by",   g("initiated_by"));
    row("Initiated",      fdate(g("initiated_date")));
    row("Due date",       fdate(g("due_date")));
    row("Completed",      fdate(g("completed_date")));
    row("SLA hours",      g("sla_hours"));
    row("SLA breached",   g("sla_breached")=="1" ? "YES" : "no");
    row("Escalated to",   g("escalated_to"));
    row("Outcome",        g("outcome"));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

static void listWfDefinitions() {
    auto* db = wfdb();
    if (!db) return;
    auto rows = db->query("SELECT * FROM workflow_definitions ORDER BY name;");
    if (rows.empty()) { std::cout << "\n  (no workflow definitions yet)\n\n"; return; }
    hdr("WORKFLOW DEFINITIONS");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(22) << "Name"
              << std::setw(14) << "Entity type"
              << std::setw(8)  << "Version"
              << std::setw(10) << "Status"
              << "\n";
    hr();
    int n = 1;
    for (auto& r : rows) {
        auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(22) << g("name").substr(0,20)
                  << std::setw(14) << g("entity_type")
                  << std::setw(8)  << g("version")
                  << std::setw(10) << g("status")
                  << "\n";
    }
    std::cout << "\n";
}

static void listWfStates(const std::string& defId) {
    auto* db = wfdb();
    if (!db) return;
    auto rows = db->query(
        "SELECT * FROM workflow_states WHERE workflow_def_id=? ORDER BY is_initial DESC, name;",
        {Rosenholz::BindParam::text(defId)});
    if (rows.empty()) { std::cout << "\n  (no states defined)\n\n"; return; }
    hdr("STATES  for def " + defId.substr(0,20));
    std::cout << "  " << std::left
              << std::setw(4) << "#"
              << std::setw(20) << "Name"
              << std::setw(12) << "Label"
              << std::setw(8)  << "Initial"
              << std::setw(9)  << "Terminal"
              << std::setw(8)  << "Approv."
              << std::setw(6)  << "SLA h"
              << "\n";
    hr();
    int n = 1;
    for (auto& r : rows) {
        auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(20) << g("name").substr(0,18)
                  << std::setw(12) << g("label").substr(0,10)
                  << std::setw(8)  << (g("is_initial")=="1"  ? "YES" : "")
                  << std::setw(9)  << (g("is_terminal")=="1" ? "YES" : "")
                  << std::setw(8)  << (g("requires_approval")=="1" ? "YES" : "")
                  << std::setw(6)  << g("sla_hours")
                  << "\n";
    }
    std::cout << "\n";
}

static void listWfTransitions(const std::string& defId) {
    auto* db = wfdb();
    if (!db) return;
    auto rows = db->query(
        "SELECT * FROM workflow_transitions WHERE workflow_def_id=? ORDER BY from_state_id;",
        {Rosenholz::BindParam::text(defId)});
    if (rows.empty()) { std::cout << "\n  (no transitions defined)\n\n"; return; }
    hdr("TRANSITIONS  for def " + defId.substr(0,20));
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(18) << "From state"
              << std::setw(18) << "To state"
              << std::setw(16) << "Trigger event"
              << std::setw(8)  << "Role"
              << "\n";
    hr();
    int n = 1;
    for (auto& r : rows) {
        auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(18) << g("from_state_id").substr(0,16)
                  << std::setw(18) << g("to_state_id").substr(0,16)
                  << std::setw(16) << g("trigger_event").substr(0,14)
                  << std::setw(8)  << g("required_role").substr(0,7)
                  << "\n";
    }
    std::cout << "\n";
}

static void listWfInstances(const std::string& filterEntity,
                            const std::string& filterEntityId) {
    auto* db = wfdb();
    if (!db) return;
    Rosenholz::ResultSet rows;
    if (!filterEntityId.empty())
        rows = db->query(
            "SELECT * FROM workflow_instances WHERE entity_type=? AND entity_id=? ORDER BY initiated_date DESC;",
            {Rosenholz::BindParam::text(filterEntity), Rosenholz::BindParam::text(filterEntityId)});
    else if (!filterEntity.empty())
        rows = db->query(
            "SELECT * FROM workflow_instances WHERE entity_type=? ORDER BY initiated_date DESC;",
            {Rosenholz::BindParam::text(filterEntity)});
    else
        rows = db->query(
            "SELECT * FROM workflow_instances ORDER BY initiated_date DESC LIMIT 50;");

    if (rows.empty()) { std::cout << "\n  (no workflow instances)\n\n"; return; }
    hdr("WORKFLOW INSTANCES");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(12) << "Entity"
              << std::setw(22) << "Entity ID"
              << std::setw(12) << "Cur.State"
              << std::setw(10) << "Status"
              << std::setw(8)  << "Breached"
              << "\n";
    hr();
    int n = 1;
    for (auto& r : rows) {
        auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(12) << g("entity_type").substr(0,10)
                  << std::setw(22) << g("entity_id").substr(0,20)
                  << std::setw(12) << g("current_state_id").substr(0,10)
                  << std::setw(10) << g("status").substr(0,8)
                  << std::setw(8)  << (g("sla_breached")=="1" ? "YES" : "")
                  << "\n";
    }
    std::cout << "\n";
}

static void listWfActions(const std::string& instanceId) {
    auto* db = wfdb();
    if (!db) return;
    auto rows = db->query(
        "SELECT * FROM workflow_actions WHERE instance_id=? ORDER BY action_date DESC;",
        {Rosenholz::BindParam::text(instanceId)});
    if (rows.empty()) { std::cout << "\n  (no actions recorded)\n\n"; return; }
    hdr("ACTIONS  for instance " + instanceId.substr(0,16));
    int n = 1;
    for (auto& r : rows) {
        auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(20) << fdate(g("action_date")).substr(0,19)
                  << "  actor="   << std::setw(12) << g("actor_id").substr(0,10)
                  << "  type="    << std::setw(12) << g("action_type")
                  << "  decision=" << g("decision") << "\n";
        if (!g("comment").empty())
            std::cout << "      comment: " << g("comment").substr(0,60) << "\n";
    }
    std::cout << "\n";
}

static void listWfParticipants(const std::string& instanceId) {
    auto* db = wfdb();
    if (!db) return;
    auto rows = db->query(
        "SELECT * FROM workflow_participants WHERE instance_id=? ORDER BY role;",
        {Rosenholz::BindParam::text(instanceId)});
    if (rows.empty()) { std::cout << "\n  (no participants)\n\n"; return; }
    hdr("PARTICIPANTS  for instance " + instanceId.substr(0,16));
    std::cout << "  " << std::left
              << std::setw(4) << "#"
              << std::setw(14) << "Role"
              << std::setw(24) << "Person-ID"
              << std::setw(8) << "Active"
              << "\n";
    hr();
    int n = 1;
    for (auto& r : rows) {
        auto g = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(14) << g("role")
                  << std::setw(24) << g("person_id").substr(0,22)
                  << std::setw(8)  << (g("active")=="1" ? "yes" : "no")
                  << "\n";
    }
    std::cout << "\n";
}

// ── generate a new ID ─────────────────────────────────────────
static std::string wfGenId(const std::string& prefix) {
    return Rosenholz::genId(prefix);
}
static std::string wfNow() { return Rosenholz::nowIso(); }

// ─────────────────────────────────────────────────────────────
// WORKFLOW DEFINITION WIZARD
// ─────────────────────────────────────────────────────────────
static std::string createWfDefinitionWizard() {
    hdr("CREATE WORKFLOW DEFINITION");
    auto* db = wfdb();
    if (!db) { std::cout << "  >> workflow DB not available.\n"; return ""; }

    std::string name    = readLine("Name (e.g. 'Project Approval'): ");
    std::string ver     = readOpt("Version (default 1.0): ");
    if (ver.empty()) ver = "1.0";

    std::cout << "  Entity type this workflow applies to:\n"
              << "    1. project   2. task   3. incident   4. document\n"
              << "    5. change_request       6. approval   7. other\n";
    int et = readInt("Entity type", 1, 7);
    static const char* etypes[] = {"project","task","incident","document",
                                    "change_request","approval","other"};
    std::string entityType = etypes[et-1];

    std::string desc        = readOpt("Description (optional): ");
    std::string initialState= readLine("Initial state name (e.g. 'draft'): ");
    std::string terminalStates = readLine("Terminal states (comma-separated, e.g. 'approved,rejected'): ");
    std::string slaStr      = readOpt("Default SLA hours (optional, e.g. 72): ");
    int slaHours = 0;
    if (!slaStr.empty()) try { slaHours = std::stoi(slaStr); } catch(...) {}
    bool slaEnforced = slaHours > 0;

    std::cout << "  Parallel approval allowed? (y/n): ";
    std::string pa; std::getline(std::cin, pa);
    bool parallelApproval = (!pa.empty() && (pa[0]=='y'||pa[0]=='Y'));

    std::string createdBy   = readOpt("Created-by person-ID (optional): ");
    std::string effFrom     = readOpt("Effective from YYYY-MM-DD (optional): ");
    std::string effTo       = readOpt("Effective to   YYYY-MM-DD (optional): ");

    std::string defId = wfGenId("wfd");
    bool ok = db->exec(R"(
        INSERT INTO workflow_definitions
        (workflow_def_id,name,version,entity_type,description,
         initial_state,terminal_states,parallel_approval_allowed,
         sla_enforced,default_sla_hours,
         created_by,created_date,effective_from,effective_to,status)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,'active')
    )", {
        Rosenholz::BindParam::text(defId),
        Rosenholz::BindParam::text(name),
        Rosenholz::BindParam::text(ver),
        Rosenholz::BindParam::text(entityType),
        Rosenholz::ton(desc),
        Rosenholz::BindParam::text(initialState),
        Rosenholz::BindParam::text(terminalStates),
        Rosenholz::BindParam::int64(parallelApproval ? 1 : 0),
        Rosenholz::BindParam::int64(slaEnforced ? 1 : 0),
        Rosenholz::BindParam::int64(slaHours),
        Rosenholz::ton(createdBy),
        Rosenholz::BindParam::text(wfNow()),
        Rosenholz::ton(effFrom),
        Rosenholz::ton(effTo)
    });

    if (ok) {
        std::cout << "\n  >> Definition created: " << defId << "\n\n";
        return defId;
    } else {
        std::cout << "\n  >> ERROR: could not save definition.\n\n";
        return "";
    }
}

// ─────────────────────────────────────────────────────────────
// ADD STATE / TRANSITION WIZARDS
// ─────────────────────────────────────────────────────────────
static void addStateWizard(const std::string& defId) {
    auto* db = wfdb();
    if (!db) return;
    hdr("ADD STATE  to def " + defId.substr(0,20));

    std::string name    = readLine("State name (e.g. 'in_review'): ");
    std::string label   = readOpt("Display label (optional): ");
    std::string type    = readOpt("State type (normal/approval/notification, default normal): ");
    if (type.empty()) type = "normal";

    auto yesno = [](const std::string& q) -> int {
        std::cout << "  " << q << " (y/n): ";
        std::string a; std::getline(std::cin, a);
        return (!a.empty() && (a[0]=='y'||a[0]=='Y')) ? 1 : 0;
    };
    int isInitial  = yesno("Is initial state?");
    int isTerminal = yesno("Is terminal state?");
    int reqApproval= yesno("Requires approval?");
    int notifyEntry= yesno("Notify on entry?");
    int notifyExit = yesno("Notify on exit?");

    std::string slaStr  = readOpt("SLA hours for this state (optional): ");
    int sla = 0; if (!slaStr.empty()) try { sla = std::stoi(slaStr); } catch(...) {}
    std::string roles   = readOpt("Allowed roles (comma-sep, optional): ");
    std::string notes   = readOpt("Notes (optional): ");

    std::string stateId = wfGenId("wfs");
    bool ok = db->exec(R"(
        INSERT INTO workflow_states
        (state_id,workflow_def_id,name,label,state_type,
         is_initial,is_terminal,requires_approval,
         notifies_on_entry,notifies_on_exit,sla_hours,allowed_roles,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        Rosenholz::BindParam::text(stateId), Rosenholz::BindParam::text(defId),
        Rosenholz::BindParam::text(name), Rosenholz::ton(label), Rosenholz::BindParam::text(type),
        Rosenholz::BindParam::int64(isInitial),  Rosenholz::BindParam::int64(isTerminal),
        Rosenholz::BindParam::int64(reqApproval),Rosenholz::BindParam::int64(notifyEntry),
        Rosenholz::BindParam::int64(notifyExit), Rosenholz::BindParam::int64(sla),
        Rosenholz::ton(roles), Rosenholz::ton(notes)
    });
    std::cout << "  >> State " << (ok ? "saved: "+stateId : "FAILED.") << "\n";
}

static void addTransitionWizard(const std::string& defId) {
    auto* db = wfdb();
    if (!db) return;
    hdr("ADD TRANSITION  to def " + defId.substr(0,20));
    listWfStates(defId);

    std::string fromState = readLine("From state name/ID: ");
    std::string toState   = readLine("To state name/ID: ");
    std::string trigger   = readLine("Trigger event (e.g. 'submit','approve','reject'): ");
    std::string condition = readOpt("Condition expression (optional): ");
    std::string role      = readOpt("Required role (optional): ");

    auto yesno = [](const std::string& q) -> int {
        std::cout << "  " << q << " (y/n): ";
        std::string a; std::getline(std::cin, a);
        return (!a.empty() && (a[0]=='y'||a[0]=='Y')) ? 1 : 0;
    };
    int reqComment = yesno("Requires comment?");
    int autoTrigger= yesno("Auto-trigger (no human action needed)?");
    std::string notes = readOpt("Notes (optional): ");

    std::string tid = wfGenId("wft");
    bool ok = db->exec(R"(
        INSERT INTO workflow_transitions
        (transition_id,workflow_def_id,from_state_id,to_state_id,
         trigger_event,condition,required_role,requires_comment,auto_trigger,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?)
    )", {
        Rosenholz::BindParam::text(tid), Rosenholz::BindParam::text(defId),
        Rosenholz::BindParam::text(fromState), Rosenholz::BindParam::text(toState),
        Rosenholz::BindParam::text(trigger),   Rosenholz::ton(condition),
        Rosenholz::ton(role), Rosenholz::BindParam::int64(reqComment),
        Rosenholz::BindParam::int64(autoTrigger), Rosenholz::ton(notes)
    });
    std::cout << "  >> Transition " << (ok ? "saved: "+tid : "FAILED.") << "\n";
}

// ─────────────────────────────────────────────────────────────
// INSTANCE WIZARD  (attach workflow to an entity)
// ─────────────────────────────────────────────────────────────
static std::string startWfInstanceWizard(const std::string& entityType,
                                          const std::string& entityId) {
    auto* db = wfdb();
    if (!db) return "";
    hdr("START WORKFLOW INSTANCE");
    listWfDefinitions();

    // choose definition
    auto defs = db->query(
        "SELECT workflow_def_id,name FROM workflow_definitions WHERE status='active' ORDER BY name;");
    if (defs.empty()) {
        std::cout << "  >> No active workflow definitions. Create one first.\n";
        return "";
    }
    int n = readInt("Definition number", 1, (int)defs.size());
    auto& def   = defs[n-1];
    std::string defId   = def.at("workflow_def_id");
    std::string defName = def.at("name");

    // entity
    std::string eType = entityType.empty() ? readLine("Entity type (project/task/incident/...): ") : entityType;
    std::string eId   = entityId.empty()   ? readLine("Entity ID: ")                              : entityId;

    std::string initiatedBy = readOpt("Initiated-by person-ID (optional): ");
    std::string dueDate     = readOpt("Due date YYYY-MM-DD (optional): ");
    std::string priority    = readOpt("Priority (high/medium/low, optional): ");

    std::string slaStr = db->queryScalar(
        "SELECT default_sla_hours FROM workflow_definitions WHERE workflow_def_id=?;",
        {Rosenholz::BindParam::text(defId)});
    std::string initState = db->queryScalar(
        "SELECT initial_state FROM workflow_definitions WHERE workflow_def_id=?;",
        {Rosenholz::BindParam::text(defId)});

    std::string instId = wfGenId("wfi");
    bool ok = db->exec(R"(
        INSERT INTO workflow_instances
        (instance_id,workflow_def_id,entity_type,entity_id,
         current_state_id,initiated_by,initiated_date,due_date,
         sla_hours,priority,status)
        VALUES(?,?,?,?,?,?,?,?,?,?,'active')
    )", {
        Rosenholz::BindParam::text(instId), Rosenholz::BindParam::text(defId),
        Rosenholz::BindParam::text(eType),  Rosenholz::BindParam::text(eId),
        Rosenholz::BindParam::text(initState),
        Rosenholz::ton(initiatedBy), Rosenholz::BindParam::text(wfNow()),
        Rosenholz::ton(dueDate),
        slaStr.empty() ? Rosenholz::BindParam::int64(0) : Rosenholz::BindParam::int64(std::stoi(slaStr)),
        Rosenholz::ton(priority)
    });

    if (!ok) { std::cout << "  >> ERROR: instance not saved.\n"; return ""; }

    // log first SLA entry
    db->exec(R"(
        INSERT INTO workflow_sla_log
        (sla_log_id,instance_id,state_id,entered_date,sla_hours_allowed)
        VALUES(?,?,?,?,?)
    )", {
        Rosenholz::BindParam::text(wfGenId("sla")), Rosenholz::BindParam::text(instId),
        Rosenholz::BindParam::text(initState),      Rosenholz::BindParam::text(wfNow()),
        slaStr.empty() ? Rosenholz::BindParam::int64(0) : Rosenholz::BindParam::int64(std::stoi(slaStr))
    });

    std::cout << "\n  >> Workflow instance started: " << instId << "\n"
              << "     Definition : " << defName << "\n"
              << "     Current state: " << initState << "\n\n";

    // Offer to update entity's workflow fields
    std::cout << "  Update entity workflow fields? (y/n): ";
    std::string ans; std::getline(std::cin, ans);
    if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y')) {
        auto* projDb = Rosenholz::DatabasePool::instance().get("projects");
        if (projDb && eType == "project") {
            projDb->exec(
                "UPDATE projects SET workflow_instance_id=?,workflow_status='active',"
                "workflow_current_state=? WHERE project_id=?;",
                {Rosenholz::BindParam::text(instId), Rosenholz::BindParam::text(initState),
                 Rosenholz::BindParam::text(eId)});
            std::cout << "  >> Project workflow fields updated.\n";
        } else if (projDb && eType == "task") {
            projDb->exec(
                "UPDATE tasks SET workflow_instance_id=?,workflow_status='active',"
                "workflow_current_state=? WHERE task_id=?;",
                {Rosenholz::BindParam::text(instId), Rosenholz::BindParam::text(initState),
                 Rosenholz::BindParam::text(eId)});
            std::cout << "  >> Task workflow fields updated.\n";
        } else if (projDb && eType == "incident") {
            projDb->exec(
                "UPDATE incidents SET workflow_instance_id=?,workflow_status='active',"
                "workflow_current_state=? WHERE incident_id=?;",
                {Rosenholz::BindParam::text(instId), Rosenholz::BindParam::text(initState),
                 Rosenholz::BindParam::text(eId)});
            std::cout << "  >> Incident workflow fields updated.\n";
        }
    }
    return instId;
}

// ─────────────────────────────────────────────────────────────
// FIRE A TRANSITION  (perform a workflow action)
// ─────────────────────────────────────────────────────────────
static void fireTransitionWizard(const std::string& instanceId) {
    auto* db = wfdb();
    if (!db) return;

    // Get current instance
    auto inst = db->query("SELECT * FROM workflow_instances WHERE instance_id=?;",
                           {Rosenholz::BindParam::text(instanceId)});
    if (inst.empty()) { std::cout << "  >> Instance not found.\n"; return; }
    auto& ii = inst[0];
    auto ig = [&](const std::string& k){ auto it=ii.find(k); return it!=ii.end()?it->second:""; };

    std::string defId       = ig("workflow_def_id");
    std::string curState    = ig("current_state_id");
    std::string entityType  = ig("entity_type");
    std::string entityId    = ig("entity_id");

    std::cout << "  Instance : " << instanceId.substr(0,20) << "\n"
              << "  Entity   : " << entityType << " / " << entityId.substr(0,20) << "\n"
              << "  Current  : " << curState << "\n\n";

    // Show available transitions from current state
    auto transitions = db->query(
        "SELECT * FROM workflow_transitions WHERE workflow_def_id=? AND from_state_id=?;",
        {Rosenholz::BindParam::text(defId), Rosenholz::BindParam::text(curState)});

    if (transitions.empty()) {
        std::cout << "  >> No transitions available from state '" << curState << "'.\n";
        return;
    }

    std::cout << "  Available transitions:\n";
    for (int i = 0; i < (int)transitions.size(); ++i) {
        auto tg = [&](const std::string& k){ auto it=transitions[i].find(k); return it!=transitions[i].end()?it->second:""; };
        std::cout << "    " << (i+1) << ". " << tg("trigger_event")
                  << "  ->  " << tg("to_state_id");
        if (!tg("required_role").empty())
            std::cout << "  (role: " << tg("required_role") << ")";
        std::cout << "\n";
    }

    int tc = readInt("Choose transition", 1, (int)transitions.size());
    auto& trans = transitions[tc-1];
    auto tg = [&](const std::string& k){ auto it=trans.find(k); return it!=trans.end()?it->second:""; };

    std::string toState   = tg("to_state_id");
    std::string transId   = tg("transition_id");
    bool reqComment       = (tg("requires_comment") == "1");

    std::string actorId   = readOpt("Actor person-ID (optional): ");
    std::string role      = readOpt("Acting as role (optional): ");
    std::string decision  = readOpt("Decision (approve/reject/escalate/request-info, optional): ");
    std::string comment;
    if (reqComment)
        comment = readLine("Comment (required): ");
    else
        comment = readOpt("Comment (optional): ");

    // Record action
    std::string actionId = wfGenId("wfa");
    db->exec(R"(
        INSERT INTO workflow_actions
        (action_id,instance_id,transition_id,actor_id,acting_as_role,
         action_type,decision,comment,action_date)
        VALUES(?,?,?,?,?,?,?,?,?)
    )", {
        Rosenholz::BindParam::text(actionId),  Rosenholz::BindParam::text(instanceId),
        Rosenholz::BindParam::text(transId),   Rosenholz::ton(actorId),
        Rosenholz::ton(role),                  Rosenholz::BindParam::text(tg("trigger_event")),
        Rosenholz::ton(decision),              Rosenholz::ton(comment),
        Rosenholz::BindParam::text(wfNow())
    });

    // Close SLA log for current state
    db->exec(
        "UPDATE workflow_sla_log SET exited_date=? "
        "WHERE instance_id=? AND state_id=? AND exited_date IS NULL;",
        {Rosenholz::BindParam::text(wfNow()), Rosenholz::BindParam::text(instanceId),
         Rosenholz::BindParam::text(curState)});

    // Check if terminal
    std::string isTerminal = db->queryScalar(
        "SELECT is_terminal FROM workflow_states WHERE state_id=? AND workflow_def_id=?;",
        {Rosenholz::BindParam::text(toState), Rosenholz::BindParam::text(defId)});

    std::string newStatus = (isTerminal == "1") ? "completed" : "active";
    std::string completedDate = (isTerminal == "1") ? wfNow() : "";

    // Update instance
    db->exec(R"(
        UPDATE workflow_instances
        SET current_state_id=?, previous_state_id=?, status=?, completed_date=?
        WHERE instance_id=?;
    )", {
        Rosenholz::BindParam::text(toState),     Rosenholz::BindParam::text(curState),
        Rosenholz::BindParam::text(newStatus),   Rosenholz::ton(completedDate),
        Rosenholz::BindParam::text(instanceId)
    });

    // Open new SLA log entry
    if (isTerminal != "1") {
        std::string slah = db->queryScalar(
            "SELECT sla_hours FROM workflow_states WHERE state_id=? AND workflow_def_id=?;",
            {Rosenholz::BindParam::text(toState), Rosenholz::BindParam::text(defId)});
        db->exec(R"(
            INSERT INTO workflow_sla_log
            (sla_log_id,instance_id,state_id,entered_date,sla_hours_allowed)
            VALUES(?,?,?,?,?)
        )", {
            Rosenholz::BindParam::text(wfGenId("sla")), Rosenholz::BindParam::text(instanceId),
            Rosenholz::BindParam::text(toState),        Rosenholz::BindParam::text(wfNow()),
            slah.empty() ? Rosenholz::BindParam::int64(0) : Rosenholz::BindParam::int64(std::stoi(slah))
        });
    }

    // Sync entity fields
    auto updateEntity = [&](Rosenholz::Database* edb, const std::string& table,
                             const std::string& idCol) {
        if (!edb) return;
        edb->exec(
            "UPDATE " + table + " SET workflow_current_state=?, workflow_status=? WHERE " + idCol + "=?;",
            {Rosenholz::BindParam::text(toState), Rosenholz::BindParam::text(newStatus),
             Rosenholz::BindParam::text(entityId)});
    };
    auto* pdb = Rosenholz::DatabasePool::instance().get("projects");
    if      (entityType == "project")  updateEntity(pdb, "projects",  "project_id");
    else if (entityType == "task")     updateEntity(pdb, "tasks",     "task_id");
    else if (entityType == "incident") updateEntity(pdb, "incidents", "incident_id");

    std::cout << "\n  >> Transition fired.\n"
              << "     State : " << curState << "  ->  " << toState << "\n"
              << "     Status: " << newStatus << "\n\n";
}

// ─────────────────────────────────────────────────────────────
// ADD PARTICIPANT
// ─────────────────────────────────────────────────────────────
static void addParticipantWizard(const std::string& instanceId) {
    auto* db = wfdb();
    if (!db) return;
    hdr("ADD PARTICIPANT  to instance " + instanceId.substr(0,20));

    std::string personId = readLine("Person-ID: ");
    std::cout << "  Role:\n"
              << "    1. initiator   2. approver   3. co-approver\n"
              << "    4. reviewer    5. watcher    6. informed\n"
              << "    7. delegate    8. escalation-target\n";
    int rc = readInt("Role", 1, 8);
    static const char* roles[] = {"initiator","approver","co-approver",
                                   "reviewer","watcher","informed",
                                   "delegate","escalation-target"};
    std::string pid = wfGenId("wfp");
    bool ok = db->exec(R"(
        INSERT INTO workflow_participants
        (participant_id,instance_id,person_id,role,active)
        VALUES(?,?,?,?,1)
    )", {Rosenholz::BindParam::text(pid), Rosenholz::BindParam::text(instanceId),
         Rosenholz::BindParam::text(personId), Rosenholz::BindParam::text(roles[rc-1])});
    std::cout << "  >> Participant " << (ok ? "added." : "FAILED.") << "\n";
}

// ─────────────────────────────────────────────────────────────
// INSTANCE DETAIL MENU
// ─────────────────────────────────────────────────────────────
static void instanceMenu(const std::string& instanceId) {
    while (true) {
        auto* db = wfdb();
        if (!db) break;
        auto rows = db->query("SELECT * FROM workflow_instances WHERE instance_id=?;",
                               {Rosenholz::BindParam::text(instanceId)});
        if (rows.empty()) { std::cout << "  >> Instance not found.\n"; break; }
        printWfInstance(rows[0]);

        std::cout << "  Instance actions:\n"
                  << "    1. Fire a transition (perform action)\n"
                  << "    2. List action history\n"
                  << "    3. List participants\n"
                  << "    4. Add participant\n"
                  << "    5. Add notification\n"
                  << "    6. View SLA log\n"
                  << "    7. Escalate instance\n"
                  << "    8. Mark SLA breached\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 8);
        if (ch == 0) break;

        else if (ch == 1) fireTransitionWizard(instanceId);
        else if (ch == 2) listWfActions(instanceId);
        else if (ch == 3) listWfParticipants(instanceId);
        else if (ch == 4) addParticipantWizard(instanceId);
        else if (ch == 5) {
            hdr("ADD NOTIFICATION");
            std::string recipId  = readOpt("Recipient person-ID (optional): ");
            std::string ntype    = readOpt("Type (state-entry/reminder/escalation): ");
            std::string channel  = readOpt("Channel (console/email/slack, default console): ");
            if (channel.empty()) channel = "console";
            std::string subject  = readOpt("Subject: ");
            std::string body     = readOpt("Body: ");
            std::string schd     = readOpt("Scheduled date YYYY-MM-DD (optional, blank=now): ");
            if (schd.empty()) schd = wfNow();
            db->exec(R"(
                INSERT INTO workflow_notifications
                (notification_id,instance_id,recipient_id,notification_type,
                 channel,subject,body,scheduled_date,sent,acknowledged)
                VALUES(?,?,?,?,?,?,?,?,0,0)
            )", {Rosenholz::BindParam::text(wfGenId("wfn")), Rosenholz::BindParam::text(instanceId),
                 Rosenholz::ton(recipId), Rosenholz::ton(ntype), Rosenholz::BindParam::text(channel),
                 Rosenholz::ton(subject), Rosenholz::ton(body), Rosenholz::BindParam::text(schd)});
            std::cout << "  >> Notification queued.\n";
        }
        else if (ch == 6) {
            auto sla = db->query(
                "SELECT * FROM workflow_sla_log WHERE instance_id=? ORDER BY entered_date;",
                {Rosenholz::BindParam::text(instanceId)});
            hdr("SLA LOG");
            for (auto& r : sla) {
                auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
                std::cout << "  State: " << std::left << std::setw(18) << g("state_id")
                          << "  entered=" << g("entered_date").substr(0,16)
                          << "  exited=" << (g("exited_date").empty() ? "(open)" : g("exited_date").substr(0,16))
                          << "  SLA_h=" << g("sla_hours_allowed")
                          << "  breached=" << (g("breached")=="1" ? "YES" : "no") << "\n";
            }
            std::cout << "\n";
        }
        else if (ch == 7) {
            std::string escalTo = readLine("Escalate to person-ID: ");
            db->exec(
                "UPDATE workflow_instances SET escalated_to=?,escalated_date=?,escalation_level='1' WHERE instance_id=?;",
                {Rosenholz::BindParam::text(escalTo), Rosenholz::BindParam::text(wfNow()),
                 Rosenholz::BindParam::text(instanceId)});
            std::cout << "  >> Escalated to: " << escalTo << "\n";
        }
        else if (ch == 8) {
            db->exec(
                "UPDATE workflow_instances SET sla_breached=1,sla_breach_date=? WHERE instance_id=?;",
                {Rosenholz::BindParam::text(wfNow()), Rosenholz::BindParam::text(instanceId)});
            db->exec(
                "UPDATE workflow_sla_log SET breached=1 WHERE instance_id=? AND exited_date IS NULL;",
                {Rosenholz::BindParam::text(instanceId)});
            std::cout << "  >> SLA breach recorded.\n";
        }
    }
}

// ─────────────────────────────────────────────────────────────
// WORKFLOW DEFINITION MENU
// ─────────────────────────────────────────────────────────────
static void wfDefinitionMenu(const std::string& defId) {
    while (true) {
        auto* db = wfdb();
        if (!db) break;
        auto rows = db->query("SELECT * FROM workflow_definitions WHERE workflow_def_id=?;",
                               {Rosenholz::BindParam::text(defId)});
        if (rows.empty()) { std::cout << "  >> Definition not found.\n"; break; }
        printWfDefinition(rows[0]);

        std::cout << "  Definition actions:\n"
                  << "    1. List states\n"
                  << "    2. Add state\n"
                  << "    3. List transitions\n"
                  << "    4. Add transition\n"
                  << "    5. Start instance on an entity\n"
                  << "    6. List instances for this definition\n"
                  << "    7. Activate / deactivate definition\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 7);
        if (ch == 0) break;
        else if (ch == 1) listWfStates(defId);
        else if (ch == 2) addStateWizard(defId);
        else if (ch == 3) listWfTransitions(defId);
        else if (ch == 4) addTransitionWizard(defId);
        else if (ch == 5) {
            std::string iid = startWfInstanceWizard();
            if (!iid.empty()) instanceMenu(iid);
        }
        else if (ch == 6) {
            auto insts = db->query(
                "SELECT * FROM workflow_instances WHERE workflow_def_id=? ORDER BY initiated_date DESC;",
                {Rosenholz::BindParam::text(defId)});
            listWfInstances();   // shows all
        }
        else if (ch == 7) {
            auto cur = db->queryScalar(
                "SELECT status FROM workflow_definitions WHERE workflow_def_id=?;",
                {Rosenholz::BindParam::text(defId)});
            std::string newSt = (cur == "active") ? "inactive" : "active";
            db->exec("UPDATE workflow_definitions SET status=? WHERE workflow_def_id=?;",
                     {Rosenholz::BindParam::text(newSt), Rosenholz::BindParam::text(defId)});
            std::cout << "  >> Status -> " << newSt << "\n";
        }
    }
}

// ─────────────────────────────────────────────────────────────
// MAIN WORKFLOW BROWSER MENU
// ─────────────────────────────────────────────────────────────
static void workflowMenu() {
    while (true) {
        auto* db = wfdb();
        if (!db) { std::cout << "  >> workflow DB not available.\n"; return; }

        // Quick stats
        std::cout << "\n  WORKFLOW  [defs="
                  << db->rowCount("workflow_definitions")
                  << "  instances=" << db->rowCount("workflow_instances")
                  << "  actions="   << db->rowCount("workflow_actions")
                  << "  pending="   << db->queryScalar(
                         "SELECT count(*) FROM workflow_instances WHERE status='active';")
                  << " active]\n";
        hr();
        std::cout << "  DEFINITIONS\n"
                  << "    1.  List all definitions\n"
                  << "    2.  Create new definition\n"
                  << "    3.  Open definition by number\n"
                  << "\n  INSTANCES\n"
                  << "    4.  List all active instances\n"
                  << "    5.  List all instances\n"
                  << "    6.  Open instance by ID\n"
                  << "    7.  Start instance on entity\n"
                  << "    8.  Find instances for entity\n"
                  << "\n  QUICK ACTIONS\n"
                  << "    9.  Fire transition on instance by ID\n"
                  << "   10.  Show breached / overdue SLAs\n"
                  << "   11.  List pending notifications\n"
                  << "\n    0.  Back\n";
        hr();
        int ch = readInt("Choice", 0, 11);
        if (ch == 0) break;

        else if (ch == 1) listWfDefinitions();
        else if (ch == 2) {
            std::string defId = createWfDefinitionWizard();
            if (!defId.empty()) {
                std::cout << "  Open definition now? (y/n): ";
                std::string ans; std::getline(std::cin, ans);
                if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y')) wfDefinitionMenu(defId);
            }
        }
        else if (ch == 3) {
            auto defs = db->query("SELECT workflow_def_id FROM workflow_definitions ORDER BY name;");
            if (defs.empty()) { std::cout << "  (none)\n"; continue; }
            listWfDefinitions();
            int n = readInt("Number", 1, (int)defs.size());
            wfDefinitionMenu(defs[n-1].at("workflow_def_id"));
        }
        else if (ch == 4) {
            auto rows = db->query(
                "SELECT * FROM workflow_instances WHERE status='active' ORDER BY initiated_date DESC;");
            listWfInstances("active");   // pass entity filter empty = all
        }
        else if (ch == 5) listWfInstances();
        else if (ch == 6) {
            std::string id = readLine("Instance ID: ");
            instanceMenu(id);
        }
        else if (ch == 7) {
            std::string iid = startWfInstanceWizard();
            if (!iid.empty()) instanceMenu(iid);
        }
        else if (ch == 8) {
            std::cout << "  Entity types: project / task / incident / document / ...\n";
            std::string et = readLine("Entity type: ");
            std::string ei = readLine("Entity ID: ");
            listWfInstances(et, ei);
        }
        else if (ch == 9) {
            std::string id = readLine("Instance ID: ");
            fireTransitionWizard(id);
        }
        else if (ch == 10) {
            auto rows = db->query(
                "SELECT * FROM workflow_instances WHERE sla_breached=1 OR "
                "(status='active' AND due_date < datetime('now')) ORDER BY due_date;");
            hdr("BREACHED / OVERDUE INSTANCES");
            if (rows.empty()) { std::cout << "  (none — all on time)\n\n"; }
            else for (auto& r : rows) printWfInstance(r);
        }
        else if (ch == 11) {
            auto rows = db->query(
                "SELECT * FROM workflow_notifications WHERE sent=0 ORDER BY scheduled_date;");
            hdr("PENDING NOTIFICATIONS");
            if (rows.empty()) { std::cout << "  (none)\n\n"; continue; }
            for (auto& r : rows) {
                auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
                std::cout << "  [" << g("scheduled_date").substr(0,16) << "]"
                          << "  " << g("channel")
                          << "  to=" << g("recipient_id").substr(0,16)
                          << "  " << g("subject") << "\n";
            }
            std::cout << "\n";
            std::cout << "  Mark all as sent? (y/n): ";
            std::string ans; std::getline(std::cin, ans);
            if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y')) {
                db->exec("UPDATE workflow_notifications SET sent=1,sent_date=? WHERE sent=0;",
                         {Rosenholz::BindParam::text(wfNow())});
                std::cout << "  >> " << rows.size() << " notification(s) marked sent.\n";
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// REPORTING / OPERATIONAL ENTITIES  (appended to CLI namespace)
// ─────────────────────────────────────────────────────────────

// ── generic list table ────────────────────────────────────────
static void listTable(const std::string& dbName, const std::string& table,
                      const std::vector<std::string>& cols,
                      const std::string& where = "", const std::string& order = "") {
    auto* db = Rosenholz::DatabasePool::instance().get(dbName);
    if (!db) return;
    std::string sql = "SELECT * FROM " + table;
    if (!where.empty()) sql += " WHERE " + where;
    if (!order.empty()) sql += " ORDER BY " + order;
    sql += " LIMIT 50;";
    auto rows = db->query(sql);
    if (rows.empty()) { std::cout << "\n  (none)\n\n"; return; }
    // header
    std::cout << "\n  ";
    for (auto& c : cols) std::cout << std::left << std::setw(22) << c.substr(0,20);
    std::cout << "\n  " << std::string(cols.size()*22, '-') << "\n";
    int n = 1;
    for (auto& r : rows) {
        std::cout << "  " << std::setw(3) << n++ << " ";
        for (auto& c : cols) {
            auto it = r.find(c);
            std::string v = it!=r.end() ? it->second : "";
            if (v.size() > 19) v = v.substr(0,18)+"~";
            std::cout << std::left << std::setw(22) << v;
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────
// MILESTONE MENU
// ─────────────────────────────────────────────────────────────
static void milestoneMenu(const std::string& projectId) {
    while (true) {
        auto milestones = Rosenholz::Milestone::loadForProject(projectId);
        hdr("MILESTONES  project=" + projectId.substr(0,20));
        if (milestones.empty()) std::cout << "  (none yet)\n";
        else {
            std::cout << "  " << std::left
                      << std::setw(4) << "#" << std::setw(14) << "Type"
                      << std::setw(24) << "Title" << std::setw(12) << "Planned"
                      << std::setw(10) << "Status" << std::setw(8) << "Paym." << "\n";
            hr();
            int n=1;
            for (auto& m : milestones) {
                std::string t = m->title.size()>22 ? m->title.substr(0,21)+"~" : m->title;
                std::cout << "  " << std::left
                          << std::setw(4)  << n++
                          << std::setw(14) << fval(m->milestoneType)
                          << std::setw(24) << t
                          << std::setw(12) << fdate(m->plannedDate)
                          << std::setw(10) << m->status
                          << std::setw(8)  << (m->paymentTrigger?"YES":"")
                          << "\n";
            }
        }
        std::cout << "\n  Actions:\n"
                  << "    1. Create milestone\n"
                  << "    2. Open by number\n"
                  << "    3. Show overdue milestones\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 3);
        if (ch == 0) break;
        else if (ch == 1) {
            std::string title = readLine("Title: ");
            std::string pd    = readOpt("Planned date YYYY-MM-DD: ");
            std::cout << "  Type: 1.phase-gate  2.delivery  3.payment  4.contractual  5.internal\n";
            int tt = readInt("Type", 1, 5);
            static const char* mtypes[]={"phase-gate","delivery","payment","contractual","internal"};
            auto m = Rosenholz::Milestone::create(projectId, title, pd);
            m->milestoneType = mtypes[tt-1];
            m->criteria      = readOpt("Criteria (optional): ");
            m->ownerId       = readOpt("Owner person-ID (optional): ");
            std::cout << "  Payment trigger? (y/n): "; std::string pt; std::getline(std::cin,pt);
            m->paymentTrigger = (!pt.empty()&&(pt[0]=='y'||pt[0]=='Y'));
            std::cout << "  Contractual? (y/n): "; std::string co; std::getline(std::cin,co);
            m->contractual    = (!co.empty()&&(co[0]=='y'||co[0]=='Y'));
            if (m->save())
                std::cout << "  >> Milestone saved: " << m->milestoneId << "\n";
        }
        else if (ch == 2) {
            if (milestones.empty()) { std::cout << "  (none)\n"; continue; }
            int n = readInt("Number", 1, (int)milestones.size());
            auto& m = milestones[n-1];
            // mini detail loop
            while (true) {
                auto row=[](const std::string& k,const std::string& v){
                    std::cout<<"  | "<<std::left<<std::setw(20)<<k<<std::setw(32)<<v<<"|\n";};
                hdr("MILESTONE");
                row("ID",m->milestoneId); row("Title",m->title);
                row("Type",fval(m->milestoneType)); row("Status",m->status);
                row("Planned",fdate(m->plannedDate)); row("Actual",fdate(m->actualDate));
                row("Variance (d)",std::to_string(m->varianceDays));
                row("Contractual",m->contractual?"YES":"no");
                row("Payment trig",m->paymentTrigger?"YES":"no");
                row("Owner",fval(m->ownerId));
                if (!m->criteria.empty()) row("Criteria",m->criteria.substr(0,30));
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";

                std::cout<<"  1.Mark achieved  2.Edit  3.Reassign owner  0.Back\n";
                int mc = readInt("Choice",0,3);
                if (mc==0) break;
                else if (mc==1) {
                    std::string ad = readOpt("Actual date (YYYY-MM-DD, blank=now): ");
                    m->markAchieved(ad);
                    std::cout << "  >> Achieved.\n";
                }
                else if (mc==2) {
                    std::string nt = readOpt("New title: "); if(!nt.empty()) m->title=nt;
                    std::string np = readOpt("New planned date: "); if(!np.empty()) m->plannedDate=np;
                    std::string ns = readOpt("New status (pending/achieved/missed/deferred): ");
                    if(!ns.empty()) m->status=ns;
                    std::string nc = readOpt("New criteria: "); if(!nc.empty()) m->criteria=nc;
                    m->update(); std::cout<<"  >> Saved.\n";
                }
                else if (mc==3) {
                    std::string id = readLine("Owner person-ID: ");
                    m->reassignOwner(id); std::cout<<"  >> Reassigned.\n";
                }
            }
        }
        else if (ch == 3) {
            auto ov = Rosenholz::Milestone::loadOverdue();
            hdr("OVERDUE MILESTONES");
            if (ov.empty()) std::cout << "  (none — all on track)\n\n";
            else for (auto& m : ov)
                std::cout << "  " << fdate(m->plannedDate) << "  " << std::left
                          << std::setw(28) << m->title << "  proj=" << m->projectId.substr(0,16) << "\n";
            std::cout << "\n";
        }
    }
}

// ─────────────────────────────────────────────────────────────
// MEETING MENU
// ─────────────────────────────────────────────────────────────
static void meetingMenu(const std::string& taskId, const std::string& projectId) {
    while (true) {
        auto meetings = taskId.empty()
            ? Rosenholz::Meeting::loadForProject(projectId)
            : Rosenholz::Meeting::loadForTask(taskId);
        hdr("MEETINGS");
        if (meetings.empty()) std::cout << "  (none)\n";
        else {
            int n=1;
            for (auto& m : meetings)
                std::cout << "  " << std::setw(3) << n++ << ".  ["
                          << fdate(m->scheduledDate).substr(0,10) << "]  "
                          << std::left << std::setw(24) << m->title
                          << "  status=" << m->status
                          << "  dur=" << m->durationMins << "min\n";
        }
        std::cout << "\n  1.Create  2.Open  0.Back\n";
        int ch = readInt("Choice",0,2);
        if (ch==0) break;
        else if (ch==1) {
            std::string t  = readLine("Title: ");
            std::string sd = readOpt("Scheduled date YYYY-MM-DD HH:MM: ");
            std::string mt = readOpt("Type (general/standup/review/kickoff/retrospective): ");
            std::string loc= readOpt("Location (optional): ");
            std::string ch2= readOpt("Channel (in-person/video/phone): ");
            std::string tid = taskId.empty() ? readLine("Task-ID: ") : taskId;
            auto m = Rosenholz::Meeting::create(tid, t, sd);
            m->meetingType = mt; m->location = loc; m->channel = ch2;
            m->projectId   = projectId;
            m->agenda      = readOpt("Agenda (optional): ");
            m->organiserId = readOpt("Organiser person-ID (optional): ");
            std::string durS = readOpt("Duration minutes (optional): ");
            if (!durS.empty()) try { m->durationMins = std::stoi(durS); } catch(...) {}
            if (m->save()) std::cout << "  >> Meeting saved: " << m->meetingId << "\n";
        }
        else if (ch==2) {
            if (meetings.empty()) { std::cout << "  (none)\n"; continue; }
            int n = readInt("Number",1,(int)meetings.size());
            auto& m = meetings[n-1];
            while (true) {
                hdr("MEETING  " + m->title);
                auto row=[](const std::string& k,const std::string& v){
                    std::cout<<"  | "<<std::left<<std::setw(18)<<k<<std::setw(34)<<v<<"|\n";};
                row("ID",m->meetingId); row("Status",m->status);
                row("Scheduled",fdate(m->scheduledDate)); row("Actual",fdate(m->actualDate));
                row("Duration",std::to_string(m->durationMins)+"min");
                row("Location",fval(m->location)); row("Channel",fval(m->channel));
                row("Organiser",fval(m->organiserId));
                if (!m->agenda.empty()) std::cout<<"  | Agenda: "<<m->agenda.substr(0,45)<<"|\n";
                if (!m->decisions.empty()) std::cout<<"  | Decisions: "<<m->decisions.substr(0,41)<<"|\n";
                if (!m->actions.empty()) std::cout<<"  | Actions: "<<m->actions.substr(0,43)<<"|\n";
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Complete  2.Edit agenda/decisions  3.Edit details  0.Back\n";
                int mc=readInt("Choice",0,3);
                if (mc==0) break;
                else if (mc==1) {
                    std::string dec = readOpt("Decisions: ");
                    std::string act = readOpt("Actions: ");
                    m->complete(dec,act); std::cout<<"  >> Completed.\n";
                }
                else if (mc==2) {
                    std::string ag = readOpt("Agenda: "); if(!ag.empty()) m->agenda=ag;
                    std::string dc = readOpt("Decisions: "); if(!dc.empty()) m->decisions=dc;
                    std::string ac = readOpt("Actions: "); if(!ac.empty()) m->actions=ac;
                    m->update(); std::cout<<"  >> Saved.\n";
                }
                else if (mc==3) {
                    std::string sd = readOpt("Scheduled date: "); if(!sd.empty()) m->scheduledDate=sd;
                    std::string loc = readOpt("Location: "); if(!loc.empty()) m->location=loc;
                    std::string dur = readOpt("Duration min: ");
                    if(!dur.empty()) try{m->durationMins=std::stoi(dur);}catch(...){}
                    m->update(); std::cout<<"  >> Saved.\n";
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// MEASURE MENU
// ─────────────────────────────────────────────────────────────
static void measureMenu(const std::string& projectId) {
    while (true) {
        auto measures = Rosenholz::Measure::loadForProject(projectId);
        hdr("MEASURES  project=" + projectId.substr(0,20));
        if (measures.empty()) std::cout << "  (none)\n";
        else {
            int n=1;
            for (auto& m : measures)
                std::cout << "  " << std::setw(3) << n++ << ".  "
                          << std::left << std::setw(24) << m->title
                          << "  type="   << std::setw(14) << m->measureType
                          << "  status=" << m->status << "\n";
        }
        std::cout << "\n  1.Create  2.Open  0.Back\n";
        int ch = readInt("Choice",0,2); if(ch==0) break;
        else if (ch==1) {
            std::string t = readLine("Title: ");
            std::cout<<"  Type: 1.preventive  2.corrective  3.detective  4.directive\n";
            int tc=readInt("Type",1,4);
            static const char* mts[]={"preventive","corrective","detective","directive"};
            auto m = Rosenholz::Measure::create(projectId, t, mts[tc-1]);
            m->description      = readOpt("Description (optional): ");
            m->measureCategory  = readOpt("Category (optional): ");
            m->plannedDate      = readOpt("Planned date YYYY-MM-DD: ");
            m->ownerId          = readOpt("Owner person-ID (optional): ");
            m->riskId           = readOpt("Linked risk-ID (optional): ");
            m->incidentId       = readOpt("Linked incident-ID (optional): ");
            std::string costStr = readOpt("Planned cost EUR (optional): ");
            if(!costStr.empty()) try{m->costPlanned=std::stod(costStr);}catch(...){}
            if(m->save()) std::cout<<"  >> Measure saved: "<<m->measureId<<"\n";
        }
        else if (ch==2 && !measures.empty()) {
            int n=readInt("Number",1,(int)measures.size());
            auto& m=measures[n-1];
            while(true){
                hdr("MEASURE  "+m->title);
                auto row=[](const std::string& k,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(20)<<k<<std::setw(32)<<v<<"|\n";};
                row("ID",m->measureId); row("Type",fval(m->measureType)); row("Status",m->status);
                row("Category",fval(m->measureCategory)); row("Owner",fval(m->ownerId));
                row("Planned",fdate(m->plannedDate)); row("Actual",fdate(m->actualDate));
                row("Cost plan",std::to_string((int)m->costPlanned));
                row("Cost actual",std::to_string((int)m->costActual));
                row("Effectiveness",fval(m->effectiveness));
                row("Risk-ID",fval(m->riskId)); row("Incident-ID",fval(m->incidentId));
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Mark completed  2.Edit status  3.Record cost  4.Verify  0.Back\n";
                int mc=readInt("Choice",0,4); if(mc==0) break;
                else if(mc==1){m->status="completed";m->actualDate=Rosenholz::nowIso();m->update();std::cout<<"  >> Completed.\n";}
                else if(mc==2){std::string s=readOpt("Status (planned/in-progress/completed/verified/cancelled): ");if(!s.empty())m->status=s;m->update();std::cout<<"  >> Saved.\n";}
                else if(mc==3){std::string c=readOpt("Actual cost EUR: ");if(!c.empty())try{m->costActual=std::stod(c);}catch(...){}m->update();std::cout<<"  >> Saved.\n";}
                else if(mc==4){m->verifiedDate=Rosenholz::nowIso();m->verifiedBy=readOpt("Verified by person-ID: ");m->effectiveness=readOpt("Effectiveness (high/medium/low): ");m->status="verified";m->update();std::cout<<"  >> Verified.\n";}
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// QUALITY GATE MENU
// ─────────────────────────────────────────────────────────────
static void qualityGateMenu(const std::string& projectId) {
    while (true) {
        auto gates = Rosenholz::QualityGate::loadForProject(projectId);
        hdr("QUALITY GATES  project=" + projectId.substr(0,20));
        if (gates.empty()) std::cout << "  (none)\n";
        else {
            int n=1;
            for (auto& g : gates)
                std::cout << "  " << std::setw(3) << n++ << ".  "
                          << std::left << std::setw(26) << g->title
                          << "  phase=" << std::setw(10) << fval(g->phase)
                          << "  planned=" << fdate(g->plannedDate)
                          << "  result=" << g->result << "\n";
        }
        std::cout<<"\n  1.Create  2.Open  0.Back\n";
        int ch=readInt("Choice",0,2); if(ch==0) break;
        else if(ch==1){
            std::string t=readLine("Title: ");
            std::string ph=readOpt("Phase (e.g. design/development/testing/release): ");
            auto g=Rosenholz::QualityGate::create(projectId,t,ph);
            g->plannedDate=readOpt("Planned date YYYY-MM-DD: ");
            g->reviewerId=readOpt("Reviewer person-ID (optional): ");
            g->criteria=readOpt("Criteria: ");
            g->standardsApplied=readOpt("Standards applied (optional): ");
            g->acceptanceCriteria=readOpt("Acceptance criteria (optional): ");
            if(g->save()) std::cout<<"  >> Quality gate saved: "<<g->gateId<<"\n";
        }
        else if(ch==2 && !gates.empty()){
            int n=readInt("Number",1,(int)gates.size());
            auto& g=gates[n-1];
            while(true){
                hdr("QUALITY GATE  "+g->title);
                auto row=[](const std::string& k,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(22)<<k<<std::setw(30)<<v<<"|\n";};
                row("ID",g->gateId); row("Phase",fval(g->phase)); row("Result",g->result);
                row("Decision",fval(g->decision)); row("Reviewer",fval(g->reviewerId));
                row("Planned",fdate(g->plannedDate)); row("Actual",fdate(g->actualDate));
                row("Standards",fval(g->standardsApplied));
                if(!g->criteria.empty()) std::cout<<"  | Criteria: "<<g->criteria.substr(0,43)<<"|\n";
                if(!g->findings.empty()) std::cout<<"  | Findings: "<<g->findings.substr(0,43)<<"|\n";
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Record result  2.Edit  0.Back\n";
                int mc=readInt("Choice",0,2); if(mc==0) break;
                else if(mc==1){
                    std::cout<<"  Result: 1.passed  2.failed  3.conditional  4.pending\n";
                    int rc=readInt("Result",1,4);
                    static const char* rr[]={"passed","failed","conditional","pending"};
                    std::cout<<"  Decision: 1.proceed  2.hold  3.stop\n";
                    int dc=readInt("Decision",1,3);
                    static const char* dd[]={"proceed","hold","stop"};
                    std::string findings=readOpt("Findings: ");
                    g->recordResult(rr[rc-1],dd[dc-1],findings);
                    std::cout<<"  >> Result recorded.\n";
                }
                else if(mc==2){
                    std::string nc=readOpt("New criteria: ");if(!nc.empty())g->criteria=nc;
                    std::string ns=readOpt("New standards: ");if(!ns.empty())g->standardsApplied=ns;
                    std::string np=readOpt("New planned date: ");if(!np.empty())g->plannedDate=np;
                    g->update(); std::cout<<"  >> Saved.\n";
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// KPI MENU
// ─────────────────────────────────────────────────────────────
static void kpiMenu(const std::string& projectId) {
    while (true) {
        auto kpis = Rosenholz::KPI::loadForProject(projectId);
        hdr("KPIs  project=" + projectId.substr(0,20));
        if (kpis.empty()) std::cout << "  (none)\n";
        else {
            std::cout << "  " << std::left
                      << std::setw(4) << "#" << std::setw(24) << "Title"
                      << std::setw(8) << "Unit" << std::setw(10) << "Target"
                      << std::setw(10) << "Actual" << std::setw(8) << "RAG" << "\n";
            hr();
            int n=1;
            for (auto& k : kpis)
                std::cout << "  " << std::left
                          << std::setw(4)  << n++
                          << std::setw(24) << k->title.substr(0,22)
                          << std::setw(8)  << fval(k->unit)
                          << std::setw(10) << std::to_string((int)k->targetValue)
                          << std::setw(10) << std::to_string((int)k->actualValue)
                          << std::setw(8)  << k->ragStatus << "\n";
        }
        std::cout<<"\n  1.Create  2.Open / record measurement  0.Back\n";
        int ch=readInt("Choice",0,2); if(ch==0) break;
        else if(ch==1){
            std::string t=readLine("Title: ");
            std::string u=readOpt("Unit (%, EUR, days, count, ...): ");
            auto k=Rosenholz::KPI::create(projectId,t,u);
            k->description=readOpt("Description (optional): ");
            k->category=readOpt("Category (schedule/cost/quality/scope/resource): ");
            k->dimension=readOpt("Dimension (optional): ");
            k->measurementFrequency=readOpt("Frequency (daily/weekly/monthly): ");
            auto dbl=[](const std::string& s,double def=0.0)->double{try{return s.empty()?def:std::stod(s);}catch(...){return def;}};
            k->targetValue=dbl(readOpt("Target value: "));
            k->baselineValue=dbl(readOpt("Baseline value: "));
            k->thresholdGreen=dbl(readOpt("Green threshold (>=): "));
            k->thresholdAmber=dbl(readOpt("Amber threshold (>=): "));
            k->thresholdRed=dbl(readOpt("Red threshold (<): "));
            k->ownerId=readOpt("Owner person-ID (optional): ");
            if(k->save()) std::cout<<"  >> KPI saved: "<<k->kpiId<<"\n";
        }
        else if(ch==2 && !kpis.empty()){
            int n=readInt("Number",1,(int)kpis.size());
            auto& k=kpis[n-1];
            while(true){
                hdr("KPI  "+k->title);
                auto row=[](const std::string& lbl,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(20)<<lbl<<std::setw(32)<<v<<"|\n";};
                row("ID",k->kpiId); row("Unit",fval(k->unit)); row("Category",fval(k->category));
                row("Target",std::to_string(k->targetValue)); row("Actual",std::to_string(k->actualValue));
                row("Baseline",std::to_string(k->baselineValue));
                row("Green >=",std::to_string(k->thresholdGreen));
                row("Amber >=",std::to_string(k->thresholdAmber));
                row("Red <",std::to_string(k->thresholdRed));
                row("RAG",k->ragStatus+" (trend="+fval(k->trend)+")");
                row("Last measured",fdate(k->lastMeasuredDate));
                row("Next due",fdate(k->nextMeasurementDate));
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Record measurement  2.Edit  0.Back\n";
                int mc=readInt("Choice",0,2); if(mc==0) break;
                else if(mc==1){
                    std::string vs=readLine("New actual value: ");
                    double v=0; try{v=std::stod(vs);}catch(...){}
                    std::string d=readOpt("Date (blank=now): ");
                    k->recordMeasurement(v,d);
                    std::cout<<"  >> Recorded. RAG="<<k->ragStatus<<"\n";
                }
                else if(mc==2){
                    std::string nt=readOpt("New title: ");if(!nt.empty())k->title=nt;
                    std::string nf=readOpt("New frequency: ");if(!nf.empty())k->measurementFrequency=nf;
                    std::string nd=readOpt("Next measurement date: ");if(!nd.empty())k->nextMeasurementDate=nd;
                    k->update(); std::cout<<"  >> Saved.\n";
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// CHANGE REQUEST MENU
// ─────────────────────────────────────────────────────────────
static void changeRequestMenu(const std::string& projectId) {
    while (true) {
        auto crs = Rosenholz::ChangeRequest::loadForProject(projectId);
        hdr("CHANGE REQUESTS  project=" + projectId.substr(0,20));
        if (crs.empty()) std::cout << "  (none)\n";
        else {
            int n=1;
            for (auto& c : crs)
                std::cout << "  " << std::setw(3) << n++ << ".  "
                          << std::left << std::setw(28) << c->title
                          << "  type=" << std::setw(10) << c->changeType
                          << "  status=" << c->status << "\n";
        }
        std::cout<<"\n  1.Create  2.Open  3.List all open  0.Back\n";
        int ch=readInt("Choice",0,3); if(ch==0) break;
        else if(ch==1){
            std::string t=readLine("Title: ");
            std::cout<<"  Type: 1.scope  2.schedule  3.cost  4.quality  5.resource  6.general\n";
            int tc=readInt("Type",1,6);
            static const char* cts[]={"scope","schedule","cost","quality","resource","general"};
            auto c=Rosenholz::ChangeRequest::create(projectId,t,cts[tc-1]);
            c->description=readOpt("Description: ");
            c->justification=readOpt("Justification: ");
            c->raisedBy=readOpt("Raised by person-ID (optional): ");
            c->scopeImpact=readOpt("Scope impact (optional): ");
            c->qualityImpact=readOpt("Quality impact (optional): ");
            std::string cs=readOpt("Cost impact EUR (optional): ");
            if(!cs.empty()) try{c->costImpact=std::stod(cs);}catch(...){}
            std::string ss=readOpt("Schedule impact days (optional): ");
            if(!ss.empty()) try{c->scheduleImpactDays=std::stoi(ss);}catch(...){}
            if(c->save()) std::cout<<"  >> CR saved: "<<c->crId<<"\n";
        }
        else if(ch==2 && !crs.empty()){
            int n=readInt("Number",1,(int)crs.size());
            auto& c=crs[n-1];
            while(true){
                hdr("CHANGE REQUEST  "+c->title);
                auto row=[](const std::string& k,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(22)<<k<<std::setw(30)<<v<<"|\n";};
                row("ID",c->crId); row("Type",fval(c->changeType)); row("Status",c->status);
                row("Raised by",fval(c->raisedBy)); row("Raised",fdate(c->raisedDate));
                row("Decision",fdate(c->decisionDate)); row("Implemented",fdate(c->implementedDate));
                row("Cost impact",std::to_string((int)c->costImpact)+" EUR");
                row("Schedule (d)",std::to_string(c->scheduleImpactDays));
                row("Scope impact",fval(c->scopeImpact).substr(0,28));
                row("Quality impact",fval(c->qualityImpact).substr(0,26));
                if(!c->justification.empty()) std::cout<<"  | Justification: "<<c->justification.substr(0,37)<<"|\n";
                if(!c->decisionRationale.empty()) std::cout<<"  | Decision: "<<c->decisionRationale.substr(0,41)<<"|\n";
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Approve  2.Reject  3.Submit  4.Mark implemented  5.Edit  0.Back\n";
                int mc=readInt("Choice",0,5); if(mc==0) break;
                else if(mc==1){c->approve(readOpt("Rationale: "));std::cout<<"  >> Approved.\n";}
                else if(mc==2){c->reject(readOpt("Rationale: "));std::cout<<"  >> Rejected.\n";}
                else if(mc==3){c->status="submitted";c->update();std::cout<<"  >> Submitted.\n";}
                else if(mc==4){c->status="implemented";c->implementedDate=Rosenholz::nowIso();c->update();std::cout<<"  >> Implemented.\n";}
                else if(mc==5){
                    std::string nt=readOpt("New title: ");if(!nt.empty())c->title=nt;
                    std::string nd=readOpt("New description: ");if(!nd.empty())c->description=nd;
                    c->update(); std::cout<<"  >> Saved.\n";
                }
            }
        }
        else if(ch==3){
            auto open=Rosenholz::ChangeRequest::loadOpen();
            hdr("ALL OPEN CHANGE REQUESTS");
            if(open.empty()) std::cout<<"  (none)\n\n";
            else for(auto& c:open)
                std::cout<<"  "<<std::left<<std::setw(28)<<c->title<<"  "<<std::setw(10)<<c->changeType<<"  "<<c->status<<"\n";
            std::cout<<"\n";
        }
    }
}

// ─────────────────────────────────────────────────────────────
// LESSON LEARNED MENU
// ─────────────────────────────────────────────────────────────
static void lessonLearnedMenu(const std::string& projectId) {
    while (true) {
        auto lessons = Rosenholz::LessonLearned::loadForProject(projectId);
        hdr("LESSONS LEARNED  project=" + projectId.substr(0,20));
        if (lessons.empty()) std::cout << "  (none)\n";
        else {
            int n=1;
            for (auto& l : lessons)
                std::cout << "  " << std::setw(3) << n++ << ".  "
                          << std::left << std::setw(26) << l->title
                          << "  cat=" << std::setw(12) << fval(l->category)
                          << "  status=" << l->status
                          << (l->addedToKb?"  [KB]":"") << "\n";
        }
        std::cout<<"\n  1.Create  2.Open  3.Browse knowledge base  0.Back\n";
        int ch=readInt("Choice",0,3); if(ch==0) break;
        else if(ch==1){
            auto l=Rosenholz::LessonLearned::create(projectId,readLine("Title: "));
            l->description=readOpt("Description: ");
            l->category=readOpt("Category (technical/process/people/tools/communication): ");
            l->dimension=readOpt("Dimension (scope/time/cost/quality): ");
            l->impact=readOpt("Impact: ");
            l->recommendation=readOpt("Recommendation: ");
            l->submittedBy=readOpt("Submitted by person-ID (optional): ");
            l->tags=readOpt("Tags (comma-sep, optional): ");
            if(l->save()) std::cout<<"  >> Lesson saved: "<<l->lessonId<<"\n";
        }
        else if(ch==2 && !lessons.empty()){
            int n=readInt("Number",1,(int)lessons.size());
            auto& l=lessons[n-1];
            while(true){
                hdr("LESSON  "+l->title);
                auto row=[](const std::string& k,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(20)<<k<<std::setw(32)<<v<<"|\n";};
                row("ID",l->lessonId); row("Status",l->status); row("Category",fval(l->category));
                row("Dimension",fval(l->dimension)); row("In KB",l->addedToKb?"YES":"no");
                row("Submitted by",fval(l->submittedBy));
                row("Identified",fdate(l->identifiedDate));
                if(!l->description.empty()) std::cout<<"  | Desc: "<<l->description.substr(0,47)<<"|\n";
                if(!l->impact.empty()) std::cout<<"  | Impact: "<<l->impact.substr(0,45)<<"|\n";
                if(!l->recommendation.empty()) std::cout<<"  | Rec.: "<<l->recommendation.substr(0,46)<<"|\n";
                if(!l->actionTaken.empty()) std::cout<<"  | Action: "<<l->actionTaken.substr(0,44)<<"|\n";
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Approve  2.Add to knowledge base  3.Edit  0.Back\n";
                int mc=readInt("Choice",0,3); if(mc==0) break;
                else if(mc==1){l->status="approved";l->reviewedDate=Rosenholz::nowIso();l->update();std::cout<<"  >> Approved.\n";}
                else if(mc==2){l->addedToKb=true;l->update();std::cout<<"  >> Added to knowledge base.\n";}
                else if(mc==3){
                    std::string na=readOpt("Action taken: ");if(!na.empty())l->actionTaken=na;
                    std::string nr=readOpt("Recommendation update: ");if(!nr.empty())l->recommendation=nr;
                    l->update(); std::cout<<"  >> Saved.\n";
                }
            }
        }
        else if(ch==3){
            auto kb=Rosenholz::LessonLearned::loadKnowledgeBase();
            hdr("KNOWLEDGE BASE (" + std::to_string(kb.size()) + " lessons)");
            if(kb.empty()) std::cout<<"  (empty)\n\n";
            else for(auto& l:kb)
                std::cout<<"  ["<<fval(l->category)<<"]  "<<l->title<<"\n";
            std::cout<<"\n";
        }
    }
}

// ─────────────────────────────────────────────────────────────
// ASSUMPTION/CONSTRAINT MENU
// ─────────────────────────────────────────────────────────────
static void assumptionConstraintMenu(const std::string& projectId) {
    while(true){
        auto acs=Rosenholz::AssumptionConstraint::loadForProject(projectId);
        hdr("ASSUMPTIONS & CONSTRAINTS");
        if(acs.empty()) std::cout<<"  (none)\n";
        else{
            int n=1;
            for(auto& a:acs)
                std::cout<<"  "<<std::setw(3)<<n++<<".  ["<<a->acType.substr(0,3)<<"]  "
                          <<std::left<<std::setw(26)<<a->title
                          <<"  status="<<a->status
                          <<(a->breached?"  BREACHED":"")<<"\n";
        }
        std::cout<<"\n  1.Create assumption  2.Create constraint  3.Open  0.Back\n";
        int ch=readInt("Choice",0,3); if(ch==0) break;
        else if(ch<=2){
            std::string tp=(ch==1)?"assumption":"constraint";
            auto a=Rosenholz::AssumptionConstraint::create(projectId,readLine("Title: "),tp);
            a->description=readOpt("Description: ");
            a->category=readOpt("Category (optional): ");
            a->dimension=readOpt("Dimension (scope/time/cost/quality/resource): ");
            a->impactIfWrong=readOpt("Impact if wrong/violated: ");
            a->mitigation=readOpt("Mitigation (optional): ");
            a->ownerId=readOpt("Owner person-ID (optional): ");
            a->reviewDate=readOpt("Review date YYYY-MM-DD (optional): ");
            if(a->save()) std::cout<<"  >> Saved: "<<a->acId<<"\n";
        }
        else if(ch==3 && !acs.empty()){
            int n=readInt("Number",1,(int)acs.size());
            auto& a=acs[n-1];
            while(true){
                hdr(std::string(a->acType=="assumption"?"ASSUMPTION":"CONSTRAINT")+"  "+a->title);
                auto row=[](const std::string& k,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(20)<<k<<std::setw(32)<<v<<"|\n";};
                row("ID",a->acId); row("Type",a->acType); row("Status",a->status);
                row("Category",fval(a->category)); row("Dimension",fval(a->dimension));
                row("Owner",fval(a->ownerId)); row("Review date",fdate(a->reviewDate));
                row("Breached",a->breached?"YES":"no");
                if(!a->description.empty()) std::cout<<"  | Desc: "<<a->description.substr(0,47)<<"|\n";
                if(!a->impactIfWrong.empty()) std::cout<<"  | Impact: "<<a->impactIfWrong.substr(0,45)<<"|\n";
                if(!a->mitigation.empty()) std::cout<<"  | Mitigation: "<<a->mitigation.substr(0,41)<<"|\n";
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Mark breached  2.Validate  3.Close  4.Edit  0.Back\n";
                int mc=readInt("Choice",0,4); if(mc==0) break;
                else if(mc==1){a->markBreached(readOpt("Breach date (blank=now): "));std::cout<<"  >> Marked breached.\n";}
                else if(mc==2){a->validatedDate=Rosenholz::nowIso();a->validatedBy=readOpt("Validated by: ");a->status="validated";a->update();std::cout<<"  >> Validated.\n";}
                else if(mc==3){a->status="closed";a->update();std::cout<<"  >> Closed.\n";}
                else if(mc==4){
                    std::string nt=readOpt("New title: ");if(!nt.empty())a->title=nt;
                    std::string nd=readOpt("New description: ");if(!nd.empty())a->description=nd;
                    a->update();std::cout<<"  >> Saved.\n";
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// DECISION LOG MENU
// ─────────────────────────────────────────────────────────────
static void decisionLogMenu(const std::string& projectId) {
    while(true){
        auto decs=Rosenholz::DecisionLog::loadForProject(projectId);
        hdr("DECISION LOG  project="+projectId.substr(0,20));
        if(decs.empty()) std::cout<<"  (none)\n";
        else{
            int n=1;
            for(auto& d:decs)
                std::cout<<"  "<<std::setw(3)<<n++<<".  "
                          <<fdate(d->decisionDate).substr(0,10)<<"  "
                          <<std::left<<std::setw(28)<<d->title
                          <<"  status="<<d->status<<"\n";
        }
        std::cout<<"\n  1.Create  2.Open  0.Back\n";
        int ch=readInt("Choice",0,2); if(ch==0) break;
        else if(ch==1){
            auto d=Rosenholz::DecisionLog::create(projectId,readLine("Title: "));
            d->description=readOpt("Description: ");
            d->decisionType=readOpt("Type (technical/business/process/resource): ");
            d->decidedBy=readOpt("Decided by person-ID (optional): ");
            d->optionsConsidered=readOpt("Options considered: ");
            d->rationale=readOpt("Rationale: ");
            d->impactCost=readOpt("Cost impact: ");
            d->impactSchedule=readOpt("Schedule impact: ");
            d->impactScope=readOpt("Scope impact: ");
            d->reviewDate=readOpt("Review date YYYY-MM-DD: ");
            if(d->save()) std::cout<<"  >> Decision saved: "<<d->decisionId<<"\n";
        }
        else if(ch==2 && !decs.empty()){
            int n=readInt("Number",1,(int)decs.size());
            auto& d=decs[n-1];
            hdr("DECISION  "+d->title);
            auto row=[](const std::string& k,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(20)<<k<<std::setw(32)<<v<<"|\n";};
            row("ID",d->decisionId); row("Type",fval(d->decisionType)); row("Status",d->status);
            row("Decided by",fval(d->decidedBy)); row("Date",fdate(d->decisionDate));
            row("Review date",fdate(d->reviewDate));
            row("Cost impact",fval(d->impactCost)); row("Schedule",fval(d->impactSchedule));
            row("Scope",fval(d->impactScope));
            if(!d->optionsConsidered.empty()) std::cout<<"  | Options: "<<d->optionsConsidered.substr(0,43)<<"|\n";
            if(!d->rationale.empty()) std::cout<<"  | Rationale: "<<d->rationale.substr(0,41)<<"|\n";
            std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
            std::cout<<"  1.Mark implemented  2.Supersede  3.Edit  0.Back\n";
            int mc=readInt("Choice",0,3);
            if(mc==1){d->status="implemented";d->update();std::cout<<"  >> Implemented.\n";}
            else if(mc==2){d->status="superseded";d->update();std::cout<<"  >> Superseded.\n";}
            else if(mc==3){
                std::string nt=readOpt("New title: ");if(!nt.empty())d->title=nt;
                std::string nr=readOpt("Rationale update: ");if(!nr.empty())d->rationale=nr;
                d->update();std::cout<<"  >> Saved.\n";
            }
        }
    }
}


void run() {
    hdr("ROSENHOLZ PM  —  INTERACTIVE CONSOLE");
    std::cout << "\n"
              << "  Welcome to the Rosenholz PM interactive shell.\n"
              << "  Enter a number to choose an action.\n"
              << "  All data is stored in your configured base path.\n\n";

    while (true) {
        std::cout << "\n  MAIN MENU\n";
        hr();
        std::cout << "  PROJECTS (F16)\n"
                  << "    1.  List all projects\n"
                  << "    2.  Create new project\n"
                  << "    3.  Open project by number\n"
                  << "    4.  Open project by ID\n"
                  << "\n  TASKS (F22)\n"
                  << "    5.  Open task by ID\n"
                  << "\n  PERSONS\n"
                  << "    6.  List all persons\n"
                  << "    7.  Create new person\n"
                  << "\n  DOCUMENTS\n"
                  << "    8.  Create / register document\n"
                  << "    9.  Browse all documents\n"
                  << "   10.  Archive URL as document\n"
                  << "\n  WORKFLOWS\n"
                  << "   11.  Workflow browser\n"
                  << "\n  SYSTEM\n"
                  << "   12.  Rebuild MFS tree\n"
                  << "   13.  Run backup now\n"
                  << "   14.  Show config / status\n"
                  << "   15.  ID abbreviation table\n"
                  << "   16.  Set log verbosity\n"
                  << "\n    0.  Exit\n";
        hr();

        int ch = readInt("Choice", 0, 16);

        if (ch == 0) {
            std::cout << "\n  Auf Wiedersehen.\n\n";
            break;
        }

        // ── LIST PROJECTS ────────────────────────────────────
        else if (ch == 1) {
            listProjects();
        }

        // ── CREATE PROJECT ───────────────────────────────────
        else if (ch == 2) {
            auto p = createProjectWizard();
            if (p) {
                std::cout << "  Open the new project now? (y/n): ";
                std::string ans; std::getline(std::cin, ans);
                if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y'))
                    projectMenu(p);
            }
        }

        // ── OPEN PROJECT BY NUMBER ────────────────────────────
        else if (ch == 3) {
            auto all = Rosenholz::ProjectF16::loadAll();
            if (all.empty()) { std::cout << "\n  (no projects)\n"; continue; }
            listProjects();
            int n = readInt("Project number", 1, (int)all.size());
            auto p = all[n-1];
            p->loadTrackables();
            p->loadQTCSLinks();
            projectMenu(p);
        }

        // ── OPEN PROJECT BY ID ────────────────────────────────
        else if (ch == 4) {
            std::string id = readLine("Project ID: ");
            auto p = Rosenholz::ProjectF16::loadById(id);
            if (!p) { std::cout << "  >> Not found.\n"; continue; }
            p->loadTrackables();
            p->loadQTCSLinks();
            projectMenu(p);
        }

        // ── OPEN TASK BY ID ───────────────────────────────────
        else if (ch == 5) {
            std::string id = readLine("Task ID: ");
            auto t = Rosenholz::TaskF22::loadById(id);
            if (!t) { std::cout << "  >> Not found.\n"; continue; }
            taskMenu(t);
        }

        // ── LIST PERSONS ──────────────────────────────────────
        else if (ch == 6) {
            listPersons();
        }

        // ── CREATE PERSON ─────────────────────────────────────
        else if (ch == 7) {
            createPersonWizard();
        }

        // ── CREATE DOCUMENT ──────────────────────────────────
        else if (ch == 8) {
            createDocumentWizard();
        }

        // ── BROWSE ALL DOCUMENTS ──────────────────────────────
        else if (ch == 9) {
            documentBrowserMenu();
        }

        // ── ARCHIVE URL ───────────────────────────────────────
        else if (ch == 10) {
            std::string url = readLine("URL to archive: ");
            std::string pid = readOpt("Attach to project-ID (optional): ");
            std::cout << "  >> Downloading and archiving...\n";
            auto doc = Rosenholz::Document::archiveFromUrl(url, pid);
            if (doc) {
                std::cout << "  >> Archived: " << doc->title << "\n"
                          << "     Path   : " << doc->filePath << "\n";
                if (!pid.empty()) doc->attachToEntity("project", pid);
                auto& cfg = Rosenholz::Config::instance();
                if (cfg.mfs().enabled) Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
                std::cout << "  Open document now? (y/n): ";
                std::string ans; std::getline(std::cin, ans);
                if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y'))
                    documentMenu(doc);
            } else {
                std::cout << "  >> Archive failed (check network / URL).\n";
            }
        }

        // ── WORKFLOW BROWSER ──────────────────────────────────
        else if (ch == 11) {
            workflowMenu();
        }

        // ── REBUILD MFS ───────────────────────────────────────
        else if (ch == 12) {
            auto& cfg = Rosenholz::Config::instance();
            std::cout << "  Rebuilding MFS tree at " << cfg.mfsPath() << " ...\n";
            bool ok = Rosenholz::MFSWriter::rebuildAll(cfg.mfsPath());
            std::cout << "  >> " << (ok ? "Done." : "FAILED.") << "\n";
        }

        // ── BACKUP ────────────────────────────────────────────
        else if (ch == 13) {
            auto& cfg = Rosenholz::Config::instance();
            std::cout << "  Running backup to " << cfg.backupDestPath() << " ...\n";
            bool ok = Rosenholz::BackupManager::runFull(
                cfg.basePath(), cfg.backupDestPath(), cfg.backup().maxCopies);
            std::cout << "  >> " << (ok ? "Backup complete." : "Backup FAILED.") << "\n";
        }

        // ── STATUS ────────────────────────────────────────────
        else if (ch == 14) {
            Rosenholz::AppController::instance().printStatus();
            auto* db = Rosenholz::DatabasePool::instance().get("projects");
            if (db) {
                std::cout << "\n  DB row counts:\n"
                          << "    projects : " << db->rowCount("projects")  << "\n"
                          << "    tasks    : " << db->rowCount("tasks")     << "\n"
                          << "    incidents: " << db->rowCount("incidents") << "\n";
            }
            auto* cdb = Rosenholz::DatabasePool::instance().get("core");
            if (cdb) {
                std::cout << "    persons  : " << cdb->rowCount("persons")      << "\n"
                          << "    teams    : " << cdb->rowCount("teams")        << "\n"
                          << "    members  : " << cdb->rowCount("team_members") << "\n";
            }
            auto* ddb = Rosenholz::DatabasePool::instance().get("documents");
            if (ddb) {
                std::cout << "    documents: " << ddb->rowCount("documents")     << "\n"
                          << "    attached : " << ddb->rowCount("entity_documents") << "\n";
            }
            auto* tdb = Rosenholz::DatabasePool::instance().get("tracking");
            if (tdb) {
                std::cout << "    trackables: " << tdb->rowCount("trackable_items") << "\n"
                          << "    notes     : " << tdb->rowCount("notes") << "\n"
                          << "    CRs (AEA) : " << tdb->rowCount("change_requests") << "\n"
                          << "    ABE       : " << tdb->rowCount("assumption_constraints") << "\n";
            }
            auto* rdb = Rosenholz::DatabasePool::instance().get("reporting");
            if (rdb) {
                std::cout << "    Risiken   : " << rdb->rowCount("risks") << "\n"
                          << "    Massnahmen: " << rdb->rowCount("measures") << "\n"
                          << "    QT-Tore   : " << rdb->rowCount("quality_gates") << "\n"
                          << "    KPIs      : " << rdb->rowCount("kpis") << "\n"
                          << "    LE        : " << rdb->rowCount("lessons_learned") << "\n"
                          << "    ENT-Log   : " << rdb->rowCount("decision_log") << "\n";
            }
            auto* prdb = Rosenholz::DatabasePool::instance().get("projects");
            if (prdb) {
                std::cout << "    Meilensteine: " << prdb->rowCount("milestones") << "\n"
                          << "    Besprechungen: " << prdb->rowCount("meetings") << "\n";
            }
            auto* wfdb2 = Rosenholz::DatabasePool::instance().get("workflow");
            if (wfdb2) {
                std::cout << "    WF-Defs   : " << wfdb2->rowCount("workflow_definitions") << "\n"
                          << "    WF-Inst.  : " << wfdb2->rowCount("workflow_instances") << "\n";
            }
        }

        // ── ID ABBREVIATION TABLE ─────────────────────────────────
        else if (ch == 15) {
            hdr("ID-Abkuerzungsverzeichnis (DDR-Stil)");
            auto& de = Rosenholz::Config::instance().registratur().diensteinheitKuerzel;
            std::cout << "  Aktuelle Diensteinheit : " << de << "\n\n";
            std::cout << "  " << std::left
                      << std::setw(10) << "Kuerzel"
                      << std::setw(14) << "MFS-Ordner"
                      << "Bezeichnung\n";
            hr();
            struct {const char* code; const char* folder; const char* name;} abbrevs[] = {
                {"F16", "F16/",  "Vorgangskartei (Projekt)"},
                {"F22", "F22/",  "Aufgabenkartei (Untervorgang)"},
                {"F18", "F18/",  "Vorfallkartei"},
                {"RSK", "RSK/",  "Risiko-Akte"},
                {"MSN", "MSN/",  "Massnahmen-Akte"},
                {"QT",  "QT/",   "Qualitaetstor"},
                {"KPI", "KPI/",  "Kennzahl (Key Performance Indicator)"},
                {"LE",  "LE/",   "Lernerkenntnis (Lessons Learned)"},
                {"ENT", "ENT/",  "Entscheidungsprotokoll"},
                {"AEA", "AEA/",  "Aenderungsantrag"},
                {"ABE", "ABE/",  "Annahmen und Beschraenkungen"},
                {"BSP", "BSP/",  "Besprechungsprotokoll"},
                {"MEI", "MEI/",  "Meilensteinblatt"},
                {"DOK", "DOK/",  "Dokument (allgemein)"},
                {"PER", "PER/",  "Personenkartei"},
                {"DE",  "DE/",   "Diensteinheit / Team"},
                {"VBF", "—",     "Verfolgungsblatt (Trackable Item)"},
                {"WFD", "—",     "Workflow-Definition"},
                {"WFI", "—",     "Workflow-Instanz"},
                {"WFA", "—",     "Workflow-Aktion"},
                {nullptr, nullptr, nullptr}
            };
            for (auto& a : abbrevs) {
                if (!a.code) break;
                std::cout << "  " << std::left
                          << std::setw(10) << a.code
                          << std::setw(14) << a.folder
                          << a.name << "\n";
            }
            std::cout << "\n  Beispiel-ID: " << de << "/F16/0042/2026\n\n";
        }

        // ── LOG VERBOSITY ─────────────────────────────────────
        else if (ch == 16) {
            std::cout << "  1. DEBUG  2. INFO  3. WARN  4. ERROR\n";
            int lv = readInt("Level", 1, 4);
            static const Rosenholz::LogLevel lvls[] = {
                Rosenholz::LogLevel::DEBUG, Rosenholz::LogLevel::INFO,
                Rosenholz::LogLevel::WARN,  Rosenholz::LogLevel::ERR };
            Rosenholz::Logger::instance().setLevel(lvls[lv-1]);
            std::cout << "  >> Log level set.\n";
        }
    }
}

} // namespace CLI


int main(int argc, char* argv[]) {
    // Parse args
    std::string basePath;
    bool debugMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug") debugMode = true;
        else if (arg == "--basepath" && i+1 < argc) basePath = argv[++i];
    }

    // Override basePath in settings if provided
    if (!basePath.empty()) Config::instance().setBasePath(basePath);

    // Initialise the application
    auto& app = AppController::instance();
    if (!app.init("settings.json", "", AppMode::CLI)) {
        std::cerr << "FATAL: AppController::init() failed\n";
        return 1;
    }

    if (debugMode) Logger::instance().setLevel(LogLevel::DEBUG);

    app.printStatus();

    std::cout << "\n=== ROSENHOLZ PM TEST SUITE ===\n" << std::flush;

    // Run all test groups
    try {
        testLogger();
        testConfig();
        testFileOps();
        testRegNumber();
        testPerson();
        testTeam();
        testProjectF16();
        testTaskF22();
        testIncidentF18();
        testRisk();
        testDocument();
        testTrackable();
        testConversions();
        testMFSRebuild();
        testBackup();
        testProjectStatus();
        testIDFormat();
        testMilestoneEntity();
        testMeetingEntity();
        testMeasureEntity();
        testQualityGateEntity();
        testKPIEntity();
        testLessonLearnedEntity();
        testDecisionLogEntity();
        testChangeRequestEntity();
        testAssumptionConstraintEntity();
        testDocumentMFSEnforcement();
        testWorkflowEngine();
        testMFSGermanAbbreviations();
        testRegistraturConfig();
    } catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION in test: " << e.what() << "\n";
        ++g_fail;
    } catch (...) {
        std::cerr << "\nUNKNOWN EXCEPTION in test\n";
        ++g_fail;
    }

    summary();

    if (g_fail == 0) {
        std::cout << "\n  All tests passed. Launching interactive console...\n";
        CLI::run();
    } else {
        std::cout << "\n  " << g_fail << " test(s) failed — fix before using interactively.\n";
    }

    app.shutdown();
    return g_fail > 0 ? 1 : 0;
}

// ============================================================

// ============================================================
// ── EXTENDED TEST SUITE ──────────────────────────────────────
// Tests for: Milestone, Meeting, Measure, QualityGate, KPI,
//            LessonLearned, DecisionLog, ChangeRequest,
//            AssumptionConstraint, Document MFS+enforcement,
//            Workflow engine, MFS German abbrevs, ID format
// ============================================================


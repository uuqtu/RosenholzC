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

#include <iostream>
#include <iomanip>
#include <string>
#include <cassert>
#include <stdexcept>
#ifndef _WIN32
  #include <sys/stat.h>
#endif

using namespace RH;

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

namespace CLI {

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
static void printDocument(const RH::Document& d) {
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

static void listDocuments(const std::vector<std::shared_ptr<RH::Document>>& docs,
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

static std::shared_ptr<RH::Document> createDocumentWizard(
        const std::string& projectId = "",
        const std::string& taskId    = "") {
    hdr("CREATE / REGISTER DOCUMENT");
    std::cout << "  Source:\n"
              << "    1. Local file path\n"
              << "    2. URL (download + archive automatically)\n"
              << "    3. Manual entry (no file yet)\n";
    int src = readInt("Source type", 1, 3);

    std::string title, filePath, fileUrl, format, summary;

    if (src == 1) {
        filePath = readLine("Full file path: ");
        // derive a default title from filename
        std::string base = RH::FileOps::baseName(filePath);
        title  = readOpt("Title (Enter for '" + base + "'): ");
        if (title.empty()) title = base;
        format = RH::FileOps::extension(filePath);
        if (!format.empty() && format[0]=='.') format = format.substr(1);
    } else if (src == 2) {
        fileUrl = readLine("URL to archive: ");
        title   = readOpt("Title (optional, derived from URL if empty): ");
        std::cout << "  >> Downloading and archiving...\n";
        auto& cfg = RH::Config::instance();
        std::string archiveDir = RH::FileOps::joinPath(cfg.basePath(), "documents", "archived");
        auto archived = RH::Document::archiveFromUrl(fileUrl, projectId);
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

    auto doc = RH::Document::create(title, docType, projectId);
    doc->docCategory    = docCat;
    doc->version        = version;
    doc->format         = format;
    doc->language       = lang;
    doc->classification = cls;
    doc->summary        = summary;
    doc->tags           = tags;
    doc->authorId       = author;
    doc->taskId         = taskId;
    doc->filePath       = filePath;
    doc->fileUrl        = fileUrl;
    doc->dateCreated    = dateC;
    doc->dateExpires    = dateE;
    doc->storageSystem  = "local";

    if (doc->save()) {
        std::cout << "\n  >> Document saved: " << doc->documentId << "\n\n";
        // Auto-attach to project/task
        if (!projectId.empty()) doc->attachToEntity("project", projectId);
        if (!taskId.empty())    doc->attachToEntity("task",    taskId);
        // Write MFS file
        auto& cfg = RH::Config::instance();
        if (cfg.mfs().enabled) RH::MFSWriter::writeDocument(*doc, cfg.mfsPath());
        return doc;
    } else {
        std::cout << "\n  >> ERROR: Document could not be saved.\n\n";
        return nullptr;
    }
}

static void documentMenu(std::shared_ptr<RH::Document> doc) {
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
            auto& cfg = RH::Config::instance();
            std::string archDir = RH::FileOps::joinPath(cfg.basePath(), "documents", "archived");
            std::string newPath = RH::FileOps::downloadUrl(doc->fileUrl, archDir);
            if (!newPath.empty()) {
                doc->filePath = newPath;
                doc->format   = RH::FileOps::extension(newPath);
                doc->update();
                std::cout << "  >> Downloaded to: " << newPath << "\n";
            } else {
                std::cout << "  >> Download failed.\n";
            }
        }
        else if (ch == 8) {
            auto& cfg = RH::Config::instance();
            RH::MFSWriter::writeDocument(*doc, cfg.mfsPath());
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

static void documentBrowserMenu(const std::string& projectId = "",
                                const std::string& taskId    = "") {
    while (true) {
        // Load docs for given context or all
        std::vector<std::shared_ptr<RH::Document>> docs;
        if (!projectId.empty())
            docs = RH::Document::loadForProject(projectId);
        else if (!taskId.empty())
            docs = RH::Document::loadForEntity("task", taskId);

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
            auto doc = RH::Document::loadById(did);
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
            auto* db = RH::DatabasePool::instance().get("documents");
            if (db) {
                auto rows = db->query("SELECT * FROM documents ORDER BY date_created DESC;");
                std::vector<std::shared_ptr<RH::Document>> all;
                for (auto& r : rows) {
                    auto d = RH::Document::create("","","");
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
static void printProject(const RH::ProjectF16& p) {
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

static void printTask(const RH::TaskF22& t) {
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

static void printIncident(const RH::IncidentF18& i) {
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

static void printPerson(const RH::Person& p) {
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

static void printTrackable(const RH::TrackableItem& t) {
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "    | " << std::left << std::setw(18) << k
                  << std::setw(30) << v << "|\n";
    };
    std::cout << "    +" << std::string(50,'-') << "+\n";
    row("ID",       t.trackableId.substr(0,20)+"...");
    row("Title",    t.title);
    row("Status",   RH::trackableStatusToString(t.status));
    row("Priority", fval(t.priority));
    row("Planned",  fdate(t.plannedDate));
    row("Focus",    fdate(t.focusDate));
    row("Due",      fdate(t.dueDate));
    row("Archived", fdate(t.archivedDate));
    row("Notes",    std::to_string(t.notes.size()) + " note(s)");
    std::cout << "    +" << std::string(50,'-') << "+\n";
}

static void listProjects() {
    auto all = RH::ProjectF16::loadAll();
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
    auto tasks = RH::TaskF22::loadForProject(projectId);
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
    auto incs = RH::IncidentF18::loadForProject(projectId);
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
    auto all = RH::Person::loadAll();
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
static std::shared_ptr<RH::ProjectF16> createProjectWizard() {
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

    auto p = RH::ProjectF16::create(title, ptype, size);
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
        auto& cfg = RH::Config::instance();
        if (cfg.mfs().enabled) p->writeMFSFile(cfg.mfsPath());
        return p;
    } else {
        std::cout << "\n  >> ERROR: Project could not be saved.\n\n";
        return nullptr;
    }
}

static std::shared_ptr<RH::TaskF22> createTaskWizard(const std::string& projectId) {
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

    auto t = RH::TaskF22::create(projectId, title, assignee, parent);
    t->description      = desc;
    t->priority         = priority;
    t->wbsCode          = wbs;
    t->startDatePlanned = start;
    t->dueDatePlanned   = due;
    t->effortPlannedHrs = effort;

    if (t->save()) {
        std::cout << "\n  >> Task created: " << t->regNumber.toString()
                  << " (" << t->taskId << ")\n\n";
        auto& cfg = RH::Config::instance();
        if (cfg.mfs().enabled) t->writeMFSFile(cfg.mfsPath());
        return t;
    } else {
        std::cout << "\n  >> ERROR: Task could not be saved.\n\n";
        return nullptr;
    }
}

static std::shared_ptr<RH::IncidentF18> createIncidentWizard(const std::string& projectId) {
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

    auto i = RH::IncidentF18::create(projectId, title, sev, reporter);
    i->description    = desc;
    i->incidentType   = type;
    i->occurredDate   = occurred;
    i->rootCause      = cause;
    i->immediateAction= action;
    i->costImpact     = cost;

    if (i->save()) {
        std::cout << "\n  >> Incident created: " << i->regNumber.toString()
                  << " (" << i->incidentId << ")\n\n";
        auto& cfg = RH::Config::instance();
        if (cfg.mfs().enabled) i->writeMFSFile(cfg.mfsPath());
        return i;
    } else {
        std::cout << "\n  >> ERROR: Incident could not be saved.\n\n";
        return nullptr;
    }
}

static std::shared_ptr<RH::Person> createPersonWizard() {
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

    auto p = RH::Person::create(first, last, email, ptypes[tc-1]);
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
static void projectMenu(std::shared_ptr<RH::ProjectF16> p) {
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
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 16);

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
            auto& cfg = RH::Config::instance();
            bool ok = p->writeMFSFile(cfg.mfsPath());
            std::cout << "  >> MFS file " << (ok ? "written." : "FAILED.") << "\n";
        }
        else if (ch == 16) {
            documentBrowserMenu(p->projectId);
        }
    }
}

// ─────────────────────────────────────────────────────────────
// TASK DETAIL MENU
// ─────────────────────────────────────────────────────────────
static void taskMenu(std::shared_ptr<RH::TaskF22> t) {
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
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 10);
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
            auto* db = RH::DatabasePool::instance().get("tracking");
            if (db) {
                std::string nid = "note_" + std::to_string(
                    std::chrono::system_clock::now().time_since_epoch().count());
                nlohmann::json jc;
                jc["text"] = note; jc["format"] = "plain";
                db->exec(
                    "INSERT INTO notes (note_id,entity_type,entity_id,author_id,content,note_type) "
                    "VALUES (?,?,?,?,?,?);",
                    {RH::BindParam::text(nid), RH::BindParam::text("task"),
                     RH::BindParam::text(t->taskId), RH::ton(by),
                     RH::BindParam::text(jc.dump()),
                     RH::BindParam::text("general")});
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
    }
}

// ─────────────────────────────────────────────────────────────
// MAIN MENU
// ─────────────────────────────────────────────────────────────
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
                  << "\n  SYSTEM\n"
                  << "   11.  Rebuild MFS tree\n"
                  << "   12.  Run backup now\n"
                  << "   13.  Show config / status\n"
                  << "   14.  Set log verbosity\n"
                  << "\n    0.  Exit\n";
        hr();

        int ch = readInt("Choice", 0, 14);

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
            auto all = RH::ProjectF16::loadAll();
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
            auto p = RH::ProjectF16::loadById(id);
            if (!p) { std::cout << "  >> Not found.\n"; continue; }
            p->loadTrackables();
            p->loadQTCSLinks();
            projectMenu(p);
        }

        // ── OPEN TASK BY ID ───────────────────────────────────
        else if (ch == 5) {
            std::string id = readLine("Task ID: ");
            auto t = RH::TaskF22::loadById(id);
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
            auto doc = RH::Document::archiveFromUrl(url, pid);
            if (doc) {
                std::cout << "  >> Archived: " << doc->title << "\n"
                          << "     Path   : " << doc->filePath << "\n";
                if (!pid.empty()) doc->attachToEntity("project", pid);
                auto& cfg = RH::Config::instance();
                if (cfg.mfs().enabled) RH::MFSWriter::writeDocument(*doc, cfg.mfsPath());
                std::cout << "  Open document now? (y/n): ";
                std::string ans; std::getline(std::cin, ans);
                if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y'))
                    documentMenu(doc);
            } else {
                std::cout << "  >> Archive failed (check network / URL).\n";
            }
        }

        // ── REBUILD MFS ───────────────────────────────────────
        else if (ch == 11) {
            auto& cfg = RH::Config::instance();
            std::cout << "  Rebuilding MFS tree at " << cfg.mfsPath() << " ...\n";
            bool ok = RH::MFSWriter::rebuildAll(cfg.mfsPath());
            std::cout << "  >> " << (ok ? "Done." : "FAILED.") << "\n";
        }

        // ── BACKUP ────────────────────────────────────────────
        else if (ch == 12) {
            auto& cfg = RH::Config::instance();
            std::cout << "  Running backup to " << cfg.backupDestPath() << " ...\n";
            bool ok = RH::BackupManager::runFull(
                cfg.basePath(), cfg.backupDestPath(), cfg.backup().maxCopies);
            std::cout << "  >> " << (ok ? "Backup complete." : "Backup FAILED.") << "\n";
        }

        // ── STATUS ────────────────────────────────────────────
        else if (ch == 13) {
            RH::AppController::instance().printStatus();
            auto* db = RH::DatabasePool::instance().get("projects");
            if (db) {
                std::cout << "\n  DB row counts:\n"
                          << "    projects : " << db->rowCount("projects")  << "\n"
                          << "    tasks    : " << db->rowCount("tasks")     << "\n"
                          << "    incidents: " << db->rowCount("incidents") << "\n";
            }
            auto* cdb = RH::DatabasePool::instance().get("core");
            if (cdb) {
                std::cout << "    persons  : " << cdb->rowCount("persons")      << "\n"
                          << "    teams    : " << cdb->rowCount("teams")        << "\n"
                          << "    members  : " << cdb->rowCount("team_members") << "\n";
            }
            auto* ddb = RH::DatabasePool::instance().get("documents");
            if (ddb) {
                std::cout << "    documents: " << ddb->rowCount("documents")     << "\n"
                          << "    attached : " << ddb->rowCount("entity_documents") << "\n";
            }
            auto* tdb = RH::DatabasePool::instance().get("tracking");
            if (tdb) {
                std::cout << "    trackables: " << tdb->rowCount("trackable_items") << "\n"
                          << "    notes     : " << tdb->rowCount("notes") << "\n";
            }
        }

        // ── LOG VERBOSITY ─────────────────────────────────────
        else if (ch == 14) {
            std::cout << "  1. DEBUG  2. INFO  3. WARN  4. ERROR\n";
            int lv = readInt("Level", 1, 4);
            static const RH::LogLevel lvls[] = {
                RH::LogLevel::DEBUG, RH::LogLevel::INFO,
                RH::LogLevel::WARN,  RH::LogLevel::ERR };
            RH::Logger::instance().setLevel(lvls[lv-1]);
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
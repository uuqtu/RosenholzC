// ============================================================
// cli_f22.cpp  —  F22 Aufgabe: Befehlshandler, Wizard, Menü
//
// Public functions:
//   cmdF22(args)             — dispatch for 'rh -f22 ...'
//   listTasks(projectId)     — see cli_utils.cpp
//   printTask(t)             — see cli_utils.cpp
//   taskMenu(t)              — interactive detail menu
//   createTaskWizard(projId) — create task under known project
//   createTaskWizardGuided() — guided: pick F16 first, then create
// ============================================================
#include "cli_common.h"
#include "../core/OperationResult.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include "../mfs/MFSWriter.h"
#include <algorithm>
#include <iomanip>

namespace CLI {

using namespace Rosenholz;


// ── editMenu (static helper for taskMenu) ────────────────────
//
// Handles all field-editing for a task (F22).
// Called from taskMenu when the task is not yet released.

static void editMenu(std::shared_ptr<Rosenholz::TaskF22> t) {
    using namespace Rosenholz;
    hdr("F22 BEARBEITEN — " + t->regNumber.toString());

    std::string title = readOpt("Titel (leer = behalten): ");
    if (!title.empty()) t->title = title;

    std::string desc = readOpt("Beschreibung (leer = behalten): ");
    if (!desc.empty()) t->description = desc;

    std::string assignee = readOpt("Bearbeiter Person-ID (leer = behalten): ");
    if (!assignee.empty()) { t->assigneeId = assignee; t->assignedBy = "system"; }

    std::cout << "  Priorität: 1.low  2.medium  3.high  4.critical  (leer = behalten)\n";
    std::string pr = readOpt("Priorität: ");
    static const char* prios[] = {"low","medium","high","critical"};
    if (pr == "1") t->priority = prios[0];
    else if (pr == "2") t->priority = prios[1];
    else if (pr == "3") t->priority = prios[2];
    else if (pr == "4") t->priority = prios[3];

    std::string pct = readOpt("Fortschritt % (0-100, leer = behalten): ");
    if (!pct.empty()) try { t->percentComplete = std::stoi(pct); } catch(...) {}

    std::string dueP = readOpt("Fälligkeitsdatum (YYYY-MM-DD, leer = behalten): ");
    if (!dueP.empty()) t->dueDatePlanned = dueP;

    std::string eff = readOpt("Geplanter Aufwand Stunden (leer = behalten): ");
    if (!eff.empty()) try { t->effortPlannedHrs = std::stod(eff); } catch(...) {}

    std::string wbs = readOpt("WBS-Code (leer = behalten): ");
    if (!wbs.empty()) t->wbsCode = wbs;

    if (opOk(t->update())) std::cout << "  >> Gespeichert.\n";
    else             std::cout << "  >> Fehler beim Speichern.\n";
}

// ── mainWorkflowMenu (static helper for taskMenu) ────────────
//
// Shows the F77 release workflow sub-menu for a task.
// Called from taskMenu option 6 (F77-Workflow).

static void mainWorkflowMenu(std::shared_ptr<Rosenholz::TaskF22> t) {
    using namespace Rosenholz;
    while (true) {
        if (auto fresh = TaskF22::loadById(t->taskId)) *t = *fresh;
        hdr("F77 — " + t->taskId);
        std::cout << "  Status       : " << t->status << "\n";

        if (t->releaseWorkflowId.empty()) {
            std::cout << "  Kein F77 aktiv.\n";
            std::cout << "  1. F77 starten...    0. Zurück\n";
            int ch = readInt("Wahl", 0, 1);
            if (ch == 0) return;
            std::string wid = startWfInstanceWizard("f22", t->taskId);
            if (!wid.empty()) instanceMenu(wid);
            return;
        }

        auto wf = F77_Workflow::loadById(t->releaseWorkflowId);
        std::string wfStatus = wf ? wf->status : "unbekannt";
        std::cout << "  F77-ID  : " << t->releaseWorkflowId.substr(0, 36) << "\n";
        std::cout << "  WF-Status    : " << wfStatus << "\n";

        if (wfStatus == "active") {
            int blockers = 0;
            F77_Engine::canRelease("f22", t->taskId, t->releaseWorkflowId, blockers);
            std::cout << (blockers > 0
                ? "  ! " + std::to_string(blockers) + " Schritte blockieren Freigabe\n"
                : "  ✓ Freigabe moeglich\n");
        }
        std::cout << "  1. F77 öffnen    0. Zurück\n";
        int ch = readInt("Wahl", 0, 1);
        if (ch == 0) return;
        if (ch == 1) instanceMenu(t->releaseWorkflowId);
        return;
    }
}


// ── createTaskWizard ──────────────────────────────────────────
//
// Step-by-step wizard for a new F22 task under a known project.
// The project ID must be supplied by the caller (either directly
// or via createTaskWizardGuided).

std::shared_ptr<TaskF22> createTaskWizard(const std::string& projectId) {
    hdr("F22 ANLEGEN (Vorgangskartei)");
    std::string title     = readLine("Titel: ");
    if (title.empty()) return nullptr;
    std::string desc      = readOpt("Beschreibung (optional): ");
    std::string assignee  = readOpt("Bearbeiter Person-ID (optional): ");
    std::string priority  = readOpt("Priorität (high/medium/low, leer=medium): ");
    std::string wbs       = readOpt("WBS-Code (z.B. 1.2.3, optional): ");
    std::string startP    = readOpt("Geplanter Start (YYYY-MM-DD, optional): ");
    std::string due       = readOpt("Fälligkeitsdatum (YYYY-MM-DD, optional): ");
    std::string effortStr = readOpt("Geplanter Aufwand Stunden (optional): ");
    double effort = 0.0;
    if (!effortStr.empty()) try { effort = std::stod(effortStr); } catch (...) {}

    auto t = Rosenholz::TaskF22::create(projectId, title, assignee, "");
    t->description       = desc;
    t->priority          = priority.empty() ? "medium" : priority;
    t->wbsCode           = wbs;
    t->startDatePlanned  = startP;
    t->dueDatePlanned    = due;
    t->effortPlannedHrs  = effort;

    if (opOk(t->save())) {
        std::cout << "\n  >> F22-Vorgang angelegt: " << t->regNumber.toString()
                  << " (" << t->taskId << ")\n";
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) t->writeMFSFile(cfg.mfsPath());
        return t;
    } else {
        std::cout << "\n  >> FEHLER: F22 konnte nicht gespeichert werden.\n";
        return nullptr;
    }
}

// ── createTaskWizardGuided ────────────────────────────────────
//
// Guided variant: first shows all F16 projects so the user can
// pick one by number, then runs the normal task creation wizard.
// Invoked by 'rh -f22 -n'.

std::shared_ptr<TaskF22> createTaskWizardGuided() {
    auto projects = ProjectF16::loadAll();
    if (projects.empty()) {
        std::cout << "  (keine F16-Karten vorhanden — bitte zuerst ein F16 anlegen)\n";
        return nullptr;
    }

    hdr("F22 ANLEGEN — F16 WÄHLEN");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(26) << "REG-NR"
              << "TITEL\n"
              << "  " << std::string(62, '-') << "\n";

    for (int i = 0; i < (int)projects.size(); ++i)
        std::cout << "  " << std::setw(4)  << (i + 1)
                  << std::setw(26) << projects[i]->regNumber.toString()
                  << projects[i]->title.substr(0, 36) << "\n";

    int pick = readInt("F16-Nr", 1, (int)projects.size());
    return createTaskWizard(projects[pick - 1]->projectId);
}

// ── cmdF22 ────────────────────────────────────────────────────
//
// Dispatch table for 'rh -f22 [args]':
//
//   rh -f22               → list 20 most recent tasks
//   rh -f22 -n            → guided: pick F16, then create task
//   rh -f22 <project-id>  → list tasks for that project
//   rh -f22 <project-id> (anything)  → create task under project
//   rh -f22 <task-id>     → open taskMenu for that task

void cmdF22(const std::vector<std::string>& args) {

    // No arguments: list 20 most recent tasks
    if (args.empty()) {
        auto all = TaskF22::loadRecent(20);
        if (all.empty()) { std::cout << "  (keine F22-Vorgänge)\n"; return; }
        std::cout << "  " << std::left
                  << std::setw(24) << "REG-NR"
                  << std::setw(32) << "TITEL"
                  << "STATUS\n"
                  << "  " << std::string(64, '-') << "\n";
        for (auto& t : all)
            std::cout << "  " << std::setw(28) << t->taskId
                      << std::setw(32) << t->title.substr(0, 30)
                      << t->status << "\n";
        std::cout << "  " << all.size() << " F22\n";
        return;
    }

    // -n  —  guided creation (list F16, pick one, then create task)
    if (args[0] == "-n" || args[0] == "--neu") {
        auto task = createTaskWizardGuided();
        if (task) printOk("  >> F22 angelegt: " + task->regNumber.toString()
                          + "  " + task->title);
        return;
    }

    // All remaining paths require a valid ID
    if (!isId(args[0])) { printErr("Ungültiges Argument: " + args[0]
                            + "  (erwartet ID oder -n)"); return; }

    // Try as task ID first
    auto task = TaskF22::loadById(args[0]);
    if (task) {
        taskMenu(task);
        return;
    }

    // Try as project ID
    auto project = ProjectF16::loadById(args[0]);
    if (!project) { printErr("ID nicht gefunden (weder F22 noch F16): " + args[0]); return; }

    if (args.size() > 1) {
        // Any additional argument signals creation
        auto t = createTaskWizard(project->projectId);
        if (t) printOk("  >> F22 angelegt: " + t->regNumber.toString()
                       + "  " + t->title);
    } else {
        // Just a project ID → list its tasks
        listTasks(project->projectId);
    }
}



// ── void taskMenu(std::shared_ptr<TaskF22> t) handlers ──────────────────────────────────────────────
// ── F22 taskMenu handlers ───────────────────────────────────────────────

static bool f22_edit(std::shared_ptr<TaskF22> t) {
    editMenu(t); return true;
}

static bool f22_dok_list(std::shared_ptr<TaskF22> t) {
    auto docs = Document::loadForEntity("f22", t->taskId);
    if (docs.empty()) { std::cout << "  (keine Dokumente)\n"; return true; }
    int n = 1;
    for (auto& d : docs)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(26) << d->documentId.substr(0,24)
                  << "  " << d->title.substr(0,30) << "  " << d->docType << "\n";
    return true;
}

static bool f22_dok_open(std::shared_ptr<TaskF22> t) {
    auto docs = Document::loadForEntity("f22", t->taskId);
    if (docs.empty()) { std::cout << "  (keine Dokumente)\n"; return true; }
    int n=1;
    for (auto& d : docs)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(26) << d->documentId.substr(0,24)
                  << "  " << d->title.substr(0,30) << "\n";
    int pick = readInt("DOK #", 1, (int)docs.size());
    documentMenu(docs[pick-1]);
    return true;
}

static bool f22_dok_new(std::shared_ptr<TaskF22> t) {
    if (!t->canAddChildren()) {
        std::cout << "  >> " << opResultMessage(OperationResult::ENTITY_RELEASED) << "\n";
        return true;
    }
    auto doc = createDocumentWizard(t->taskId, "");
    if (doc) documentMenu(doc);
    return true;
}

static bool f22_f18_list(std::shared_ptr<TaskF22> t) {
    auto items = F18Operation::loadForTask(t->taskId);
    if (items.empty()) { std::cout << "  (keine F18-Vorgaenge)\n"; return true; }
    int n = 1;
    for (auto& v : items)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << "[" << std::left << std::setw(14) << v->vorgangType.substr(0,13) << "] "
                  << std::setw(28) << v->title.substr(0,26)
                  << "  " << v->status << "\n";
    return true;
}

static bool f22_f18_open(std::shared_ptr<TaskF22> t) {
    auto items = F18Operation::loadForTask(t->taskId);
    if (items.empty()) { std::cout << "  (keine F18-Vorgaenge)\n"; return true; }
    int n=1;
    for (auto& v : items)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << "[" << std::left << std::setw(14) << v->vorgangType.substr(0,13) << "] "
                  << v->title.substr(0,30) << "\n";
    int pick = readInt("F18 #", 1, (int)items.size());
    f18Menu(items[pick-1]);
    return true;
}

static bool f22_f18_new(std::shared_ptr<TaskF22> t) {
    if (!t->canAddChildren()) {
        std::cout << "  >> " << opResultMessage(OperationResult::ENTITY_RELEASED) << "\n";
        return true;
    }
    // Pre-select this F22 as the parent, then launch guided wizard
    auto v = createF18Wizard(t->taskId);
    if (v) f18Menu(v);
    return true;
}

static bool f22_kom_list(std::shared_ptr<TaskF22> t) {
    listComms(t->taskId, "f22"); return true;
}

static bool f22_kom_open(std::shared_ptr<TaskF22> t) {
    auto items = listComms(t->taskId, "f22");
    if (items.empty()) return true;
    int pick = readInt("KOM #", 1, (int)items.size());
    commDetailMenu(items[pick-1]);
    return true;
}

static bool f22_kom_new(std::shared_ptr<TaskF22> t) {
    communicationMenu(t->taskId, "f22"); return true;
}

static bool f22_f77(std::shared_ptr<TaskF22> t) {
    mainWorkflowMenu(t); return true;
}

using tskMenuFn = bool(*)(std::shared_ptr<TaskF22> t);
static const tskMenuFn tskMenuTable[12] = {
    nullptr,       // 0
    f22_edit,      // 1
    f22_dok_list,  // 2
    f22_dok_open,  // 3
    f22_dok_new,   // 4
    f22_f18_list,  // 5
    f22_f18_open,  // 6
    f22_f18_new,   // 7
    f22_kom_list,  // 8
    f22_kom_open,  // 9
    f22_kom_new,   // 10
    f22_f77,       // 11
};

void taskMenu(std::shared_ptr<TaskF22> t) {
    while (true) {
        if (auto fresh = TaskF22::loadById(t->taskId)) *t = *fresh;
        printTask(*t);
        if (t->isReleased())
            std::cout << "  ⚠ RELEASED — keine weiteren Aenderungen moeglich\n";
        std::cout
            << "  1.Bearbeiten\n"
            << "  DOK: 2.listen | 3.<#> | 4.neu\n"
            << "  F18: 5.listen | 6.<#> | 7.neu\n"
            << "  KOM: 8.listen | 9.<#> | 10.neu\n"
            << "  11.F77  0.Zurück\n";
        int ch = readInt("Wahl", 0, 11);
        if (ch == 0) break;
        if (ch >= 1 && ch <= 11 && tskMenuTable[ch])
            if (!tskMenuTable[ch](t)) break;
    }
}

} // namespace CLI

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
    hdr("AUFGABE BEARBEITEN — " + t->regNumber.toString());

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

    if (t->update()) std::cout << "  >> Gespeichert.\n";
    else             std::cout << "  >> Fehler beim Speichern.\n";
}

// ── mainWorkflowMenu (static helper for taskMenu) ────────────
//
// Shows the F77 release workflow sub-menu for a task.
// Called from taskMenu option 6 (F77-Workflow).

static void mainWorkflowMenu(std::shared_ptr<Rosenholz::TaskF22> t) {
    using namespace Rosenholz;
    while (true) {
        hdr("F77 WORKFLOW — " + t->regNumber.toString());
        std::cout << "  Status : " << t->status << "\n";

        if (t->releaseWorkflowId.empty()) {
            std::cout << "  (kein F77-Workflow aktiv)\n";
            std::cout << "  1. Workflow starten    0. Zurück\n";
            int ch = readInt("Wahl", 0, 1);
            if (ch == 0) return;
            auto* t22db = DatabasePool::instance().get("f22");
            auto wf = F77_Engine::startDefault("f22", t->taskId);
            if (wf) {
                t->releaseWorkflowId = wf->workflowId;
                if (t22db) t22db->exec(
                    "UPDATE tasks SET release_workflow_id=?, updated_at=? WHERE task_id=?;",
                    {BindParam::text(wf->workflowId),
                     BindParam::text(nowIso()),
                     BindParam::text(t->taskId)});
                std::cout << "  >> Workflow gestartet: " << wf->workflowId << "\n";
                instanceMenu(wf->workflowId);
            }
            return;
        }

        std::cout << "  Workflow-ID: " << t->releaseWorkflowId.substr(0, 36) << "\n";
        std::cout << "  1. Workflow öffnen    0. Zurück\n";
        int ch = readInt("Wahl", 0, 1);
        if (ch == 0) return;
        if (ch == 1) { instanceMenu(t->releaseWorkflowId); return; }
    }
}


// ── createTaskWizard ──────────────────────────────────────────
//
// Step-by-step wizard for a new F22 task under a known project.
// The project ID must be supplied by the caller (either directly
// or via createTaskWizardGuided).

std::shared_ptr<TaskF22> createTaskWizard(const std::string& projectId) {
    hdr("AUFGABE ANLEGEN (F22)");
    std::string title     = readLine("Titel: ");
    if (title.empty()) return nullptr;
    std::string desc      = readOpt("Beschreibung (optional): ");
    std::string assignee  = readOpt("Bearbeiter Person-ID (optional): ");
    std::string parent    = readOpt("Übergeordnete Aufgabe-ID (optional): ");
    std::string priority  = readOpt("Priorität (high/medium/low, leer=medium): ");
    std::string wbs       = readOpt("WBS-Code (z.B. 1.2.3, optional): ");
    std::string startP    = readOpt("Geplanter Start (YYYY-MM-DD, optional): ");
    std::string due       = readOpt("Fälligkeitsdatum (YYYY-MM-DD, optional): ");
    std::string effortStr = readOpt("Geplanter Aufwand Stunden (optional): ");
    double effort = 0.0;
    if (!effortStr.empty()) try { effort = std::stod(effortStr); } catch (...) {}

    auto t = Rosenholz::TaskF22::create(projectId, title, assignee, parent);
    t->description       = desc;
    t->priority          = priority.empty() ? "medium" : priority;
    t->wbsCode           = wbs;
    t->startDatePlanned  = startP;
    t->dueDatePlanned    = due;
    t->effortPlannedHrs  = effort;

    if (t->save()) {
        std::cout << "\n  >> Aufgabe angelegt: " << t->regNumber.toString()
                  << " (" << t->taskId << ")\n\n";
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) t->writeMFSFile(cfg.mfsPath());
        return t;
    } else {
        std::cout << "\n  >> FEHLER: Aufgabe konnte nicht gespeichert werden.\n\n";
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
        std::cout << "  (keine Projekte vorhanden — bitte zuerst ein F16 anlegen)\n";
        return nullptr;
    }

    hdr("AUFGABE ANLEGEN — PROJEKT WÄHLEN");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(26) << "REG-NR"
              << "TITEL\n"
              << "  " << std::string(62, '-') << "\n";

    for (int i = 0; i < (int)projects.size(); ++i)
        std::cout << "  " << std::setw(4)  << (i + 1)
                  << std::setw(26) << projects[i]->regNumber.toString()
                  << projects[i]->title.substr(0, 36) << "\n";

    int pick = readInt("Projektnummer", 1, (int)projects.size());
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
        if (all.empty()) { std::cout << "  (keine Aufgaben)\n"; return; }
        std::cout << "  " << std::left
                  << std::setw(24) << "REG-NR"
                  << std::setw(32) << "TITEL"
                  << "STATUS\n"
                  << "  " << std::string(64, '-') << "\n";
        for (auto& t : all)
            std::cout << "  " << std::setw(24) << t->regNumber.toString()
                      << std::setw(32) << t->title.substr(0, 30)
                      << t->status << "\n";
        std::cout << "  " << all.size() << " Aufgabe(n)\n";
        return;
    }

    // -n  —  guided creation (list F16, pick one, then create task)
    if (args[0] == "-n" || args[0] == "--neu") {
        auto task = createTaskWizardGuided();
        if (task) printOk("  >> Aufgabe angelegt: " + task->regNumber.toString()
                          + "  " + task->title);
        return;
    }

    // All remaining paths require a valid ID
    if (!isId(args[0])) die("Ungültiges Argument: " + args[0]
                            + "  (erwartet ID oder -n)");

    // Try as task ID first
    auto task = TaskF22::loadById(args[0]);
    if (task) {
        taskMenu(task);
        return;
    }

    // Try as project ID
    auto project = ProjectF16::loadById(args[0]);
    if (!project) die("ID nicht gefunden (weder F22 noch F16): " + args[0]);

    if (args.size() > 1) {
        // Any additional argument signals creation
        auto t = createTaskWizard(project->projectId);
        if (t) printOk("  >> Aufgabe angelegt: " + t->regNumber.toString()
                       + "  " + t->title);
    } else {
        // Just a project ID → list its tasks
        listTasks(project->projectId);
    }
}


void taskMenu(std::shared_ptr<TaskF22> t) {
    while (true) {
        if (auto fresh = TaskF22::loadById(t->taskId)) *t = *fresh;
        printTask(*t);

        if (t->isReleased())
            std::cout << "  ⚠ RELEASED — keine weiteren Aenderungen moeglich\n\n";

        std::cout
            << "  [AUFGABE]\n"
            << "    1. Bearbeiten (Edit-Untermenü)\n"
            << "    2. Teilaufgabe anlegen\n"
            << "\n  [DOKUMENTE]\n"
            << "    3. Dokumente anzeigen\n"
            << "    4. Neues Dokument anlegen\n"
            << "\n  [F18 VORGÄNGE]\n"
            << "    5. F18 Vorgänge anzeigen / öffnen\n"
            << "    6. Neuen F18 Vorgang anlegen\n"
            << "\n  [KOMMUNIKATION]\n"
            << "    7. Communications (Meetings, Calls, ...)\n"
            << "\n  [MAIN WORKFLOW]\n"
            << "    8. Main Workflow / Freigabe\n"
            << "\n    0. Zurück\n";
        hr();

        int ch = readInt("Wahl",0,8); if (ch==0) break;

        if (ch==1) {
            if (t->canEdit()) editMenu(t);
            else std::cout << "  >> Released — kein Bearbeiten.\n";

        } else if (ch==2) {
            if (!t->canAddChildren()) { std::cout << "  >> Released — keine neuen Teilaufgaben.\n"; continue; }
            auto child = createTaskWizard(t->projectId);
            if (child) {
                child->parentTaskId = t->taskId;
                child->update();
                std::cout << "  Teilaufgabe öffnen? (j/n): ";
                std::string yn; std::getline(std::cin, yn);
                if (yn=="j"||yn=="J") taskMenu(child);
            }

        } else if (ch==3) {
            documentBrowserMenu("", t->taskId);

        } else if (ch==4) {
            if (!t->canAddChildren()) { std::cout << "  >> Released — keine neuen Dokumente.\n"; continue; }
            auto doc = createDocumentWizard(t->projectId, t->taskId);
            if (doc) documentMenu(doc);

        } else if (ch==5) {
            f18BrowserMenu("", t->taskId);

        } else if (ch==6) {
            if (!t->canAddChildren()) { std::cout << "  >> Released — keine neuen F18-Vorgaenge.\n"; continue; }
            auto v = createF18Wizard(t->projectId, t->taskId);
            if (v) f18Menu(v);

        } else if (ch==7) {
            communicationMenu(t->taskId, "task");

        } else if (ch==8) {
            mainWorkflowMenu(t);
        }
    }
}

} // namespace CLI

// ============================================================
// cli_f16.cpp  —  F16 Projekt: Befehlshandler, Wizard, Menü
//
// Public functions:
//   cmdF16(args)        — dispatch for 'rh -f16 ...'
//   listProjects()      — see cli_utils.cpp
//   printProject(p)     — see cli_utils.cpp
//   projectMenu(p)      — interactive detail menu
//   createProjectWizard — step-by-step project creation
// ============================================================
#include "cli_common.h"
#include "../core/OperationResult.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include "../mfs/MFSWriter.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace CLI {

using namespace Rosenholz;

// ── cmdF16 ────────────────────────────────────────────────────
//
// Dispatch table for 'rh -f16 [args]':
//
//   rh -f16                    → list all projects (listProjects)
//   rh -f16 -s <query>         → search by title / reg-number
//   rh -f16 -status <status>   → filter by status string
//   rh -f16 <id>               → open projectMenu for that project
//   rh -f16 -n                 → guided creation wizard
//   rh -f16 (anything else)    → direct creation wizard

void cmdF16(const std::vector<std::string>& args) {

    // No arguments: list all projects
    if (args.empty()) {
        listProjects();
        return;
    }

    // -s <query>  —  search by title or registration number
    if (args[0] == "-s" || args[0] == "--search") {
        std::string q = args.size() > 1 ? args[1] : readLine("Suche: ");
        if (q.empty()) return;
        std::string ql = q;
        std::transform(ql.begin(), ql.end(), ql.begin(), ::tolower);

        auto all = ProjectF16::loadAll();
        int found = 0;
        for (auto& p : all) {
            std::string t = p->title, r = p->regNumber.toString();
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            std::transform(r.begin(), r.end(), r.begin(), ::tolower);
            if (t.find(ql) != std::string::npos || r.find(ql) != std::string::npos) {
                std::cout << "  " << std::left << std::setw(26) << p->regNumber.toString()
                          << "  " << std::setw(36) << p->title.substr(0, 35)
                          << "  [" << p->status << "]\n";
                ++found;
            }
        }
        if (!found) std::cout << "  (keine Treffer für \"" << q << "\")\n";
        return;
    }

    // -status <status>  —  filter by lifecycle status
    if (args[0] == "-status" || args[0] == "--status") {
        std::string st = args.size() > 1 ? args[1] : readLine("Status: ");
        auto list = ProjectF16::loadByStatus(st);
        if (list.empty()) {
            std::cout << "  (keine F16 mit status=" << st << ")\n";
            return;
        }
        for (auto& p : list)
            std::cout << "  " << std::left << std::setw(26) << p->regNumber.toString()
                      << "  " << p->title.substr(0, 38) << "\n";
        std::cout << "  " << list.size() << " Treffer.\n";
        return;
    }

    // -n  —  guided creation (same as direct wizard; kept for symmetry)
    if (args[0] == "-n" || args[0] == "--neu") {
        auto p = createProjectWizard();
        if (p) printOk("  >> F16 angelegt: " + p->regNumber.toString() + "  " + p->title);
        return;
    }

    // <id>  —  open project menu
    if (isId(args[0])) {
        auto p = ProjectF16::loadById(args[0]);
        if (!p) { printErr("F16 nicht gefunden: " + args[0]); return; }
        projectMenu(p);
        return;
    }

    // Anything else → direct creation wizard
    auto p = createProjectWizard();
    if (p) printOk("  >> F16 angelegt: " + p->regNumber.toString() + "  " + p->title);
}


// ── editMenu (static helper for projectMenu) ──────────────────
//
// Handles all field-editing for a project.
// Called from projectMenu when the project is not yet released.

static void editMenu(std::shared_ptr<Rosenholz::ProjectF16> p) {
    using namespace Rosenholz;
    hdr("F16 BEARBEITEN — " + p->regNumber.toString());

    // Title and core fields
    std::string title = readOpt("Titel (leer = behalten): ");
    if (!title.empty()) p->title = title;

    std::string phase = readOpt("Phase (leer = behalten): ");
    if (!phase.empty()) p->phase = phase;

    std::cout << "  Priorität: 1.low  2.medium  3.high  4.critical  (leer = behalten)\n";
    std::string pr = readOpt("Priorität: ");
    static const char* prios[] = {"low","medium","high","critical"};
    if (pr == "1") p->priority = prios[0];
    else if (pr == "2") p->priority = prios[1];
    else if (pr == "3") p->priority = prios[2];
    else if (pr == "4") p->priority = prios[3];

    std::string complex = readOpt("Komplexität (simple/moderate/complex, leer = behalten): ");
    if (!complex.empty()) p->complexity = complex;

    std::string meth = readOpt("Methodik (agile/waterfall/kanban, leer = behalten): ");
    if (!meth.empty()) p->methodology = meth;

    // Scope
    std::string scope = readOpt("Scope-Beschreibung (leer = behalten): ");
    if (!scope.empty()) {
        p->scopeStatement  = scope;
        p->scopeVersion    = readOpt("Scope-Version (leer = behalten): ");
        p->scopeChangeReason = readOpt("Änderungsgrund: ");
        p->scopeChangeCount++;
        p->scopeLastChanged = nowIso();
    }

    // Dates
    std::string startP = readOpt("Geplanter Start (YYYY-MM-DD, leer = behalten): ");
    if (!startP.empty()) p->startDatePlanned = startP;

    std::string endP = readOpt("Geplantes Ende (YYYY-MM-DD, leer = behalten): ");
    if (!endP.empty()) p->endDatePlanned = endP;

    // Budget
    std::string bgt = readOpt("Budget geplant EUR (leer = behalten): ");
    if (!bgt.empty()) { try { p->budgetPlanned = std::stod(bgt); } catch(...) {} }

    std::string bga = readOpt("Budget genehmigt EUR (leer = behalten): ");
    if (!bga.empty()) { try { p->budgetApproved = std::stod(bga); } catch(...) {} }

    std::string bact = readOpt("Budget ist EUR (leer = behalten): ");
    if (!bact.empty()) {
        try { p->budgetActual = std::stod(bact); } catch(...) {}
        p->recalcEarnedValue();
    }

    // Notes, ext ref, links
    std::string ext = readOpt("Externe Referenz (leer = behalten): ");
    if (!ext.empty()) p->externalRef = ext;

    std::string links = readOpt("Links (leer = behalten): ");
    if (!links.empty()) p->links = links;

    if (opOk(p->update())) std::cout << "  >> Gespeichert.\n";
    else             std::cout << "  >> Fehler beim Speichern.\n";
}

// ── mainWorkflowMenu (static helper for projectMenu) ──────────
//
// Shows the F77 release workflow sub-menu for a project.
// Called from projectMenu option 6 (F77-Workflow).

static void mainWorkflowMenu(std::shared_ptr<Rosenholz::ProjectF16> p) {
    using namespace Rosenholz;
    while (true) {
        if (auto fresh = ProjectF16::loadById(p->projectId)) *p = *fresh;
        hdr("F77 — " + p->projectId);
        std::cout << "  Status       : " << p->status << "\n";

        if (p->releaseWorkflowId.empty()) {
            // No workflow yet — offer to start one (always asks targetState via wizard)
            std::cout << "  Kein F77 aktiv.\n";
            std::cout << "  1. F77 starten...    0. Zurück\n";
            int ch = readInt("Wahl", 0, 1);
            if (ch == 0) return;
            std::string wid = startWfInstanceWizard("f16", p->projectId);
            if (!wid.empty()) instanceMenu(wid);
            return;
        }

        // Workflow exists
        auto wf = F77_Workflow::loadById(p->releaseWorkflowId);
        std::string wfStatus = wf ? wf->status : "unbekannt";
        std::cout << "  F77-ID  : " << p->releaseWorkflowId.substr(0, 36) << "\n";
        std::cout << "  WF-Status    : " << wfStatus << "\n";

        if (wfStatus == "active") {
            int blockers = 0;
            F77_Engine::canRelease("f16", p->projectId, p->releaseWorkflowId, blockers);
            if (blockers > 0)
                std::cout << "  ! " << blockers << " Schritte blockieren Freigabe\n";
            else
                std::cout << "  ✓ Freigabe moeglich\n";
            std::cout << "  1. F77 öffnen    0. Zurück\n";
            int ch = readInt("Wahl", 0, 1);
            if (ch == 1) instanceMenu(p->releaseWorkflowId);
        } else if (wfStatus == "completed" || wfStatus == "cancelled") {
            // Completed or cancelled: allow starting a new one
            std::cout << "  (abgeschlossen oder abgebrochen)\n";
            std::cout << "  1. Neuen F77 starten...    0. Zurück\n";
            int ch = readInt("Wahl", 0, 1);
            if (ch == 1) {
                std::string wid = startWfInstanceWizard("f16", p->projectId);
                if (!wid.empty()) instanceMenu(wid);
            }
        } else {
            std::cout << "  1. F77 öffnen    0. Zurück\n";
            int ch = readInt("Wahl", 0, 1);
            if (ch == 1) instanceMenu(p->releaseWorkflowId);
        }
        return;
    }
}

// ── createProjectWizard ───────────────────────────────────────
//
// Step-by-step wizard for new F16 projects.
// Asks for: title, type, size, codename, priority, complexity,
//           methodology, scope, start/end dates, budget.
// Saves to DB and writes MFS file if MFS is enabled.

std::shared_ptr<Rosenholz::ProjectF16> createProjectWizard() {
    hdr("F16 ANLEGEN (Projektkartei)");
    std::string title = readLine("Title: ");
    std::cout << "  F16-Typ:\n"
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

    if (opOk(p->save())) {
        // write MFS file
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) p->writeMFSFile(cfg.mfsPath());
        return p;
    } else {
        std::cout << "\n  >> FEHLER: F16 konnte nicht gespeichert werden.\n";
        return nullptr;
    }
}






// ── projectMenu ───────────────────────────────────────────────
//
// Interactive detail menu for a single F16 project.
// Re-loads the project each loop iteration to pick up any
// changes made by sub-menus.


// ── void projectMenu(std::shared_ptr<ProjectF16> p) handlers ──────────────────────────────────────────────
static bool prjMenuOpt1(std::shared_ptr<ProjectF16> p) {
    if (p->canEdit()) editMenu(p);
    else std::cout << "  >> Released — kein Bearbeiten moeglich.\n";


    return true;
}

// 2: Liste aller F22
static bool prjMenuOpt2(std::shared_ptr<ProjectF16> p) {
    listTasks(p->projectId);
    return true;
}

// 3: F22 nach ID öffnen
static bool prjMenuOpt3_openById(std::shared_ptr<ProjectF16> p) {
    std::string id = readLine("F22-ID: ");
    if (id.empty()) return true;
    auto t = Rosenholz::TaskF22::loadById(id);
    if (!t || t->projectId != p->projectId) {
        std::cout << "  >> F22 nicht gefunden oder nicht in diesem F16.\n";
        return true;
    }
    taskMenu(t);
    return true;
}

static bool prjMenuOpt3(std::shared_ptr<ProjectF16> p) {
    if (!p->canAddChildren()) { std::cout << "  >> " << opResultMessage(p->isWorkflowComplete() ? OperationResult::ENTITY_WF_COMPLETE : OperationResult::ENTITY_RELEASED) << " — keine neuer F22-Vorgangn.\n"; return true; }
    auto task = createTaskWizard(p->projectId);
    if (task) {
std::cout << "  F22 jetzt öffnen? (j/n): ";
std::string yn; std::getline(std::cin, yn);
if (yn=="j"||yn=="J") taskMenu(task);
    }


    return true;
}

static bool prjMenuOpt4(std::shared_ptr<ProjectF16> p) {
    documentBrowserMenu();


    return true;
}

// 5: DOK alle
static bool prjMenuOpt5(std::shared_ptr<ProjectF16> p) {
    documentBrowserMenu();
    return true;
}

// 6: DOK nach ID öffnen
static bool prjMenuOpt5_openById(std::shared_ptr<ProjectF16> p) {
    std::string id = readLine("DOK-ID: ");
    if (id.empty()) return true;
    auto d = Rosenholz::Document::loadById(id);
    if (!d) {
        std::cout << "  >> DOK nicht gefunden.\n";
        return true;
    }
    // Verify the DOK is linked to a F22 under this F16:
    if (!d->taskId.empty()) {
        auto t = Rosenholz::TaskF22::loadById(d->taskId);
        if (!t || t->projectId != p->projectId) {
            std::cout << "  >> DOK gehört nicht zu diesem F16.\n";
            return true;
        }
    }
    documentMenu(d);
    return true;
}

// 7: DOK neu
static bool prjMenuOpt5_new(std::shared_ptr<ProjectF16> p) {
    if (!p->canAddChildren()) { std::cout << "  >> " << opResultMessage(p->isWorkflowComplete() ? OperationResult::ENTITY_WF_COMPLETE : OperationResult::ENTITY_RELEASED) << " — kein neues DOK.\n"; return true; }
    auto doc = createDocumentWizard();
    if (doc) documentMenu(doc);
    return true;
}



static bool prjMenuOpt6(std::shared_ptr<ProjectF16> p) {
    communicationMenu(p->projectId, "project");


    return true;
}

static bool prjMenuOpt7(std::shared_ptr<ProjectF16> p) {
    hdr("MEILENSTEIN-NOTIZEN — " + p->projectId.substr(0,20));
    if (!p->milestones.empty())
std::cout << "  Aktuell:\n" << p->milestones << "\n";
    else
std::cout << "  (keine Notizen)\n";
    std::cout << "  1.Neu schreiben  2.Anfügen  0.Zurück\n";
    int ms = readInt("Wahl",0,2);
    if (ms==1) {
std::cout << "  Text (leere Zeile = fertig):\n";
std::string all, line;
while (std::getline(std::cin, line) && !line.empty())
    all += line + "\n";
p->milestones = all; p->update();
std::cout << "  >> Gespeichert.\n";
    } else if (ms==2) {
std::string add = readLine("Anfügen: ");
if (!add.empty()) {
    if (!p->milestones.empty()) p->milestones += "\n";
    p->milestones += add; p->update();
    std::cout << "  >> Angefügt.\n";
}
    }


    return true;
}

static bool prjMenuOpt6_NEW(std::shared_ptr<ProjectF16> p) {
    mainWorkflowMenu(p);

    return true;
}

using prjMenuFn = bool(*)(std::shared_ptr<ProjectF16> p);
static const prjMenuFn prjMenuTable[11] = {
    nullptr,           // 0
    prjMenuOpt1,       // 1 Bearbeiten
    prjMenuOpt2,       // 2 F22: Alle
    prjMenuOpt3_openById, // 3 F22: Öffnen per ID
    prjMenuOpt3,       // 4 F22: Neu
    prjMenuOpt5,       // 5 DOK: Alle
    prjMenuOpt5_openById, // 6 DOK: Öffnen per ID
    prjMenuOpt5_new,   // 7 DOK: Neu
    prjMenuOpt6,       // 8 Komm.
    prjMenuOpt7,       // 9 Meilenstein
    prjMenuOpt6_NEW,   // 10 F77
};

void projectMenu(std::shared_ptr<ProjectF16> p) {
    while (true) {
        // Reload to show current state
        if (auto fresh = ProjectF16::loadById(p->projectId)) *p = *fresh;
        printProject(*p);

        // Block editing if released
        if (p->isReleased())
            std::cout << "  ⚠ RELEASED — keine weiteren Aenderungen moeglich\n";

        std::cout
            << "  1.Bearbeiten  2.F22  3.F22+  4.Dokumente  5.Dok+\n"
            << "  6.Komm.       7.Meilenstein  8.F77  0.Zurück\n";
        hr();

        int ch = readInt("Wahl", 0, 10);
        if (ch == 0) break;
        if (ch >= 1 && ch <= 10 && prjMenuTable[ch])
            if (!prjMenuTable[ch](p)) break;
    }
}

} // namespace CLI

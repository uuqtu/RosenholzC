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

        auto all = F16::loadAll();
        int found = 0;
        for (auto& p : all) {
            std::string t = p->title, r = p->regNumber.toString();
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            std::transform(r.begin(), r.end(), r.begin(), ::tolower);
            if (t.find(ql) != std::string::npos || r.find(ql) != std::string::npos) {
                std::cout << "  " << std::left << std::setw(26) << p->regNumber.toString()
                          << "  " << std::setw(36) << p->title.substr(0, 35)
                          << "  [" << (p->archived ? "archiviert" : "aktiv") << "]\n";
                ++found;
            }
        }
        if (!found) std::cout << "  (keine Treffer für \"" << q << "\")\n";
        return;
    }

    // -status <status>  —  filter by lifecycle status
    if (args[0] == "-status" || args[0] == "--status") {
        std::string st = args.size() > 1 ? args[1] : readLine("Status: ");
        auto list = F16::loadByStatus(st);
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
        auto p = F16::loadById(args[0]);
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

static void editMenu(std::shared_ptr<Rosenholz::F16> p) {
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

// ── createProjectWizard ───────────────────────────────────────
//
// Step-by-step wizard for new F16 projects.
// Asks for: title, type, size, codename, priority, complexity,
//           methodology, scope, start/end dates, budget.
// Saves to DB and writes MFS file if MFS is enabled.

std::shared_ptr<Rosenholz::F16> createProjectWizard() {
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

    auto p = Rosenholz::F16::create(title, ptype, size);
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


// ── F16 projectMenu handlers ───────────────────────────────────────────────

// Inline handler helpers (lambdas captured by ref would not work in fn-ptr table;
// use free static functions instead)

static bool f16_edit(std::shared_ptr<F16> p) {
    editMenu(p); return true;
}

static bool f16_f22_list(std::shared_ptr<F16> p) {
    auto tasks = F22::loadForProject(p->projectId);
    if (tasks.empty()) { std::cout << "  (keine F22-Vorgänge)\n"; return true; }
    int n = 1;
    for (auto& t : tasks)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(26) << t->regNumber.toString()
                  << "  " << std::setw(28) << t->title.substr(0,26)
                  << "  " << entityStatusToString(t->status) << "\n";
    return true;
}

static bool f16_f22_open(std::shared_ptr<F16> p) {
    auto tasks = F22::loadForProject(p->projectId);
    if (tasks.empty()) { std::cout << "  (keine F22-Vorgänge)\n"; return true; }
    int pick = readInt("F22 #", 1, (int)tasks.size());
    taskMenu(tasks[pick-1]);
    return true;
}

static bool f16_f22_new(std::shared_ptr<F16> p) {
    if (!p->canAddChildren()) {
        std::cout << "  >> Projekt ist archiviert — keine neuen Eintraege.\n";
        return true;
    }
    auto t = createTaskWizard(p->projectId);
    if (t) {
        std::cout << "  F22 jetzt öffnen? (j/n): ";
        std::string yn; std::getline(std::cin, yn);
        if (yn=="j"||yn=="J") taskMenu(t);
    }
    return true;
}

static bool f16_dok_list(std::shared_ptr<F16> p) {
    // Collect DOK via all F22 tasks
    auto tasks = F22::loadForProject(p->projectId);
    std::vector<std::shared_ptr<Folder>> docs;
    for (auto& t : tasks) {
        auto td = Folder::loadForEntity("f22", t->taskId);
        docs.insert(docs.end(), td.begin(), td.end());
    }
    if (docs.empty()) { std::cout << "  (keine Akten)\n"; return true; }
    int n = 1;
    for (auto& d : docs)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(26) << d->folderId.substr(0,24)
                  << "  " << std::setw(30) << d->title.substr(0,28)
                  << "  " << d->docType << "\n";
    return true;
}

static bool f16_dok_open(std::shared_ptr<F16> p) {
    auto tasks = F22::loadForProject(p->projectId);
    std::vector<std::shared_ptr<Folder>> docs;
    for (auto& t : tasks) {
        auto td = Folder::loadForEntity("f22", t->taskId);
        docs.insert(docs.end(), td.begin(), td.end());
    }
    if (docs.empty()) { std::cout << "  (keine Akten)\n"; return true; }
    int n=1;
    for (auto& d : docs)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(26) << d->folderId.substr(0,24)
                  << "  " << d->title.substr(0,30) << "\n";
    int pick = readInt("AKT #", 1, (int)docs.size());
    documentMenu(docs[pick-1]);
    return true;
}

static bool f16_dok_new(std::shared_ptr<F16> p) {
    if (!p->canAddChildren()) {
        std::cout << "  >> Projekt ist archiviert — kein neues AKT.\n";
        return true;
    }
    auto doc = createDocumentWizardGuided();
    if (doc) documentMenu(doc);
    return true;
}

static bool f16_kom_list(std::shared_ptr<F16> p) {
    listComms(p->projectId, "f16");
    return true;
}

static bool f16_kom_open(std::shared_ptr<F16> p) {
    auto items = listComms(p->projectId, "f16");
    if (items.empty()) return true;
    int pick = readInt("KOM #", 1, (int)items.size());
    commDetailMenu(items[pick-1]);
    return true;
}

static bool f16_kom_new(std::shared_ptr<F16> p) {
    communicationMenu(p->projectId, "f16");
    return true;
}


using prjMenuFn = bool(*)(std::shared_ptr<F16> p);
static const prjMenuFn prjMenuTable[11] = {
    nullptr,        // 0
    f16_edit,       // 1 Bearbeiten
    f16_f22_list,   // 2 F22 listen
    f16_f22_open,   // 3 F22 <#>
    f16_f22_new,    // 4 F22+
    f16_dok_list,   // 5 AKT listen
    f16_dok_open,   // 6 AKT <#>
    f16_dok_new,    // 7 AKT+
    f16_kom_list,   // 8 KOM listen
    f16_kom_open,   // 9 KOM <#>
    f16_kom_new,    // 10 KOM+
};

void projectMenu(std::shared_ptr<F16> p) {
    // Push to navigation stack
    Rosenholz::NavigationStack::instance().push({
        Rosenholz::EntityType::F16, p->projectId, p->title, p->regNumber.toString()});
    while (true) {
        if (auto fresh = F16::loadById(p->projectId)) *p = *fresh;
        printProject(*p);
        if (p->isArchived())
            std::cout << "  ⚠ ARCHIVIERT — kein Bearbeitungsmodus.\n";
        std::cout
            << "  1.Bearbeiten\n"
            << "  F22: 2.listen | 3.<#> | 4.neu\n"
            << "  AKT: 5.listen | 6.<#> | 7.neu\n"
            << "  KOM: 8.listen | 9.<#> | 10.neu\n"
            << "  0.Zurück\n";
        int ch = readInt("Wahl", 0, 10);
        if (ch == 0) break;
        if (ch >= 1 && ch <= 10 && prjMenuTable[ch])
            if (!prjMenuTable[ch](p)) break;
    }
}

} // namespace CLI

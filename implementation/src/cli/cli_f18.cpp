// ============================================================
// cli_f18.cpp  —  F18 Operation: Befehlshandler, Wizard, Menü
//
// Public functions:
//   cmdF18(args)                — dispatch für 'rh -f18 ...'
//   printF18Operation(v)        — structured F18 card display
//   f18Menu(v)                  — interactive detail menu
//   f18BrowserMenu(task, type)  — filtered browser (F22-scoped)
//   createF18Wizard(task,type)  — create F18 under known F22 task
//   createF18WizardGuided()     — guided: select F22 task first

#include "cli_common.h"
#include "../model/f18/F18Operation.h"
#include "../model/f18/F18OperationStep.h"
#include "../model/f22/F22.h"
#include "../model/akt/Folder.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include <iomanip>
#include <algorithm>

using namespace Rosenholz;
using namespace CLI;

namespace CLI {

// ── createF18WizardGuided ─────────────────────────────────────
// Guided wizard: select F22 task first, then create F18.
// F18 belongs exclusively to F22 — no direct F16 attachment.
std::shared_ptr<F18Operation> createF18WizardGuided() {
    hdr("F18 ANLEGEN — F22 WÄHLEN");
    std::string taskId = readLine("F22-ID (XV/F22/...): ");
    if (taskId.empty()) return nullptr;
    auto task = Rosenholz::F22::loadById(taskId);
    if (!task) { printErr("F22 nicht gefunden: " + taskId); return nullptr; }
    return createF18Wizard(taskId, "");
}

// ── cmdF18 ────────────────────────────────────────────────────
//
// Dispatch table for 'rh -f18 [args]':
//
//   rh -f18                           → list 20 most recent F18 ops
//   rh -f18 -n                        → guided: pick F16/F22, then create
//   rh -f18 <f18-id>                  → open f18Menu
//   rh -f18 <project-id>              → open f18BrowserMenu for project
//   rh -f18 <project-id> -t <type>    → create with specific type
//   rh -f18 <project-id> (anything)   → open creation wizard

void cmdF18(const std::vector<std::string>& args) {

    // No arguments: list 20 most recent F18 operations
    if (args.empty()) {
        auto all = F18Operation::loadRecent(20);
        if (all.empty()) { std::cout << "  (keine F18-Operationen)\n"; return; }
        std::cout << "  " << std::left
                  << std::setw(26) << "ID (für rh -f18 <id>)"
                  << std::setw(18) << "TYP"
                  << std::setw(12) << "STATUS"
                  << "TITEL\n"
                  << "  " << std::string(72, '-') << "\n";
        for (auto& v : all)
            std::cout << "  " << std::setw(26) << v->operationId.substr(0, 24)
                      << std::setw(18) << ("[" + v->operationType + "]")
                      << std::setw(12) << entityStatusToString(v->status)
                      << v->title.substr(0, 30) << "\n";
        std::cout << "  " << all.size() << " F18\n";
        return;
    }

    // -o: list F18, pick → navigate
    // -so [q]: search + navigate
    if (args[0] == "-o" || args[0] == "-so") {
        bool doSearch = (args[0] == "-so");
        std::string q;
        for (std::size_t i=1; i<args.size(); i++) { if(!q.empty()) q+=" "; q+=args[i]; }
        auto all = F18Operation::loadRecent(200);
        std::vector<std::shared_ptr<F18Operation>> hits;
        for (auto& v : all) {
            if (q.empty()) { hits.push_back(v); continue; }
            if (matchesPattern(v->title, q) || matchesPattern(v->operationId, q))
                hits.push_back(v);
        }
        if (hits.empty()) { std::cout << "  (keine F18)\n"; return; }
        std::cout << "\n  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(12) << "TYP"
                  << std::setw(28) << "TITEL"
                  << std::setw(18) << "F22-AUFGABE"
                  << "STATUS\n"
                  << "  " << std::string(92,'-') << "\n";
        for (int i=0; i<(int)hits.size(); i++) {
            auto t = F22::loadById(hits[i]->taskId);
            std::string tname = t ? t->regNumber.toString() : hits[i]->taskId;
            std::cout << "  " << std::setw(4) << (i+1)
                      << std::setw(26) << hits[i]->operationId
                      << std::setw(12) << hits[i]->operationType.substr(0,10)
                      << std::setw(28) << hits[i]->title.substr(0,26)
                      << std::setw(18) << tname
                      << Color::statusColor(entityStatusToString(hits[i]->status)) << "\n";
        }
        if (!doSearch) return;
        int pick = readInt("  Auswahl [0=Abbrechen]", 0, (int)hits.size());
        if (pick < 1) return;
        CLI::cmdCd({hits[pick-1]->operationId});
        return;
    }

    // -s <q>: search and list (no navigation)
    if (args[0] == "-s") {
        std::string q;
        for (std::size_t i=1; i<args.size(); i++) { if(!q.empty()) q+=" "; q+=args[i]; }
        if (q.empty()) { printErr("-s benoetigt einen Suchbegriff"); return; }
        auto all = F18Operation::loadRecent(9999);
        bool found = false;
        for (auto& v : all) {
            if (matchesPattern(v->title, q) || matchesPattern(v->operationId, q)) {
                auto t = F22::loadById(v->taskId);
                std::string tname = t ? t->regNumber.toString() : v->taskId;
                std::cout << "  " << std::left << std::setw(26) << v->operationId
                          << std::setw(12) << v->operationType.substr(0,10)
                          << std::setw(28) << v->title.substr(0,26)
                          << "  " << tname
                          << "  [" << entityStatusToString(v->status) << "]\n";
                found = true;
            }
        }
        if (!found) std::cout << "  (keine Treffer)\n";
        return;
    }

    // -n  —  guided creation
    if (args[0] == "-n" || args[0] == "--neu") {
        auto op = createF18WizardGuided();
        if (op) printOk("  >> F18 angelegt: " + op->operationId
                        + "  [" + op->operationType + "]  " + op->title);
        return;
    }

    if (!isId(args[0])) { printErr("Ungültiges Argument: " + args[0]
                            + "  (erwartet ID oder -n)"); return; }

    // Try as F18 operation ID → open menu
    auto v = F18Operation::loadById(args[0]);
    if (v) {
        f18Menu(v);
        return;
    }

    // Try as project ID
    auto p = F16::loadById(args[0]);
    if (!p) { printErr("ID nicht gefunden: " + args[0]); return; }

    // Check for -t <type> flag anywhere in remaining args
    std::string typeArg;
    for (int i = 1; i + 1 < (int)args.size(); ++i) {
        if (args[i] == "-t" || args[i] == "--type") {
            typeArg = args[i + 1];
            break;
        }
    }

    // Additional args signal creation; just a project ID → browser
    bool hasExtra = args.size() > 1 && (args[1][0] != '-' || !typeArg.empty());
    if (hasExtra || !typeArg.empty()) {
        // F18 requires a task context — open guided wizard
    auto op = createF18WizardGuided();
        if (op) printOk("  >> F18 angelegt: " + op->operationId
                        + "  [" + op->operationType + "]  " + op->title);
    } else {
        printErr("F18 sind F22 zugeordnet. Bitte F22-ID eingeben.");
    }
}


// ── drawF18Chain (static display helper) ─────────────────────
//
// Renders the linear step chain of a F18 operation as a
// horizontal ASCII diagram:  [OK] Init --> [> ] Step1 --> [  ] End

static void drawF18Chain(const std::vector<Rosenholz::F18OperationStep>& steps) {
    if (steps.empty()) { std::cout << "\n  (keine Schritte)\n"; return; }
    std::cout << "\n";
    bool first = true;
    for (auto& s : steps) {
        if (!first) std::cout << "-->";
        first = false;
        const char* mark = f18StepSymbolStr(s.displaySymbol());
        std::string label = s.title.size() > 14 ? s.title.substr(0, 13) + "~" : s.title;
        std::cout << "[" << mark << "] " << std::left << std::setw(14) << label;

    }
    std::cout << "\n";
}

// ── stepMenu (static helper for f18Menu) ──────────────────────
//
// Interactive sub-menu for a single F18 operation step.
// Handles: execution (approve/reject/skip), tracking updates,
// notes, communications, and document attachments.

void stepMenu(Rosenholz::F18OperationStep& step,
              std::vector<Rosenholz::F18OperationStep>& allSteps) {
    using namespace Rosenholz;
    while (true) {
        hdr("F18S — " + step.stepId.substr(0, 22));
        std::cout << "  Titel   : " << step.title << "\n"
                  << "  Typ     : " << step.stepType << "\n"
                  << "  Status  : " << Rosenholz::f18StepStatusToString(step.status) << "\n";
        if (step.percentComplete > 0)
            std::cout << "  Fortsch.: " << step.percentComplete << "%\n";
        if (!step.assignedTo.empty())
            std::cout << "  Assigned: " << step.assignedTo << "\n";

        // Numbered KOM and DOK sub-lists
        auto koms = Communication::loadForOwner(step.stepId, "f18step");
        auto docs  = Folder::loadForEntity("f18s", step.stepId);

        std::cout << "  1.Ausführen | 2.Tracking | 3.Notiz\n"
                  << "  KOM: 4.listen(" << koms.size() << ") | 5.<#> | 6.neu\n"
                  << "  AKT: 7.listen(" << docs.size() << ") | 8.<#> | 9.neu\n"
                  << "  0.Zurück\n";
        int ch = readInt("Wahl", 0, 9);
        if (ch == 0) return;

        if (ch == 1) {
            std::cout << "  1.Abschliessen  2.Ablehnen  3.Überspringen\n";
            int sc = readInt("Status",1,3);
            if (sc==1) step.status = Rosenholz::F18StepStatus::DONE;
            else if (sc==2) step.status = Rosenholz::F18StepStatus::REJECTED;
            else step.status = Rosenholz::F18StepStatus::SKIPPED;
            step.complete();
            std::cout << "  >> Status: " << Rosenholz::f18StepStatusToString(step.status) << "\n";
        } else if (ch == 2) {
            std::string ts = readOpt("Tracking-Status: ");
            std::string pct = readOpt("Fortschritt % (0-100): ");
            if (!ts.empty()) step.trackingStatus = ts;
            if (!pct.empty()) try { step.percentComplete = std::stoi(pct); } catch(...) {}
            step.save(); std::cout << "  >> Aktualisiert.\n";
        } else if (ch == 3) {
            std::string n = readLine("Notiz: ");
            if (!n.empty()) { step.notes = n; step.save(); }
        } else if (ch == 4) {
            listComms(step.stepId, "f18step");
        } else if (ch == 5) {
            if (koms.empty()) { std::cout << "  (keine)\n"; continue; }
            int pick = readInt("KOM #", 1, (int)koms.size());
            commDetailMenu(koms[pick-1]);
        } else if (ch == 6) {
            communicationMenu(step.stepId, "f18step");
        } else if (ch == 7) {
            if (docs.empty()) { std::cout << "  (keine Akten)\n"; continue; }
            int n=1;
            for (auto& d : docs)
                std::cout << "  " << std::setw(3) << n++ << ". "
                          << d->folderId.substr(0,24) << "  " << d->title.substr(0,30) << "\n";
        } else if (ch == 8) {
            if (docs.empty()) { std::cout << "  (keine Akten)\n"; continue; }
            int n=1;
            for (auto& d : docs)
                std::cout << "  " << std::setw(3) << n++ << ". "
                          << d->folderId.substr(0,24) << "  " << d->title.substr(0,30) << "\n";
            int pick = readInt("AKT #", 1, (int)docs.size());
            documentMenu(docs[pick-1]);
        } else if (ch == 9) {
            auto doc = createDocumentWizard("", step.stepId);
            if (doc) documentMenu(doc);
        }
    }
}

// ── printF18Operation ─────────────────────────────────────────
//
// Structured display of an F18 operation card.
// Shows common fields first, then type-specific fields.

void printF18Operation(const Rosenholz::F18Operation& v) {
    using namespace Rosenholz;
    hdr("F18 " + v.operationId.substr(0,24) + "  " + v.title.substr(0,28));
    std::cout << "  Typ:" << v.operationType << "  Status:" << entityStatusToString(v.status)
              << "  Prio:" << v.priority << "\n";
    if (!v.taskId.empty())
        std::cout << "  F22:" << v.taskId.substr(0,26) << "\n";
    if (!v.releaseWorkflowId.empty())
        std::cout << "  WFI:" << v.releaseWorkflowId.substr(0,36) << "\n";
    // Type-specific key fields
    if (v.operationType == "incident" && !v.severity.empty())
        std::cout << "  Severity:" << v.severity << "  Type:" << v.incidentType << "\n";
    if (v.operationType == "risk" && v.overallRiskScore > 0)
        std::cout << "  RiskScore:" << v.overallRiskScore
                  << "  Level:" << v.riskLevel << "\n";
    // Show steps if loaded
    if (!v.steps.empty()) {
        std::cout << "  Steps (" << v.steps.size() << "):\n";
        for (auto& s : v.steps)
            std::cout << "    [" << f18StepStatusToString(s.status) << "] " << s.title.substr(0,40) << "\n";
    }
}



// ── createF18Wizard ───────────────────────────────────────────
//
// Step-by-step wizard for a new F18 operation.
// The project ID must be supplied. Task ID and type are optional
// and may be provided by the guided variant.

std::shared_ptr<Rosenholz::F18Operation> createF18Wizard(
    const std::string& projectId,
    const std::string& taskId,
    const std::string& type)
{
    using namespace Rosenholz;
    hdr("F18 ANLEGEN");

    // Step 1: Title
    std::string title = readLine("F18-Titel: ");
    if (title.empty()) return nullptr;

    // Step 2: Type (ask after title, not upfront)
    std::string chosenType = type;
    if (chosenType.empty()) {
        hdr("F18-TYP WÄHLEN");
        std::cout
            << "    1.  Incident              (Vorfall)\n"
            << "    2.  Risk                 (Risiko)\n"
            << "    3.  Measure              (Maßnahme)\n"
            << "    4.  QualityGate          (Qualitätsstor)\n"
            << "    5.  AssumptionConstraint (Annahme/Beschränkung)\n"
            << "    6.  CommunicationPlan    (Kommunikationsplan)\n"
            << "    7.  LessonsLearned       (Erfahrungen)\n"
            << "    8.  DecisionLog          (Entscheidungsprotokoll)\n"
            << "    9.  ChangeRequest        (Änderungsantrag)\n"
            << "   10.  ChangeObject         (Änderungsobjekt)\n"
            << "   11.  Generic              (Generisch)\n";
        static const char* types[] = {
            "incident","risk","measure","qualityGate",
            "assumptionConstraint","communicationPlan","lessonsLearned",
            "decisionLog","changeRequest","changeObject","generic"};
        int t = readInt("Typ",1,11);
        chosenType = types[t-1];
    }
    // Wizard: common fields (title was already asked above)
    std::string owner  = readOpt("Verantwortlich (Person-ID, leer=offen): ");
    std::string prio   = readOpt("Priorität (low/medium/high/critical, leer=medium): ");

    auto v = Rosenholz::F18Operation::create(taskId, title, chosenType);
    if (!v) {
        std::cout << "  >> FEHLER: F18 Vorgang konnte nicht angelegt werden.\n";
        return nullptr;
    }
    if (!owner.empty()) v->ownerId   = owner;
    if (!prio.empty())  v->priority  = prio;
    else                v->priority  = "medium";
    v->update();

    std::cout << "  >> F18 angelegt: " << v->operationId << "\n";
    std::cout << "  >> Typ: " << v->operationType << "  Status: " << entityStatusToString(v->status) << "\n";
    return v;
}

// ── f18Menu and f18BrowserMenu ────────────────────────────────
//
// Interactive menus copied from F18Menu.cpp.


// ── void f18Menu(std::shared_ptr<F18Operation> v) handlers ──────────────────────────────────────────────
// ── F18 f18Menu handlers ────────────────────────────────────────────────

static bool f18_edit(std::shared_ptr<F18Operation> v) {
    using namespace Rosenholz;
    std::string title = readOpt("Titel (leer=behalten): ");
    if (!title.empty()) v->title = title;
    std::string pr = readOpt("Priorität (high/medium/low, leer=behalten): ");
    if (!pr.empty()) v->priority = pr;
    std::string own = readOpt("Verantwortliche Person-ID (leer=behalten): ");
    if (!own.empty()) v->ownerId = own;
    if (opOk(v->update())) std::cout << "  >> Gespeichert.\n";
    return true;
}

static bool f18_note(std::shared_ptr<F18Operation> v) {
    std::string n = readLine("Notiz: ");
    if (!n.empty()) {
        v->notes = (v->notes.empty() || v->notes == "{}") ? n : v->notes + "\n" + n;
        v->update(); std::cout << "  >> Notiz gespeichert.\n";
    }
    return true;
}

static bool f18_step_list(std::shared_ptr<F18Operation> v) {
    v->loadSteps();
    if (v->steps.empty()) { std::cout << "  (keine Schritte)\n"; return true; }
    int n = 1;
    for (auto& s : v->steps)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << "[" << std::left << std::setw(8) << std::string(Rosenholz::f18StepStatusToString(s.status)).substr(0,7) << "] "
                  << std::setw(30) << s.title.substr(0,28)
                  << "  " << s.stepType << "\n";
    return true;
}

static bool f18_step_open(std::shared_ptr<F18Operation> v) {
    v->loadSteps();
    if (v->steps.empty()) { std::cout << "  (keine Schritte)\n"; return true; }
    int n=1;
    for (auto& s : v->steps)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << "[" << std::left << std::setw(8) << std::string(Rosenholz::f18StepStatusToString(s.status)).substr(0,7) << "] "
                  << s.title.substr(0,34) << "\n";
    int pick = readInt("F18S #", 1, (int)v->steps.size());
    stepMenu(v->steps[pick-1], v->steps);
    return true;
}

static bool f18_step_new(std::shared_ptr<F18Operation> v) {
    std::string title = readLine("Schritt-Titel: ");
    if (title.empty()) return true;
    std::cout << "  Typ: 1.task  2.approval  3.review  4.notification\n";
    int st = readInt("Typ",1,4);
    static const char* sts[]={"task","approval","review","notification"};
    std::string ass = readOpt("Zugewiesen an (Person-ID, leer=offen): ");
    bool req = yesno("  Pflichtschritt?");
    v->addStep(title, sts[st-1], ass, req);
    std::cout << "  >> Schritt hinzugefuegt.\n";
    return true;
}

static bool f18_kom_list(std::shared_ptr<F18Operation> v) {
    listComms(v->operationId, "f18"); return true;
}

static bool f18_kom_open(std::shared_ptr<F18Operation> v) {
    auto items = listComms(v->operationId, "f18");
    if (items.empty()) return true;
    int pick = readInt("KOM #", 1, (int)items.size());
    commDetailMenu(items[pick-1]);
    return true;
}

static bool f18_kom_new(std::shared_ptr<F18Operation> v) {
    communicationMenu(v->operationId, "f18"); return true;
}

static bool f18_dok_list(std::shared_ptr<F18Operation> v) {
    auto docs = Folder::loadForEntity("f18", v->operationId);
    if (docs.empty()) { std::cout << "  (keine Akten)\n"; return true; }
    int n = 1;
    for (auto& d : docs)
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(26) << d->folderId.substr(0,24)
                  << "  " << d->title.substr(0,30) << "\n";
    return true;
}

static bool f18_dok_open(std::shared_ptr<F18Operation> v) {
    auto docs = Folder::loadForEntity("f18", v->operationId);
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

static bool f18_dok_new(std::shared_ptr<F18Operation> v) {
    auto doc = createDocumentWizard("", v->operationId);
    if (doc) documentMenu(doc);
    return true;
}

static bool f18_f77(std::shared_ptr<F18Operation> v) {
    hdr("F77 — " + v->operationId.substr(0,22));
    if (v->releaseWorkflowId.empty()) {
        std::cout << "  Kein F77 aktiv.\n  1. F77 starten...    0. Zurück\n";
        int wfc = readInt("Wahl",0,1);
        if (wfc==1) {
            std::string wid = startWfInstanceWizard("f18", v->operationId);
            if (!wid.empty()) instanceMenu(wid);
        }
    } else {
        std::cout << "  F77-ID: " << v->releaseWorkflowId.substr(0,36) << "\n";
        std::cout << "  1.F77 öffnen  0.Zurück\n";
        int wfc = readInt("Wahl",0,1);
        if (wfc==1) instanceMenu(v->releaseWorkflowId);
    }
    return true;
}

using f18MenuFn = bool(*)(std::shared_ptr<F18Operation> v);
static const f18MenuFn f18MenuTable[13] = {
    nullptr,       // 0
    f18_edit,      // 1 Bearbeiten
    f18_note,      // 2 Notiz+
    f18_step_list, // 3 F18S listen
    f18_step_open, // 4 F18S <#>
    f18_step_new,  // 5 F18S+
    f18_kom_list,  // 6 KOM listen
    f18_kom_open,  // 7 KOM <#>
    f18_kom_new,   // 8 KOM+
    f18_dok_list,  // 9 AKT listen
    f18_dok_open,  // 10 AKT <#>
    f18_dok_new,   // 11 AKT+
    f18_f77,       // 12 F77
};

void f18Menu(std::shared_ptr<F18Operation> v) {
    while (true) {
        v->loadSteps();
        printF18Operation(*v);
        drawF18Chain(v->steps);
        bool v_released = v->isReleased();
        if (v_released) std::cout << "  ⚠ RELEASED — nur Lesezugriff\n";
        std::cout
            << "  1.Bearbeiten | 2.Notiz+\n"
            << "  F18S: 3.listen | 4.<#> | 5.neu\n"
            << "  KOM:  6.listen | 7.<#> | 8.neu\n"
            << "  AKT:  9.listen | 10.<#> | 11.neu\n"
            << "  12.F77  0.Zurück\n";
        int ch = readInt("Wahl", 0, 12);
        if (ch == 0) break;
        if (ch >= 1 && ch <= 12 && f18MenuTable[ch])
            if (!f18MenuTable[ch](v)) break;
    }
}

// ── cmdF18s: F18 Step (F18S) management ──────────────────────────────────────
// Global: asks which F18. Contextual (via cmdContextual): uses current F18.
void cmdF18s(const std::vector<std::string>& args,
             std::shared_ptr<Rosenholz::F18Operation> v) {
    using namespace Rosenholz;

    // If no F18 provided: global — ask:
    if (!v) {
        if (!args.empty() && isId(args[0])) {
            v = F18Operation::loadById(args[0]);
            if (!v) { printErr("F18 nicht gefunden: " + args[0]); return; }
            // Shift args past the ID:
            std::vector<std::string> rest(args.begin()+1, args.end());
            cmdF18s(rest, v);
            return;
        }
        // No ID — try global search/list for F18 selection:
        auto all = F18Operation::loadRecent(50);
        if (all.empty()) { printErr("Keine F18-Vorgaenge vorhanden."); return; }
        std::cout << "\n  F18 waehlen:\n";
        for (int i=0; i<(int)all.size(); i++)
            std::cout << "  " << std::setw(3) << (i+1) << ". "
                      << std::left << std::setw(26) << all[i]->operationId
                      << "  " << all[i]->title.substr(0,30) << "\n";
        int pick = readInt("  F18 [0=Abbrechen]", 0, (int)all.size());
        if (pick < 1) return;
        v = all[pick-1];
    }
    v->loadSteps();

    // -n: new step
    if (!args.empty() && args[0] == "-n") {
        std::string title = readLine("  Schritt-Titel: ");
        if (title.empty()) return;
        std::cout << "  Typ: 1.task  2.approval  3.review  4.notification  ";
        int st = readInt("", 1, 4);
        static const char* sts[]={"task","approval","review","notification"};
        std::string ass   = readOpt("  Zugewiesen an (Person-ID, leer=offen): ");
        std::string start = promptDate("  Geplanter Start (.  +Nd  YYYY-MM-DD, leer): ");
        std::string end   = promptDate("  Geplantes Ende  (.  +Nd  YYYY-MM-DD, leer): ");
        bool freeStep = yesno("  Freier Schritt (kein Vorgaenger)?");
        v->addStep(title, sts[st-1], ass, freeStep, start, end);
        printOk("F18S hinzugefuegt.");
        return;
    }

    // -e [n]: edit step
    if (!args.empty() && args[0] == "-e") {
        int n = 0;
        if (args.size() >= 2) { try { n = std::stoi(args[1]); } catch(...) {} }
        if (v->steps.empty()) { std::cout << "  (keine Schritte)\n"; return; }
        if (n < 1 || n > (int)v->steps.size()) {
            for (int i=0; i<(int)v->steps.size(); i++)
                std::cout << "  " << std::setw(3) << (i+1) << ". "
                          << v->steps[i].title.substr(0,30) << "\n";
            n = readInt("  Schritt", 1, (int)v->steps.size());
        }
        auto& s = v->steps[n-1];
        std::string newTitle = readOpt("  Titel (leer=behalten): ");
        if (!newTitle.empty()) s.title = newTitle;
        std::string newAss = readOpt("  Zugewiesen an (leer=behalten): ");
        if (!newAss.empty()) s.assignedTo = newAss;
        std::string newPrio = readChar(
            "Prioritaet (h=high m=medium l=low c=critical, leer=behalten): ",
            {{"h","high"},{"m","medium"},{"l","low"},{"c","critical"}}, true);
        if (!newPrio.empty()) s.priority = newPrio;
        std::string newStart = promptDate("  Geplanter Start (leer=behalten): ");
        if (!newStart.empty()) s.startDatePlanned = newStart;
        std::string newEnd = promptDate("  Geplantes Ende  (leer=behalten): ");
        if (!newEnd.empty()) { s.endDatePlanned = newEnd; s.dueDate = newEnd; }
        std::string pctStr = readOpt("  Fortschritt % (0-100, leer=behalten): ");
        if (!pctStr.empty()) try { s.percentComplete = std::stoi(pctStr); } catch(...) {}
        s.computeTrackingStatus();
        if (s.update()) printOk("F18S aktualisiert.");
        else            printErr("Fehler beim Speichern.");
        return;
    }

    // -o / -so: list steps, pick → open stepMenu
    if (!args.empty() && (args[0] == "-o" || args[0] == "-so")) {
        bool doSearch = (args[0] == "-so");
        std::string q = (doSearch && args.size()>1) ? args[1] : "";
        if (v->steps.empty()) { std::cout << "  (keine Schritte)\n"; return; }
        std::vector<F18OperationStep*> hits;
        for (auto& s : v->steps) {
            if (q.empty() || matchesPattern(s.title, q) || matchesPattern(s.stepId, q))
                hits.push_back(&s);
        }
        if (hits.empty()) { std::cout << "  (keine Treffer)\n"; return; }
        std::cout << "\n  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(12) << "STATUS"
                  << std::setw(28) << "TITEL"
                  << "TYP\n"
                  << "  " << std::string(74,'-') << "\n";
        for (int i=0; i<(int)hits.size(); i++)
            std::cout << "  " << std::setw(4) << (i+1)
                      << std::setw(26) << hits[i]->stepId
                      << std::setw(12) << f18StepStatusToString(hits[i]->status)
                      << std::setw(28) << hits[i]->title.substr(0,26)
                      << hits[i]->stepType << "\n";
        if (!doSearch) return;
        int pick = readInt("  Auswahl [0=Abbrechen]", 0, (int)hits.size());
        if (pick < 1) return;
        stepMenu(*hits[pick-1], v->steps);
        return;
    }

    // -s <q>: search and list steps
    if (!args.empty() && args[0] == "-s") {
        std::string q;
        for (std::size_t i=1; i<args.size(); i++) { if(!q.empty()) q+=" "; q+=args[i]; }
        if (q.empty()) { printErr("-s benoetigt Suchbegriff"); return; }
        bool found = false;
        for (auto& s : v->steps) {
            if (matchesPattern(s.title, q) || matchesPattern(s.stepId, q)) {
                std::cout << "  " << std::setw(26) << s.stepId
                          << "  " << f18StepStatusToString(s.status)
                          << "  " << s.title << "\n";
                found = true;
            }
        }
        if (!found) std::cout << "  (keine Treffer)\n";
        return;
    }

    // <n>: open step #n directly
    if (!args.empty()) {
        int n = 0;
        try { n = std::stoi(args[0]); } catch(...) {}
        if (n >= 1 && n <= (int)v->steps.size()) {
            stepMenu(v->steps[n-1], v->steps);
            return;
        }
    }

    // No args: list all steps and pick
    if (v->steps.empty()) {
        std::cout << "  (keine F18S)\n"
                  << "  f18s -n    neuen Schritt anlegen\n";
        return;
    }
    std::cout << "\n  F18S-Schritte fuer " << Color::magenta(v->operationId)
              << " — " << v->title << ":\n"
              << "  " << std::left << std::setw(4) << "#"
              << std::setw(26) << "ID"
              << std::setw(12) << "STATUS"
              << std::setw(24) << "TITEL"
              << std::setw(12) << "START-PLAN"
              << "ENDE-PLAN\n"
              << "  " << std::string(82,'-') << "\n";
    for (int i=0; i<(int)v->steps.size(); i++) {
        auto& s = v->steps[i];
        std::string sym = f18StepSymbolStr(s.displaySymbol());
        std::cout << "  " << std::setw(4) << (i+1)
                  << std::setw(26) << s.stepId
                  << std::setw(12) << f18StepStatusToString(s.status)
                  << std::setw(24) << s.title.substr(0,22)
                  << std::setw(12) << (s.startDatePlanned.empty() ? "-" : s.startDatePlanned.substr(0,10))
                  << (s.endDatePlanned.empty() ? "-" : s.endDatePlanned.substr(0,10)) << "\n";
    }
    std::cout << "\n  f18s <n>  oeffnen  |  f18s -n  neu  |  f18s -e <n>  bearbeiten\n\n";
    int pick = readInt("  Schritt oeffnen [0=Abbrechen]", 0, (int)v->steps.size());
    if (pick >= 1) stepMenu(v->steps[pick-1], v->steps);
}

} // namespace CLI

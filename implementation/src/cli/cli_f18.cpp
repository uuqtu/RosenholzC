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
#include "../model/f22/TaskF22.h"
#include "../model/dok/Document.h"
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
    auto task = Rosenholz::TaskF22::loadById(taskId);
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
            std::cout << "  " << std::setw(26) << v->vorgangId.substr(0, 24)
                      << std::setw(18) << ("[" + v->vorgangType + "]")
                      << std::setw(12) << v->status
                      << v->title.substr(0, 30) << "\n";
        std::cout << "  " << all.size() << " F18\n";
        return;
    }

    // -n  —  guided creation
    if (args[0] == "-n" || args[0] == "--neu") {
        auto op = createF18WizardGuided();
        if (op) printOk("  >> F18 angelegt: " + op->vorgangId
                        + "  [" + op->vorgangType + "]  " + op->title);
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
    auto p = ProjectF16::loadById(args[0]);
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
        if (op) printOk("  >> F18 angelegt: " + op->vorgangId
                        + "  [" + op->vorgangType + "]  " + op->title);
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
        std::string mark;
        if      (s.status == "done")        mark = "OK";
        else if (s.status == "skipped")     mark = "--";
        else if (s.status == "in_progress") mark = " >";
        else if (s.status == "waiting")     mark = " W";
        else if (s.status == "blocked")     mark = "!!";
        else                               mark = "  ";
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

static void stepMenu(Rosenholz::F18OperationStep& step,
                     std::vector<Rosenholz::F18OperationStep>& allSteps) {
    using namespace Rosenholz;
    while (true) {
        hdr("SCHRITT — " + step.stepId.substr(0, 22));
        std::cout << "  Titel   : " << step.title << "\n";
        std::cout << "  Typ     : " << step.stepType << "\n";
        std::cout << "  Status  : " << step.status << "\n";
        std::cout << "  Tracking: " << step.trackingStatus;
        if (step.percentComplete > 0)
            std::cout << "  " << step.percentComplete << "%";
        std::cout << "\n";
        if (!step.assignedTo.empty())
            std::cout << "  Assigned: " << step.assignedTo << "\n";

        std::cout << "\n  1.Ausführen  2.Tracking  3.Notiz  4.Communications  5.Dokumente  0.Zurück\n";
        int ch = readInt("Wahl", 0, 5);
        if (ch == 0) return;

        if (ch == 1) {
            // Status transition for this step.
            // Free steps can always transition; regular steps need
            // all predecessors to be complete first.
            if (!step.canStart(allSteps)) {
                std::cout << "  >> Vorgänger noch nicht abgeschlossen.\n";
                return;
            }
            hdr("STATUS SETZEN — " + step.stepId.substr(0, 22));
            std::cout << "  Aktuell: " << step.status << "\n";
            if (step.isFree)
                std::cout << "  (freier Schritt — keine Abhängigkeiten)\n";
            std::cout << "  1. in_progress   2. waiting\n"
                         "  3. blocked       4. skipped\n"
                         "  5. done          0. Abbrechen\n";
            int dec = readInt("Neuer Status", 0, 5);
            if (dec == 0) return;
            static const char* newStates[] =
                {"", "in_progress", "waiting", "blocked", "skipped", "done"};
            std::string target = newStates[dec];
            // Require comment when skipping or marking done
            std::string comment;
            if (step.requiresComment || dec == 4 || dec == 5) {
                comment = readLine("Kommentar/Begründung: ");
            }
            std::string actor = readOpt("Bearbeiter (Person-ID, leer=System): ");
            if (actor.empty()) actor = "system";
            step.status     = target;
            step.decisionBy = actor;
            step.decisionDate = nowIso();
            step.comment    = comment;
            // Set timestamps for terminal and start states
            if (target == "in_progress" && step.startedDate.empty())
                step.startedDate = nowIso();
            if (target == "done" || target == "skipped")
                step.completedDate = nowIso();
            step.computeTrackingStatus();
            if (step.save())
                std::cout << "  >> Status: " << step.status << "\n";
            else
                std::cout << "  >> Fehler beim Speichern.\n";
            // Return to f18Menu after a terminal transition
            if (target == "done" || target == "skipped") return;

        } else if (ch == 2) {
            // Tracking update
            std::cout << "  >> Schritt: " << step.status << "\n";
            std::cout << "  1.Tracking-Status  2.Fortschritt%  3.Notiz  4.Priorität\n";
            int sc = readInt("Sub-Wahl", 1, 4);
            if (sc == 1) {
                std::cout << "  1.planned 2.focused 3.due 4.in_work 5.archived\n";
                int ts = readInt("Status", 1, 5);
                static const char* tstates[] = {"planned","focused","due","in_work","archived"};
                step.trackingStatus = tstates[ts - 1];
            } else if (sc == 2) {
                std::string ps = readOpt("Fortschritt % (0-100): ");
                if (!ps.empty()) try { step.percentComplete = std::stoi(ps); } catch(...) {}
            } else if (sc == 3) {
                step.progressNote = readLine("Notiz: ");
            } else if (sc == 4) {
                std::cout << "  1.low 2.medium 3.high 4.critical\n";
                int pr = readInt("Priorität", 1, 4);
                static const char* prios[] = {"low","medium","high","critical"};
                step.priority = prios[pr - 1];
            }
            if (step.save()) std::cout << "  >> Tracking aktualisiert.\n";

        } else if (ch == 3) {
            std::string note = readLine("Notiz: ");
            std::string by   = readOpt("Von (Person-ID): ");
            // Append note to the step's notes JSON field
            if (!step.notes.empty() && step.notes != "{}") {
                step.notes = step.notes.substr(0, step.notes.rfind('}'))
                             + ",\"" + nowIso() + "\":\"" + note + "\"}";
            } else {
                step.notes = "{\"" + nowIso() + "\":\"" + note + "\"}";
            }
            std::cout << "  >> Notiz gespeichert.\n";

        } else if (ch == 4) {
            communicationMenu(step.stepId, "f18step");

        } else if (ch == 5) {
            documentBrowserMenu("", step.stepId);
        }
    }
}

// ── printF18Operation ─────────────────────────────────────────
//
// Structured display of an F18 operation card.
// Shows common fields first, then type-specific fields.

void printF18Operation(const Rosenholz::F18Operation& v) {
    using namespace Rosenholz;
    hdr("F18 " + v.vorgangId.substr(0,24) + "  " + v.title.substr(0,28));
    std::cout << "  Typ:" << v.vorgangType << "  Status:" << v.status
              << "  Prio:" << v.priority << "\n";
    if (!v.taskId.empty())
        std::cout << "  F22:" << v.taskId.substr(0,26) << "\n";
    if (!v.releaseWorkflowId.empty())
        std::cout << "  WFI:" << v.releaseWorkflowId.substr(0,36) << "\n";
    // Type-specific key fields
    if (v.vorgangType == "incident" && !v.severity.empty())
        std::cout << "  Severity:" << v.severity << "  Type:" << v.incidentType << "\n";
    if (v.vorgangType == "risk" && v.overallRiskScore > 0)
        std::cout << "  RiskScore:" << v.overallRiskScore
                  << "  Level:" << v.riskLevel << "\n";
    // Show steps if loaded
    if (!v.steps.empty()) {
        std::cout << "  Steps (" << v.steps.size() << "):\n";
        for (auto& s : v.steps)
            std::cout << "    [" << s.status.substr(0,8) << "] " << s.title.substr(0,40) << "\n";
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

    std::cout << "  >> F18 angelegt: " << v->vorgangId << "\n";
    std::cout << "  >> Typ: " << v->vorgangType << "  Status: " << v->status << "\n";
    return v;
}

// ── f18Menu and f18BrowserMenu ────────────────────────────────
//
// Interactive menus copied from F18Menu.cpp.


// ── void f18Menu(std::shared_ptr<F18Operation> v) handlers ──────────────────────────────────────────────
static bool f18MenuOpt1(std::shared_ptr<F18Operation> v) {
    // Edit common + type-specific fields
    std::cout << "  Titel (leer=behalten): ";
    std::string t; std::getline(std::cin, t);
    if (!t.empty()) v->title = t;
    v->description = readOpt("Beschreibung: ");
    std::cout << "  Priorität: 1.low 2.medium 3.high 4.critical\n";
    int p = readInt("Priorität",1,4);
    static const char* ps[]={"low","medium","high","critical"};
    v->priority = ps[p-1];

    // Dispatch type-specific edit — each handler is a named function.
    // Adding a new F18 type: add one entry to typeEditHandlers.
    using TypeEditFn = std::function<bool(F18Operation&)>;
    static const std::unordered_map<std::string, TypeEditFn> typeEditHandlers = {
{"incident", [](F18Operation& v) {
    v.severity   = readOpt("Schwere (low|medium|high|critical): ");
    v.rootCause  = readOpt("Ursache: ");
    v.resolution = readOpt("Lösung: ");
    std::string ci = readOpt("Kostenauswirkung (€): ");
    if (!ci.empty()) try { v.costImpact = std::stod(ci); } catch(...) {}
    return true;  // call update()
}},
{"risk", [](F18Operation& v) {
    auto readScore = [](const std::string& p) {
        std::string s = readOpt(p);
        if (!s.empty()) try { return std::stoi(s); } catch(...) {}
        return 0;
    };
    if (int n = readScore("W-Score (1-5): "))       v.probabilityScore = n;
    if (int n = readScore("A-Zeit (1-5): "))        v.impactScoreTime = n;
    if (int n = readScore("A-Kosten (1-5): "))      v.impactScoreCost = n;
    if (int n = readScore("A-Qual. (1-5): "))       v.impactScoreQuality = n;
    if (int n = readScore("A-Scope (1-5): "))       v.impactScoreScope = n;
    v.recalcRiskScore();
    std::cout << "  >> Score: " << v.overallRiskScore
              << " Level: " << v.riskLevel << "\n";
    return false;  // recalcRiskScore calls update() already
}},
{"measure", [](F18Operation& v) {
    v.measureCategory = readOpt("Kategorie: ");
    v.effectiveness   = readOpt("Wirksamkeit: ");
    v.actualDate      = readOpt("Ist-Datum: ");
    return true;
}},
{"qualityGate", [](F18Operation& v) {
    v.gateResult   = readOpt("Ergebnis (passed|failed|conditional|pending): ");
    v.gateDecision = readOpt("Entscheidung (proceed|hold|stop): ");
    v.findings     = readOpt("Befunde: ");
    return true;
}},
{"changeRequest", [](F18Operation& v) {
    v.justification       = readOpt("Begründung: ");
    v.crDecisionDate      = readOpt("Entscheidungsdatum: ");
    v.crDecisionRationale = readOpt("Entscheidungsbegründung: ");
    return true;
}},
    };
    bool needsUpdate = true;
    auto it = typeEditHandlers.find(v->vorgangType);
    if (it != typeEditHandlers.end()) needsUpdate = it->second(*v);
    if (needsUpdate) { v->update(); std::cout << "  >> Gespeichert.\n"; }


    return true;
}

static bool f18MenuOpt2(std::shared_ptr<F18Operation> v) {
    // Add step
    std::string title = readLine("Schritt-Titel: ");
    std::cout << "  Typ: 1.task 2.approval 3.review 4.notification\n";
    int st = readInt("Typ",1,4);
    static const char* sts[]={"task","approval","review","notification"};
    std::string ass = readOpt("Zugewiesen an (Person-ID, leer=offen): ");
    bool isFreeStep = yesno("Freier Schritt (keine Voraussetzungen)?");
    auto step = v->addStep(title, sts[st-1], ass, isFreeStep);
    if (step)
std::cout << "  >> Schritt: " << step->stepId << "\n";


    return true;
}

static bool f18MenuOpt3(std::shared_ptr<F18Operation> v) {
    // Open a step
    if (v->steps.empty()) { std::cout << "  >> Keine Schritte.\n"; return true; }
    for (int i=0; i<(int)v->steps.size(); ++i)
std::cout << "  " << (i+1) << ". " << v->steps[i].title
          << " [" << v->steps[i].status << "]\n";
    int pick = readInt("Schritt #",1,(int)v->steps.size());
    stepMenu(v->steps[pick-1], v->steps);


    return true;
}

static bool f18MenuOpt4(std::shared_ptr<F18Operation> v) {
    std::string note = readLine("Notiz: ");
    std::string by   = readOpt("Von (Person-ID): ");
    v->addNote(by.empty()?"system":by, note);
    std::cout << "  >> Notiz gespeichert.\n";


    return true;
}

static bool f18MenuOpt5(std::shared_ptr<F18Operation> v) {
    communicationMenu(v->vorgangId, "project"); // F18 owns comms via vorgangId
    // Note: use vorgangId as ownerId with type project to allow flexible lookup


    return true;
}

static bool f18MenuOpt6(std::shared_ptr<F18Operation> v) {
    // Documents for this F18 Operation
    documentBrowserMenu("", v->taskId);
    // Also show docs attached via f18OperationId
    auto f18docs = Rosenholz::Document::loadForEntity("f18", v->vorgangId);
    if (!f18docs.empty()) {
hdr("DOKUMENTE AN F18 " + v->vorgangId.substr(0,22));
for (auto& d : f18docs)
    std::cout << "  " << d->documentId.substr(0,26)
              << "  " << d->title.substr(0,28) << "\n";
    }


    return true;
}

static bool f18MenuOpt7(std::shared_ptr<F18Operation> v) {
    std::cout << "  Status: 1.draft 2.active 3.completed 4.archived\n";
    int s = readInt("Status",1,4);
    static const char* ss[]={"draft","active","completed","archived"};
    v->status = ss[s-1];
    v->update();
    std::cout << "  >> Status: " << v->status << "\n";

    return true;
}

static bool f18MenuOpt8(std::shared_ptr<F18Operation> v) {
    // Starte Workflow...
    hdr("F77 — " + v->vorgangId.substr(0,22));
    std::cout << "  Status: " << v->status << "\n";
    if (v->releaseWorkflowId.empty()) {
std::cout << "  Kein F77 aktiv.\n";
std::cout << "  1. F77 starten...    0. Zurück\n";
int wfc = readInt("Wahl",0,1);
if (wfc==1) {
    std::string wid = startWfInstanceWizard("f18", v->vorgangId);
    if (!wid.empty()) instanceMenu(wid);
}
    } else {
auto wf = Rosenholz::F77_Workflow::loadById(v->releaseWorkflowId);
std::string wfStatus = wf ? wf->status : "unbekannt";
int blockers=0;
Rosenholz::F77_Engine::canRelease(
    "f18",v->vorgangId,v->releaseWorkflowId,blockers);
std::cout << "  WFI    : " << v->releaseWorkflowId.substr(0,36) << "\n";
std::cout << "  Status : " << wfStatus << "\n";
std::cout << (blockers>0 ? "  ! " + std::to_string(blockers)
    + " Schritte blockieren\n" : "  ✓ Freigabe moeglich\n");
std::cout << "  1.F77 öffnen  0.Zurück\n";
int mch = readInt("Wahl",0,1);
if (mch==1) instanceMenu(v->releaseWorkflowId);
    }

    return true;
}

using f18MenuFn = bool(*)(std::shared_ptr<F18Operation> v);
static const f18MenuFn f18MenuTable[9] = {
    nullptr,
    f18MenuOpt1,
    f18MenuOpt2,
    f18MenuOpt3,
    f18MenuOpt4,
    f18MenuOpt5,
    f18MenuOpt6,
    f18MenuOpt7,
    f18MenuOpt8,
};

void f18Menu(std::shared_ptr<F18Operation> v) {
    while (true) {
        v->loadSteps();
        printF18Operation(*v);
        drawF18Chain(v->steps);

        // Show release status in header
        bool v_released = (v->status == "released");
        if (v_released) std::cout << "  ⚠ RELEASED — nur Lesezugriff\n";
        std::cout << "  1.Bearbeiten | 2.Schritt+ | 3.Schritt öffnen\n"
                     "  4.Notiz | 5.Komm. | 6.Dokumente | 7.Notes | 8.F77 | 0.Zurück\n";
        int ch = readInt("Wahl", 0, 8);
        if (ch == 0) break;
        if (ch >= 1 && ch <= 8 && f18MenuTable[ch])
            if (!f18MenuTable[ch](v)) break;
    }
}

// ── F18 browser menu ─────────────────────────────────────────
void f18BrowserMenu(const std::string& taskId,
                    const std::string& typeFilter) {
    while (true) {
        std::vector<std::shared_ptr<F18Operation>> items;
        if (!taskId.empty())
            items = F18Operation::loadForTask(taskId, typeFilter);
        else
            items = F18Operation::loadRecent(50);

        std::string ctxLabel = typeFilter.empty() ? "alle Typen" : typeFilter;
        hdr("F18 (" + std::to_string(items.size()) + ") — " + ctxLabel);

        if (items.empty()) {
            std::cout << "  (keine F18)\n";
        } else {
            int n=1;
            for (auto& v : items)
                std::cout << "  " << std::setw(3) << n++ << ". "
                          << "[" << std::left << std::setw(14) << v->vorgangType.substr(0,13) << "] "
                          << std::setw(28) << v->title.substr(0,26)
                          << "  " << v->status << "\n";
        }

        // Check whether the parent entity is released (no new children allowed)
        bool parentReleased = false;
        if (!taskId.empty()) {
            auto t = TaskF22::loadById(taskId);
            parentReleased = t && (t->isReleased() || t->isWorkflowComplete());
        }

        if (parentReleased)
            std::cout << "\n  1.Öffnen  0.Zurück  (Released — kein Neu anlegen)\n";
        else
            std::cout << "\n  1.Öffnen  2.Neu anlegen  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;

        if (ch==1) {
            if (items.empty()) return;
            int pick = readInt("Nummer",1,(int)items.size());
            f18Menu(items[pick-1]);
        } else if (ch==2) {
            if (parentReleased) { std::cout << "  >> Released — kein Neu anlegen.\n"; return; }
            auto v = createF18Wizard(taskId, "");
            if (v) f18Menu(v);
        }
    }
}

} // namespace CLI

// ============================================================
// cli_f18.cpp  —  F18 Operation: Befehlshandler, Wizard, Menü
//
// Public functions:
//   cmdF18(args)                — dispatch for 'rh -f18 ...'
//   printF18Operation(v)        — display structured F18 card
//   f18Menu(v)                  — interactive detail menu
//   f18BrowserMenu(proj, task, type) — filtered browser
//   createF18Wizard(proj,task,type)  — create under known project
//   createF18WizardGuided()     — guided: pick F16/F22 first
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

// ── createF18WizardGuided ─────────────────────────────────────
//
// Guided variant: first shows all F16 projects so the user can
// pick one. Optionally also picks a F22 task. Then runs the
// normal F18 creation wizard.
// Invoked by 'rh -f18 -n'.

std::shared_ptr<F18Operation> createF18WizardGuided() {
    auto projects = ProjectF16::loadAll();
    if (projects.empty()) {
        std::cout << "  (keine Projekte — bitte zuerst ein F16 anlegen)\n";
        return nullptr;
    }

    hdr("F18 ANLEGEN — PROJEKT WÄHLEN");
    std::cout << "  " << std::left
              << std::setw(4) << "#"
              << std::setw(26) << "REG-NR"
              << "TITEL\n"
              << "  " << std::string(62, '-') << "\n";
    for (int i = 0; i < (int)projects.size(); ++i)
        std::cout << "  " << std::setw(4)  << (i + 1)
                  << std::setw(26) << projects[i]->regNumber.toString()
                  << projects[i]->title.substr(0, 36) << "\n";

    int ppick = readInt("Projektnummer", 1, (int)projects.size());
    auto& proj = projects[ppick - 1];

    // Optionally also pick a task
    std::string taskId;
    auto tasks = TaskF22::loadForProject(proj->projectId);
    if (!tasks.empty()) {
        std::cout << "\n  Aufgabe verknüpfen? (leer = nur Projekt)\n";
        for (int i = 0; i < (int)tasks.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << tasks[i]->regNumber.toString()
                      << tasks[i]->title.substr(0, 30) << "\n";
        std::cout << "  0  Keine Aufgabe\n";
        int tpick = readInt("Aufgabennummer", 0, (int)tasks.size());
        if (tpick > 0) taskId = tasks[tpick - 1]->taskId;
    }

    return createF18Wizard(proj->projectId, taskId, "");
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
        std::cout << "  " << all.size() << " Operation(en)\n";
        return;
    }

    // -n  —  guided creation
    if (args[0] == "-n" || args[0] == "--neu") {
        auto op = createF18WizardGuided();
        if (op) printOk("  >> F18 angelegt: " + op->vorgangId
                        + "  [" + op->vorgangType + "]  " + op->title);
        return;
    }

    if (!isId(args[0])) die("Ungültiges Argument: " + args[0]
                            + "  (erwartet ID oder -n)");

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
        auto op = createF18Wizard(p->projectId, "", typeArg);
        if (op) printOk("  >> F18 angelegt: " + op->vorgangId
                        + "  [" + op->vorgangType + "]  " + op->title);
    } else {
        f18BrowserMenu(p->projectId);
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
    std::cout << "\n\n";
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
                continue;
            }
            hdr("STATUS SETZEN — " + step.stepId.substr(0, 22));
            std::cout << "  Aktuell: " << step.status << "\n";
            if (step.isFree)
                std::cout << "  (freier Schritt — keine Abhängigkeiten)\n";
            std::cout << "  1. in_progress   2. waiting\n"
                         "  3. blocked       4. skipped\n"
                         "  5. done          0. Abbrechen\n";
            int dec = readInt("Neuer Status", 0, 5);
            if (dec == 0) continue;
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
    hdr("F18 VORGANG  " + v.vorgangId.substr(0, 22));
    auto row = [](const std::string& k, const std::string& val) {
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(30) << val.substr(0,29) << "|\n";
    };
    auto hr = []() { std::cout << "  +" << std::string(52,'-') << "+\n"; };
    row("ID",          v.vorgangId);
    row("Typ",         v.vorgangType);
    row("Titel",       v.title);
    row("Status",      v.status);
    row("Priorität",   v.priority);
    row("Owner-ID",    fval(v.ownerId));
    if (!v.projectId.empty()) row("Projekt-ID", v.projectId);
    if (!v.taskId.empty())    row("Aufgabe-ID", v.taskId);
    hr();
    // Type-specific fields
    if (v.vorgangType == "incident") {
        row("Vorfall-Typ",   fval(v.incidentType));
        row("Schwere",       fval(v.severity));
        row("Aufgetreten",   fdate(v.occurredDate));
        row("Gelöst",        fdate(v.resolvedDate));
        row("Ursache",       fval(v.rootCause));
    } else if (v.vorgangType == "risk") {
        row("Risiko-Level",  v.riskLevel);
        row("W-Score",       std::to_string(v.probabilityScore));
        row("Ges.Score",     std::to_string(v.overallRiskScore));
        row("Strategie",     fval(v.responseStrategy));
    } else if (v.vorgangType == "measure") {
        row("Kategorie",     fval(v.measureCategory));
        row("Geplant",       fdate(v.plannedDate));
        row("Ist",           fdate(v.actualDate));
        row("Wirksamkeit",   fval(v.effectiveness));
    } else if (v.vorgangType == "qualityGate") {
        row("Phase",         fval(v.phase));
        row("Ergebnis",      fval(v.gateResult));
        row("Entscheidung",  fval(v.gateDecision));
    } else if (v.vorgangType == "changeRequest") {
        row("CR-Typ",        fval(v.changeType));
        row("Begründung",    fval(v.justification));
        row("Entscheidung",  fval(v.crDecisionDate));
    } else if (v.vorgangType == "changeObject") {
        row("Basis-CR",      fval(v.parentVorgangId));
        row("Ausgeführt von",fval(v.executedBy));
        row("Ausf.-Datum",   fdate(v.executionDate));
    } else if (v.vorgangType == "lessonsLearned") {
        row("Typ",           fval(v.lessonType));
        row("Empfehlung",    fval(v.recommendation));
    } else if (v.vorgangType == "decisionLog") {
        row("Entsch.-Typ",   fval(v.decisionType));
        row("Rationale",     fval(v.rationale));
        row("Datum",         fdate(v.decisionDate));
    } else if (v.vorgangType == "assumptionConstraint") {
        row("AC-Typ",        fval(v.acType));
        row("Auswirkung",    fval(v.impact));
    } else if (v.vorgangType == "communicationPlan") {
        row("Zielgruppe",    fval(v.audience));
        row("Häufigkeit",    fval(v.frequency));
        row("Kanal",         fval(v.channel));
    }
    hr();
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
    hdr("NEUEN F18 VORGANG ANLEGEN");

    // Step 1: Title
    std::string title = readLine("Titel des Vorgangs: ");
    if (title.empty()) return nullptr;

    // Step 2: Type (ask after title, not upfront)
    std::string chosenType = type;
    if (chosenType.empty()) {
        hdr("TYP WÄHLEN");
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

    auto v = Rosenholz::F18Operation::create(projectId, title, chosenType, taskId);
    if (!v) {
        std::cout << "  >> FEHLER: F18 Vorgang konnte nicht angelegt werden.\n";
        return nullptr;
    }
    if (!owner.empty()) v->ownerId   = owner;
    if (!prio.empty())  v->priority  = prio;
    else                v->priority  = "medium";
    v->update();

    std::cout << "  >> F18 Vorgang angelegt: " << v->vorgangId << "\n";
    std::cout << "  >> Typ: " << v->vorgangType << "  Status: " << v->status << "\n";
    return v;
}

// ── f18Menu and f18BrowserMenu ────────────────────────────────
//
// Interactive menus copied from F18Menu.cpp.

void f18Menu(std::shared_ptr<F18Operation> v) {
    while (true) {
        v->loadSteps();
        printF18Operation(*v);
        drawF18Chain(v->steps);

        // Show release status in header
        bool v_released = (v->status == "released");
        if (v_released) std::cout << "  ⚠ RELEASED — nur Lesezugriff\n";
        std::cout << "  1.Bearbeiten     2.Schritt hinzufügen  3.Schritt öffnen\n"
                     "  4.Notiz          5.Communications       6.Dokumente\n"
                     "  7.Notes anfügen  8.Starte Workflow...   0.Zurück\n";
        int ch = readInt("Wahl",0,8); if (ch==0) break;

        if (ch==1) {
            // Edit common + type-specific fields
            std::cout << "  Titel (leer=behalten): ";
            std::string t; std::getline(std::cin, t);
            if (!t.empty()) v->title = t;
            v->description = readOpt("Beschreibung: ");
            std::cout << "  Priorität: 1.low 2.medium 3.high 4.critical\n";
            int p = readInt("Priorität",1,4);
            static const char* ps[]={"low","medium","high","critical"};
            v->priority = ps[p-1];

            // Type-specific edits
            if (v->vorgangType == "incident") {
                v->severity   = readOpt("Schwere (low|medium|high|critical): ");
                v->rootCause  = readOpt("Ursache: ");
                v->resolution = readOpt("Lösung: ");
                std::string ci = readOpt("Kostenauswirkung (€): ");
                if (!ci.empty()) try { v->costImpact = std::stod(ci); } catch(...) {}
            } else if (v->vorgangType == "risk") {
                std::string ps2 = readOpt("W-Score (1-5): ");
                if (!ps2.empty()) try { v->probabilityScore = std::stoi(ps2); } catch(...) {}
                std::string is = readOpt("A-Zeit (1-5): ");
                if (!is.empty()) try { v->impactScoreTime = std::stoi(is); } catch(...) {}
                std::string ic = readOpt("A-Kosten (1-5): ");
                if (!ic.empty()) try { v->impactScoreCost = std::stoi(ic); } catch(...) {}
                std::string iq = readOpt("A-Qual. (1-5): ");
                if (!iq.empty()) try { v->impactScoreQuality = std::stoi(iq); } catch(...) {}
                std::string isc = readOpt("A-Scope (1-5): ");
                if (!isc.empty()) try { v->impactScoreScope = std::stoi(isc); } catch(...) {}
                v->recalcRiskScore();
                std::cout << "  >> Score: " << v->overallRiskScore
                          << " Level: " << v->riskLevel << "\n";
                return; // recalcRiskScore calls update() already
            } else if (v->vorgangType == "measure") {
                v->measureCategory  = readOpt("Kategorie: ");
                v->effectiveness    = readOpt("Wirksamkeit: ");
                v->actualDate       = readOpt("Ist-Datum: ");
            } else if (v->vorgangType == "qualityGate") {
                v->gateResult   = readOpt("Ergebnis (passed|failed|conditional|pending): ");
                v->gateDecision = readOpt("Entscheidung (proceed|hold|stop): ");
                v->findings     = readOpt("Befunde: ");
            } else if (v->vorgangType == "changeRequest") {
                v->justification      = readOpt("Begründung: ");
                v->crDecisionDate     = readOpt("Entscheidungsdatum: ");
                v->crDecisionRationale= readOpt("Entscheidungsbegründung: ");
            }
            v->update();
            std::cout << "  >> Gespeichert.\n";

        } else if (ch==2) {
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

        } else if (ch==3) {
            // Open a step
            if (v->steps.empty()) { std::cout << "  >> Keine Schritte.\n"; continue; }
            for (int i=0; i<(int)v->steps.size(); ++i)
                std::cout << "  " << (i+1) << ". " << v->steps[i].title
                          << " [" << v->steps[i].status << "]\n";
            int pick = readInt("Schritt #",1,(int)v->steps.size());
            stepMenu(v->steps[pick-1], v->steps);

        } else if (ch==4) {
            std::string note = readLine("Notiz: ");
            std::string by   = readOpt("Von (Person-ID): ");
            v->addNote(by.empty()?"system":by, note);
            std::cout << "  >> Notiz gespeichert.\n";

        } else if (ch==5) {
            communicationMenu(v->vorgangId, "project"); // F18 owns comms via vorgangId
            // Note: use vorgangId as ownerId with type project to allow flexible lookup

        } else if (ch==6) {
            // Documents for this F18 Operation
            documentBrowserMenu(v->projectId, "");
            // Also show docs attached via f18OperationId
            auto f18docs = Rosenholz::Document::loadForEntity("f18", v->vorgangId);
            if (!f18docs.empty()) {
                hdr("DOKUMENTE AN F18 " + v->vorgangId.substr(0,22));
                for (auto& d : f18docs)
                    std::cout << "  " << d->documentId.substr(0,26)
                              << "  " << d->title.substr(0,28) << "\n";
            }

        } else if (ch==7) {
            std::cout << "  Status: 1.draft 2.active 3.completed 4.archived\n";
            int s = readInt("Status",1,4);
            static const char* ss[]={"draft","active","completed","archived"};
            v->status = ss[s-1];
            v->update();
            std::cout << "  >> Status: " << v->status << "\n";
        } else if (ch==8) {
            // Starte Workflow...
            hdr("WORKFLOW — " + v->vorgangId.substr(0,22));
            std::cout << "  Status: " << v->status << "\n";
            if (v->releaseWorkflowId.empty()) {
                std::cout << "  Kein Workflow aktiv.\n";
                std::cout << "  1. Workflow starten...    0. Zurück\n";
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
                std::cout << "  1.Workflow öffnen  0.Zurück\n";
                int mch = readInt("Wahl",0,1);
                if (mch==1) instanceMenu(v->releaseWorkflowId);
            }
        }
    }
}

// ── F18 browser menu ─────────────────────────────────────────
void f18BrowserMenu(const std::string& projectId, const std::string& taskId,
                    const std::string& typeFilter) {
    while (true) {
        std::vector<std::shared_ptr<F18Operation>> items;
        if (!taskId.empty())
            items = F18Operation::loadForTask(taskId, typeFilter);
        else if (!projectId.empty())
            items = F18Operation::loadForProject(projectId, typeFilter);
        else
            items = F18Operation::loadRecent(50);

        std::string ctxLabel = typeFilter.empty() ? "alle Typen" : typeFilter;
        hdr("F18 VORGÄNGE (" + std::to_string(items.size()) + ") — " + ctxLabel);

        if (items.empty()) {
            std::cout << "  (keine F18-Vorgänge)\n\n";
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
        } else if (!projectId.empty()) {
            auto p = ProjectF16::loadById(projectId);
            parentReleased = p && (p->isReleased() || p->isWorkflowComplete());
        }

        if (parentReleased)
            std::cout << "\n  1.Öffnen  0.Zurück  (Released — kein Neu anlegen)\n";
        else
            std::cout << "\n  1.Öffnen  2.Neu anlegen  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;

        if (ch==1) {
            if (items.empty()) continue;
            int pick = readInt("Nummer",1,(int)items.size());
            f18Menu(items[pick-1]);
        } else if (ch==2) {
            if (parentReleased) { std::cout << "  >> Released — kein Neu anlegen.\n"; continue; }
            auto v = createF18Wizard(projectId, taskId, "");
            if (v) f18Menu(v);
        }
    }
}

} // namespace CLI

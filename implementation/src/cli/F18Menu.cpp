// ============================================================
// F18Menu.cpp  —  F18Workflow browser and detail menu
//
// This single menu handles ALL F18 Workflow types:
//   incident, risk, measure, qualityGate,
//   assumptionConstraint, communicationPlan,
//   lessonsLearned, decisionLog,
//   changeRequest, changeObject, generic
// ============================================================
#include "cli_common.h"
#include "../model/f18/F18Workflow.h"
#include "../model/f18/F18WorkflowStep.h"
#include "../model/f18/Communication.h"
#include <iomanip>
#include <algorithm>

namespace CLI {

using namespace Rosenholz;

// ── Step chain display ────────────────────────────────────────
static void drawF18Chain(const std::vector<F18WorkflowStep>& steps) {
    auto sorted = steps;
    std::sort(sorted.begin(), sorted.end(),
        [](const F18WorkflowStep& a, const F18WorkflowStep& b) {
            if (a.isInitialize) return true;
            if (b.isInitialize) return false;
            if (a.isFinal) return false;
            if (b.isFinal) return true;
            return a.sequenceOrder < b.sequenceOrder;
        });

    std::cout << "  Workflow-Kette:\n";
    for (int i = 0; i < (int)sorted.size(); ++i) {
        const auto& s = sorted[i];
        std::string sym = s.status=="approved"?"✓":s.status=="rejected"?"✗":
                          s.status=="in_progress"?">":s.status=="skipped"?"~":" ";
        std::string box = s.isInitialize ? "[INIT" : s.isFinal ? "[ END" : "[    ";
        std::string track;
        if (s.percentComplete > 0 && !s.isInitialize && !s.isFinal)
            track = " " + std::to_string(s.percentComplete) + "%";
        if (s.trackingStatus == "focused")       track += "●";
        else if (s.trackingStatus == "due")      track += "!";
        else if (s.trackingStatus == "archived") track += "■";
        std::cout << "  " << (i>0?"──►":"   ")
                  << box << sym << "] " << std::left << std::setw(18)
                  << s.title.substr(0,17) << track << "\n";
    }
    std::cout << "\n";
}

// ── Step detail menu ─────────────────────────────────────────
static void stepMenu(F18WorkflowStep& step, std::vector<F18WorkflowStep>& allSteps) {
    while (true) {
        hdr("SCHRITT — " + step.stepId.substr(0,22));
        std::cout << "  Titel   : " << step.title << "\n";
        std::cout << "  Typ     : " << step.stepType << "\n";
        std::cout << "  Status  : " << step.status << "\n";
        std::cout << "  Tracking: " << step.trackingStatus
                  << " " << step.percentComplete << "%\n";
        if (!step.assignedTo.empty())
            std::cout << "  Assigned: " << step.assignedTo << "\n";
        std::cout << "\n  1.Ausführen  2.Tracking  3.Notiz  4.Communications  5.Dokumente  0.Zurück\n";
        int ch = readInt("Wahl",0,5); if (ch==0) break;

        if (ch==1) {
            if (!step.canStart(allSteps)) {
                std::cout << "  >> Vorgänger noch nicht abgeschlossen.\n"; continue;
            }
            std::cout << "  1.Genehmigen  2.Ablehnen  3.Überspringen\n";
            int d = readInt("Entscheid",1,3);
            static const char* ds[]={"approved","rejected","skipped"};
            step.decision    = ds[d-1];
            step.decisionBy  = readOpt("Von (Person-ID): ");
            step.comment     = readOpt("Kommentar: ");
            step.status      = ds[d-1];
            step.decisionDate= nowIso();
            step.completedDate=nowIso();
            step.updatedAt   = nowIso();
            step.save();
            // Update allSteps in memory
            for (auto& s : allSteps)
                if (s.stepId == step.stepId) { s = step; break; }
            std::cout << "  >> Schritt: " << step.status << "\n";
        } else if (ch==2) {
            std::cout << "  1.Tracking-Status  2.Fortschritt%  3.Notiz  4.Priorität\n";
            int t = readInt("Was",1,4);
            if (t==1) {
                std::cout << "  1.planned 2.focused 3.due 4.archived\n";
                int s = readInt("Status",1,4);
                static const char* ss[]={"planned","focused","due","archived"};
                step.trackingStatus = ss[s-1];
            } else if (t==2) {
                std::string p = readOpt("% (0-100): ");
                if (!p.empty()) try { step.percentComplete = std::stoi(p); } catch(...) {}
            } else if (t==3) {
                step.progressNote = readOpt("Notiz: ");
            } else {
                std::cout << "  1.low 2.medium 3.high 4.critical\n";
                int p = readInt("Priorität",1,4);
                static const char* ps[]={"low","medium","high","critical"};
                step.priority = ps[p-1];
            }
            step.updatedAt = nowIso(); step.save();
            std::cout << "  >> Tracking aktualisiert.\n";
        } else if (ch==3) {
            std::string note = readLine("Notiz: ");
            std::string by   = readOpt("Von (Person-ID): ");
            // Append to JSON notes
            std::string entry = "{\"text\":\"" + note + "\",\"by\":\"" + by +
                                "\",\"at\":\"" + nowIso() + "\"}";
            if (step.notes == "{}" || step.notes.empty()) step.notes = "[" + entry + "]";
            else if (step.notes.back() == ']') {
                step.notes.pop_back();
                if (step.notes.size()>1) step.notes += ",";
                step.notes += entry + "]";
            }
            step.save();
            std::cout << "  >> Notiz gespeichert.\n";
        } else if (ch==4) {
            communicationMenu(step.stepId, "f18step");
        } else if (ch==5) {
            documentBrowserMenu("", step.stepId);
        }
    }
}

// ── F18 detail menu ───────────────────────────────────────────
void f18Menu(std::shared_ptr<F18Workflow> v) {
    while (true) {
        v->loadSteps();
        printF18Workflow(*v);
        drawF18Chain(v->steps);

        std::cout << "  1.Bearbeiten  2.Schritt hinzufügen  3.Schritt öffnen\n"
                     "  4.Notiz       5.Communications       6.Dokumente\n"
                     "  7.Status ändern  0.Zurück\n";
        int ch = readInt("Wahl",0,7); if (ch==0) break;

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
            auto step = v->addStep(title, sts[st-1], ass);
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
            documentBrowserMenu(v->projectId, v->taskId);

        } else if (ch==7) {
            std::cout << "  Status: 1.draft 2.active 3.completed 4.archived\n";
            int s = readInt("Status",1,4);
            static const char* ss[]={"draft","active","completed","archived"};
            v->status = ss[s-1];
            v->update();
            std::cout << "  >> Status: " << v->status << "\n";
        }
    }
}

// ── F18 browser menu ─────────────────────────────────────────
void f18BrowserMenu(const std::string& projectId, const std::string& taskId) {
    while (true) {
        std::vector<std::shared_ptr<F18Workflow>> items;
        if (!taskId.empty())
            items = F18Workflow::loadForTask(taskId);
        else if (!projectId.empty())
            items = F18Workflow::loadForProject(projectId);
        else
            items = F18Workflow::loadRecent(50);

        hdr("F18 WORKFLOWS (" + std::to_string(items.size()) + ")");
        int n=1;
        for (auto& v : items)
            std::cout << "  " << std::setw(3) << n++ << ". "
                      << "[" << std::left << std::setw(8) << v->vorgangType.substr(0,7) << "] "
                      << std::setw(26) << v->title.substr(0,24)
                      << "  " << v->status << "\n";

        std::cout << "\n  1.Neu anlegen  2.Öffnen\n";
        std::string filterType = readOpt("  Typ-Filter (leer=alle, z.B. risk/incident/...): ");
        std::cout << "  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;

        if (ch==1) {
            auto v = createF18Wizard(projectId, taskId, filterType);
            if (v) f18Menu(v);
        } else if (ch==2) {
            if (items.empty()) continue;
            if (!filterType.empty()) {
                std::vector<std::shared_ptr<F18Workflow>> filtered;
                for (auto& v : items)
                    if (v->vorgangType == filterType) filtered.push_back(v);
                items = filtered;
            }
            if (items.empty()) { std::cout << "  >> Keine Treffer.\n"; continue; }
            int pick = readInt("Nummer",1,(int)items.size());
            f18Menu(items[pick-1]);
        }
    }
}

} // namespace CLI

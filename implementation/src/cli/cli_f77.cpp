// ============================================================
// cli_f77.cpp  —  F77 Workflow: Befehlshandler, Template- und Instanz-Menü
//
// Public functions:
//   cmdF77(args)              — dispatch for 'rh -f77 ...'
//   workflowMenu()            — interactive template + workflow browser
//   instanceMenu(workflowId)  — step execution, validation, locking
//   listWfInstances(...)      — list workflows filtered by entity
//   startWfInstanceWizard(..) — guided workflow start wizard
// ============================================================
#include "cli_common.h"
#include "../model/Utils.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include "../model/f16/ProjectF16.h"
#include "../model/f22/TaskF22.h"
#include "../model/f18/F18Operation.h"
#include "../model/dok/Document.h"
#include <algorithm>
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── -f77 ──────────────────────────────────────────────────────

void cmdF77(const std::vector<std::string>& args) {
    if (args.empty()) {
        auto wfs = F77_Workflow::loadActive();
        if (wfs.empty()) { std::cout << "  (keine aktiven Workflows)\n"; return; }
        std::cout << "  " << std::left
                  << std::setw(28) << "WORKFLOW-ID"
                  << std::setw(24) << "VORLAGE"
                  << std::setw(8)  << "TYP"
                  << "ZIEL\n"
                  << "  " << std::string(68,'-') << "\n";
        for (auto& w : wfs)
            std::cout << "  " << std::setw(28) << w->workflowId
                      << std::setw(24) << w->templateName.substr(0,23)
                      << std::setw(8)  << w->entityType
                      << w->targetState << "\n";
        return;
    }

    if (args[0] == "-tpl" || args[0] == "--templates") {
        auto tpls = F77_WorkflowTemplate::loadAll();
        if (tpls.empty()) { std::cout << "  (keine Vorlagen)\n"; return; }
        for (auto& t : tpls)
            std::cout << "  " << std::left << std::setw(28) << t->templateId
                      << "  " << std::setw(24) << t->name.substr(0,23)
                      << "  ->" << t->targetState
                      << "  [" << t->status << "]\n";
        return;
    }

    if (isId(args[0])) { CLI::instanceMenu(args[0]); return; }

    printErr("Ungültiges Argument: " + args[0]); return;
}


// ── Draw step chain ──────────────────────────────────────────
static void drawF77Chain(const std::vector<F77_WorkflowStep>& steps, bool compact = false) {
    auto sorted = steps;
    std::sort(sorted.begin(), sorted.end(),
        [](const F77_WorkflowStep& a, const F77_WorkflowStep& b) {
            if (a.isInitialize) return true;
            if (b.isInitialize) return false;
            if (a.isFinal) return false;
            if (b.isFinal) return true;
            return a.sequenceOrder < b.sequenceOrder;
        });

    if (compact) {
        std::cout << "  ";
        for (int i = 0; i < (int)sorted.size(); ++i) {
            const auto& s = sorted[i];
            std::string sym = s.status == "approved"    ? "OK" :
                              s.status == "rejected"    ? "X"  :
                              s.status == "in_progress" ? ">"  :
                              s.status == "skipped"     ? "~"  : " ";
            std::string label = s.isInitialize ? "INIT" :
                                s.isFinal      ? "END"  :
                                s.title.substr(0, 10);
            std::cout << "[" << label << sym << "]";
            if (i < (int)sorted.size() - 1) std::cout << "-->";
        }
        std::cout << "\n";
        return;
    }

    std::cout << "\n";
    for (int i = 0; i < (int)sorted.size(); ++i) {
        const auto& s = sorted[i];
        std::string sym = s.status == "approved"    ? "[OK]" :
                          s.status == "rejected"    ? "[X] " :
                          s.status == "in_progress" ? "[ >]" :
                          s.status == "skipped"     ? "[~] " : "[  ]";
        std::string conn = (i == 0) ? "   " : "-->";
        std::string label = s.isInitialize ? "Init" : s.isFinal ? "End " : "    ";
        std::cout << "  " << conn << sym << " " << label
                  << std::left << std::setw(20) << s.title.substr(0, 19);
        if (!s.f18OperationId.empty())
            std::cout << "  F18:" << s.f18OperationId.substr(0, 16);
        if (!s.waitF18OperationId.empty())
            std::cout << "  [WARTE auf F18]";
        std::cout << "\n";
    }
    std::cout << "\n";
}

// ── List F77 workflows ────────────────────────────────────────
void listWfInstances(const std::string& entityType, const std::string& entityId) {
    std::vector<std::shared_ptr<F77_Workflow>> workflows;
    if (!entityType.empty() && !entityId.empty())
        workflows = F77_Workflow::loadForEntity(entityType, entityId);
    else
        workflows = F77_Workflow::loadActive();

    if (workflows.empty()) { std::cout << "  (keine F77-Workflows)\n"; return; }

    int n = 1;
    for (auto& wf : workflows) {
        int done = 0, total = (int)wf->steps.size();
        for (auto& s : wf->steps) if (s.isComplete()) done++;

        std::string sm = wf->status == "completed" ? "[OK]" :
                         wf->status == "active"    ? "[ >]" :
                         wf->status == "locked"    ? "[L] " : "[  ]";

        std::cout << "  " << std::setw(3) << n++ << ". " << sm << " "
                  << std::left << std::setw(26) << wf->templateName.substr(0, 24)
                  << "  ->" << wf->targetState
                  << "  " << done << "/" << total << " Schritte\n";
        if (!wf->steps.empty())
            drawF77Chain(wf->steps, true);
        if (!wf->entityType.empty())
            std::cout << "       Entitaet: " << wf->entityType
                      << " / " << wf->entityId.substr(0, 24) << "\n";
        std::cout << "\n";
    }
}

// ── F77 workflow detail menu ──────────────────────────────────
void instanceMenu(const std::string& workflowId) {
    while (true) {
        auto wf = F77_Workflow::loadById(workflowId);
        if (!wf) { std::cout << "  Workflow nicht gefunden: " << workflowId << "\n"; return; }

        hdr("F77 WORKFLOW  " + wf->templateName);
        auto row = [](const std::string& k, const std::string& v) {
            std::cout << "  | " << std::left << std::setw(18) << k
                      << std::setw(34) << v << "|\n";
        };
        row("ID",          wf->workflowId);
        row("Vorlage",     wf->templateName);
        row("Entitaet",    wf->entityType + " / " + wf->entityId);
        row("Zielzustand", wf->targetState);
        row("Status",      wf->status);
        row("Gestartet",   wf->initiatedDate.substr(0, 16));
        row("Abgeschl.",   fdate(wf->completedDate));
        std::cout << "  +" << std::string(52, '-') << "+\n";

        std::cout << "  Workflow-Kette:";
        drawF77Chain(wf->steps, false);

        bool adminMode = Config::instance().admin().enabled;
        // In non-admin mode F77 runs fully automatic — users only add steps or cancel.
        // Admin mode additionally allows manual step firing for diagnostics.
        if (adminMode)
            std::cout << "  [ADMIN] 1.Schritt manuell ausfuehren  2.Schritt validieren\n"
                         "         3.Schritt hinzufuegen  4.Engine-Tick\n"
                         "  5.Workflow abbrechen  0.Zurueck\n";
        else
            std::cout << "  1.Schritt hinzufuegen  2.Schritt validieren\n"
                         "  3.Engine-Tick (automatisch)\n"
                         "  4.Workflow abbrechen   0.Zurueck\n";
        int ch = readInt("Wahl", 0, adminMode ? 5 : 4);
        if (ch == 0) break;

        // Remap ch to logical action:
        // Admin:     1=fire 2=validate 3=add-step 4=tick 5=cancel
        // Non-admin: 1=add-step 2=validate 3=tick 4=cancel
        int action = ch;
        if (!adminMode) {
            // Non-admin mapping: 1→add, 2→validate, 3→tick, 4→cancel
            static const int map[] = {0, 3, 2, 4, 5};
            action = (ch >= 1 && ch <= 4) ? map[ch] : 0;
        }

        if (action == 1) {
            // ── [ADMIN ONLY] Manuelles Step-Firing ──────────────────────────
            std::vector<F77_WorkflowStep*> fireable;
            for (auto& s : wf->steps)
                if (!s.isInitialize && !s.isFinal && !s.isComplete())
                    fireable.push_back(&s);
            if (fireable.empty()) {
                std::cout << "  Keine ausfuehrbaren Schritte.\n"; continue;
            }
            for (int i = 0; i < (int)fireable.size(); ++i)
                std::cout << "    " << (i+1) << ". " << fireable[i]->title
                          << " [" << fireable[i]->status << "]\n";
            int pick = readInt("Schritt #", 1, (int)fireable.size());
            const std::string stepId   = fireable[pick-1]->stepId;
            const bool needsComment    = fireable[pick-1]->requiresComment;
            std::cout << "  Entscheidung:  1.Genehmigen  2.Ablehnen  3.Ueberspringen\n";
            int dec = readInt("Entscheidung", 1, 3);
            static const char* decisions[] = {"approved", "rejected", "skipped"};
            std::string comment;
            if (needsComment || dec == 2) {
                std::cout << "  Kommentar: ";
                std::getline(std::cin, comment);
            }
            std::string actor = readOpt("Bearbeiter (Person-ID, leer=System): ");
            if (actor.empty()) actor = "system";
            if (F77_Engine::fireStep(*wf, stepId, decisions[dec-1], actor, comment))
                std::cout << "  >> Schritt ausgefuehrt.\n";
            else
                std::cout << "  >> Fehler — Vorbedingungen nicht erfuellt?\n";
        }

        else if (action == 2) {
            // Validierung — Trockenlauf ohne Zustandsänderung
            if (wf->steps.empty()) { std::cout << "  Keine Schritte.\n"; continue; }
            hdr("VALIDIERUNG — " + wf->templateName);
            for (const auto& s : wf->steps) {
                std::string result = F77_Engine::validateStep(*wf, s.stepId);
                std::cout << "  " << std::left << std::setw(22) << s.title.substr(0,21)
                          << "  " << result << "\n";
            }
            std::cout << "\n  (Keine Aenderungen vorgenommen)\n";
        }

        else if (action == 3) {
            // ── Optionalen Schritt hinzufügen ─────────────────────────────────
            // Only allowed while workflow is active and End step not yet reached
            if (wf->status != "active") {
                std::cout << "  >> Nur bei aktivem Workflow moeglich.\n"; continue;
            }
            // Check End step not yet done
            bool endDone = false;
            for (auto& s : wf->steps) if (s.isFinal && s.isComplete()) { endDone = true; break; }
            if (endDone) {
                std::cout << "  >> End-Schritt bereits abgeschlossen.\n"; continue;
            }
            std::cout << "  Neuen optionalen Schritt hinzufuegen.\n"
                      << "  Jeder Schritt spawnt eine F18-Operation (Typ: measure)\n"
                      << "  die manuell als erledigt markiert werden muss.\n";
            std::string title = readLine("Schritt-Titel (leer=Abbrechen): ");
            if (title.empty()) continue;
            std::string desc  = readOpt("Beschreibung (optional): ");

            // Spawn F18 Operation of type "measure" linked to this workflow step
            auto f18 = F18Operation::create(title, "measure", wf->entityId.empty()
                                             ? wf->workflowId : wf->entityId);
            if (f18) {
                if (!desc.empty()) f18->description = desc;
                f18->notes = "Automatisch aus F77-Workflow: " + wf->workflowId;
                f18->save();

                // Add the step to the running workflow instance (before End step)
                F77_WorkflowStep step;
                step.stepId      = genId("F77S");
                step.workflowId  = wf->workflowId;
                step.title       = title;
                step.autoApprove = false;
                step.isInitialize= false;
                step.isFinal     = false;
                step.f18OperationId = f18->vorgangId;
                // Insert before End step: give it sequenceOrder = end-1
                int endOrder = 999;
                for (auto& s : wf->steps)
                    if (s.isFinal) { endOrder = s.sequenceOrder - 1; break; }
                step.sequenceOrder = endOrder;
                // Predecessor: last non-final step
                for (auto& s : wf->steps)
                    if (!s.isFinal && !s.predecessors.empty())
                        step.predecessors = {s.stepId};
                step.save();
                wf->loadSteps();
                std::cout << "  >> Schritt '" << title << "' angelegt.\n"
                          << "  >> F18-Operation: " << f18->vorgangId << "\n"
                          << "  >> Schritt wird automatisch abgehakt, sobald F18 abgeschlossen.\n";
            } else {
                std::cout << "  >> Fehler beim Anlegen der F18-Operation.\n";
            }
        }

        else if (action == 4) {
            F77_Engine::tick(*wf);
            std::cout << "  >> Engine-Tick ausgefuehrt.\n";
        }

        else if (action == 5) {
            // Workflow abbrechen
            std::string confirm = readOpt("Workflow wirklich abbrechen? (ja/nein): ");
            if (confirm == "ja") {
                F77_Engine::cancelWorkflow(*wf);
                std::cout << "  >> Workflow abgebrochen. Entitaet kann neuen Workflow starten.\n";
                break;
            }
        }
    }
}

// ── Start new F77 workflow ────────────────────────────────────
std::string startWfInstanceWizard(const std::string& entityType,
                                   const std::string& entityId)
{
    hdr("NEUEN F77-WORKFLOW STARTEN");
    std::string effType = entityType.empty()
        ? readLine("Entitaetstyp (f16/f22/f18/dok): ") : entityType;
    std::string effId = entityId.empty()
        ? readLine("Entitaets-ID: ") : entityId;

    // One-workflow guard is enforced by F77_Engine::startDefault/startFromTemplate.

    auto templates = F77_WorkflowTemplate::loadForEntityType(effType);
    std::shared_ptr<F77_Workflow> wf;

    if (!templates.empty()) {
        std::cout << "\n  Verfuegbare Vorlagen:\n";
        int ti = 1;
        for (auto& t : templates)
            std::cout << "    " << ti++ << ". " << t->name
                      << "  -> " << t->targetState << "\n";
        std::cout << "    " << ti << ". Standard-Freigabe (ohne Vorlage)\n";
        int choice = readInt("Vorlage waehlen", 1, (int)templates.size() + 1);
        std::string actor = readOpt("Gestartet von (Person-ID, leer=System): ");
        if (actor.empty()) actor = "system";

        if (choice <= (int)templates.size()) {
            wf = F77_Engine::startFromTemplate(
                templates[choice-1]->templateId, effType, effId, actor);
        } else {
            std::cout << "  1.in_work  2.pre_released  3.released  4.locked  5.closed\n";
            static const char* sts[] = {"in_work","pre_released","released","locked","closed"};
            int si = readInt("Zielzustand", 1, 5);
            wf = F77_Engine::startDefault(effType, effId, sts[si-1], actor);
        }
    } else {
        std::string actor = readOpt("Gestartet von (Person-ID, leer=System): ");
        if (actor.empty()) actor = "system";
        std::cout << "  1.in_work  2.pre_released  3.released  4.locked  5.closed\n";
        static const char* sts[] = {"in_work","pre_released","released","locked","closed"};
        int si = readInt("Zielzustand", 1, 5);
        wf = F77_Engine::startDefault(effType, effId, sts[si-1], actor);
    }

    if (!wf) { std::cout << "  >> FEHLER beim Starten.\n"; return ""; }

    F77_Engine::attachWorkflow(effType, effId, wf->workflowId);
    std::cout << "  >> F77-Workflow gestartet: " << wf->workflowId << "\n";

    // ── Optional: zusätzliche Schritte hinzufügen ─────────────────────────────
    // Ask if the user wants to insert manual steps (spawning F18 operations)
    // BEFORE the automatic DB-write and End steps run.
    std::cout << "\n  Optional: Weitere Schritte hinzufuegen?\n"
              << "  (Jeder Schritt erzeugt eine F18-Operation, die manuell\n"
              << "   abzuschliessen ist. Der Workflow wartet auf jeden Schritt.)\n";
    while (yesno("  Weiteren Schritt hinzufuegen?")) {
        std::string title = readLine("  Schritt-Titel: ");
        if (title.empty()) break;
        std::string desc = readOpt("  Beschreibung (optional): ");

        auto f18 = F18Operation::create(title, "measure", effId);
        if (f18) {
            if (!desc.empty()) f18->description = desc;
            f18->notes = "F77-Workflow Pflichtschritt: " + wf->workflowId;
            f18->save();

            F77_WorkflowStep step;
            step.stepId         = genId("F77S");
            step.workflowId     = wf->workflowId;
            step.title          = title;
            step.autoApprove    = false;
            step.isInitialize   = false;
            step.isFinal        = false;
            step.f18OperationId = f18->vorgangId;
            // Place before the DB-write/End steps
            wf->loadSteps();
            int endOrder = 999;
            for (auto& s : wf->steps)
                if (s.isFinal) { endOrder = s.sequenceOrder - 1; break; }
            step.sequenceOrder = endOrder;
            // Chain after last non-final step
            for (auto& s : wf->steps)
                if (!s.isFinal && s.isInitialize == false && !s.predecessors.empty())
                    step.predecessors = {s.stepId};
            step.save();
            wf->loadSteps();
            std::cout << "  >> Schritt '" << title << "' + F18 '" << f18->vorgangId
                      << "' angelegt.\n";
        } else {
            std::cout << "  >> Fehler beim Anlegen.\n"; break;
        }
    }

    // Run initial tick to start the engine
    F77_Engine::tick(*wf);
    return wf->workflowId;
}

// ── Template management ───────────────────────────────────────
static void templateMenu(const std::string& templateId) {
    auto tpl = F77_WorkflowTemplate::loadById(templateId);
    if (!tpl) return;
    while (true) {
        hdr("F77-VORLAGE  " + tpl->name);
        std::cout << "  ID:           " << tpl->templateId << "\n"
                  << "  Version:      " << tpl->version    << "\n"
                  << "  Zielzustand:  " << tpl->targetState << "\n"
                  << "  Entitaeten:   " << fval(tpl->entityTypes) << "\n"
                  << "  Status:       " << tpl->status << "\n";

        std::cout << "  Vorlage-Schritte:\n";
        if (tpl->steps.empty()) {
            std::cout << "    (keine Schritte)\n";
        } else {
            auto sorted = tpl->steps;
            std::sort(sorted.begin(), sorted.end(),
                [](const F77_WorkflowTemplateStep& a, const F77_WorkflowTemplateStep& b) {
                    if (a.isInitialize) return true;
                    if (b.isInitialize) return false;
                    if (a.isFinal) return false;
                    if (b.isFinal) return true;
                    return a.sequenceOrder < b.sequenceOrder;
                });
            for (int ai = 0; ai < (int)sorted.size(); ++ai) {
                const auto& s = sorted[ai];
                std::string box = s.isInitialize ? "[INIT" :
                                  s.isFinal      ? "[ END" : "[    ";
                std::string flags;
                if (s.autoApprove) flags += " AUTO";
                if (!s.waitConditionF18Type.empty()) flags += " WARTE:" + s.waitConditionF18Type;
                std::cout << "  " << (ai > 0 ? "-->" : "   ")
                          << box << " ] "
                          << std::left << std::setw(22) << s.title.substr(0, 21)
                          << flags << "\n";
            }
        }

        std::cout << "\n  1.Schritt hinzufuegen  2.Vorlage deaktivieren  0.Zurueck\n";
        int ch = readInt("Wahl", 0, 2); if (ch == 0) break;

        else if (ch == 1) {
            std::string title = readLine("Schritt-Titel: ");
            if (title.empty()) continue;
            std::cout << "  Modus:  1.sequentiell  2.parallel\n";
            int et = readInt("Modus", 1, 2);
            auto step = tpl->addTemplateStep(title, et == 2 ? "parallel" : "sequential", false, false);
            step.requiredRole = readOpt("Erforderliche Rolle (leer=beliebig): ");
            std::string slaS = readOpt("SLA Stunden (0=ohne): ");
            if (!slaS.empty()) try { step.slaHours = std::stoi(slaS); } catch(...) {}
            std::string autoS = readOpt("Automatisch genehmigen? (j/n): ");
            step.autoApprove = (!autoS.empty() && (autoS[0]=='j'||autoS[0]=='J'));
            std::string waitS = readOpt("Wartebedingung F18-Typ (leer=keine, z.B. measure): ");
            step.waitConditionF18Type = waitS;
            if (!waitS.empty())
                step.waitConditionTitle = readOpt("Titel fuer Warte-F18-Operation: ");
            step.save();
            std::cout << "  >> Schritt hinzugefuegt: " << step.tplStepId << "\n";
        }

        else if (ch == 2) {
            tpl->status = "inactive";
            tpl->save();
            std::cout << "  >> Vorlage deaktiviert.\n";
        }
    }
}

// ── Main workflow browser ─────────────────────────────────────
void workflowMenu() {
    while (true) {
        hdr("F77 WORKFLOW-VERWALTUNG");
        std::cout << "  VORLAGEN (deklarativ, nur Admin)\n"
                     "    1. Alle Vorlagen anzeigen\n"
                     "    2. Neue Vorlage erstellen\n"
                     "    3. Vorlage oeffnen/bearbeiten\n"
                     "  LAUFENDE WORKFLOWS\n"
                     "    4. Alle aktiven F77-Workflows\n"
                     "    5. Workflow per ID oeffnen\n"
                     "    6. Neuen Workflow starten\n"
                     "    0. Zurueck\n";
        int ch = readInt("Wahl", 0, 6); if (ch == 0) break;

        else if (ch == 1) {
            auto templates = F77_WorkflowTemplate::loadAll();
            hdr("F77-VORLAGEN (" + std::to_string(templates.size()) + ")");
            if (templates.empty()) std::cout << "  (keine)\n";
            int n = 1;
            for (auto& t : templates)
                std::cout << "  " << std::setw(3) << n++ << ". "
                          << std::left << std::setw(28) << t->name
                          << "  -> " << std::setw(14) << t->targetState
                          << "  v" << t->version << "  [" << t->status << "]\n";
            std::cout << "\n";
        }

        else if (ch == 2) {
            std::string name = readLine("Vorlagenname: ");
            if (name.empty()) continue;
            std::cout << "  Zielzustand:\n"
                         "  1.in_work  2.pre_released  3.released  4.locked  5.closed\n";
            static const char* sts[] = {"in_work","pre_released","released","locked","closed"};
            int si = readInt("Zielzustand", 1, 5);
            std::string entityTypes = readOpt("Entitaetstypen (z.B. f16,f22,dok, leer=alle): ");
            if (entityTypes.empty()) entityTypes = "f16,f22,f18,dok";
            auto tpl = F77_WorkflowTemplate::create(name, sts[si-1], entityTypes);
            tpl->description = readOpt("Beschreibung: ");
            tpl->save();
            // Auto-add Init + End bookends
            auto init = tpl->addTemplateStep("Init", "sequential", true, false);
            init.save();
            auto end = tpl->addTemplateStep("End", "sequential", false, true);
            end.autoApprove = true;
            end.predecessorTplStepIds = init.tplStepId;
            end.save();
            std::cout << "  >> Vorlage angelegt: " << tpl->templateId << "\n";
            templateMenu(tpl->templateId);
        }

        else if (ch == 3) {
            auto templates = F77_WorkflowTemplate::loadAll();
            if (templates.empty()) { std::cout << "  (keine Vorlagen)\n"; continue; }
            int n = 1;
            for (auto& t : templates)
                std::cout << "  " << n++ << ". " << t->name << "\n";
            int pick = readInt("Nummer", 1, (int)templates.size());
            templateMenu(templates[pick-1]->templateId);
        }

        else if (ch == 4) {
            hdr("AKTIVE F77-WORKFLOWS");
            listWfInstances("", "");
        }

        else if (ch == 5) {
            std::string id = readLine("Workflow-ID: ");
            instanceMenu(id);
        }

        else if (ch == 6) {
            std::string wid = startWfInstanceWizard("", "");
            if (!wid.empty()) instanceMenu(wid);
        }
    }
}

} // namespace CLI

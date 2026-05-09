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
#include "../workflow/F77Task.h"
#include "../model/Utils.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/f18/F18Operation.h"
#include "../model/akt/Folder.h"
#include <algorithm>
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── -f77 ──────────────────────────────────────────────────────

void cmdF77(const std::vector<std::string>& args) {
    if (args.empty()) {
        auto wfs = F77W::loadActive();
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
                      << entityStatusToString(w->targetState) << "\n";
        return;
    }

    // -o: list all F77 workflows (active + recent), pick one to open
    // -so <q>: search by template name or entity ID
    if (args[0] == "-o" || args[0] == "-so") {
        bool doSearch = (args[0] == "-so");
        std::string q = (doSearch && args.size()>1) ? args[1] : "";
        auto all = F77W::loadAll(100);
        std::vector<std::shared_ptr<F77W>> hits;
        for (auto& w : all) {
            if (q.empty()) { hits.push_back(w); continue; }
            if (matchesPattern(w->templateName, q) || matchesPattern(w->entityId, q)
                || matchesPattern(w->workflowId, q)) hits.push_back(w);
        }
        if (hits.empty()) { std::cout << "  (keine F77)\n"; return; }
        std::cout << "\n  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(22) << "VORLAGE"
                  << std::setw(8)  << "TYP"
                  << std::setw(18) << "ENTITÄT"
                  << std::setw(12) << "ZIEL"
                  << "STATUS\n"
                  << "  " << std::string(92,'-') << "\n";
        for (int i=0; i<(int)hits.size(); i++)
            std::cout << "  " << std::setw(4) << (i+1)
                      << std::setw(26) << hits[i]->workflowId
                      << std::setw(22) << hits[i]->templateName.substr(0,20)
                      << std::setw(8)  << hits[i]->entityType
                      << std::setw(18) << hits[i]->entityId.substr(0,16)
                      << std::setw(12) << entityStatusToString(hits[i]->targetState)
                      << Color::statusColor(std::string(toString(hits[i]->status))) << "\n";
        int pick = readInt("  Auswahl [0=Abbrechen]", 0, (int)hits.size());
        if (pick < 1) return;
        CLI::instanceMenu(hits[pick-1]->workflowId);
        return;
    }

    if (args[0] == "-tpl" || args[0] == "--templates") {
        auto tpls = F77W_Template::loadAll();
        if (tpls.empty()) { std::cout << "  (keine Vorlagen)\n"; return; }
        for (auto& t : tpls)
            std::cout << "  " << std::left << std::setw(28) << t->templateId
                      << "  " << std::setw(24) << t->name.substr(0,23)
                      << "  ->" << entityStatusToString(t->targetState)
                      << "  [" << templateStatusToString(t->status) << "]\n";
        return;
    }

    if (isId(args[0])) { CLI::instanceMenu(args[0]); return; }

    printErr("Ungültiges Argument: " + args[0]); return;
}


// ── Draw step chain ──────────────────────────────────────────
static void drawF77Chain(const std::vector<F77W_Operation>& steps, bool compact = false) {
    auto sorted = steps;
    std::sort(sorted.begin(), sorted.end(),
        [](const F77W_Operation& a, const F77W_Operation& b) {
            if (a.isInitialize) return true;
            if (b.isInitialize) return false;
            if (a.isFinal) return false;
            if (b.isFinal) return true;
            return a.sequenceOrder < b.sequenceOrder;
        });

    if (compact) {
        std::cout << "  ";
        for (auto& step : sorted) {
            std::cout << Color::stepSymbolColored(step.status) << " ";
        }
        std::cout << "\n";
        return;
    }

    // Full display with descriptions
    for (auto& s : sorted) {
        std::string indent = s.isInitialize || s.isFinal ? "   " : "-->";
        std::cout << "  " << indent
                  << Color::stepSymbolColored(s.status) << " "
                  << std::left << std::setw(24) << s.title.substr(0, 22);

        // Status explanation
        if (s.isSystem) {
            switch (s.systemAction) {
                case SystemAction::SCAN_UNREGISTERED_FILES:
                    std::cout << " [System: Dateien prüfen]";     break;
                case SystemAction::COMMIT_DB_OBJECTS:
                    std::cout << " [System: Objekte sichern]";    break;
                default:
                    std::cout << " [System]";                     break;
            }
        } else if (s.isFinal) {
            std::cout << " [Auto-Abschluss]";
        } else if (s.isInitialize) {
            std::cout << " [Auto-Start]";
        } else {
            // Show pending reason
            if (s.status == StepStatus::PENDING || s.status == StepStatus::IN_PROGRESS) {
                auto tasks = F77Task::loadForOperation(s.stepId);
                int open = 0;
                for (auto& t : tasks) if (t->isOpen()) open++;
                if (open > 0)
                    std::cout << " [" << open << " offene Aufgabe(n)]";
                else
                    std::cout << " [wartet auf Vorgänger]";
            }
        }
        std::cout << "\n";
    }
}
// ── F77 workflow detail menu ──────────────────────────────────
void instanceMenu(const std::string& workflowId) {
    while (true) {
        auto wf = F77W::loadById(workflowId);
        if (!wf) { std::cout << "  Workflow nicht gefunden: " << workflowId << "\n"; return; }

        hdr("F77  " + wf->templateName);
        auto row = [](const std::string& k, const std::string& v) {
            std::cout << "  | " << std::left << std::setw(18) << k
                      << std::setw(34) << v << "|\n";
        };
        row("ID",          wf->workflowId);
        row("Vorlage",     wf->templateName);
        // Resolve entity display name:
        std::string entityDisplay = wf->entityType + " / " + wf->entityId;
        std::string entityTitle;
        if (wf->entityType == "f22") {
            auto t = F22::loadById(wf->entityId);
            if (t) entityTitle = t->title.substr(0,30);
        } else if (wf->entityType == "f18") {
            auto v = F18Operation::loadById(wf->entityId);
            if (v) entityTitle = v->title.substr(0,30);
        } else if (wf->entityType == "akt") {
            auto d = Folder::loadById(wf->entityId);
            if (d) entityTitle = d->title.substr(0,30);
        }
        row("Entitaet",    entityDisplay);
        if (!entityTitle.empty()) row("  Bezeichnung", entityTitle);
        row("Zielzustand", std::string(entityStatusToString(wf->targetState)));
        row("Status", std::string(toString(wf->status)));
        row("Gestartet",   wf->initiatedDate.substr(0, 16));
        row("Abgeschl.",   fdate(wf->completedDate));
        std::cout << "  +" << std::string(52, '-') << "+\n";

        std::cout << "  F77-Kette:";
        drawF77Chain(wf->steps, false);

        bool adminMode = Config::instance().admin().enabled;
        // In non-admin mode F77 runs fully automatic — users only add steps or cancel.
        // Admin mode additionally allows manual step firing for diagnostics.
        if (adminMode)
            std::cout << "  [ADMIN] 1.Schritt manuell ausfuehren  2.Schritt simulieren\n"
                         "         3.Schritt hinzufuegen  4.Engine-Tick\n"
                         "  5.F77 abbrechen  0.Zurueck\n";
        else
            std::cout << "  1.Schritt hinzufuegen  2.Schritt simulieren\n"
                         "  3.Engine-Tick (automatisch)\n"
                         "  4.F77 abbrechen   0.Zurueck\n";
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
            std::vector<F77W_Operation*> fireable;
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
            if (F77Engine::fireStep(*wf, stepId, decisions[dec-1], actor, comment))
                std::cout << "  >> Schritt ausgefuehrt.\n";
            else
                std::cout << "  >> Fehler — Vorbedingungen nicht erfuellt?\n";
        }

        else if (action == 2) {
            // Validierung — Trockenlauf ohne Zustandsänderung
            if (wf->steps.empty()) { std::cout << "  Keine Schritte.\n"; continue; }
            hdr("VALIDIERUNG — " + wf->templateName);
            for (const auto& s : wf->steps) {
                std::string result = F77Engine::validateStep(*wf, s.stepId);
                std::cout << "  " << std::left << std::setw(22) << s.title.substr(0,21)
                          << "  " << result << "\n";
            }
            std::cout << "\n  (Keine Aenderungen vorgenommen)\n";
        }

        else if (action == 3) {
            // ── Optionalen Schritt hinzufügen ─────────────────────────────────
            // Only allowed while workflow is active and End step not yet reached
            if (wf->status != WorkflowStatus::ACTIVE) {
                std::cout << "  >> Nur bei aktivem F77 möglich.\n"; continue;
            }
            // Check End step not yet done
            bool endDone = false;
            for (auto& s : wf->steps) if (s.isFinal && s.isComplete()) { endDone = true; break; }
            if (endDone) {
                std::cout << "  >> End-Schritt bereits abgeschlossen.\n"; continue;
            }
            std::cout << "  Neuen optionalen Schritt hinzufuegen.\n"
                      << "  Jeder Schritt erzeugt eine F18 (Typ: measure)\n"
                      << "  die manuell als erledigt markiert werden muss.\n";
            std::string title = readLine("Schritt-Titel (leer=Abbrechen): ");
            if (title.empty()) continue;
            std::string desc  = readOpt("Beschreibung (optional): ");

            std::string ass = readOpt("Zugewiesen an Person-ID (leer=offen): ");
            std::string opId = F77Engine::addManualOperation(*wf, title, desc, ass);
            if (!opId.empty()) {
                wf->loadSteps();
                std::cout << "  >> Operation '" << title << "' angelegt.\n"
                          << "  >> F77-Task erstellt — erreichbar unter: rh -tasks\n";
            } else {
                std::cout << "  >> Fehler beim Anlegen.\n";
            }
        }

        else if (action == 4) {
            F77Engine::tick(*wf);
            std::cout << "  >> Engine-Tick ausgefuehrt.\n";
        }

        else if (action == 5) {
            // Workflow abbrechen
            std::string confirm = readOpt("F77 wirklich abbrechen? (ja/nein): ");
            if (confirm == "ja") {
                F77Engine::cancelWorkflow(*wf);
                std::cout << "  >> F77 abgebrochen. Entität kann neuen F77 starten.\n";
                break;
            }
        }
    }
}

// ── Start new F77 workflow ────────────────────────────────────
std::string startWfInstanceWizard(const std::string& entityType,
                                   const std::string& entityId)
{
    hdr("F77 STARTEN");
    std::string effType = entityType.empty()
        ? readLine("Entitaetstyp (f16/f22/f18/akt): ") : entityType;
    std::string effId = entityId.empty()
        ? readLine("Entitaets-ID: ") : entityId;

    // One-workflow guard is enforced by F77Engine::startDefault/startFromTemplate.

    auto templates = F77W_Template::loadForEntityType(effType);
    std::shared_ptr<F77W> wf;

    if (!templates.empty()) {
        std::cout << "\n  Verfuegbare Vorlagen:\n";
        int ti = 1;
        for (auto& t : templates)
            std::cout << "    " << ti++ << ". " << t->name
                      << "  -> " << entityStatusToString(t->targetState) << "\n";
        std::cout << "    " << ti << ". Standard-Freigabe (ohne Vorlage)\n";
        int choice = readInt("Vorlage waehlen", 1, (int)templates.size() + 1);
        std::string actor = readOpt("Gestartet von (Person-ID, leer=System): ");
        if (actor.empty()) actor = "system";

        if (choice <= (int)templates.size()) {
            wf = F77Engine::startFromTemplate(
                templates[choice-1]->templateId, effType, effId, actor);
        } else {
            std::cout << "  1.in_work  2.pre_released  3.released  4.locked  5.closed\n";
            static const EntityStatus sts[] = {
            EntityStatus::IN_WORK, EntityStatus::PRE_RELEASED,
            EntityStatus::RELEASED, EntityStatus::LOCKED, EntityStatus::CLOSED
        };
            int si = readInt("Zielzustand", 1, 5);
            wf = F77Engine::startDefault(effType, effId, sts[si-1], actor);
        }
    } else {
        std::string actor = readOpt("Gestartet von (Person-ID, leer=System): ");
        if (actor.empty()) actor = "system";
        std::cout << "  1.in_work  2.pre_released  3.released  4.locked  5.closed\n";
        static const EntityStatus sts[] = {
            EntityStatus::IN_WORK, EntityStatus::PRE_RELEASED,
            EntityStatus::RELEASED, EntityStatus::LOCKED, EntityStatus::CLOSED
        };
        int si = readInt("Zielzustand", 1, 5);
        wf = F77Engine::startDefault(effType, effId, sts[si-1], actor);
    }

    if (!wf) { std::cout << "  >> FEHLER beim Starten.\n"; return ""; }

    F77Engine::attachWorkflow(effType, effId, wf->workflowId);
    std::cout << "  >> F77 gestartet: " << wf->workflowId << "\n";

    // ── Auto-spawn F77Tasks for pending F18S steps (if F18 entity) ─────────────
    if (effType == "f18") {
        auto f18op = F18Operation::loadById(effId);
        if (f18op) {
            f18op->loadSteps();
            int spawned = 0;
            for (auto& s : f18op->steps) {
                if (!s.isInitialize && !s.isFinal && !s.isComplete()) {
                    auto opId = F77Engine::addManualOperation(
                        *wf, "F18S: " + s.title, s.stepId, s.assignedTo);
                    if (!opId.empty()) {
                        std::cout << "  >> F77-Task angelegt für F18S: "
                                  << s.title << "\n";
                        ++spawned;
                    }
                }
            }
            if (spawned > 0)
                std::cout << "  >> " << spawned
                          << " F77-Task(s) für offene F18S-Schritte angelegt.\n";
        }
    }

    // ── Optional: manuelle Schritte vor dem ersten Tick hinzufügen ────────────
    // IMPORTANT: steps must be added BEFORE tick() so End is not yet auto-approved.
    std::cout << "\n  Manuelle Operationen hinzufuegen?\n"
              << "  Jede Operation erzeugt einen F77-Task in Meine Aufgaben.\n"
              << "  Die F77-Engine wartet auf Abschluss aller Tasks.\n";
    while (yesno("  Operation hinzufuegen?")) {
        std::string title = readLine("  Titel: ");
        if (title.empty()) break;
        std::string desc  = readOpt("  Beschreibung (optional): ");
        std::string ass   = readOpt("  Zugewiesen an Person-ID (leer=offen): ");

        std::string opId = F77Engine::addManualOperation(*wf, title, desc, ass);
        if (!opId.empty())
            std::cout << "  >> Operation '" << title << "' angelegt — F77-Task erstellt.\n"
                      << "  >> Erreichbar unter: rh -tasks\n";
        else
            std::cout << "  >> Fehler beim Anlegen.\n";
        wf->loadSteps();
    }

    // Run initial tick — End will be blocked until all F77Tasks are closed
    F77Engine::tick(*wf);
    return wf->workflowId;
}

// ── Template management ───────────────────────────────────────
static void templateMenu(const std::string& templateId) {
    auto tpl = F77W_Template::loadById(templateId);
    if (!tpl) return;
    while (true) {
        hdr("F77-VORLAGE  " + tpl->name);
        std::cout << "  ID:           " << tpl->templateId << "\n"
                  << "  Version:      " << tpl->version    << "\n"
                  << "  Zielzustand:  " << entityStatusToString(tpl->targetState) << "\n"
                  << "  Entitaeten:   " << fval(tpl->entityTypes) << "\n"
                  << "  Status:       " << templateStatusToString(tpl->status) << "\n";

        std::cout << "  Vorlage-Schritte:\n";
        if (tpl->steps.empty()) {
            std::cout << "    (keine Schritte)\n";
        } else {
            auto sorted = tpl->steps;
            std::sort(sorted.begin(), sorted.end(),
                [](const F77W_TemplateStep& a, const F77W_TemplateStep& b) {
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
            step.save();
            std::cout << "  >> Schritt hinzugefuegt: " << step.tplStepId << "\n";
        }

        else if (ch == 2) {
            tpl->deactivate();
            std::cout << "  >> Vorlage deaktiviert.\n";
        }
    }
}
} // namespace CLI

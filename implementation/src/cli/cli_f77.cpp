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

    die("Ungültiges Argument: " + args[0]);
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

        std::cout << "  1.Schritt ausfuehren  2.Schritt validieren  3.Engine-Tick\n"
                     "  4.Workflow sperren    5.Zielzustand aendern\n"
                     "  6.Workflow abbrechen  0.Zurueck\n";
        int ch = readInt("Wahl", 0, 6);
        if (ch == 0) break;

        else if (ch == 1) {
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
            // Capture stepId and flags as plain values before any reload
            const std::string stepId       = fireable[pick-1]->stepId;
            const bool needsComment        = fireable[pick-1]->requiresComment;

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
                std::cout << "  >> Fehler -- Vorbedingungen nicht erfuellt?\n";
        }

        else if (ch == 2) {
            // Validation: dry-run check without state change
            if (wf->steps.empty()) { std::cout << "  Keine Schritte.\n"; continue; }
            hdr("VALIDIERUNG — " + wf->templateName);
            for (const auto& s : wf->steps) {
                std::string result = F77_Engine::validateStep(*wf, s.stepId);
                std::cout << "  " << std::left << std::setw(22) << s.title.substr(0,21)
                          << "  " << result << "\n";
            }
            std::cout << "\n  (Keine Aenderungen vorgenommen)\n";
        }

        else if (ch == 3) {
            F77_Engine::tick(*wf);
            std::cout << "  >> Engine-Tick ausgefuehrt.\n";
        }

        else if (ch == 4) {
            std::string confirm = readOpt("Workflow wirklich sperren? (ja/nein): ");
            if (confirm == "ja") {
                wf->status = "locked";
                wf->update();
                std::cout << "  >> Workflow gesperrt.\n";
            }
        }

        else if (ch == 5) {
            std::cout << "  1.in_work  2.pre_released  3.released  4.locked  5.closed\n";
            static const char* sts[] = {"in_work","pre_released","released","locked","closed"};
            int si = readInt("Zielzustand", 1, 5);
            wf->targetState = sts[si-1];
            wf->update();
            std::cout << "  >> Zielzustand gesetzt: " << wf->targetState << "\n";
        }

        else if (ch == 6) {
            // Cancel workflow: sets status = cancelled.
            // The entity's releaseWorkflowId remains set so history is visible,
            // but the entity is free to start a new workflow afterwards.
            std::string confirm = readOpt("Workflow wirklich abbrechen? (ja/nein): ");
            if (confirm == "ja") {
                wf->status = "cancelled";
                wf->update();
                // Clear releaseWorkflowId on the entity so a new workflow can be started
                auto* f16db = DatabasePool::instance().get("f16");
                auto* f22db = DatabasePool::instance().get("f22");
                auto* f18db = DatabasePool::instance().get("f18");
                auto* dokdb = DatabasePool::instance().get("dok");
                std::string now = nowIso();
                if (wf->entityType == "f16" && f16db)
                    f16db->exec("UPDATE projects SET release_workflow_id=NULL, updated_at=? WHERE project_id=?;",
                        {BindParam::text(now), BindParam::text(wf->entityId)});
                else if (wf->entityType == "f22" && f22db)
                    f22db->exec("UPDATE tasks SET release_workflow_id=NULL, updated_at=? WHERE task_id=?;",
                        {BindParam::text(now), BindParam::text(wf->entityId)});
                else if (wf->entityType == "f18" && f18db)
                    f18db->exec("UPDATE f18_operations SET release_workflow_id=NULL, updated_at=? WHERE vorgang_id=?;",
                        {BindParam::text(now), BindParam::text(wf->entityId)});
                else if (wf->entityType == "dok" && dokdb)
                    dokdb->exec("UPDATE documents SET release_workflow_id=NULL, updated_at=? WHERE document_id=?;",
                        {BindParam::text(now), BindParam::text(wf->entityId)});
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
        std::cout << "    " << ti << ". Standard-Freigabe (ohne Vorlage)\n\n";
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

    // Store workflow ID back into the entity record
    std::string now = nowIso();
    auto* f16db = DatabasePool::instance().get("f16");
    auto* f22db = DatabasePool::instance().get("f22");
    auto* f18db = DatabasePool::instance().get("f18");
    auto* dokdb = DatabasePool::instance().get("dok");
    if      (effType == "f16" && f16db)
        f16db->exec("UPDATE projects SET release_workflow_id=?, updated_at=? WHERE project_id=?;",
            {BindParam::text(wf->workflowId), BindParam::text(now), BindParam::text(effId)});
    else if (effType == "f22" && f22db)
        f22db->exec("UPDATE tasks SET release_workflow_id=?, updated_at=? WHERE task_id=?;",
            {BindParam::text(wf->workflowId), BindParam::text(now), BindParam::text(effId)});
    else if (effType == "f18" && f18db)
        f18db->exec("UPDATE f18_operations SET release_workflow_id=?, updated_at=? WHERE vorgang_id=?;",
            {BindParam::text(wf->workflowId), BindParam::text(now), BindParam::text(effId)});
    else if (effType == "dok" && dokdb)
        dokdb->exec("UPDATE documents SET release_workflow_id=?, updated_at=? WHERE document_id=?;",
            {BindParam::text(wf->workflowId), BindParam::text(now), BindParam::text(effId)});

    std::cout << "  >> F77-Workflow gestartet: " << wf->workflowId << "\n";
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
                  << "  Status:       " << tpl->status << "\n\n";

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
                     "    3. Vorlage oeffnen/bearbeiten\n\n"
                     "  LAUFENDE WORKFLOWS\n"
                     "    4. Alle aktiven F77-Workflows\n"
                     "    5. Workflow per ID oeffnen\n"
                     "    6. Neuen Workflow starten\n\n"
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

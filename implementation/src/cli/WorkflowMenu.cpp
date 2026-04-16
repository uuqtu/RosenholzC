// ============================================================
// WorkflowMenu.cpp  —  Workflow browser, instance, and template menus
//
// workflowMenu()   : top-level browser (templates + instances)
// instanceMenu()   : per-instance operations (fire, add step,
//                    escalate, participants, docs, tracking)
// templateMenu()   : view/edit workflow templates
// ise-cobra tracking state is set on individual WorkflowActions
// ============================================================
// ============================================================
// WorkflowMenu.cpp  —  Workflow engine CLI using WorkflowEngine API
// ============================================================
#include "cli_common.h"
#include "../workflow/WorkflowEngine.h"
#include "../core/Logger.h"
#include <iomanip>
#include <map>
#include <algorithm>

using namespace Rosenholz;

namespace CLI {

// ── List all instances on an entity ──────────────────────────
// ------------------------------
// listWfInstances
//
// Display a list of workflow instances, each with its chain.
//
// Parameters:
//   entityType : filter by entity type (empty = show all active)
//   entityId   : filter by entity ID (empty = show all active)
//
// Shows:
//   - Instance name, status, execution type
//   - Mini graphical chain: [INIT✓]──►[Step1 ]──►[END ]
//   - Progress: approved/total actions
// ------------------------------

// ──────────────────────────────────────────────────────────────
// drawChain
//
// Renders a visual representation of the workflow action chain.
//
// Parameters:
//   actions   : the list of WorkflowActions to render
//   compact   : true = single-line per action (for list views)
//               false = multi-line with details (for detail view)
//
// Output format (compact=false):
//   [INIT✓] ──► [    >] Schritt 1       ──► [    ] Schritt 2   ──► [END ]
//
// Output format (compact=true, one line):
//   INIT✓──►Step1(>)──►Step2( )──►END
// ──────────────────────────────────────────────────────────────
static void drawChain(const std::vector<WorkflowAction>& actions, bool compact = false) {
    // Sort by sequenceOrder, keeping Init first and End last
    auto sorted = actions;
    std::sort(sorted.begin(), sorted.end(),
        [](const WorkflowAction& a, const WorkflowAction& b) {
            if (a.isInitialize) return true;
            if (b.isInitialize) return false;
            if (a.isFinal) return false;
            if (b.isFinal) return true;
            return a.sequenceOrder < b.sequenceOrder;
        });

    if (compact) {
        // Single-line mini chain
        std::cout << "  ";
        for (int i = 0; i < (int)sorted.size(); ++i) {
            const auto& a = sorted[i];
            std::string sym = (a.status=="approved") ? "✓" :
                              (a.status=="rejected") ? "✗" :
                              (a.status=="in_progress") ? ">" :
                              (a.status=="skipped")  ? "~" : " ";
            std::string label = a.isInitialize ? "INIT" :
                                a.isFinal      ? "END"  :
                                a.title.substr(0, 10);
            std::cout << "[" << label << sym << "]";
            if (i < (int)sorted.size()-1) std::cout << "──►";
        }
        std::cout << "\n";
        return;
    }

    // Full multi-line chain with details
    // Detect parallel groups (actions sharing the same predecessor)
    std::map<std::string, std::vector<int>> predGroups; // pred → indices
    for (int i = 0; i < (int)sorted.size(); ++i) {
        if (!sorted[i].isInitialize && !sorted[i].isFinal)
            predGroups[sorted[i].predecessorActionIds].push_back(i);
    }

    std::cout << "\n";
    bool inParallelGroup = false;
    std::string lastPred;

    for (int i = 0; i < (int)sorted.size(); ++i) {
        const auto& a = sorted[i];

        // Status icon
        std::string sym = (a.status=="approved")    ? "✓" :
                          (a.status=="rejected")    ? "✗" :
                          (a.status=="in_progress") ? ">" :
                          (a.status=="skipped")     ? "~" :
                          (a.status=="cancelled")   ? "x" : " ";

        // Box type
        std::string boxL = a.isInitialize ? "[INIT" :
                           a.isFinal      ? "[ END" :
                                            "[    ";

        // ise-cobra tracking badge
        std::string track;
        if (a.percentComplete > 0 && !a.isInitialize && !a.isFinal)
            track = " " + std::to_string(a.percentComplete) + "%";
        if (a.trackingStatus == "focused")       track += "●";
        else if (a.trackingStatus == "due")      track += "!";
        else if (a.trackingStatus == "archived") track += "■";

        // Detect parallel: same predecessor as previous non-init action
        bool isParallel = !a.isInitialize && !a.isFinal &&
                          !a.predecessorActionIds.empty() &&
                          a.predecessorActionIds == lastPred &&
                          i > 0 && !sorted[i-1].isInitialize;

        // Connector
        std::string conn;
        if (i == 0)           conn = "   ";
        else if (isParallel)  conn = "─┬►";
        else                  conn = "──►";

        // Draw
        std::cout << "  " << conn << boxL << sym << "] "
                  << std::left << std::setw(18) << a.title.substr(0,17)
                  << track;

        // Extra info
        if (!a.assignedTo.empty()) std::cout << "  @" << a.assignedTo.substr(0,12);
        if (a.slaBreached)         std::cout << " [SLA!]";
        if (a.percentComplete > 0 && a.isInitialize) {} // skip
        std::cout << "\n";

        if (!a.isInitialize && !a.isFinal)
            lastPred = a.predecessorActionIds;
    }
    std::cout << "\n";
}

void listWfInstances(const std::string& entityType, const std::string& entityId) {
    std::vector<std::shared_ptr<WorkflowInstance>> instances;
    if (!entityType.empty() && !entityId.empty())
        instances = WorkflowInstance::loadForEntity(entityType, entityId);
    else
        instances = WorkflowInstance::loadActive();

    if (instances.empty()) { std::cout << "  (keine Instanzen)\n"; return; }

    int n = 1;
    for (auto& inst : instances) {
        // Count progress
        int done = 0, total = (int)inst->actions.size();
        for (auto& a : inst->actions) if (a.isComplete()) done++;

        // Status color indicator
        std::string statusMark =
            inst->status == "completed" ? "[✓]" :
            inst->status == "active"    ? "[→]" :
            inst->status == "cancelled" ? "[x]" : "[ ]";

        // Header row
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << statusMark << " "
                  << std::left << std::setw(28) << inst->name.substr(0,26)
                  << "  " << inst->executionType
                  << "  " << done << "/" << total << " Schritte";
        if (inst->slaBreached) std::cout << "  [SLA!]";
        if (!inst->escalatedTo.empty()) std::cout << "  ↑" << inst->escalatedTo.substr(0,10);
        std::cout << "\n";

        // Mini chain (compact = true for list view)
        if (!inst->actions.empty())
            drawChain(inst->actions, /*compact=*/true);

        // Entity context
        if (!inst->entityType.empty())
            std::cout << "       Entität: " << inst->entityType
                      << " / " << inst->entityId.substr(0,24) << "\n";
        std::cout << "\n";
    }
}

// ── Print one action/step ─────────────────────────────────────


static void printAction(const WorkflowAction& a, int idx) {
    std::string statusIcon =
        a.status == "approved"   ? "[✓]" :
        a.status == "rejected"   ? "[✗]" :
        a.status == "in_progress"? "[→]" :
        a.status == "skipped"    ? "[~]" : "[ ]";
    std::cout << "  " << std::setw(3) << idx << " "
              << statusIcon << " "
              << std::left << std::setw(28) << a.title.substr(0,26)
              << std::setw(14) << a.executionType
              << std::setw(10) << a.status;
    if (!a.assignedTo.empty()) std::cout << "  →" << a.assignedTo.substr(0,16);
    if (!a.dueDate.empty())    std::cout << "  fällig:" << a.dueDate.substr(0,10);
    if (a.slaBreached)         std::cout << "  [SLA!]";
    std::cout << "\n";
}

// ── Instance detail menu ──────────────────────────────────────
void instanceMenu(const std::string& instanceId) {
    while (true) {
        auto inst = WorkflowInstance::loadById(instanceId);
        if (!inst) { std::cout << "  Instanz nicht gefunden: " << instanceId << "\n"; return; }

        hdr("WORKFLOW-INSTANZ  " + inst->name);
        auto row = [](const std::string& k, const std::string& v) {
            std::cout << "  | " << std::left << std::setw(18) << k
                      << std::setw(34) << v << "|\n";
        };
        row("ID",         inst->instanceId);
        row("Entität",    inst->entityType + " / " + inst->entityId);
        row("Status",     inst->status);
        row("Typ",        inst->executionType);
        row("Gestartet",  inst->initiatedDate.substr(0,16));
        row("Abgeschl.",  fdate(inst->completedDate));
        row("Fällig",     fdate(inst->dueDate));
        row("SLA",        inst->slaBreached ? "VERLETZT!" : "OK");
        row("Eskaliert",  fval(inst->escalatedTo));
        std::cout << "  +" << std::string(52,'-') << "+\n";

        // ── Graphische Workflow-Kette ─────────────────────────
        std::cout << "  Workflow-Kette:";
        drawChain(inst->actions, /*compact=*/false);

        // Document count
        auto docs = Rosenholz::WorkflowEngine::loadDocumentsForInstance(inst->instanceId);
        if (!docs.empty())
            std::cout << "  Dokumente: " << docs.size() << " angehängt\n";

        std::cout << "  1.Schritt ausführen  2.Schritt hinzufügen  3.Eskalieren\n"
                     "  4.Teilnehmer         5.Tick                6.Dok. anhängen\n"
                     "  7.Dokumente          8.Tracking            0.Zurück\n";
        int ch = readInt("Wahl", 0, 8);
        if (ch == 0) break;

        else if (ch == 1) {
            if (inst->actions.empty()) { std::cout << "  Keine Schritte.\n"; continue; }
            int an = readInt("Schritt #", 1, (int)inst->actions.size());
            auto& act = inst->actions[an-1];

            // If this is the End step of a document Main WFI, ask for target state
            if (act.isFinal && inst->entityType == "document") {
                std::cout << "  ── END-SCHRITT: Dokument-Zielzustand wählen ──\n"
                          << "  1.in_work    2.pre_released  3.released\n"
                          << "  4.locked     5.closed\n";
                static const char* sts[] = {
                    "in_work","pre_released","released","locked","closed"};
                int si = readInt("Zielzustand", 1, 5);
                act.targetState = sts[si-1];
                act.save();
                std::cout << "  >> Zielzustand gesetzt: " << act.targetState << "\n";
            }

            std::cout << "  Entscheidung:  1.Genehmigen  2.Ablehnen  3.Überspringen\n";
            int dec = readInt("Entscheidung", 1, 3);
            static const char* decisions[] = {"approved","rejected","skipped"};
            std::string comment;
            if (act.requiresComment || dec == 2) {
                std::cout << "  Kommentar: ";
                std::getline(std::cin, comment);
            }
            std::string actor = readOpt("Bearbeiter (Person-ID, leer=System): ");
            if (actor.empty()) actor = "system";
            if (WorkflowEngine::fireAction(*inst, act.actionId, decisions[dec-1], actor, comment))
                std::cout << "  >> Schritt ausgeführt.\n";
                // Optionally update tracking progress
                std::cout << "  Fortschritt aktualisieren? (y/n): ";
                std::string upd; std::getline(std::cin, upd);
                if (!upd.empty() && (upd[0]=='y'||upd[0]=='Y')) {
                    auto& act = inst->actions[an-1];
                    std::cout << "  % abgeschlossen (0-100): ";
                    std::string pct; std::getline(std::cin, pct);
                    if (!pct.empty()) try { act.percentComplete = std::stoi(pct); } catch(...) {}
                    act.progressNote = readOpt("Fortschrittsnotiz: ");
                    std::cout << "  Status: 1.planned 2.focused 3.due 4.archived\n";
                    int ts = readInt("Tracking-Status",1,4);
                    static const char* tstats[] = {"planned","focused","due","archived"};
                    act.trackingStatus = tstats[ts-1];
                    if (ts == 4) act.archivedDate = Rosenholz::nowIso();
                    act.save();
                    std::cout << "  >> Tracking aktualisiert.\n";
                }
            else
                std::cout << "  >> Fehler — Vorbedingungen nicht erfüllt?\n";
        }

        else if (ch == 2) {
            // ── Schritt hinzufügen ───────────────────────────────
            std::string title = readLine("Titel des neuen Schritts: ");
            if (title.empty()) continue;

            std::cout << "  Typ:\n"
                         "    1. Sequentiell  (nach dem letzten Schritt)\n"
                         "    2. Parallel     (neben einem bestehenden Schritt)\n"
                         "    3. Frei         (keine Vorgänger-Pflicht)\n";
            int et = readInt("Typ", 1, 3);
            static const char* etypes[] = {"sequential","parallel","free"};

            // Collect non-Init, non-End actions for display
            std::vector<WorkflowAction*> midSteps;
            WorkflowAction* initAct = nullptr;
            WorkflowAction* endAct  = nullptr;
            for (auto& a : inst->actions) {
                if (a.isInitialize) initAct = &a;
                else if (a.isFinal) endAct  = &a;
                else midSteps.push_back(&a);
            }

            std::string pred, assignee, due;

            if (et == 1) {
                // Sequential: predecessor = last mid step (or Init if none)
                std::string defPred = midSteps.empty()
                    ? (initAct ? initAct->actionId : "")
                    : midSteps.back()->actionId;
                std::cout << "  Vorgänger (vorausgefüllt: "
                          << (midSteps.empty() ? "Init" : midSteps.back()->title.substr(0,20))
                          << "):\n";
                std::string userPred = readOpt("  Schritt-ID überschreiben (leer=Standard): ");
                pred = userPred.empty() ? defPred : userPred;

            } else if (et == 2) {
                // Parallel: predecessor = same as the step it runs alongside
                if (midSteps.empty() && !initAct) {
                    std::cout << "  Kein Referenz-Schritt vorhanden.\n"; continue;
                }
                // Show list of steps that can serve as the "sibling"
                std::cout << "  Welcher Schritt soll parallel ausgeführt werden?\n";
                int si = 1;
                if (initAct) std::cout << "    " << si++ << ". Init (Startpunkt)\n";
                for (auto& s : midSteps)
                    std::cout << "    " << si++ << ". " << s->title.substr(0,25) << "\n";
                int pick = readInt("Schritt #", 1, si-1);
                // Resolve the sibling action
                WorkflowAction* sibling = nullptr;
                if (pick == 1 && initAct) sibling = initAct;
                else sibling = midSteps[pick - (initAct ? 2 : 1)];
                // New step shares same predecessors as the sibling
                pred = sibling->predecessorActionIds;
                std::cout << "  >> Vorgänger: " << pred << "\n";

                // Successor: which existing step should wait for the new one?
                std::cout << "  Welcher Nachfolger soll den neuen Schritt abwarten?\n"
                             "  (leer = nur End-Schritt wartet)\n";
                si = 1;
                for (auto& s : midSteps)
                    std::cout << "    " << si++ << ". " << s->title.substr(0,25) << "\n";
                if (endAct) std::cout << "    " << si++ << ". End\n";
                std::string succInput = readOpt("Nachfolger-Nummer (leer=End): ");
                if (!succInput.empty()) {
                    int succPick = 0;
                    try { succPick = std::stoi(succInput); } catch(...) {}
                    WorkflowAction* succAct = nullptr;
                    if (succPick >= 1 && succPick <= (int)midSteps.size())
                        succAct = midSteps[succPick-1];
                    else if (endAct && succPick == (int)midSteps.size()+1)
                        succAct = endAct;
                    if (succAct) {
                        // We'll update successor's predecessors after adding the new step
                        // Store pending update
                        std::cout << "  >> Nachfolger wird nach dem Anlegen aktualisiert.\n";
                    }
                }

            } else {
                // Free: no predecessor constraint
                pred = "";
            }

            assignee = readOpt("Zugewiesen an (Person-ID, leer=offen): ");
            due      = readOpt("Fällig (JJJJ-MM-TT, leer=unbefristet): ");
            int sla  = 0;
            std::string slaS = readOpt("SLA Stunden (leer=0): ");
            if (!slaS.empty()) try { sla = std::stoi(slaS); } catch(...) {}

            auto newAct = WorkflowEngine::addAction(
                *inst, title, etypes[et-1], 0, pred, assignee, due, sla);

            // For parallel: update the nominated successor to also wait for newAct
            // (we re-read inst->actions to get the freshly added action)
            if (et == 2) {
                // Re-ask successor now that we have the new actionId
                std::cout << "  Neuer Schritt: " << newAct->actionId << "\n";
                std::cout << "  Nachfolger aktualisieren? Schritt-Nummer eingeben "
                             "(leer=überspringen): ";
                std::string updInput; std::getline(std::cin, updInput);
                if (!updInput.empty()) {
                    // Find the successor in updated actions list
                    std::vector<WorkflowAction*> midNow;
                    WorkflowAction* endNow = nullptr;
                    for (auto& a : inst->actions)
                        if (a.isFinal) endNow = &a;
                        else if (!a.isInitialize) midNow.push_back(&a);
                    int succIdx = 0;
                    try { succIdx = std::stoi(updInput)-1; } catch(...) {}
                    WorkflowAction* succAct = nullptr;
                    if (succIdx >= 0 && succIdx < (int)midNow.size())
                        succAct = midNow[succIdx];
                    else if (endNow) succAct = endNow;
                    if (succAct) {
                        if (!succAct->predecessorActionIds.empty())
                            succAct->predecessorActionIds += "," + newAct->actionId;
                        else
                            succAct->predecessorActionIds = newAct->actionId;
                        succAct->save();
                        for (auto& a : inst->actions)
                            if (a.actionId == succAct->actionId)
                                a.predecessorActionIds = succAct->predecessorActionIds;
                        std::cout << "  >> Nachfolger " << succAct->title
                                  << " wartet jetzt auch auf " << newAct->title << "\n";
                    }
                }
            }

            WorkflowEngine::tick(*inst);
            std::cout << "  >> Schritt angelegt: " << newAct->actionId << "\n"
                      << "     Typ: " << etypes[et-1]
                      << "  Vorgänger: " << (pred.empty() ? "(kein)" : pred.substr(0,20)) << "\n";
        }

        else if (ch == 3) {
            std::string target = readLine("Eskalieren an Person-ID: ");
            std::string reason = readOpt("Grund (optional): ");
            WorkflowEngine::escalate(*inst, target, reason);
            std::cout << "  >> Eskaliert.\n";
        }

        else if (ch == 4) {
            std::string pid  = readLine("Person-ID: ");
            std::string role = readOpt("Rolle (approver/reviewer/watcher/informed): ");
            if (role.empty()) role = "watcher";
            WorkflowEngine::addParticipant(*inst, pid, role);
            std::cout << "  >> Teilnehmer hinzugefügt.\n";
        }

        else if (ch == 5) {
            WorkflowEngine::tick(*inst);
            std::cout << "  >> Engine-Tick ausgeführt.\n";
        }

        else if (ch == 6) {
            // Attach document to instance or specific action
            // Derive project context from entity if possible
            std::string ctx_proj, ctx_task;
            if (inst->entityType == "project") ctx_proj = inst->entityId;
            else if (inst->entityType == "task") ctx_task = inst->entityId;

            auto doc = attachDocumentDialog(ctx_proj, ctx_task);
            if (!doc) { std::cout << "  >> Abgebrochen.\n"; continue; }
            std::string docId = doc->documentId;

            std::cout << "  Anhängen an:  1.Instanz gesamt  2.Bestimmten Schritt\n";
            int target = readInt("Ziel", 1, 2);
            std::string rel  = readOpt("Beziehung (attached/mandatory/reference, leer=attached): ");
            if (rel.empty()) rel = "attached";
            std::string note = readOpt("Notiz (optional): ");

            if (target == 1) {
                bool ok = WorkflowEngine::attachDocumentToInstance(
                    inst->instanceId, docId, rel, note);
                // Also attach to entity for cross-referencing
                doc->attachToEntity(inst->entityType, inst->entityId);
                std::cout << (ok ? "  >> Dokument an Instanz angehängt.\n"
                                 : "  >> Fehler beim Anhängen.\n");
            } else {
                if (inst->actions.empty()) {
                    std::cout << "  Keine Schritte vorhanden.\n"; continue;
                }
                std::cout << "  Schritte:\n";
                int ai = 1;
                for (auto& a : inst->actions)
                    std::cout << "    " << ai++ << ". " << a.title
                              << " [" << a.status << "]\n";
                int pick = readInt("Schritt #", 1, (int)inst->actions.size());
                bool ok = WorkflowEngine::attachDocumentToAction(
                    inst->actions[pick-1].actionId, docId, rel);
                std::cout << (ok ? "  >> Dokument an Schritt angehängt.\n"
                                 : "  >> Fehler beim Anhängen.\n");
            }
        }

        else if (ch == 7) {
            auto allDocs = WorkflowEngine::loadDocumentsForInstance(inst->instanceId);
            hdr("ANGEHÄNGTE DOKUMENTE (" + std::to_string(allDocs.size()) + ")");
            if (allDocs.empty()) {
                std::cout << "  (keine Dokumente angehängt)\n\n";
            } else {
                std::cout << "  " << std::left
                          << std::setw(26) << "ID"
                          << std::setw(28) << "Titel"
                          << std::setw(12) << "Typ"
                          << "Version\n";
                std::cout << "  " << std::string(72, '-') << "\n";
                for (auto& d : allDocs) {
                    std::cout << "  " << std::left
                              << std::setw(26) << d->documentId.substr(0,24)
                              << std::setw(28) << d->title.substr(0,26)
                              << std::setw(12) << d->docType.substr(0,10)
                              << d->version << "\n";
                }
                std::cout << "\n";
            }
        }

        else if (ch == 8) {
            // ise-cobra tracking state on a specific action
            if (inst->actions.empty()) { std::cout << "  Keine Schritte.\n"; continue; }
            std::cout << "  Schritt wählen:\n";
            for (int ai = 0; ai < (int)inst->actions.size(); ++ai) {
                auto& a = inst->actions[ai];
                std::cout << "    " << (ai+1) << ". " << a.title
                          << " [" << a.trackingStatus << " " << a.percentComplete << "%]\n";
            }
            int an = readInt("Schritt #", 1, (int)inst->actions.size());
            auto& act = inst->actions[an-1];

            std::cout << "  1.Tracking-Status  2.Fortschritt %  3.Fortschrittsnotiz\n"
                         "  4.Priorität        5.Geplantes Datum 6.Fokus-Datum\n";
            int tc = readInt("Was ändern", 1, 6);

            if (tc == 1) {
                std::cout << "  1.planned 2.focused 3.due 4.archived\n";
                int s = readInt("Status",1,4);
                static const char* ss[] = {"planned","focused","due","archived"};
                act.trackingStatus = ss[s-1];
                if (s == 4) act.archivedDate = Rosenholz::nowIso();
                if (s == 2) act.focusDate = Rosenholz::nowIso();
            } else if (tc == 2) {
                std::string pct = readOpt("% abgeschlossen (0-100): ");
                if (!pct.empty()) try { act.percentComplete = std::stoi(pct); } catch(...) {}
            } else if (tc == 3) {
                act.progressNote = readOpt("Fortschrittsnotiz: ");
            } else if (tc == 4) {
                std::cout << "  1.low  2.medium  3.high  4.critical\n";
                int p = readInt("Priorität",1,4);
                static const char* ps[] = {"low","medium","high","critical"};
                act.priority = ps[p-1];
            } else if (tc == 5) {
                act.plannedDate = readOpt("Geplantes Datum (JJJJ-MM-TT): ");
            } else if (tc == 6) {
                act.focusDate = readOpt("Fokus-Datum (JJJJ-MM-TT, leer=heute): ");
                if (act.focusDate.empty()) act.focusDate = Rosenholz::nowIso().substr(0,10);
            }

            act.save();
            std::cout << "  >> Tracking aktualisiert: " << act.trackingStatus
                      << " " << act.percentComplete << "%\n";
        }
    }
}

// ── Start a new workflow instance ─────────────────────────────
std::string startWfInstanceWizard(const std::string& entityType,
                                   const std::string& entityId)
{
    hdr("NEUEN WORKFLOW STARTEN");
    std::string effType = entityType.empty() ? readLine("Entitätstyp (project/task/document/incident): ") : entityType;
    std::string effId;
    if (entityId.empty()) {
        // Show recent items to help the user pick an ID
        showRecentItems(effType == "project" ? "F16" :
                        effType == "task"    ? "F22" :
                        effType == "incident"? "F18" :
                        effType == "risk"    ? "RSK" : "");
        effId = readLine("Entitäts-ID: ");
    } else {
        effId = entityId;
    }

    // Offer templates
    auto templates = WorkflowTemplate::loadForEntityType(effType);
    std::shared_ptr<WorkflowInstance> inst;

    if (!templates.empty()) {
        std::cout << "\n  Verfügbare Vorlagen:\n";
        int ti = 1;
        for (auto& t : templates)
            std::cout << "    " << ti++ << ". " << t->name
                      << " [" << t->executionType << "]\n";
        std::cout << "    " << ti << ". Ad-hoc (ohne Vorlage)\n\n";
        int choice = readInt("Vorlage wählen", 1, (int)templates.size()+1);

        std::string name    = readLine("Instanz-Bezeichnung: ");
        std::string actor   = readOpt("Gestartet von (Person-ID, leer=System): ");
        if (actor.empty()) actor = "system";

        if (choice <= (int)templates.size()) {
            inst = WorkflowEngine::startFromTemplate(
                templates[choice-1]->templateId, effType, effId, name, actor);
        } else {
            std::cout << "  Ausführungstyp:  1.sequentiell  2.parallel  3.frei\n";
            int et = readInt("Typ", 1, 3);
            static const char* etypes[] = {"sequential","parallel","free"};
            inst = WorkflowEngine::startAdHoc(effType, effId, name, etypes[et-1], actor);
        }
    } else {
        std::string name  = readLine("Instanz-Bezeichnung: ");
        std::string actor = readOpt("Gestartet von (Person-ID): ");
        std::cout << "  Ausführungstyp:  1.sequentiell  2.parallel  3.frei\n";
        int et = readInt("Typ", 1, 3);
        static const char* etypes[] = {"sequential","parallel","free"};
        inst = WorkflowEngine::startAdHoc(effType, effId, name, etypes[et-1], actor);
    }

    if (!inst) { std::cout << "  >> FEHLER beim Starten.\n"; return ""; }
    std::cout << "  >> Workflow gestartet: " << inst->instanceId << "\n";
    return inst->instanceId;
}

// ── Template management ───────────────────────────────────────
static void templateMenu(const std::string& templateId) {
    auto tpl = WorkflowTemplate::loadById(templateId);
    if (!tpl) return;
    while (true) {
        hdr("WORKFLOW-VORLAGE  " + tpl->name);
        std::cout << "  ID:        " << tpl->templateId << "\n"
                  << "  Version:   " << tpl->version    << "\n"
                  << "  Typ:       " << tpl->executionType << "\n"
                  << "  Entitäten: " << fval(tpl->entityTypes) << "\n\n";
        // Draw template chain (convert TemplateActions to a chain visual)
        std::cout << "  Vorlage-Kette:\n";
        if (tpl->templateActions.empty()) {
            std::cout << "    (keine Schritte)\n";
        } else {
            // Sort by sequenceOrder for display
            auto sorted = tpl->templateActions;
            std::sort(sorted.begin(), sorted.end(),
                [](const WorkflowTemplateAction& a, const WorkflowTemplateAction& b){
                    if (a.isInitialize) return true;
                    if (b.isInitialize) return false;
                    if (a.isFinal) return false;
                    if (b.isFinal) return true;
                    return a.sequenceOrder < b.sequenceOrder;
                });
            for (int ai = 0; ai < (int)sorted.size(); ++ai) {
                const auto& a = sorted[ai];
                std::string box = a.isInitialize ? "[INIT" :
                                  a.isFinal      ? "[ END" : "[    ";
                std::string flags;
                if (a.autoApprove)               flags += " AUTO";
                if (a.requiresDecisionLogEntry)   flags += " DL";
                if (a.requiresLessonLearnedEntry) flags += " LL";
                std::cout << "  ";
                if (ai > 0) std::cout << "──►";
                else        std::cout << "   ";
                std::cout << box << " ] "
                          << std::left << std::setw(20) << a.title.substr(0,19)
                          << " ord=" << a.sequenceOrder
                          << flags << "\n";
            }
        }
        std::cout << "\n  1.Schritt hinzufügen  2.Vorlage löschen  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;
        else if (ch==1) {
            WorkflowTemplateAction a;
            a.tplActionId = genId("WFT");
            a.templateId  = tpl->templateId;
            a.title = readLine("Titel: ");
            a.description = readOpt("Beschreibung: ");
            std::cout << "  Typ:  1.sequentiell  2.parallel  3.frei\n";
            int et = readInt("Typ",1,3);
            static const char* etypes[] = {"sequential","parallel","free"};
            a.executionType = etypes[et-1];
            a.sequenceOrder = (int)tpl->templateActions.size();
            std::string slaS = readOpt("SLA Stunden (0=ohne): ");
            if (!slaS.empty()) try{a.slaHours=std::stoi(slaS);}catch(...){}
            std::string autoS = readOpt("Automatisch genehmigen? (j/n): ");
            a.autoApprove = (!autoS.empty() && (autoS[0]=='j'||autoS[0]=='J'));
            std::string finS  = readOpt("Abschlussschritt? (j/n): ");
            a.isFinal = (!finS.empty() && (finS[0]=='j'||finS[0]=='J'));
            std::string dlS   = readOpt("Entscheidungslog-Eintrag erforderlich? (j/n): ");
            a.requiresDecisionLogEntry = (!dlS.empty()&&(dlS[0]=='j'||dlS[0]=='J'));
            std::string llS   = readOpt("Lernerkenntnis-Eintrag erforderlich? (j/n): ");
            a.requiresLessonLearnedEntry = (!llS.empty()&&(llS[0]=='j'||llS[0]=='J'));
            a.requiredRole = readOpt("Erforderliche Rolle (leer=beliebig): ");
            a.save();
            tpl->templateActions.push_back(a);
            std::cout << "  >> Schritt hinzugefügt: " << a.tplActionId << "\n";
        }
        else if (ch==2) {
            std::string confirm = readOpt("Vorlage wirklich löschen? (ja/nein): ");
            if (confirm=="ja") { tpl->remove(); std::cout<<"  >> Gelöscht.\n"; return; }
        }
    }
}

// ── Main workflow browser ─────────────────────────────────────
void workflowMenu() {
    while (true) {
        hdr("WORKFLOW-VERWALTUNG");
        std::cout << "  VORLAGEN\n"
                     "    1. Alle Vorlagen anzeigen\n"
                     "    2. Neue Vorlage erstellen\n"
                     "    3. Vorlage öffnen\n\n"
                     "  INSTANZEN\n"
                     "    4. Alle aktiven Instanzen\n"
                     "    5. Instanz per ID öffnen\n"
                     "    6. Instanz starten (ad-hoc)\n"
                     "    7. Verfallene SLAs anzeigen\n"
                     "    8. Workflow suchen / filtern\n\n"
                     "    0. Zurück\n";
        int ch = readInt("Wahl", 0, 8); if (ch==0) break;

        else if (ch==1) {
            auto templates = WorkflowTemplate::loadAll();
            hdr("WORKFLOW-VORLAGEN (" + std::to_string(templates.size()) + ")");
            if (templates.empty()) std::cout << "  (keine)\n";
            int n=1;
            for (auto& t : templates)
                std::cout << "  " << std::setw(3) << n++ << ". "
                          << std::left << std::setw(28) << t->name
                          << "  [" << t->executionType << "]  v" << t->version << "\n";
            std::cout << "\n";
        }

        else if (ch==2) {
            std::string name = readLine("Vorlagenname: ");
            std::cout << "  Typ:  1.sequentiell  2.parallel  3.frei\n";
            int et = readInt("Typ",1,3);
            static const char* etypes[] = {"sequential","parallel","free"};
            auto tpl = WorkflowTemplate::create(name, etypes[et-1]);
            tpl->description = readOpt("Beschreibung: ");
            tpl->entityTypes = readOpt("Entitätstypen (z.B. project,task, leer=alle): ");
            tpl->save();
            std::cout << "  >> Vorlage angelegt: " << tpl->templateId << "\n";
            templateMenu(tpl->templateId);
        }

        else if (ch==3) {
            auto templates = WorkflowTemplate::loadAll();
            if (templates.empty()) { std::cout << "  (keine Vorlagen)\n"; continue; }
            hdr("VORLAGEN");
            int n=1;
            for (auto& t : templates)
                std::cout << "  " << n++ << ". " << t->name << "\n";
            int pick = readInt("Nummer", 1, (int)templates.size());
            templateMenu(templates[pick-1]->templateId);
        }

        else if (ch==4) {
            hdr("AKTIVE INSTANZEN");
            listWfInstances("", "");
        }

        else if (ch==5) {
            std::string id = readLine("Instanz-ID: ");
            instanceMenu(id);
        }

        else if (ch==6) {
            std::string iid = startWfInstanceWizard("","");
            if (!iid.empty()) instanceMenu(iid);
        }

        else if (ch==7) {
            auto breached = WorkflowInstance::loadBreached();
            hdr("VERFALLENE SLAs (" + std::to_string(breached.size()) + ")");
            if (breached.empty()) { std::cout << "  (keine)\n\n"; continue; }
            for (auto& i : breached) {
                std::cout << "  [SLA!] " << std::left << std::setw(26)
                          << i->name.substr(0,24)
                          << "  " << i->entityType << "/" << i->entityId.substr(0,18)
                          << "  Fällig: " << fdate(i->dueDate) << "\n";
                drawChain(i->actions, /*compact=*/true);
            }
        }

        else if (ch==8) {
            // ── Workflow suchen / filtern ────────────────────
            hdr("WORKFLOW SUCHEN");
            std::string fType = readOpt("Entitätstyp (project/task/risk/…, leer=alle): ");
            std::string fStat = readOpt("Status (active/completed/cancelled, leer=alle): ");
            std::string fName = readOpt("Name enthält (leer=alle): ");
            std::cout << "  Nur SLA-Verletzungen? (j/n): ";
            std::string fSla; std::getline(std::cin, fSla);
            bool slaOnly = (!fSla.empty() && (fSla[0]=='j'||fSla[0]=='y'));

            auto results = WorkflowEngine::searchInstances(fType, fStat, fName, slaOnly);
            hdr("SUCHERGEBNISSE (" + std::to_string(results.size()) + ")");
            if (results.empty()) {
                std::cout << "  (keine Treffer)\n\n";
                continue;
            }
            int n=1;
            for (auto& inst : results) {
                std::string sm = inst->status=="completed"?"[✓]":
                                 inst->status=="active"   ?"[→]":"[ ]";
                std::cout << "  " << std::setw(3) << n++ << ". " << sm << " "
                          << std::left << std::setw(26) << inst->name.substr(0,24)
                          << "  " << inst->entityType
                          << (inst->slaBreached?"  [SLA!]":"") << "\n";
                drawChain(inst->actions, /*compact=*/true);
            }
            std::string pick = readOpt("Nummer öffnen (leer=zurück): ");
            if (!pick.empty()) {
                int idx=0;
                try { idx=std::stoi(pick)-1; } catch(...) {}
                if (idx>=0 && idx<(int)results.size())
                    instanceMenu(results[idx]->instanceId);
            }
        }
    }
}

} // namespace CLI

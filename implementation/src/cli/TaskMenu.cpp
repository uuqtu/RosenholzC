// ============================================================
// TaskMenu.cpp  —  Task (F22) detail menu
//
// Opts 1–6: metadata edits
// Opt  7: progress note on active WorkflowInstance (ise-cobra)
// Opt  8: create child task
// Opts 10–12: documents, workflow, meetings
// ============================================================
#include "cli_common.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"

namespace CLI {
// ─────────────────────────────────────────────────────────────
// CREATE HELPERS
// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
// PROJECT DETAIL MENU
// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
// TASK DETAIL MENU
// ─────────────────────────────────────────────────────────────
void taskMenu(std::shared_ptr<Rosenholz::TaskF22> t) {
    while (true) {
        printTask(*t);
        std::cout << "  Aufgaben-Aktionen:\n"
                  << "    1. Titel / Beschreibung bearbeiten\n"
                  << "    2. Status / Priorität / % bearbeiten\n"
                  << "    3. Termine bearbeiten\n"
                  << "    4. Aufwand / Kosten bearbeiten\n"
                  << "    5. Person zuweisen\n"
                  << "    6. Notiz hinzufügen\n"
                  << "    7. Teilaufgabe anlegen\n"
                  << "    8. Als Projekt konvertieren\n"
                  << "\n  DOKUMENTE & WORKFLOW\n"
                  << "    9. Dokumente (anlegen / anzeigen / öffnen)\n"
                  << "   10. Workflow (starten / anzeigen)\n"
                  << "\n  F18 VORGÄNGE\n"
                  << "   11. Neuen F18 Vorgang anlegen\n"
                  << "   12. F18 Vorgänge anzeigen\n"
                  << "   13. Incidents\n"
                  << "   14. Risiken\n"
                  << "   15. Assumptions & Constraints (max. 1)\n"
                  << "   16. Lessons Learned (max. 1)\n"
                  << "   17. Decision Log (max. 1)\n"
                  << "   18. Change Requests\n"
                  << "\n  KOMMUNIKATION\n"
                  << "   19. Communications (Meetings, Calls, ...)\n"
                  << "    0. Zurück\n";
        int ch = readInt("Choice", 0, 19);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string ti = readOpt("New title (Enter to keep): ");
            if (!ti.empty()) t->title = ti;
            std::string de = readOpt("New description (Enter to keep): ");
            if (!de.empty()) t->description = de;
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 2) {
            std::cout << "  Status: draft/active/in-review/done/blocked\n";
            std::string s = readOpt("New status (Enter to keep): ");
            if (!s.empty()) t->status = s;
            std::string pr = readOpt("Priority (high/medium/low, Enter to keep): ");
            if (!pr.empty()) t->priority = pr;
            std::string pc = readOpt("% complete 0-100 (Enter to keep): ");
            if (!pc.empty()) try { t->percentComplete = std::stoi(pc); } catch(...) {}
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 3) {
            std::string sp = readOpt("Planned start YYYY-MM-DD: ");
            if (!sp.empty()) t->startDatePlanned = sp;
            std::string dp = readOpt("Planned due   YYYY-MM-DD: ");
            if (!dp.empty()) t->dueDatePlanned = dp;
            std::string da = readOpt("Actual due    YYYY-MM-DD: ");
            if (!da.empty()) t->dueDateActual = da;
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 4) {
            std::string ep = readOpt("Planned effort h: ");
            if (!ep.empty()) try { t->effortPlannedHrs = std::stod(ep); } catch(...) {}
            std::string ea = readOpt("Actual effort h: ");
            if (!ea.empty()) try { t->effortActualHrs = std::stod(ea); } catch(...) {}
            std::string er = readOpt("Remaining effort h: ");
            if (!er.empty()) try { t->effortRemainingHrs = std::stod(er); } catch(...) {}
            std::string cp = readOpt("Planned cost EUR: ");
            if (!cp.empty()) try { t->costPlanned = std::stod(cp); } catch(...) {}
            std::string ca = readOpt("Actual cost EUR: ");
            if (!ca.empty()) try { t->costActual = std::stod(ca); } catch(...) {}
            t->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 5) {
            std::string pid = readLine("New assignee person-ID: ");
            t->reassignTo(pid);
            std::cout << "  >> Reassigned.\n";
        }
        else if (ch == 6) {
            std::string note = readLine("Note text: ");
            std::string by   = readOpt("Author person-ID (optional): ");
            // Save note directly to notes DB
            auto* db = Rosenholz::DatabasePool::instance().get("tracking");
            if (db) {
                std::string nid = "note_" + std::to_string(
                    std::chrono::system_clock::now().time_since_epoch().count());
                nlohmann::json jc;
                jc["text"] = note; jc["format"] = "plain";
                db->exec(
                    "INSERT INTO notes (note_id,entity_type,entity_id,author_id,content,note_type) "
                    "VALUES (?,?,?,?,?,?);",
                    {Rosenholz::BindParam::text(nid), Rosenholz::BindParam::text("task"),
                     Rosenholz::BindParam::text(t->taskId), Rosenholz::textOrNull(by),
                     Rosenholz::BindParam::text(jc.dump()),
                     Rosenholz::BindParam::text("general")});
                std::cout << "  >> Note saved.\n";
            }
        }
        else if (ch == 7) {
            // Progress note on the task's active workflow instance (replaces VBF)
            // Progress notes now go on F18Workflow steps
            auto insts = Rosenholz::F18Workflow::loadForTask(t->taskId);
            if (insts.empty()) {
                std::cout << "  >> Kein aktiver Workflow auf dieser Aufgabe.\n"
                          << "  Tipp: Workflow starten (Opt. 11), dann Notizen hinzufügen.\n";
            } else {
                std::string note = readLine("Fortschrittsnotiz: ");
                std::string by   = readOpt("Autor Person-ID (leer=system): ");
                std::string type;
                std::cout << "  Typ: 1.general  2.decision  3.action  4.blocker\n";
                int nt = readInt("Typ", 1, 4);
                static const char* ntypes[] = {"general","decision","action","blocker"};
                type = ntypes[nt-1];
                if (!insts.empty() && insts[0]->addNote(by.empty()?"system":by, note))
                    std::cout << "  >> Notiz gespeichert (WFI " << insts[0]->vorgangId.substr(0,20) << ").\n";
                else
                    std::cout << "  >> Fehler beim Speichern.\n";
            }
        }
        else if (ch ==  7) {
            createTaskWizard(t->projectId);
        }
        else if (ch ==  8) {
            std::cout << "  Type: OV/IM/OPK/GMS/AU/SVG\n";
            std::string ptype = readLine("Project type: ");
            std::string newPid = t->convertToProject(ptype);
            if (!newPid.empty())
                std::cout << "  >> Project created: " << newPid << "\n";
        }
        else if (ch ==  9) {
            documentBrowserMenu("", t->taskId);
        }
        else if (ch == 10) {
            std::cout << "  1. Start instance  2. List instances\n";
            int wch = readInt("Choice", 1, 2);
            if (wch == 1) {
                std::string iid = startWfInstanceWizard("task", t->taskId);
                if (!iid.empty()) instanceMenu(iid);
            } else {
                listWfInstances("task", t->taskId);
            }
        }
        else if (ch == 11) {
            // New F18 Workflow for this task
            auto v = createF18Wizard(t->projectId, t->taskId);
            if (v) f18Menu(v);
        }
        else if (ch == 12) { f18BrowserMenu("", t->taskId); }
        else if (ch == 13) { f18BrowserMenu("", t->taskId, "incident"); }
        else if (ch == 14) { f18BrowserMenu("", t->taskId, "risk"); }
        else if (ch == 15) { f18BrowserMenu("", t->taskId, "assumptionConstraint"); }
        else if (ch == 16) { f18BrowserMenu("", t->taskId, "lessonsLearned"); }
        else if (ch == 17) { f18BrowserMenu("", t->taskId, "decisionLog"); }
        else if (ch == 18) { f18BrowserMenu("", t->taskId, "changeRequest"); }
        else if (ch == 19) { communicationMenu(t->taskId, "task"); }
    }
}

// ─────────────────────────────────────────────────────────────
// MAIN MENU
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// WORKFLOW HELPERS
// ─────────────────────────────────────────────────────────────

// ── low-level DB helpers (workflow.db) ────────────────────────

// ── display ───────────────────────────────────────────────────

} // namespace CLI

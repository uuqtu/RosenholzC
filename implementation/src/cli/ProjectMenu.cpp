// ============================================================
// ProjectMenu.cpp  —  Project (F16) detail and sub-menus
//
// Opened from main menu options 3/4 after loading a ProjectF16
// Opts 1–7: metadata edits    Opts 10–13: F22/F18 sub-entities
// Opts 16–17: documents + workflow
// Opts 18–24: reporting sub-menus
// ============================================================
#include "cli_common.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"

namespace CLI {
void printTask(const Rosenholz::TaskF22& t) {
    hdr("TASK (F22)  " + t.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",            t.taskId);
    row("Reg-Nr",        t.regNumber.toString());
    row("Title",         t.title);
    row("Project-ID",    fval(t.projectId));
    row("Parent-Task",   fval(t.parentTaskId));
    row("Status",        fval(t.status));
    row("Priority",      fval(t.priority));
    row("Assignee-ID",   fval(t.assigneeId));
    row("WBS",           fval(t.wbsCode));
    row("Sprint/Phase",  fval(t.sprintOrPhase));
    hr();
    row("Effort plan(h)",std::to_string((int)t.effortPlannedHrs));
    row("Effort act.(h)",std::to_string((int)t.effortActualHrs));
    row("Effort rem.(h)",std::to_string((int)t.effortRemainingHrs));
    row("% complete",   std::to_string(t.percentComplete) + "%");
    row("Cost plan",    std::to_string((int)t.costPlanned));
    row("Cost actual",  std::to_string((int)t.costActual));
    hr();
    row("Start planned", fdate(t.startDatePlanned));
    row("Due planned",   fdate(t.dueDatePlanned));
    row("Due actual",    fdate(t.dueDateActual));
    row("Sched.var.(d)", std::to_string(t.scheduleVarianceDays));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

// ─────────────────────────────────────────────────────────────
// CREATE HELPERS
// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
// PROJECT DETAIL MENU
// ─────────────────────────────────────────────────────────────
void projectMenu(std::shared_ptr<Rosenholz::ProjectF16> p) {
    while (true) {
        printProject(*p);
        std::cout << "  Project actions:\n"
                  << "    1. Edit title / codename\n"
                  << "    2. Edit status / phase\n"
                  << "    3. Edit dates (planned start / end)\n"
                  << "    4. Edit budget\n"
                  << "    5. Edit scope statement\n"
                  << "    6. Recalculate Earned Value\n"
                  << "    7. Reassign lead / team / sponsor\n"

                  << "   10. Create task (F22)\n"
                  << "   11. List tasks (F22)\n"
                  << "   12. Create incident (F18)\n"
                  << "   13. List incidents (F18)\n"
                  << "   14. Convert project -> task\n"
                  << "   15. Write MFS file\n"
                  << "   16. Documents (create / list / open)\n"
                  << "   17. Workflow (start / view instances)\n"
                  << "\n  REPORTING & TRACKING\n"
                  << "\n  F18 WORKFLOWS\n"
                  << "   18. Neuen F18 Workflow anlegen\n"
                  << "   19. F18 Workflows anzeigen / öffnen\n"
                  << "   20. Incidents anzeigen\n"
                  << "   21. Risiken anzeigen\n"
                  << "   22. Maßnahmen anzeigen\n"
                  << "   23. Change Requests anzeigen\n"
                  << "\n  KOMMUNIKATION\n"
                  << "   24. Communications (Meetings, Calls, ...)\n"
                  << "\n  MEILENSTEINE\n"
                  << "   25. Meilenstein-Notizen bearbeiten\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 25);

        if (ch == 0) break;

        else if (ch == 1) {
            std::string t = readLine("New title (Enter to keep): ");
            if (!t.empty()) p->title = t;
            std::string c = readOpt("New codename (Enter to keep): ");
            if (!c.empty()) p->codename = c;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 2) {
            std::cout << "  Status options: draft / active / on-hold / closed / archived\n";
            std::string s = readOpt("New status (Enter to keep): ");
            if (!s.empty()) p->status = s;
            std::string ph = readOpt("New phase (Enter to keep): ");
            if (!ph.empty()) p->phase = ph;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 3) {
            std::string sp = readOpt("Planned start YYYY-MM-DD (Enter to keep): ");
            if (!sp.empty()) p->startDatePlanned = sp;
            std::string ep = readOpt("Planned end   YYYY-MM-DD (Enter to keep): ");
            if (!ep.empty()) p->endDatePlanned = ep;
            std::string sa = readOpt("Actual start  YYYY-MM-DD (Enter to keep): ");
            if (!sa.empty()) p->startDateActual = sa;
            std::string ea = readOpt("Actual end    YYYY-MM-DD (Enter to keep): ");
            if (!ea.empty()) p->endDateActual = ea;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 4) {
            std::string bp = readOpt("Budget planned EUR (Enter to keep): ");
            if (!bp.empty()) try { p->budgetPlanned = std::stod(bp); } catch(...) {}
            std::string ba = readOpt("Budget actual  EUR (Enter to keep): ");
            if (!ba.empty()) try { p->budgetActual  = std::stod(ba); } catch(...) {}
            std::string ev = readOpt("Earned value       (Enter to keep): ");
            if (!ev.empty()) try { p->earnedValue   = std::stod(ev); } catch(...) {}
            std::string pv = readOpt("Planned value      (Enter to keep): ");
            if (!pv.empty()) try { p->plannedValue  = std::stod(pv); } catch(...) {}
            std::string ac = readOpt("Actual cost        (Enter to keep): ");
            if (!ac.empty()) try { p->actualCost    = std::stod(ac); } catch(...) {}
            p->recalcEarnedValue();
            p->update();
            std::cout << "  >> Saved.  CPI=" << p->cpi << "  SPI=" << p->spi << "\n";
        }
        else if (ch == 5) {
            std::cout << "  Current: " << fval(p->scopeStatement) << "\n";
            std::string sc = readLine("New scope statement: ");
            p->scopeStatement = sc;
            p->scopeChangeCount++;
            std::string reason = readOpt("Change reason (optional): ");
            p->scopeChangeReason = reason;
            p->update();
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 6) {
            p->recalcEarnedValue();
            p->update();
            std::cout << "  >> EV recalculated.  CPI=" << p->cpi
                      << "  SPI=" << p->spi << "  EAC=" << p->eac << "\n";
        }
        else if (ch == 7) {
            std::string lead = readOpt("New lead person-ID (Enter to keep): ");
            if (!lead.empty()) p->reassignLead(lead);
            std::string team = readOpt("New team-ID (Enter to keep): ");
            if (!team.empty()) p->reassignTeam(team);
            std::string spon = readOpt("New sponsor person-ID (Enter to keep): ");
            if (!spon.empty()) p->reassignSponsor(spon);
            std::cout << "  >> Saved.\n";
        }

        else if (ch == 10) {
            createTaskWizard(p->projectId);
        }
        else if (ch == 11) {
            listTasks(p->projectId);
        }
        else if (ch == 12) {
            {
                auto v = createF18Wizard(p->projectId, "", "incident");
                if (v) f18Menu(v);
            }
        }
        else if (ch == 13) {
            f18BrowserMenu(p->projectId, "");
        }
        else if (ch == 14) {
            std::string parentPid = readLine("Parent project-ID to attach task to: ");
            std::string newTaskId = p->convertToTask(parentPid);
            if (!newTaskId.empty())
                std::cout << "  >> Task created: " << newTaskId << "\n";
        }
        else if (ch == 15) {
            auto& cfg = Rosenholz::Config::instance();
            bool ok = p->writeMFSFile(cfg.mfsPath());
            std::cout << "  >> MFS file " << (ok ? "written." : "FAILED.") << "\n";
        }
        else if (ch == 16) {
            documentBrowserMenu(p->projectId);
        }
        else if (ch == 17) {
            std::cout << "  1. Start instance  2. List instances  3. Browser\n";
            int wch = readInt("Choice", 1, 3);
            if (wch == 1) { std::string iid=startWfInstanceWizard("project",p->projectId); if(!iid.empty()) instanceMenu(iid); }
            else if (wch == 2) { listWfInstances("project", p->projectId); }
            else { workflowMenu(); }
        }
        else if (ch == 18) {
            // Create new F18 Workflow
            auto v = createF18Wizard(p->projectId, "");
            if (v) f18Menu(v);
        }
        else if (ch == 19) { f18BrowserMenu(p->projectId, ""); }
        else if (ch == 20) { f18BrowserMenu(p->projectId, "", "incident"); }
        else if (ch == 21) { f18BrowserMenu(p->projectId, "", "risk"); }
        else if (ch == 22) { f18BrowserMenu(p->projectId, "", "measure"); }
        else if (ch == 23) { f18BrowserMenu(p->projectId, "", "changeRequest"); }
        else if (ch == 24) { communicationMenu(p->projectId, "project"); }
        else if (ch == 25) {
            // Meilenstein-Notizen (free-text field on F16)
            hdr("MEILENSTEIN-NOTIZEN — " + p->projectId.substr(0,20));
            if (!p->milestones.empty())
                std::cout << "  Aktuelle Notizen:\n" << p->milestones << "\n\n";
            else
                std::cout << "  (keine Notizen)\n\n";
            std::cout << "  1.Bearbeiten  2.Anfügen  0.Zurück\n";
            int ms = readInt("Wahl",0,2);
            if (ms==1) {
                std::cout << "  Notizen eingeben (leere Zeile = fertig):\n";
                std::string all, line;
                while (std::getline(std::cin, line) && !line.empty())
                    all += line + "\n";
                p->milestones = all;
                p->update();
                std::cout << "  >> Meilenstein-Notizen gespeichert.\n";
            } else if (ms==2) {
                std::string add = readLine("Notiz anfügen: ");
                if (!add.empty()) {
                    p->milestones += (p->milestones.empty() ? "" : "\n") + add;
                    p->update();
                    std::cout << "  >> Angefügt.\n";
                }
            }
        }
        else if (ch == 24) {
            std::cout << "  Workflow:  1.Neue Instanz starten  2.Instanzen anzeigen\n";
            int wch = CLI::readInt("Wahl",1,2);
            if (wch==1) { std::string iid=CLI::startWfInstanceWizard("project",p->projectId); if(!iid.empty()) CLI::instanceMenu(iid); }
            else         { CLI::listWfInstances("project",p->projectId); }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// TASK DETAIL MENU
// ─────────────────────────────────────────────────────────────

} // namespace CLI

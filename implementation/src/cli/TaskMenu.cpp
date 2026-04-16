// ============================================================
// TaskMenu.cpp  —  F22 Aufgaben-Detailmenü
// ============================================================
#include "cli_common.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../workflow/WorkflowEngine.h"
#include <iomanip>

namespace CLI {
using namespace Rosenholz;

static void row(const std::string& k, const std::string& v) {
    std::cout << "  | " << std::left << std::setw(22) << k
              << std::setw(30) << v.substr(0,29) << "|\n";
}

void printTask(const TaskF22& t) {
    hdr("F22 — " + t.regNumber.toString() + "  " + t.title.substr(0,30));
    std::cout << "  +" << std::string(52,'-') << "+\n";
    row("ID",     t.taskId.substr(0,26));
    row("Status", t.status + " / " + t.priority);
    row("Projekt",t.projectId.substr(0,26));
    row("Person", t.assigneeId.empty() ? "—" : t.assigneeId.substr(0,26));
    row("Fortsch",std::to_string(t.percentComplete) + "%");
    row("Start",  t.startDatePlanned.empty() ? "—" : t.startDatePlanned.substr(0,10));
    row("Ende",   t.dueDatePlanned.empty()   ? "—" : t.dueDatePlanned.substr(0,10));
    if (!t.mainWorkflowId.empty())
        row("Main WFI", t.mainWorkflowId.substr(0,26));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

// ── Edit-Untermenü ─────────────────────────────────────────────
static void editMenu(std::shared_ptr<TaskF22> t) {
    while (true) {
        hdr("BEARBEITEN — " + t->regNumber.toString());
        std::cout
            << "    1. Titel / Beschreibung\n"
            << "    2. Status / Priorität / Fortschritt %\n"
            << "    3. Termine (geplant Start / Ende)\n"
            << "    4. Aufwand / Kosten\n"
            << "    5. Person zuweisen\n"
            << "    6. Notiz hinzufügen\n"
            << "    7. Aufgabe → Projekt konvertieren\n"
            << "    0. Zurück\n";
        int ch = readInt("Wahl",0,7); if (ch==0) break;

        if (ch==1) {
            std::string ti = readOpt("Titel (leer=behalten): ");
            if (!ti.empty()) t->title = ti;
            std::string de = readOpt("Beschreibung: ");
            if (!de.empty()) t->description = de;
            t->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==2) {
            std::cout << "  Status: in_work|released|in-review|done|blocked\n";
            std::string s = readOpt("Status (leer=behalten): ");
            if (!s.empty()) t->status = s;
            std::string pr = readOpt("Priorität (low/medium/high): ");
            if (!pr.empty()) t->priority = pr;
            std::string pct = readOpt("Fortschritt % (0-100): ");
            if (!pct.empty()) try { t->percentComplete = std::stoi(pct); } catch(...) {}
            t->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==3) {
            t->startDatePlanned = readOpt("Geplanter Start (JJJJ-MM-TT): ");
            t->dueDatePlanned   = readOpt("Geplantes Ende  (JJJJ-MM-TT): ");
            t->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==4) {
            std::string ep = readOpt("Aufwand geplant (Std): ");
            if (!ep.empty()) try { t->effortPlannedHrs = std::stod(ep); } catch(...) {}
            std::string ea = readOpt("Aufwand ist (Std): ");
            if (!ea.empty()) try { t->effortActualHrs = std::stod(ea); } catch(...) {}
            t->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==5) {
            std::string pid = readLine("Person-ID: ");
            if (!pid.empty()) { t->assigneeId = pid; t->update(); }
            std::cout << "  >> Neu zugewiesen.\n";

        } else if (ch==6) {
            std::string note = readLine("Notiz: ");
            if (!note.empty()) {
                std::string cur = t->notes == "{}" ? "" : t->notes;
                t->notes = cur + "\n" + note; t->update();
            }
            std::cout << "  >> Notiz hinzugefügt.\n";

        } else if (ch==7) {
            std::cout << "  Aufgabe → Projekt konvertieren\n";
            std::string type = readOpt("Projekttyp (OV/IM/...): ");
            auto proj = ProjectF16::create(t->title, type.empty()?"OV":type, "medium");
            proj->scopeStatement = t->description;
            proj->leadId         = t->assigneeId;
            proj->save(); proj->ensureMainWorkflow();
            std::cout << "  >> Projekt angelegt: " << proj->projectId << "\n";
        }
    }
}

// ── Main Workflow Untermenü ────────────────────────────────────
static void mainWorkflowMenu(std::shared_ptr<TaskF22> t) {
    hdr("MAIN WORKFLOW — " + t->regNumber.toString());
    std::cout << "  Status    : " << t->status << "\n";
    if (t->mainWorkflowId.empty()) {
        std::cout << "  (kein Main Workflow — wird angelegt)\n";
        t->ensureMainWorkflow();
        auto rt = TaskF22::loadById(t->taskId);
        if (rt) *t = *rt;
    }
    if (!t->mainWorkflowId.empty()) {
        int blockers = 0;
        bool canRel = WorkflowEngine::canReleaseEntity(
            "task", t->taskId, t->mainWorkflowId, blockers);
        std::cout << "  Main WFI  : " << t->mainWorkflowId.substr(0,36) << "\n";
        if (blockers > 0)
            std::cout << "  ⚠ " << blockers << " offene WFI(s) blockieren die Freigabe\n";
        else
            std::cout << "  ✓ Alle Sub-WFIs abgeschlossen — Freigabe möglich\n";
        std::cout << "\n  1.Main WFI öffnen  2.Sub-WFIs sperren  0.Zurück\n";
        int ch = readInt("Wahl",0,2);
        if (ch==1) instanceMenu(t->mainWorkflowId);
        else if (ch==2 && blockers>0) {
            std::cout << "  'ja' eingeben zum Sperren: ";
            std::string conf; std::getline(std::cin, conf);
            if (conf=="ja") {
                int locked = WorkflowEngine::lockAllOpenWorkflows(
                    "task", t->taskId, t->mainWorkflowId, true);
                std::cout << "  >> " << locked << " WFI(s) gesperrt.\n";
            }
        }
    }
}

// ── Haupt-Aufgabenmenü ─────────────────────────────────────────
void taskMenu(std::shared_ptr<TaskF22> t) {
    while (true) {
        if (auto fresh = TaskF22::loadById(t->taskId)) *t = *fresh;
        printTask(*t);

        bool released = (t->status == "released");
        if (released)
            std::cout << "  ⚠ RELEASED — keine weiteren Änderungen möglich\n\n";

        std::cout
            << "  [AUFGABE]\n"
            << "    1. Bearbeiten (Edit-Untermenü)\n"
            << "    2. Teilaufgabe anlegen\n"
            << "\n  [DOKUMENTE]\n"
            << "    3. Dokumente anzeigen\n"
            << "    4. Neues Dokument anlegen\n"
            << "\n  [F18 VORGÄNGE]\n"
            << "    5. F18 Vorgänge anzeigen / öffnen\n"
            << "    6. Neuen F18 Vorgang anlegen\n"
            << "\n  [KOMMUNIKATION]\n"
            << "    7. Communications (Meetings, Calls, ...)\n"
            << "\n  [MAIN WORKFLOW]\n"
            << "    8. Main Workflow / Freigabe\n"
            << "\n    0. Zurück\n";
        hr();

        int ch = readInt("Wahl",0,8); if (ch==0) break;

        if (ch==1) {
            if (!released) editMenu(t);
            else std::cout << "  >> Released — kein Bearbeiten.\n";

        } else if (ch==2) {
            if (released) { std::cout << "  >> Released — keine neuen Teilaufgaben.\n"; continue; }
            auto child = createTaskWizard(t->projectId);
            if (child) {
                child->parentTaskId = t->taskId;
                child->update();
                std::cout << "  Teilaufgabe öffnen? (j/n): ";
                std::string yn; std::getline(std::cin, yn);
                if (yn=="j"||yn=="J") taskMenu(child);
            }

        } else if (ch==3) {
            documentBrowserMenu("", t->taskId);

        } else if (ch==4) {
            if (released) { std::cout << "  >> Released — keine neuen Dokumente.\n"; continue; }
            auto doc = createDocumentWizard(t->projectId, t->taskId);
            if (doc) documentMenu(doc);

        } else if (ch==5) {
            f18BrowserMenu("", t->taskId);

        } else if (ch==6) {
            if (released) { std::cout << "  >> Released — keine neuen F18-Vorgänge.\n"; continue; }
            auto v = createF18Wizard(t->projectId, t->taskId);
            if (v) f18Menu(v);

        } else if (ch==7) {
            communicationMenu(t->taskId, "task");

        } else if (ch==8) {
            mainWorkflowMenu(t);
        }
    }
}

} // namespace CLI

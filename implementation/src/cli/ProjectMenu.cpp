// ============================================================
// ProjectMenu.cpp  —  F16 Projekt-Detailmenü
//
// Struktur:
//   [BEARBEITEN]  — Untermenü für alle Felder
//   [F22 AUFGABEN] — Erstellen / Öffnen
//   [DOKUMENTE]   — Erstellen / Browser (nur mit Projekt-Referenz)
//   [F18 VORGÄNGE] — Browser / Neu anlegen
//   [KOMMUNIKATION] — Communications
//   [MAIN WORKFLOW] — Status + Freigabe
//   [MEILENSTEINE] — Free-text Notizen
// ============================================================
#include "cli_common.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../workflow/WorkflowEngine.h"
#include <iomanip>

namespace CLI {
using namespace Rosenholz;

// ── printProject (Kurzdarstellung) ────────────────────────────
static void row(const std::string& k, const std::string& v) {
    std::cout << "  | " << std::left << std::setw(24) << k
              << std::setw(28) << v.substr(0,27) << "|\n";
}

void printProject(const ProjectF16& p) {
    hdr("F16 — " + p.regNumber.toString() + "  " + p.title.substr(0,30));
    std::cout << "  +" << std::string(52,'-') << "+\n";
    row("ID",       p.projectId.substr(0,26));
    row("Status",   p.status + " / " + p.phase);
    row("Lead",     p.leadId.empty() ? "—" : p.leadId.substr(0,26));
    row("Budget",   std::to_string((int)p.budgetPlanned) + " " + p.currency);
    row("CPI/SPI",  std::to_string(p.cpi).substr(0,4) + " / " + std::to_string(p.spi).substr(0,4));
    row("Start",    p.startDatePlanned.empty() ? "—" : p.startDatePlanned.substr(0,10));
    row("Ende",     p.endDatePlanned.empty()   ? "—" : p.endDatePlanned.substr(0,10));
    if (!p.mainWorkflowId.empty())
        row("Main WFI", p.mainWorkflowId.substr(0,26));
    if (!p.milestones.empty())
        std::cout << "  Meilsteine:\n" << p.milestones.substr(0,120) << "\n";
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

// ── Edit-Untermenü ─────────────────────────────────────────────
static void editMenu(std::shared_ptr<ProjectF16> p) {
    while (true) {
        hdr("BEARBEITEN — " + p->regNumber.toString());
        std::cout
            << "    1. Titel / Codename\n"
            << "    2. Status / Phase\n"
            << "    3. Geplante Termine (Start / Ende)\n"
            << "    4. Budget / Währung\n"
            << "    5. Scope-Statement\n"
            << "    6. Lead / Team / Sponsor neu zuweisen\n"
            << "    7. Earned Value neu berechnen\n"
            << "    8. MFS-Datei schreiben\n"
            << "    9. Projekt → Aufgabe konvertieren\n"
            << "    0. Zurück\n";
        int ch = readInt("Wahl",0,9); if (ch==0) break;

        if (ch==1) {
            std::string t = readOpt("Titel (leer=behalten): ");
            if (!t.empty()) p->title = t;
            std::string c = readOpt("Codename (leer=behalten): ");
            if (!c.empty()) p->codename = c;
            p->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==2) {
            std::cout << "  Status: in_work | released | on-hold | closed | archived\n";
            std::string s = readOpt("Neuer Status (leer=behalten): ");
            if (!s.empty()) p->status = s;
            std::string ph = readOpt("Phase (leer=behalten): ");
            if (!ph.empty()) p->phase = ph;
            p->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==3) {
            p->startDatePlanned = readOpt("Geplanter Start (JJJJ-MM-TT): ");
            p->endDatePlanned   = readOpt("Geplantes Ende  (JJJJ-MM-TT): ");
            p->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==4) {
            std::string b = readOpt("Budget geplant (leer=behalten): ");
            if (!b.empty()) try { p->budgetPlanned = std::stod(b); } catch(...) {}
            std::string c = readOpt("Währung (EUR/USD/...): ");
            if (!c.empty()) p->currency = c;
            p->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==5) {
            std::cout << "  Aktuell: " << p->scopeStatement.substr(0,60) << "\n";
            std::string s = readOpt("Neues Scope-Statement (leer=behalten): ");
            if (!s.empty()) { p->scopeStatement = s; p->update(); }
            std::cout << "  >> Gespeichert.\n";

        } else if (ch==6) {
            std::string lid = readOpt("Lead Person-ID (leer=behalten): ");
            if (!lid.empty()) p->leadId = lid;
            std::string tid = readOpt("Team-ID (leer=behalten): ");
            if (!tid.empty()) p->ownerTeamId = tid;
            std::string sid = readOpt("Sponsor-ID (leer=behalten): ");
            if (!sid.empty()) p->sponsorId = sid;
            p->update(); std::cout << "  >> Gespeichert.\n";

        } else if (ch==7) {
            p->recalcEarnedValue(); p->update();
            std::cout << "  >> EV neu berechnet. CPI=" << p->cpi << " SPI=" << p->spi << "\n";

        } else if (ch==8) {
            MFSWriter::writeProject(*p, Config::instance().mfsPath());
            std::cout << "  >> MFS-Datei geschrieben.\n";

        } else if (ch==9) {
            std::cout << "  Projekt → Aufgabe: ziel-Projekt-ID eingeben\n";
            std::string pid = readLine("Ziel-Projekt-ID: ");
            if (pid.empty()) continue;
            auto task = TaskF22::create(pid, p->title, p->leadId, "");
            task->description = "Konvertiert aus Projekt " + p->projectId;
            task->save(); task->ensureMainWorkflow();
            std::cout << "  >> Aufgabe angelegt: " << task->taskId << "\n";
        }
    }
}

// ── Main Workflow Untermenü ────────────────────────────────────
static void mainWorkflowMenu(std::shared_ptr<ProjectF16> p) {
    hdr("MAIN WORKFLOW — " + p->regNumber.toString());
    std::cout << "  Status    : " << p->status << "\n";
    if (p->mainWorkflowId.empty()) {
        std::cout << "  (kein Main Workflow — wird angelegt)\n";
        p->ensureMainWorkflow();
        auto rp = ProjectF16::loadById(p->projectId);
        if (rp) *p = *rp;
    }
    if (!p->mainWorkflowId.empty()) {
        int blockers = 0;
        bool canRel = WorkflowEngine::canReleaseEntity(
            "project", p->projectId, p->mainWorkflowId, blockers);
        std::cout << "  Main WFI  : " << p->mainWorkflowId.substr(0,36) << "\n";
        if (blockers > 0)
            std::cout << "  ⚠ " << blockers << " offene WFI(s) blockieren die Freigabe\n";
        else
            std::cout << "  ✓ Alle Sub-WFIs abgeschlossen — Freigabe möglich\n";

        std::cout << "\n  1.Main WFI öffnen  2.Alle Sub-WFIs sperren  0.Zurück\n";
        int ch = readInt("Wahl",0,2); 
        if (ch==1) instanceMenu(p->mainWorkflowId);
        else if (ch==2 && blockers>0) {
            std::cout << "  " << blockers << " WFI(s) sperren? 'ja' eingeben: ";
            std::string conf; std::getline(std::cin, conf);
            if (conf=="ja") {
                int locked = WorkflowEngine::lockAllOpenWorkflows(
                    "project", p->projectId, p->mainWorkflowId, true);
                std::cout << "  >> " << locked << " WFI(s) gesperrt.\n";
            }
        }
    }
}

// ── Haupt-Projektmenü ─────────────────────────────────────────
void projectMenu(std::shared_ptr<ProjectF16> p) {
    while (true) {
        // Reload to show current state
        if (auto fresh = ProjectF16::loadById(p->projectId)) *p = *fresh;
        printProject(*p);

        // Block editing if released
        bool released = (p->status == "released");
        if (released)
            std::cout << "  ⚠ RELEASED — keine weiteren Änderungen möglich\n\n";

        std::cout
            << "  [PROJEKT]\n"
            << "    1. Bearbeiten (Edit-Untermenü)\n"
            << "\n  [F22 AUFGABEN]\n"
            << "    2. Aufgaben anzeigen\n"
            << "    3. Neue Aufgabe anlegen\n"
            << "\n  [DOKUMENTE]\n"
            << "    4. Dokumente anzeigen\n"
            << "    5. Neues Dokument anlegen\n"
            << "\n  [F18 VORGÄNGE]\n"
            << "    6. F18 Vorgänge anzeigen / öffnen\n"
            << "    7. Neuen F18 Vorgang anlegen\n"
            << "\n  [KOMMUNIKATION]\n"
            << "    8. Communications (Meetings, Calls, ...)\n"
            << "\n  [MEILENSTEINE]\n"
            << "    9. Meilenstein-Notizen bearbeiten\n"
            << "\n  [MAIN WORKFLOW]\n"
            << "   10. Main Workflow / Freigabe\n"
            << "\n    0. Zurück\n";
        hr();

        int ch = readInt("Wahl", 0, 10); if (ch==0) break;

        if (ch==1) {
            if (!released) editMenu(p);
            else std::cout << "  >> Released — kein Bearbeiten möglich.\n";

        } else if (ch==2) {
            listTasks(p->projectId);

        } else if (ch==3) {
            if (released) { std::cout << "  >> Released — keine neuen Aufgaben.\n"; continue; }
            auto task = createTaskWizard(p->projectId);
            if (task) {
                std::cout << "  Aufgabe jetzt öffnen? (j/n): ";
                std::string yn; std::getline(std::cin, yn);
                if (yn=="j"||yn=="J") taskMenu(task);
            }

        } else if (ch==4) {
            documentBrowserMenu(p->projectId, "");

        } else if (ch==5) {
            if (released) { std::cout << "  >> Released — keine neuen Dokumente.\n"; continue; }
            auto doc = createDocumentWizard(p->projectId, "");
            if (doc) documentMenu(doc);

        } else if (ch==6) {
            f18BrowserMenu(p->projectId, "");

        } else if (ch==7) {
            if (released) { std::cout << "  >> Released — keine neuen F18-Vorgänge.\n"; continue; }
            auto v = createF18Wizard(p->projectId, "");
            if (v) f18Menu(v);

        } else if (ch==8) {
            communicationMenu(p->projectId, "project");

        } else if (ch==9) {
            hdr("MEILENSTEIN-NOTIZEN — " + p->projectId.substr(0,20));
            if (!p->milestones.empty())
                std::cout << "  Aktuell:\n" << p->milestones << "\n\n";
            else
                std::cout << "  (keine Notizen)\n\n";
            std::cout << "  1.Neu schreiben  2.Anfügen  0.Zurück\n";
            int ms = readInt("Wahl",0,2);
            if (ms==1) {
                std::cout << "  Text (leere Zeile = fertig):\n";
                std::string all, line;
                while (std::getline(std::cin, line) && !line.empty())
                    all += line + "\n";
                p->milestones = all; p->update();
                std::cout << "  >> Gespeichert.\n";
            } else if (ms==2) {
                std::string add = readLine("Anfügen: ");
                if (!add.empty()) {
                    if (!p->milestones.empty()) p->milestones += "\n";
                    p->milestones += add; p->update();
                    std::cout << "  >> Angefügt.\n";
                }
            }

        } else if (ch==10) {
            mainWorkflowMenu(p);
        }
    }
}

} // namespace CLI

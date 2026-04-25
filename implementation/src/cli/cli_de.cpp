// ============================================================
// cli_de.cpp  —  Diensteinheit / Team: Befehlshandler und Menü
//
// Public functions:
//   cmdDe(args)   — dispatch for 'rh -de ...'
//   teamMenu()    — interactive team browser
// ============================================================
#include "cli_common.h"
#include "../core/OperationResult.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include <algorithm>
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── -de ───────────────────────────────────────────────────────

void cmdDe(const std::vector<std::string>& /*args*/) {
    CLI::teamMenu();
}


static void printTeam(const Team& t) {
    auto row = [&](const std::string& k, const std::string& v) {
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(38) << v << "|\n";
    };
    std::cout << "  +" << std::string(60,'-') << "+\n";
    row("ID",          t.teamId);
    row("Name",        t.name);
    row("Typ",         fval(t.type));
    row("Lead-ID",     fval(t.leadId));
    row("Parent-Team", fval(t.parentTeamId));
    row("Status",      t.status);
    row("Standort",    fval(t.location));
    row("Budget",      t.budgetAllocated > 0 ?
                       std::to_string((int)t.budgetAllocated) + " EUR" : "—");
    std::cout << "  +" << std::string(60,'-') << "+\n\n";
}

static void teamDetailMenu(std::shared_ptr<Team> t) {
    while (true) {
        t->load(t->teamId);
        t->loadMembers();
        hdr("DIENSTEINHEIT  " + t->teamId.substr(0,22));
        printTeam(*t);

        // Show members
        std::cout << "  Mitglieder (" << t->members.size() << "):\n";
        if (t->members.empty()) {
            std::cout << "    (keine)\n";
        } else {
            std::cout << "    " << std::left
                      << std::setw(26) << "Person-ID"
                      << std::setw(18) << "Rolle"
                      << std::setw(10) << "Allok.%"
                      << "Lead\n";
            std::cout << "    " << std::string(60,'-') << "\n";
            for (auto& m : t->members) {
                std::cout << "    " << std::left
                          << std::setw(26) << m->personId.substr(0,24)
                          << std::setw(18) << fval(m->role).substr(0,16)
                          << std::setw(10) << (int)m->allocationPct
                          << (m->isLead ? "  ★" : "") << "\n";
            }
        }
        std::cout << "\n";

        // Sub-teams
        auto children = Team::loadChildren(t->teamId);
        if (!children.empty()) {
            std::cout << "  Untereinheiten (" << children.size() << "):\n";
            for (auto& c : children)
                std::cout << "    " << c->teamId.substr(0,24) << "  " << c->name << "\n";
            std::cout << "\n";
        }

        std::cout << "  1.Name/Typ bearbeiten  2.Lead zuweisen     3.Parent setzen\n"
                     "  4.Mitglied hinzufügen  5.Mitglied entfernen 6.Budget setzen\n"
                     "  7.Status ändern        0.Zurück\n";
        int ch = readInt("Wahl", 0, 7);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string n = readOpt("Neuer Name (leer=unverändert): ");
            if (!n.empty()) t->name = n;
            t->type  = readOpt("Typ (delivery/platform/governance/advisory/ops, leer=delivery): ");
            t->location  = readOpt("Standort: ");
                            t->update();
            std::cout << "  >> Gespeichert.\n";
        }

        else if (ch == 2) {
            std::string pid = readLine("Neue Lead Person-ID: ");
            if (opOk(t->reassignLead(pid)))
                std::cout << "  >> Lead geändert: " << pid << "\n";
            else
                std::cout << "  >> Fehler.\n";
        }

        else if (ch == 3) {
            std::string pid = readOpt("Parent-Team-ID (leer=kein Parent): ");
            t->parentTeamId = pid;
            t->update();
            std::cout << "  >> Parent gesetzt.\n";
        }

        else if (ch == 4) {
            std::string pid = readLine("Person-ID: ");
            if (pid.empty()) continue;
            std::string role = readOpt("Rolle (developer/qa/pm/designer/..., leer=member): ");
            if (role.empty()) role = "member";
            std::string allStr = readOpt("Allokation % (leer=100): ");
            int alloc = 100;
            if (!allStr.empty()) try { alloc = std::stoi(allStr); } catch(...) {}
            auto m = t->addMember(pid, role);
            if (m) {
                m->allocationPct = (double)alloc;
                m->save();
                std::cout << "  >> Mitglied hinzugefügt: " << pid << "\n";
            } else {
                std::cout << "  >> Fehler beim Hinzufügen.\n";
            }
        }

        else if (ch == 5) {
            if (t->members.empty()) { std::cout << "  Keine Mitglieder.\n"; continue; }
            std::cout << "  Mitglieder:\n";
            int n = 1;
            for (auto& m : t->members)
                std::cout << "    " << n++ << ". " << m->personId << "\n";
            int pick = readInt("Nummer", 1, (int)t->members.size());
            auto& m = t->members[pick-1];
            m->remove();
            std::cout << "  >> Mitglied entfernt.\n";
        }

        else if (ch == 6) {
            std::string bStr = readOpt("Budget EUR: ");
            if (!bStr.empty()) try { t->budgetAllocated = std::stod(bStr); } catch(...) {}
            t->update();
            std::cout << "  >> Budget gesetzt: " << (int)t->budgetAllocated << " EUR\n";
        }

        else if (ch == 7) {
            std::cout << "  Status: 1.active  2.inactive  3.archived\n";
            int s = readInt("Status", 1, 3);
            static const char* stats[] = {"active","inactive","archived"};
            t->status = stats[s-1];
            t->update();
            std::cout << "  >> Status: " << t->status << "\n";
        }
    }
}

void teamMenu() {
    while (true) {
        auto teams = Team::loadAll();

        hdr("DIENSTEINHEITEN (DE)");
        if (teams.empty()) {
            std::cout << "  (keine Diensteinheiten)\n\n";
        } else {
            std::cout << "  " << std::left
                      << std::setw(5)  << "#"
                      << std::setw(26) << "ID"
                      << std::setw(28) << "Name"
                      << std::setw(14) << "Typ"
                      << "Mitgl.\n";
            std::cout << "  " << std::string(76,'-') << "\n";
            int n = 1;
            for (auto& t : teams) {
                t->loadMembers();
                std::cout << "  " << std::left
                          << std::setw(5)  << n++
                          << std::setw(26) << t->teamId.substr(0,24)
                          << std::setw(28) << t->name.substr(0,26)
                          << std::setw(14) << fval(t->type)
                          << t->members.size() << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "  1.Neue Diensteinheit  2.Öffnen  0.Zurück\n";
        int ch = readInt("Wahl", 0, 2);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string name = readLine("Name der Diensteinheit: ");
            if (name.empty()) continue;
            std::string typ = readOpt("Typ (delivery/platform/governance/advisory/ops, leer=delivery): ");
            if (typ.empty()) typ = "delivery";
            auto t = Team::create(name, typ);
            t->location  = readOpt("Standort (optional): ");
                    std::string lid = readOpt("Lead Person-ID (optional): ");
            if (!lid.empty()) t->leadId = lid;
            if (opOk(t->save())) {
                std::cout << "  >> Diensteinheit angelegt: " << t->teamId << "\n";
                teamDetailMenu(t);
            }
        }

        else if (ch == 2) {
            if (teams.empty()) { std::cout << "  Keine Teams.\n"; continue; }
            int n = readInt("Nummer", 1, (int)teams.size());
            teamDetailMenu(teams[n-1]);
        }
    }
}

} // namespace CLI

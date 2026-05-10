// ============================================================
// cli_comm.cpp  —  Communication: Detailmenü
//
// Public functions:
//   communicationMenu(ownerId, ownerType)
//     — list and manage communications for any owner entity
//     — ownerType: "f16" | "f22" | "f18" | "f24"
// ============================================================
#include "cli_common.h"
#include "../model/f18/Communication.h"
#include <iomanip>

namespace CLI {

using namespace Rosenholz;


using namespace Rosenholz;

// ── Communication detail menu ─────────────────────────────────
void commDetailMenu(std::shared_ptr<Communication> c) {
    while (true) {
        hdr("COMMUNICATION — " + c->communicationId.substr(0,22));
        std::cout << "  Typ    : " << c->commType << "\n";
        std::cout << "  Titel  : " << c->title << "\n";
        std::cout << "  Status : " << commStatusToString(c->status) << "\n";
        std::cout << "  Geplant: " << fdate(c->scheduledDate) << "\n";
        std::cout << "  Ist    : " << fdate(c->actualDate) << "\n";
        std::cout << "  Dauer  : " << c->durationMins << " min\n";
        std::cout << "  Kanal  : " << fval(c->channel) << "\n";
        if (!c->agenda.empty())
            std::cout << "  Agenda : " << c->agenda.substr(0,60) << "\n";
        if (!c->decisions.empty())
            std::cout << "  Beschl.: " << c->decisions.substr(0,60) << "\n";
        std::cout << "\n  1.Abschließen  2.Details bearbeiten  3.Agenda/Beschlüsse  0.Zurück\n";
        int ch = readInt("Wahl",0,3); if (ch==0) break;

        if (ch==1) {
            std::string decs = readOpt("Beschlüsse: ");
            std::string acts = readOpt("Aktionspunkte: ");
            c->complete(decs, acts);
            std::cout << "  >> Abgeschlossen.\n";
        } else if (ch==2) {
            std::string t = readOpt("Titel (leer=behalten): ");
            if (!t.empty()) c->title = t;
            c->channel       = readOpt("Kanal (teams/zoom/phone/email/in-person): ");
            c->scheduledDate = parseDate(readOpt("Geplantes Datum (JJJJ-MM-TT, . +1d +2w): "));
            std::string dm   = readOpt("Dauer (Minuten): ");
            if (!dm.empty()) try { c->durationMins = std::stoi(dm); } catch(...) {}
            c->location      = readOpt("Ort / Raum: ");
            c->organiserId   = readOpt("Organisator Person-ID: ");
            c->update();
            std::cout << "  >> Gespeichert.\n";
        } else if (ch==3) {
            c->agenda    = readOpt("Agenda (leer=behalten): ");
            c->decisions = readOpt("Beschlüsse: ");
            c->actions   = readOpt("Aktionspunkte: ");
            c->notes     = readOpt("Notizen: ");
            c->update();
            std::cout << "  >> Gespeichert.\n";
        }
    }
}

// ── Communication browser/menu ────────────────────────────────
void communicationMenu(const std::string& ownerId, const std::string& ownerType) {
    while (true) {
        auto comms = Communication::loadForOwner(ownerId, ownerType);
        hdr("COMMUNICATIONS — " + ownerType + " / " + ownerId.substr(0,20));
        if (comms.empty()) {
            std::cout << "  (keine Communications)\n";
        } else {
            int n = 1;
            for (auto& c : comms)
                std::cout << "  " << std::setw(3) << n++ << ". "
                          << "[" << std::left << std::setw(7) << c->commType.substr(0,6) << "] "
                          << std::setw(26) << c->title.substr(0,24)
                          << "  " << fdate(c->scheduledDate)
                          << "  " << commStatusToString(c->status) << "\n";
        }
        std::cout << "\n  1.Neu anlegen  2.Öffnen  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;

        if (ch==1) {
            std::cout << "  Typ: 1.meeting  2.message  3.call  4.email  5.report\n";
            int ct = readInt("Typ",1,5);
            static const CommType ctypes[] = {
                CommType::MEETING, CommType::MESSAGE, CommType::CALL,
                CommType::EMAIL, CommType::REPORT};
            CommType ctype = ctypes[ct-1];
            std::string t = readLine("Titel: ");
            if (t.empty()) continue;
            auto c = Communication::create(
                ownerId, ownerType, t, commTypeToString(ctype));
            if (c) {
                c->scheduledDate = parseDate(readOpt("Datum (JJJJ-MM-TT, . +1d +2w +3m +1y): "));
                c->channel       = readOpt("Kanal: ");
                c->location      = readOpt("Ort: ");
                c->agenda        = readOpt("Agenda: ");
                c->organiserId   = readOpt("Organisator Person-ID: ");
                // KOM-only: offer notiz template (Feature 23)
                std::string tpl  = Communication::notizTemplate(ctype);
                if (!tpl.empty()) {
                    std::cout << "  Notiz-Vorlage verwenden? (j/n): ";
                    if (yesno("")) c->notes = tpl;
                }
                c->update();
                std::cout << "  >> Communication angelegt: " << c->communicationId << "\n";
                commDetailMenu(c);
            }
        } else if (ch==2) {
            if (comms.empty()) continue;
            int pick = readInt("Nummer",1,(int)comms.size());
            commDetailMenu(comms[pick-1]);
        }
    }
}


// ── cmdKom: standalone kom command with -o/-so ────────────────────────────
void cmdKom(const std::vector<std::string>& args) {
    // -o: list all recent communications, navigate (open commDetailMenu)
    // -so <q>: search by title
    bool doSearch = (!args.empty() && args[0] == "-so");
    bool doList   = (!args.empty() && args[0] == "-o");
    if (doList || doSearch) {
        std::string q = (doSearch && args.size() > 1) ? args[1] : "";
        // Load all comms (flat — across all owners):
        auto all = Communication::loadAll(200);
        std::vector<std::shared_ptr<Communication>> hits;
        for (auto& c : all) {
            if (q.empty()) { hits.push_back(c); continue; }
            if (matchesPattern(c->title, q) || matchesPattern(c->communicationId, q))
                hits.push_back(c);
        }
        if (hits.empty()) { std::cout << "  (keine Kommunikation)\n"; return; }
        std::cout << "\n  " << std::left << std::setw(4) << "#"
                  << std::setw(8)  << "TYP"
                  << std::setw(28) << "TITEL"
                  << std::setw(14) << "GEPLANT"
                  << std::setw(18) << "OWNER"
                  << "STATUS\n"
                  << "  " << std::string(82, '-') << "\n";
        for (int i = 0; i < (int)hits.size(); i++) {
            std::cout << "  " << std::setw(4) << (i+1)
                      << std::setw(8)  << hits[i]->commType.substr(0,7)
                      << std::setw(28) << hits[i]->title.substr(0,26)
                      << std::setw(14) << fdate(hits[i]->scheduledDate)
                      << std::setw(18) << (hits[i]->ownerType + ":" + hits[i]->ownerId.substr(0,12))
                      << Color::statusColor(commStatusToString(hits[i]->status)) << "\n";
        }
        if (!doList) return;  // -so: just list
        int pick = readInt("  Auswahl [0=Abbrechen]", 0, (int)hits.size());
        if (pick < 1) return;
        commDetailMenu(hits[pick-1]);
        return;
    }
    // No args or -n: show usage hint
    std::cout << "  kom -o    Alle Kommunikationen listen und öffnen\n"
              << "  kom -so <q>  Suchen und öffnen\n"
              << "  kom -n    Neue Kommunikation (im Kontext)\n";
}

} // namespace CLI

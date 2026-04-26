// ============================================================
// cli_comm.cpp  —  Communication: Detailmenü
//
// Public functions:
//   communicationMenu(ownerId, ownerType)
//     — list and manage communications for any owner entity
//     — ownerType: "f16" | "f22" | "f18" | "f18step"
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
        hdr("COMMUNICATION — " + c->commId.substr(0,22));
        std::cout << "  Typ    : " << c->commType << "\n";
        std::cout << "  Titel  : " << c->title << "\n";
        std::cout << "  Status : " << c->status << "\n";
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
            c->scheduledDate = readOpt("Geplantes Datum (JJJJ-MM-TT): ");
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
                          << "  " << c->status << "\n";
        }
        std::cout << "\n  1.Neu anlegen  2.Öffnen  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;

        if (ch==1) {
            std::cout << "  Typ: 1.meeting  2.message  3.call  4.email  5.report\n";
            int ct = readInt("Typ",1,5);
            static const char* cts[] = {"meeting","message","call","email","report"};
            std::string t = readLine("Titel: ");
            if (t.empty()) continue;
            auto c = Communication::create(ownerId, ownerType, t, cts[ct-1]);
            if (c) {
                c->scheduledDate = readOpt("Datum (JJJJ-MM-TT): ");
                c->channel       = readOpt("Kanal: ");
                c->location      = readOpt("Ort: ");
                c->agenda        = readOpt("Agenda: ");
                c->organiserId   = readOpt("Organisator Person-ID: ");
                c->update();
                std::cout << "  >> Communication angelegt: " << c->commId << "\n";
                commDetailMenu(c);
            }
        } else if (ch==2) {
            if (comms.empty()) continue;
            int pick = readInt("Nummer",1,(int)comms.size());
            commDetailMenu(comms[pick-1]);
        }
    }
}

} // namespace CLI

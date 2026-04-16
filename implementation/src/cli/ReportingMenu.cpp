// ============================================================
// ReportingMenu.cpp  —  Milestones + Communications
//
// After the F18 refactor, Measure, QualityGate, and Meeting
// are all F18Workflow entities. Only Milestone stays here as
// a pure reporting utility (no F18 lifecycle).
//
// The old meetingMenu → now communicationMenu (Communication)
// Measure/QT → f18BrowserMenu with type filter
// ============================================================
#include "cli_common.h"
#include "../model/Milestone.h"
#include "../model/f18/Communication.h"
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── Milestone sub-menu ────────────────────────────────────────
static void milestoneDetailMenu(std::shared_ptr<Milestone> m) {
    while (true) {
        hdr("MEILENSTEIN — " + m->milestoneId.substr(0,22));
        std::cout << "  Titel  : " << m->title << "\n";
        std::cout << "  Typ    : " << m->milestoneType << "\n";
        std::cout << "  Geplant: " << fdate(m->plannedDate) << "\n";
        std::cout << "  Ist    : " << fdate(m->actualDate) << "\n";
        std::cout << "  Status : " << m->status << "\n";
        std::cout << "  1.Als erreicht markieren  2.Bearbeiten  3.Verantwortl. zuweisen  0.Zurück\n";
        int ch = readInt("Wahl",0,3); if (ch==0) break;
        if (ch==1) {
            m->actualDate = nowIso().substr(0,10);
            m->status     = "achieved";
            m->update();
            std::cout << "  >> Meilenstein erreicht.\n";
        } else if (ch==2) {
            m->title = readOpt("Titel (leer=behalten): ");
            if (m->title.empty()) { auto tmp = Milestone::loadById(m->milestoneId); if(tmp) m->title=tmp->title; }
            std::cout << "  Typ: 1.phase-gate 2.delivery 3.payment 4.contractual 5.internal\n";
            int tt = readInt("Typ",1,5);
            static const char* tts[]={"phase-gate","delivery","payment","contractual","internal"};
            m->milestoneType = tts[tt-1];
            m->update();
        } else if (ch==3) {
            m->ownerId = readLine("Verantwortliche Person-ID: ");
            m->update();
        }
    }
}

void milestoneMenu(const std::string& projectId) {
    while (true) {
        auto milestones = Milestone::loadForProject(projectId);
        hdr("MEILENSTEINE (" + std::to_string(milestones.size()) + ")");
        int n=1;
        for (auto& m : milestones)
            std::cout << "  " << std::setw(3) << n++ << ". "
                      << std::left << std::setw(26) << m->title.substr(0,24)
                      << "  " << fdate(m->plannedDate)
                      << (m->status=="achieved" ? "  ✓" : "") << "\n";
        std::cout << "  1.Anlegen  2.Öffnen  3.Überfällige  0.Zurück\n";
        int ch = readInt("Wahl",0,3); if (ch==0) break;
        if (ch==1) {
            std::string t = readLine("Titel: ");
            std::string d = readLine("Geplantes Datum (JJJJ-MM-TT): ");
            auto m = Milestone::create(projectId, t, d);
            std::cout << "  Typ: 1.phase-gate 2.delivery 3.payment 4.contractual 5.internal\n";
            int tt = readInt("Typ",1,5);
            static const char* tts[]={"phase-gate","delivery","payment","contractual","internal"};
            m->milestoneType = tts[tt-1];
            m->save();
            std::cout << "  >> Meilenstein: " << m->milestoneId << "\n";
        } else if (ch==2) {
            if (milestones.empty()) continue;
            int pick = readInt("Nummer",1,(int)milestones.size());
            milestoneDetailMenu(milestones[pick-1]);
        } else if (ch==3) {
            auto ov = Milestone::loadForProject(projectId);
            hdr("ÜBERFÄLLIGE MEILENSTEINE");
            if (ov.empty()) { std::cout << "  (keine)\n"; continue; }
            for (auto& m : ov)
                std::cout << "  " << fdate(m->plannedDate) << "  " << m->title << "\n";
        }
    }
}

// ── Communication sub-menu ────────────────────────────────────
static void commDetailMenu(std::shared_ptr<Communication> c) {
    while (true) {
        hdr("COMMUNICATION — " + c->commId.substr(0,22));
        std::cout << "  Typ    : " << c->commType << "\n";
        std::cout << "  Titel  : " << c->title << "\n";
        std::cout << "  Geplant: " << fdate(c->scheduledDate) << "\n";
        std::cout << "  Ist    : " << fdate(c->actualDate) << "\n";
        std::cout << "  Dauer  : " << c->durationMins << " min\n";
        std::cout << "  Kanal  : " << fval(c->channel) << "\n";
        std::cout << "  Status : " << c->status << "\n";
        std::cout << "  1.Abschließen  2.Agenda/Beschlüsse  3.Details  0.Zurück\n";
        int ch = readInt("Wahl",0,3); if (ch==0) break;
        if (ch==1) {
            std::string decs = readOpt("Beschlüsse: ");
            std::string acts = readOpt("Aktionen: ");
            c->complete(decs, acts);
            std::cout << "  >> Abgeschlossen.\n";
        } else if (ch==2) {
            c->agenda    = readOpt("Agenda (leer=behalten): ");
            c->decisions = readOpt("Beschlüsse: ");
            c->actions   = readOpt("Aktionen: ");
            c->update();
        } else if (ch==3) {
            c->title        = readOpt("Titel: ");
            c->channel      = readOpt("Kanal (teams/zoom/phone/email/in-person): ");
            c->scheduledDate= readOpt("Datum (JJJJ-MM-TT): ");
            std::string dm  = readOpt("Dauer (Minuten): ");
            if (!dm.empty()) try { c->durationMins = std::stoi(dm); } catch(...) {}
            c->update();
        }
    }
}

void communicationMenu(const std::string& ownerId, const std::string& ownerType) {
    while (true) {
        auto comms = Communication::loadForOwner(ownerId, ownerType);
        hdr("COMMUNICATIONS (" + std::to_string(comms.size()) + ")");
        int n=1;
        for (auto& c : comms)
            std::cout << "  " << std::setw(3) << n++ << ". "
                      << "[" << c->commType.substr(0,4) << "] "
                      << std::left << std::setw(26) << c->title.substr(0,24)
                      << "  " << fdate(c->scheduledDate)
                      << "  " << c->status << "\n";
        std::cout << "  1.Neu  2.Öffnen  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;
        if (ch==1) {
            std::cout << "  Typ: 1.meeting 2.message 3.call 4.email 5.report\n";
            int ct = readInt("Typ",1,5);
            static const char* cts[]={"meeting","message","call","email","report"};
            std::string t = readLine("Titel: ");
            auto c = Communication::create(ownerId, ownerType, t, cts[ct-1]);
            if (c) {
                c->scheduledDate = readOpt("Datum (JJJJ-MM-TT): ");
                c->channel       = readOpt("Kanal: ");
                c->agenda        = readOpt("Agenda: ");
                c->update();
                std::cout << "  >> Communication: " << c->commId << "\n";
            }
        } else if (ch==2) {
            if (comms.empty()) continue;
            int pick = readInt("Nummer",1,(int)comms.size());
            commDetailMenu(comms[pick-1]);
        }
    }
}

} // namespace CLI

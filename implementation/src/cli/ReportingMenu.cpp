// ============================================================
// ReportingMenu.cpp  —  Milestones, Meetings, Measures, Quality Gates
//
// milestoneMenu()   : MEI lifecycle (planned → achieved)
// meetingMenu()     : BSP scheduling and completion
// measureMenu()     : MSN cost tracking and verification
// qualityGateMenu() : QT gate criteria and result recording
// ============================================================
#include "cli_common.h"
using namespace Rosenholz;
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include <sstream>
#include <algorithm>

namespace CLI {
void milestoneMenu(const std::string& projectId) {
    while (true) {
        auto milestones = Rosenholz::Milestone::loadForProject(projectId);
        hdr("MILESTONES  project=" + projectId.substr(0,20));
        if (milestones.empty()) std::cout << "  (none yet)\n";
        else {
            std::cout << "  " << std::left
                      << std::setw(4) << "#" << std::setw(14) << "Type"
                      << std::setw(24) << "Title" << std::setw(12) << "Planned"
                      << std::setw(10) << "Status" << std::setw(8) << "Paym." << "\n";
            hr();
            int n=1;
            for (auto& m : milestones) {
                std::string t = m->title.size()>22 ? m->title.substr(0,21)+"~" : m->title;
                std::cout << "  " << std::left
                          << std::setw(4)  << n++
                          << std::setw(14) << fval(m->milestoneType)
                          << std::setw(24) << t
                          << std::setw(12) << fdate(m->plannedDate)
                          << std::setw(10) << m->status
                          << std::setw(8)  << (m->paymentTrigger?"YES":"")
                          << "\n";
            }
        }
        std::cout << "\n  Actions:\n"
                  << "    1. Create milestone\n"
                  << "    2. Open by number\n"
                  << "    3. Show overdue milestones\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 3);
        if (ch == 0) break;
        else if (ch == 1) {
            std::string title = readLine("Title: ");
            std::string pd    = readOpt("Planned date YYYY-MM-DD: ");
            std::cout << "  Type: 1.phase-gate  2.delivery  3.payment  4.contractual  5.internal\n";
            int tt = readInt("Type", 1, 5);
            static const char* mtypes[]={"phase-gate","delivery","payment","contractual","internal"};
            auto m = Rosenholz::Milestone::create(projectId, title, pd);
            m->milestoneType = mtypes[tt-1];
            m->criteria      = readOpt("Criteria (optional): ");
            m->ownerId       = readOpt("Owner person-ID (optional): ");
            std::cout << "  Payment trigger? (y/n): "; std::string pt; std::getline(std::cin,pt);
            m->paymentTrigger = (!pt.empty()&&(pt[0]=='y'||pt[0]=='Y'));
            std::cout << "  Contractual? (y/n): "; std::string co; std::getline(std::cin,co);
            m->contractual    = (!co.empty()&&(co[0]=='y'||co[0]=='Y'));
            if (m->save())
                std::cout << "  >> Milestone saved: " << m->milestoneId << "\n";
        }
        else if (ch == 2) {
            if (milestones.empty()) { std::cout << "  (none)\n"; continue; }
            int n = readInt("Number", 1, (int)milestones.size());
            auto& m = milestones[n-1];
            // mini detail loop
            while (true) {
                auto row=[](const std::string& k,const std::string& v){
                    std::cout<<"  | "<<std::left<<std::setw(20)<<k<<std::setw(32)<<v<<"|\n";};
                hdr("MILESTONE");
                row("ID",m->milestoneId); row("Title",m->title);
                row("Type",fval(m->milestoneType)); row("Status",m->status);
                row("Planned",fdate(m->plannedDate)); row("Actual",fdate(m->actualDate));
                row("Variance (d)",std::to_string(m->varianceDays));
                row("Contractual",m->contractual?"YES":"no");
                row("Payment trig",m->paymentTrigger?"YES":"no");
                row("Owner",fval(m->ownerId));
                if (!m->criteria.empty()) row("Criteria",m->criteria.substr(0,30));
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";

                std::cout<<"  1.Mark achieved  2.Edit  3.Reassign owner  0.Back\n";
                int mc = readInt("Choice",0,3);
                if (mc==0) break;
                else if (mc==1) {
                    std::string ad = readOpt("Actual date (YYYY-MM-DD, blank=now): ");
                    m->markAchieved(ad);
                    std::cout << "  >> Achieved.\n";
                }
                else if (mc==2) {
                    std::string nt = readOpt("New title: "); if(!nt.empty()) m->title=nt;
                    std::string np = readOpt("New planned date: "); if(!np.empty()) m->plannedDate=np;
                    std::string ns = readOpt("New status (pending/achieved/missed/deferred): ");
                    if(!ns.empty()) m->status=ns;
                    std::string nc = readOpt("New criteria: "); if(!nc.empty()) m->criteria=nc;
                    m->update(); std::cout<<"  >> Saved.\n";
                }
                else if (mc==3) {
                    std::string id = readLine("Owner person-ID: ");
                    m->reassignOwner(id); std::cout<<"  >> Reassigned.\n";
                }
            }
        }
        else if (ch == 3) {
            auto ov = Rosenholz::Milestone::loadOverdue();
            hdr("OVERDUE MILESTONES");
            if (ov.empty()) std::cout << "  (none — all on track)\n\n";
            else for (auto& m : ov)
                std::cout << "  " << fdate(m->plannedDate) << "  " << std::left
                          << std::setw(28) << m->title << "  proj=" << m->projectId.substr(0,16) << "\n";
            std::cout << "\n";
        }
    }
}

// ─────────────────────────────────────────────────────────────
// MEETING MENU
// ─────────────────────────────────────────────────────────────
void meetingMenu(const std::string& taskId, const std::string& projectId) {
    while (true) {
        auto meetings = taskId.empty()
            ? Rosenholz::Meeting::loadForProject(projectId)
            : Rosenholz::Meeting::loadForTask(taskId);
        hdr("MEETINGS");
        if (meetings.empty()) std::cout << "  (none)\n";
        else {
            int n=1;
            for (auto& m : meetings)
                std::cout << "  " << std::setw(3) << n++ << ".  ["
                          << fdate(m->scheduledDate).substr(0,10) << "]  "
                          << std::left << std::setw(24) << m->title
                          << "  status=" << m->status
                          << "  dur=" << m->durationMins << "min\n";
        }
        std::cout << "\n  1.Create  2.Open  0.Back\n";
        int ch = readInt("Choice",0,2);
        if (ch==0) break;
        else if (ch==1) {
            std::string t  = readLine("Title: ");
            std::string sd = readOpt("Scheduled date YYYY-MM-DD HH:MM: ");
            std::string mt = readOpt("Type (general/standup/review/kickoff/retrospective): ");
            std::string loc= readOpt("Location (optional): ");
            std::string ch2= readOpt("Channel (in-person/video/phone): ");
            std::string tid = taskId.empty() ? readLine("Task-ID: ") : taskId;
            auto m = Rosenholz::Meeting::create(tid, t, sd);
            m->meetingType = mt; m->location = loc; m->channel = ch2;
            m->projectId   = projectId;
            m->agenda      = readOpt("Agenda (optional): ");
            m->organiserId = readOpt("Organiser person-ID (optional): ");
            std::string durS = readOpt("Duration minutes (optional): ");
            if (!durS.empty()) try { m->durationMins = std::stoi(durS); } catch(...) {}
            if (m->save()) std::cout << "  >> Meeting saved: " << m->meetingId << "\n";
        }
        else if (ch==2) {
            if (meetings.empty()) { std::cout << "  (none)\n"; continue; }
            int n = readInt("Number",1,(int)meetings.size());
            auto& m = meetings[n-1];
            while (true) {
                hdr("MEETING  " + m->title);
                auto row=[](const std::string& k,const std::string& v){
                    std::cout<<"  | "<<std::left<<std::setw(18)<<k<<std::setw(34)<<v<<"|\n";};
                row("ID",m->meetingId); row("Status",m->status);
                row("Scheduled",fdate(m->scheduledDate)); row("Actual",fdate(m->actualDate));
                row("Duration",std::to_string(m->durationMins)+"min");
                row("Location",fval(m->location)); row("Channel",fval(m->channel));
                row("Organiser",fval(m->organiserId));
                if (!m->agenda.empty()) std::cout<<"  | Agenda: "<<m->agenda.substr(0,45)<<"|\n";
                if (!m->decisions.empty()) std::cout<<"  | Decisions: "<<m->decisions.substr(0,41)<<"|\n";
                if (!m->actions.empty()) std::cout<<"  | Actions: "<<m->actions.substr(0,43)<<"|\n";
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Complete  2.Edit agenda/decisions  3.Edit details  0.Back\n";
                int mc=readInt("Choice",0,3);
                if (mc==0) break;
                else if (mc==1) {
                    std::string dec = readOpt("Decisions: ");
                    std::string act = readOpt("Actions: ");
                    m->complete(dec,act); std::cout<<"  >> Completed.\n";
                }
                else if (mc==2) {
                    std::string ag = readOpt("Agenda: "); if(!ag.empty()) m->agenda=ag;
                    std::string dc = readOpt("Decisions: "); if(!dc.empty()) m->decisions=dc;
                    std::string ac = readOpt("Actions: "); if(!ac.empty()) m->actions=ac;
                    m->update(); std::cout<<"  >> Saved.\n";
                }
                else if (mc==3) {
                    std::string sd = readOpt("Scheduled date: "); if(!sd.empty()) m->scheduledDate=sd;
                    std::string loc = readOpt("Location: "); if(!loc.empty()) m->location=loc;
                    std::string dur = readOpt("Duration min: ");
                    if(!dur.empty()) try{m->durationMins=std::stoi(dur);}catch(...){}
                    m->update(); std::cout<<"  >> Saved.\n";
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// MEASURE MENU
// ─────────────────────────────────────────────────────────────
void measureMenu(const std::string& projectId) {
    while (true) {
        auto measures = Rosenholz::Measure::loadForProject(projectId);
        hdr("MEASURES  project=" + projectId.substr(0,20));
        if (measures.empty()) std::cout << "  (none)\n";
        else {
            int n=1;
            for (auto& m : measures)
                std::cout << "  " << std::setw(3) << n++ << ".  "
                          << std::left << std::setw(24) << m->title
                          << "  type="   << std::setw(14) << m->measureType
                          << "  status=" << m->status << "\n";
        }
        std::cout << "\n  1.Create  2.Open  0.Back\n";
        int ch = readInt("Choice",0,2); if(ch==0) break;
        else if (ch==1) {
            std::string t = readLine("Title: ");
            std::cout<<"  Type: 1.preventive  2.corrective  3.detective  4.directive\n";
            int tc=readInt("Type",1,4);
            static const char* mts[]={"preventive","corrective","detective","directive"};
            auto m = Rosenholz::Measure::create(projectId, t, mts[tc-1]);
            m->description      = readOpt("Description (optional): ");
            m->measureCategory  = readOpt("Category (optional): ");
            m->plannedDate      = readOpt("Planned date YYYY-MM-DD: ");
            m->ownerId          = readOpt("Owner person-ID (optional): ");
            m->riskId           = readOpt("Linked risk-ID (optional): ");
            m->incidentId       = readOpt("Linked incident-ID (optional): ");
            std::string costStr = readOpt("Planned cost EUR (optional): ");
            if(!costStr.empty()) try{m->costPlanned=std::stod(costStr);}catch(...){}
            if(m->save()) std::cout<<"  >> Measure saved: "<<m->measureId<<"\n";
        }
        else if (ch==2 && !measures.empty()) {
            int n=readInt("Number",1,(int)measures.size());
            auto& m=measures[n-1];
            while(true){
                hdr("MEASURE  "+m->title);
                auto row=[](const std::string& k,const std::string& v){std::cout<<"  | "<<std::left<<std::setw(20)<<k<<std::setw(32)<<v<<"|\n";};
                row("ID",m->measureId); row("Type",fval(m->measureType)); row("Status",m->status);
                row("Category",fval(m->measureCategory)); row("Owner",fval(m->ownerId));
                row("Planned",fdate(m->plannedDate)); row("Actual",fdate(m->actualDate));
                row("Cost plan",std::to_string((int)m->costPlanned));
                row("Cost actual",std::to_string((int)m->costActual));
                row("Effectiveness",fval(m->effectiveness));
                row("Risk-ID",fval(m->riskId)); row("Incident-ID",fval(m->incidentId));
                std::cout<<"  +"<<std::string(52,'-')<<"+\n\n";
                std::cout<<"  1.Mark completed  2.Edit status  3.Record cost  4.Verify  0.Back\n";
                int mc=readInt("Choice",0,4); if(mc==0) break;
                else if(mc==1){m->status="completed";m->actualDate=Rosenholz::nowIso();m->update();std::cout<<"  >> Completed.\n";}
                else if(mc==2){std::string s=readOpt("Status (planned/in-progress/completed/verified/cancelled): ");if(!s.empty())m->status=s;m->update();std::cout<<"  >> Saved.\n";}
                else if(mc==3){std::string c=readOpt("Actual cost EUR: ");if(!c.empty())try{m->costActual=std::stod(c);}catch(...){}m->update();std::cout<<"  >> Saved.\n";}
                else if(mc==4){m->verifiedDate=Rosenholz::nowIso();m->verifiedBy=readOpt("Verified by person-ID: ");m->effectiveness=readOpt("Effectiveness (high/medium/low): ");m->status="verified";m->update();std::cout<<"  >> Verified.\n";}
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// QUALITY GATE MENU
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// KPI MENU
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// LESSON LEARNED MENU
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// ASSUMPTION/CONSTRAINT MENU
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// DECISION LOG MENU
// ─────────────────────────────────────────────────────────────



} // namespace CLI

// ── QualityGate full menu ─────────────────────────────────────
namespace CLI {

void qualityGateMenu(const std::string& projectId) {
    using namespace Rosenholz;

    while (true) {
        auto gates = QualityGate::loadForProject(projectId);
        hdr("QUALITY GATES (QT)  —  Projekt " + projectId.substr(0,20));

        if (gates.empty()) {
            std::cout << "  (keine Quality Gates)\n\n";
        } else {
            std::cout << "  " << std::left
                      << std::setw(5) << "#"
                      << std::setw(26) << "ID"
                      << std::setw(28) << "Titel"
                      << std::setw(12) << "Ergebnis"
                      << "Phase\n";
            std::cout << "  " << std::string(74,'-') << "\n";
            int n = 1;
            for (auto& g : gates)
                std::cout << "  " << std::left
                          << std::setw(5)  << n++
                          << std::setw(26) << g->gateId.substr(0,24)
                          << std::setw(28) << g->title.substr(0,26)
                          << std::setw(12) << g->result
                          << fval(g->phase) << "\n";
            std::cout << "\n";
        }

        std::cout << "  1.Neues Gate  2.Öffnen  0.Zurück\n";
        int ch = readInt("Wahl", 0, 2);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string title = readLine("Gate-Titel: ");
            if (title.empty()) continue;
            std::string phase = readOpt("Phase (z.B. Planung/Ausführung/Abschluss): ");
            auto g = QualityGate::create(projectId, title, phase);
            g->plannedDate      = readOpt("Geplantes Datum (JJJJ-MM-TT): ");
            g->criteria         = readOpt("Kriterien: ");
            g->acceptanceCriteria = readOpt("Abnahmekriterien: ");
            if (g->save())
                std::cout << "  >> Quality Gate angelegt: " << g->gateId << "\n";
        }

        else if (ch == 2) {
            if (gates.empty()) { std::cout << "  Keine Gates.\n"; continue; }
            int n = readInt("Nummer", 1, (int)gates.size());
            auto& g = gates[n-1];

            while (true) {
                g->load(g->gateId);
                hdr("QUALITY GATE  " + g->gateId.substr(0,20));
                auto row = [&](const std::string& k, const std::string& v) {
                    std::cout << "  | " << std::left << std::setw(22) << k
                              << std::setw(38) << v << "|\n";
                };
                std::cout << "  +" << std::string(60,'-') << "+\n";
                row("ID",             g->gateId);
                row("Titel",          g->title);
                row("Phase",          fval(g->phase));
                row("Ergebnis",       g->result);
                row("Entscheidung",   fval(g->decision));
                row("Geplant",        fdate(g->plannedDate));
                row("Durchgeführt",   fdate(g->actualDate));
                row("Kriterien",      fval(g->criteria.empty() ? "" :
                                          g->criteria.substr(0,36)));
                row("Befunde",        fval(g->findings.empty() ? "" :
                                          g->findings.substr(0,36)));
                row("WF-Instanz",     fval(g->workflowInstanceId));
                std::cout << "  +" << std::string(60,'-') << "+\n\n";

                std::cout << "  1.Bearbeiten  2.Ergebnis erfassen  "
                             "3.Workflow starten  0.Zurück\n";
                int gch = readInt("Wahl", 0, 3);
                if (gch == 0) break;

                else if (gch == 1) {
                    std::string t = readOpt("Titel (leer=unverändert): ");
                    if (!t.empty()) g->title = t;
                    g->phase     = readOpt("Phase: ");
                    g->criteria  = readOpt("Kriterien: ");
                    g->acceptanceCriteria = readOpt("Abnahmekriterien: ");
                    g->plannedDate = readOpt("Geplantes Datum: ");
                    g->update();
                    std::cout << "  >> Gespeichert.\n";
                }

                else if (gch == 2) {
                    std::cout << "  Ergebnis: 1.passed  2.failed  3.conditional  4.pending\n";
                    int r = readInt("Ergebnis", 1, 4);
                    static const char* res[] = {"passed","failed","conditional","pending"};
                    std::cout << "  Entscheidung: 1.proceed  2.hold  3.stop\n";
                    int d = readInt("Entscheidung", 1, 3);
                    static const char* decs[] = {"proceed","hold","stop"};
                    std::string findings = readOpt("Befunde / Anmerkungen: ");
                    g->recordResult(res[r-1], decs[d-1], findings);
                    std::cout << "  >> Ergebnis erfasst: " << res[r-1]
                              << " → " << decs[d-1] << "\n";
                }

                else if (gch == 3) {
                    std::string iid = startWfInstanceWizard("quality_gate", g->gateId);
                    if (!iid.empty()) {
                        g->workflowInstanceId = iid;
                        g->update();
                        instanceMenu(iid);
                    }
                }
            }
        }
    }
}

} // namespace CLI

// ============================================================
// IncidentMenu.cpp  —  Incident (F18) detail menu
//
// incidentDetailMenu() : full incident lifecycle
// Supports: severity rating, root-cause, corrective actions,
//   risk linking (create new RSK or link existing), workflow
// ============================================================
// ============================================================
// IncidentMenu.cpp — IncidentF18 (F18) vollständiges CLI-Menü
// ============================================================
#include "cli_common.h"
#include "../model/IncidentF18.h"
#include "../model/Risk.h"
#include <iomanip>

using namespace Rosenholz;

namespace CLI {

static void printIncidentDetail(const IncidentF18& i) {
    auto row = [&](const std::string& k, const std::string& v) {
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(38) << v << "|\n";
    };
    std::cout << "  +" << std::string(60,'-') << "+\n";
    row("ID",           i.incidentId);
    row("Titel",        i.title);
    row("Typ",          fval(i.incidentType));
    row("Schwere",      i.severity);
    row("Status",       i.status);
    row("Aufgetreten",  fdate(i.occurredDate));
    row("Gemeldet",     fdate(i.reportedDate));
    row("Gelöst",       fdate(i.resolvedDate));
    row("Kosten",       std::to_string((int)i.costImpact) + " EUR");
    row("Zeitauswirk.", std::to_string(i.scheduleImpactDays) + " Tage");
    row("Ursache",      fval(i.rootCause.empty() ? "" : i.rootCause.substr(0,36)));
    row("Sofortmaßn.",  fval(i.immediateAction.empty() ? "" :
                            i.immediateAction.substr(0,36)));
    row("Korrekturm.",  fval(i.correctiveAction.empty() ? "" :
                            i.correctiveAction.substr(0,36)));
    row("Eskaliert",    i.escalated ? "Ja" : "Nein");
    row("Risiko-ID",    fval(i.riskId));
    row("WF-Instanz",   fval(i.workflowInstanceId));
    std::cout << "  +" << std::string(60,'-') << "+\n\n";
}

void incidentDetailMenu(std::shared_ptr<IncidentF18> inc) {
    while (true) {
        inc->load(inc->incidentId);
        hdr("VORFALL  " + inc->incidentId.substr(0,22));
        printIncidentDetail(*inc);

        std::cout << "  1.Beschreibung/Typ   2.Schwere/Status    3.Auswirkungen\n"
                     "  4.Ursache/Maßnahmen  5.Mit Risiko verknüpfen\n"
                     "  6.Workflow starten   7.Lösen (resolve)   0.Zurück\n";
        int ch = readInt("Wahl", 0, 7);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string t = readOpt("Neuer Titel (leer=unverändert): ");
            if (!t.empty()) inc->title = t;
            inc->description  = readOpt("Beschreibung: ");
            inc->incidentType = readOpt("Typ (technisch/prozess/mensch/extern/...): ");
            inc->category     = readOpt("Kategorie: ");
            inc->impactArea   = readOpt("Betroffener Bereich: ");
            inc->update();
            std::cout << "  >> Gespeichert.\n";
        }

        else if (ch == 2) {
            std::cout << "  Schwere: 1.low  2.medium  3.high  4.critical\n";
            int s = readInt("Schwere", 1, 4);
            static const char* sevs[] = {"low","medium","high","critical"};
            inc->severity = sevs[s-1];
            std::cout << "  Status: 1.open  2.investigating  3.resolved  4.closed\n";
            int st = readInt("Status", 1, 4);
            static const char* stats[] = {"open","investigating","resolved","closed"};
            inc->status = stats[st-1];
            if (st >= 3) inc->resolvedDate = nowIso();
            inc->update();
            std::cout << "  >> Schwere: " << inc->severity
                      << "  Status: " << inc->status << "\n";
        }

        else if (ch == 3) {
            std::string cv = readOpt("Kostenauswirkung EUR (leer=0): ");
            if (!cv.empty()) try { inc->costImpact = std::stod(cv); } catch(...) {}
            std::string sv = readOpt("Zeitauswirkung Tage (leer=0): ");
            if (!sv.empty()) try { inc->scheduleImpactDays = std::stoi(sv); } catch(...) {}
            inc->scopeImpact   = readOpt("Scope-Auswirkung: ");
            inc->qualityImpact = readOpt("Qualitäts-Auswirkung: ");
            inc->update();
            std::cout << "  >> Auswirkungen gespeichert.\n";
        }

        else if (ch == 4) {
            inc->rootCause       = readOpt("Grundursache (Root Cause): ");
            inc->immediateAction = readOpt("Sofortmaßnahme: ");
            inc->correctiveAction= readOpt("Korrekturmaßnahme: ");
            inc->resolution      = readOpt("Lösung / Abschlusstext: ");
            inc->update();
            std::cout << "  >> Ursache und Maßnahmen gespeichert.\n";
        }

        else if (ch == 5) {
            std::string riskId = readOpt("Risiko-ID (XV/RSK/...) oder 'neu': ");
            if (riskId == "neu") {
                auto r = Risk::create(inc->projectId, "Risiko aus Vorfall: " + inc->title);
                r->save();
                riskId = r->riskId;
                std::cout << "  >> Neues Risiko angelegt: " << riskId << "\n";
            }
            if (inc->linkToRisk(riskId))
                std::cout << "  >> Mit Risiko " << riskId << " verknüpft.\n";
        }

        else if (ch == 6) {
            std::string iid = startWfInstanceWizard("incident", inc->incidentId);
            if (!iid.empty()) {
                inc->workflowInstanceId = iid;
                inc->update();
                instanceMenu(iid);
            }
        }

        else if (ch == 7) {
            inc->status       = "resolved";
            inc->resolvedDate = nowIso();
            inc->resolution   = readOpt("Auflösungstext: ");
            inc->update();
            std::cout << "  >> Vorfall als gelöst markiert.\n";
        }
    }
}

} // namespace CLI

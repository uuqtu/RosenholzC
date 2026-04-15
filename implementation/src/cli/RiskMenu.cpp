// ============================================================
// RiskMenu.cpp  —  Risk (RSK) management
//
// riskMenu()       : list/create risks for a project
// riskDetailMenu() : scoring, response strategy, escalation
// 5×5 scoring: W×max(A-Zeit, A-Kosten, A-Qual, A-Scope)
// ============================================================
// ============================================================
// RiskMenu.cpp — Risiko-Akte (RSK) vollständiges CLI-Menü
// ============================================================
#include "cli_common.h"
#include "../model/Risk.h"
#include "../core/Database.h"
#include <iomanip>

using namespace Rosenholz;

namespace CLI {

static const char* RAG(int score) {
    if (score >= 15) return "[KRITISCH]";
    if (score >= 9)  return "[HOCH    ]";
    if (score >= 4)  return "[MITTEL  ]";
    return "[NIEDRIG ]";
}

static void printRisk(const Risk& r) {
    auto row = [&](const std::string& k, const std::string& v) {
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(38) << v << "|\n";
    };
    std::cout << "  +" << std::string(60,'-') << "+\n";
    row("ID",              r.riskId);
    row("Titel",           r.title);
    row("Kategorie",       fval(r.category));
    row("Typ",             fval(r.riskType));
    row("Status",          r.status);
    row("Risikoniveau",    fval(r.riskLevel) + "  " + RAG(r.overallRiskScore));
    row("Gesamtscore",     std::to_string(r.overallRiskScore));
    row("W-Score",         std::to_string(r.probabilityScore));
    row("A-Zeit",          std::to_string(r.impactScoreTime));
    row("A-Kosten",        std::to_string(r.impactScoreCost));
    row("A-Qualität",      std::to_string(r.impactScoreQuality));
    row("A-Scope",         std::to_string(r.impactScoreScope));
    row("Reaktionsstrat.", fval(r.responseStrategy));
    row("Notfallplan",     fval(r.contingencyPlan.empty() ? "" :
                               r.contingencyPlan.substr(0,36)));
    row("Frühwarnung",     fval(r.earlyWarning.empty() ? "" :
                               r.earlyWarning.substr(0,36)));
    row("Restrisiko",      fval(r.residualRiskLevel));
    row("Eskaliert",       r.escalated ? ("Ja → " + r.escalatedTo) : "Nein");
    row("Prüftermin",      fdate(r.reviewDate));
    row("WF-Instanz",      fval(r.workflowInstanceId));
    std::cout << "  +" << std::string(60,'-') << "+\n\n";
}

static void riskDetailMenu(std::shared_ptr<Risk> r) {
    while (true) {
        r->load(r->riskId);
        hdr("RISIKO-AKTE  " + r->riskId.substr(0,22));
        printRisk(*r);

        std::cout << "  1.Beschreibung/Kat.  2.Bewertung (W×A)  3.Reaktionsstrategie\n"
                     "  4.Status ändern      5.Eskalieren        6.Workflow starten\n"
                     "  7.Maßnahme verknüpfen  0.Zurück\n";
        int ch = readInt("Wahl", 0, 7);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string t = readOpt("Neuer Titel (leer=unveränd.): ");
            if (!t.empty()) r->title = t;
            r->description = readOpt("Beschreibung: ");
            r->category    = readOpt("Kategorie (technisch/organisatorisch/extern/...): ");
            r->riskType    = readOpt("Typ (Bedrohung/Chance): ");
            r->triggerCondition = readOpt("Auslösebedingung: ");
            r->earlyWarning    = readOpt("Frühwarnsignal: ");
            r->update();
            std::cout << "  >> Gespeichert.\n";
        }

        else if (ch == 2) {
            // Scoring 1-5 scale
            std::cout << "  Wahrscheinlichkeit 1-5 (1=sehr unwahrscheinlich, 5=fast sicher): ";
            r->probabilityScore = readInt("W", 1, 5);
            std::cout << "  Auswirkung Zeit 1-5: ";
            r->impactScoreTime = readInt("A-Zeit", 1, 5);
            std::cout << "  Auswirkung Kosten 1-5: ";
            r->impactScoreCost = readInt("A-Kosten", 1, 5);
            std::cout << "  Auswirkung Qualität 1-5: ";
            r->impactScoreQuality = readInt("A-Qual.", 1, 5);
            std::cout << "  Auswirkung Scope 1-5: ";
            r->impactScoreScope = readInt("A-Scope", 1, 5);
            r->recalcScore();
            // Set riskLevel based on score
            if (r->overallRiskScore >= 15)      r->riskLevel = "critical";
            else if (r->overallRiskScore >= 9)  r->riskLevel = "high";
            else if (r->overallRiskScore >= 4)  r->riskLevel = "medium";
            else                                r->riskLevel = "low";
            r->update();
            std::cout << "  >> Score: " << r->overallRiskScore
                      << "  Niveau: " << r->riskLevel << "\n";
        }

        else if (ch == 3) {
            std::cout << "  Strategie: 1.vermeiden  2.mindern  3.transferieren  4.akzeptieren\n";
            int s = readInt("Strategie", 1, 4);
            static const char* strats[] = {"avoid","mitigate","transfer","accept"};
            r->responseStrategy = strats[s-1];
            r->contingencyPlan  = readOpt("Notfallplan: ");
            r->residualRiskLevel= readOpt("Restrisiko-Niveau (low/medium/high/critical): ");
            r->update();
            std::cout << "  >> Reaktionsstrategie gespeichert.\n";
        }

        else if (ch == 4) {
            std::cout << "  Status: 1.open  2.mitigated  3.closed  4.accepted  5.transferred\n";
            int s = readInt("Status", 1, 5);
            static const char* stats[] = {"open","mitigated","closed","accepted","transferred"};
            r->status = stats[s-1];
            if (s == 3) r->closedDate = nowIso();
            r->update();
            std::cout << "  >> Status: " << r->status << "\n";
        }

        else if (ch == 5) {
            r->escalatedTo = readLine("Eskaliert an Person-ID: ");
            r->escalated   = true;
            r->update();
            std::cout << "  >> Eskaliert an: " << r->escalatedTo << "\n";
        }

        else if (ch == 6) {
            std::string iid = startWfInstanceWizard("risk", r->riskId);
            if (!iid.empty()) {
                r->workflowInstanceId = iid;
                r->update();
                instanceMenu(iid);
            }
        }

        else if (ch == 7) {
            // Link a measure to this risk
            std::string msId = readLine("Maßnahmen-ID (XV/MSN/...): ");
            auto* db = DatabasePool::instance().get("reporting");
            if (db && !msId.empty()) {
                db->exec("UPDATE measures SET risk_id=? WHERE measure_id=?;",
                         {BindParam::text(r->riskId), BindParam::text(msId)});
                std::cout << "  >> Maßnahme verknüpft.\n";
            }
        }
    }
}

void riskMenu(const std::string& projectId) {
    while (true) {
        auto risks = Risk::loadForProject(projectId);

        hdr("RISIKO-AKTE (RSK)  —  Projekt " + projectId.substr(0,20));
        if (risks.empty()) {
            std::cout << "  (keine Risiken erfasst)\n\n";
        } else {
            std::cout << "  " << std::left
                      << std::setw(5) << "#"
                      << std::setw(26) << "ID"
                      << std::setw(28) << "Titel"
                      << std::setw(12) << "Score"
                      << "Status\n";
            std::cout << "  " << std::string(74,'-') << "\n";
            int n = 1;
            for (auto& r : risks) {
                std::cout << "  " << std::left
                          << std::setw(5)  << n++
                          << std::setw(26) << r->riskId.substr(0,24)
                          << std::setw(28) << r->title.substr(0,26)
                          << std::setw(12) << (std::string(RAG(r->overallRiskScore)) +
                                              " " + std::to_string(r->overallRiskScore))
                          << r->status << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "  1.Neues Risiko  2.Öffnen  3.Hohe Risiken  0.Zurück\n";
        int ch = readInt("Wahl", 0, 3);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string title = readLine("Risikotitel: ");
            if (title.empty()) continue;
            auto r = Risk::create(projectId, title);
            r->identifiedDate = nowIso();
            r->identifiedBy   = readOpt("Erfasst von (Person-ID, leer=system): ");
            if (r->save()) {
                std::cout << "  >> Risiko angelegt: " << r->riskId << "\n";
                riskDetailMenu(r);
            }
        }

        else if (ch == 2) {
            if (risks.empty()) { std::cout << "  Keine Risiken.\n"; continue; }
            int n = readInt("Nummer", 1, (int)risks.size());
            riskDetailMenu(risks[n-1]);
        }

        else if (ch == 3) {
            auto high = Risk::loadHighRisks();
            hdr("HOHE / KRITISCHE RISIKEN (" + std::to_string(high.size()) + ")");
            if (high.empty()) { std::cout << "  (keine hohen Risiken)\n\n"; continue; }
            int n = 1;
            for (auto& r : high)
                std::cout << "  " << n++ << ". " << RAG(r->overallRiskScore)
                          << " " << r->title.substr(0,30)
                          << "  Score=" << r->overallRiskScore << "\n";
            std::string pick = readOpt("Nummer zum Öffnen (leer=zurück): ");
            if (!pick.empty()) {
                int idx = std::stoi(pick)-1;
                if (idx >= 0 && idx < (int)high.size())
                    riskDetailMenu(high[idx]);
            }
        }
    }
}

} // namespace CLI

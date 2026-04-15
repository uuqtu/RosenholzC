// ============================================================
// ChangeRequestMenu.cpp  —  ChangeRequest (AEA) and ChangeObject (CO)
//
// changeRequestMenu() : list/create CRs for a project
// crDetailMenu()      : full CR lifecycle (draft→approved→implemented)
// coMenu()            : manage Change Objects and their workflows
// ============================================================
// ============================================================
// ChangeRequestMenu.cpp — ChangeRequest (AEA) + ChangeObject (CO)
//
// Ein CR aggregiert Änderungen an einem Projekt/Aufgabe und
// durchläuft einen Genehmigungsworkflow. Nach Genehmigung wird
// ein CO (Change Object) als Ausführungs-Workflow angelegt.
// ============================================================
#include "cli_common.h"
#include "../model/ReportingModels.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include <iomanip>

using namespace Rosenholz;

namespace CLI {

// ── Helpers ──────────────────────────────────────────────────
static void printCR(const ChangeRequest& cr) {
    auto row = [&](const std::string& k, const std::string& v) {
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(38) << v << "|\n";
    };
    std::cout << "  +" << std::string(60, '-') << "+\n";
    row("ID",          cr.crId);
    row("Titel",       cr.title);
    row("Typ",         cr.changeType);
    row("Status",      cr.status);
    row("Priorität",   cr.status == "draft" ? "offen" : cr.status);
    row("Projekt",     fval(cr.projectId));
    row("Aufgabe",     fval(cr.taskId));
    row("Gestellt von",fval(cr.raisedBy));
    row("Gestellt am", fdate(cr.raisedDate));
    row("Entschieden", fdate(cr.decisionDate));
    row("Umgesetzt",   fdate(cr.implementedDate));
    row("Kostenauswirkung",   std::to_string((int)cr.costImpact) + " EUR");
    row("Zeitauswirkung",     std::to_string(cr.scheduleImpactDays) + " Tage");
    row("Begründung",  fval(cr.justification.empty() ? "" : cr.justification.substr(0,36)));
    row("Entsch.-Begr.",fval(cr.decisionRationale.empty() ? "" :
                             cr.decisionRationale.substr(0,36)));
    row("WF-Instanz",  fval(cr.workflowInstanceId));
    std::cout << "  +" << std::string(60, '-') << "+\n\n";
}

static void listCRAttachments(const std::string& crId) {
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return;
    auto rows = db->query(
        "SELECT entity_type, entity_id, relationship FROM cr_attachments "
        "WHERE cr_id=? ORDER BY added_at;",
        {BindParam::text(crId)});
    if (rows.empty()) { std::cout << "  (keine Anhänge)\n"; return; }
    std::cout << "  Anhänge:\n";
    for (auto& r : rows)
        std::cout << "    [" << rowGet(r,"relationship") << "] "
                  << rowGet(r,"entity_type") << " → "
                  << rowGet(r,"entity_id") << "\n";
}

static void addCRAttachment(const std::string& crId) {
    std::cout << "  Entitätstyp:  1.Aufgabe(F22)  2.Dokument(DOK)  "
                 "3.Maßnahme(MSN)  4.Vorfall(F18)\n";
    int et = readInt("Typ", 1, 4);
    static const char* etypes[] = {"task","document","measure","incident"};
    std::string eid = readLine("Entitäts-ID: ");
    if (eid.empty()) return;
    std::string rel = readOpt("Beziehung (affected/caused-by/resolves, leer=affected): ");
    if (rel.empty()) rel = "affected";

    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return;
    std::string aid = genId("AEA");
    bool ok = db->exec(
        "INSERT INTO cr_attachments(attachment_id,cr_id,entity_type,entity_id,relationship,added_at)"
        " VALUES(?,?,?,?,?,?);",
        {BindParam::text(aid), BindParam::text(crId),
         BindParam::text(etypes[et-1]), BindParam::text(eid),
         BindParam::text(rel), BindParam::text(nowIso())});
    std::cout << (ok ? "  >> Anhang hinzugefügt.\n" : "  >> Fehler.\n");
}

// ── ChangeObject (CO) Untermenü ───────────────────────────────
static void coMenu(const std::string& crId) {
    auto* db = DatabasePool::instance().get("tracking");
    if (!db) return;

    while (true) {
        auto rows = db->query(
            "SELECT * FROM change_objects WHERE cr_id=? ORDER BY created_at;",
            {BindParam::text(crId)});

        hdr("CHANGE OBJECTS — CR " + crId.substr(0, 20));
        if (rows.empty()) {
            std::cout << "  (kein Change Object angelegt)\n\n";
        } else {
            for (auto& r : rows) {
                std::cout << "  [" << rowGet(r,"status") << "] "
                          << rowGet(r,"co_id").substr(0,22) << "  "
                          << rowGet(r,"title") << "\n";
                auto wfid = rowGet(r,"workflow_instance_id");
                if (!wfid.empty())
                    std::cout << "    WF: " << wfid << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "  1.CO anlegen  2.CO-Workflow starten  3.CO-Status setzen  0.Zurück\n";
        int ch = readInt("Wahl", 0, 3);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string title = readLine("CO-Bezeichnung: ");
            std::string planned = readOpt("Geplantes Datum (JJJJ-MM-TT, leer=offen): ");
            std::string exec_by = readOpt("Ausgeführt von (Person-ID, leer=offen): ");
            std::string coId = genId("CO");
            bool ok = db->exec(
                "INSERT INTO change_objects(co_id,cr_id,title,status,planned_date,executed_by,created_at)"
                " VALUES(?,?,?,?,?,?,?);",
                {BindParam::text(coId), BindParam::text(crId),
                 BindParam::text(title), BindParam::text("planned"),
                 textOrNull(planned), textOrNull(exec_by),
                 BindParam::text(nowIso())});
            if (ok) std::cout << "  >> CO angelegt: " << coId << "\n";
            // Update CR's co_id field
            db->exec("UPDATE change_requests SET co_id=? WHERE cr_id=?;",
                     {BindParam::text(coId), BindParam::text(crId)});
        }

        else if (ch == 2) {
            if (rows.empty()) { std::cout << "  Erst CO anlegen.\n"; continue; }
            std::string coId = rowGet(rows[0], "co_id");
            std::string iid = startWfInstanceWizard("change_object", coId);
            if (!iid.empty()) {
                db->exec("UPDATE change_objects SET workflow_instance_id=? WHERE co_id=?;",
                         {BindParam::text(iid), BindParam::text(coId)});
                instanceMenu(iid);
            }
        }

        else if (ch == 3) {
            if (rows.empty()) { std::cout << "  Kein CO vorhanden.\n"; continue; }
            std::string coId = rowGet(rows[0], "co_id");
            std::cout << "  Status: 1.planned  2.in-progress  3.completed  4.cancelled\n";
            int s = readInt("Status", 1, 4);
            static const char* stats[] = {"planned","in-progress","completed","cancelled"};
            std::string actual = (s == 3) ? nowIso() : "";
            db->exec("UPDATE change_objects SET status=?,actual_date=? WHERE co_id=?;",
                     {BindParam::text(stats[s-1]), textOrNull(actual),
                      BindParam::text(coId)});
            std::cout << "  >> Status gesetzt: " << stats[s-1] << "\n";
        }
    }
}

// ── CR Detail-Menü ────────────────────────────────────────────
static void crDetailMenu(std::shared_ptr<ChangeRequest> cr) {
    while (true) {
        cr->load(cr->crId);  // refresh
        hdr("ÄNDERUNGSANTRAG  " + cr->crId.substr(0,20));
        printCR(*cr);
        listCRAttachments(cr->crId);
        std::cout << "\n";
        std::cout << "  1.Titel/Typ bearb.  2.Beschreibung/Begr.  3.Auswirkungen\n"
                     "  4.Status ändern     5.Genehmigen          6.Ablehnen\n"
                     "  7.Anhang hinzufügen 8.Workflow starten     9.Change Objects\n"
                     "  0.Zurück\n";
        int ch = readInt("Wahl", 0, 9);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string t = readOpt("Neuer Titel (leer = unverändert): ");
            if (!t.empty()) cr->title = t;
            std::cout << "  Typ (general/scope/schedule/budget/technical/regulatory): ";
            std::string typ = readOpt("");
            if (!typ.empty()) cr->changeType = typ;
            cr->update();
            std::cout << "  >> Gespeichert.\n";
        }

        else if (ch == 2) {
            std::cout << "  Beschreibung: "; cr->description = readLine("");
            std::cout << "  Begründung:   "; cr->justification = readLine("");
            cr->update();
            std::cout << "  >> Gespeichert.\n";
        }

        else if (ch == 3) {
            std::string cv = readOpt("Kostenauswirkung EUR (leer=0): ");
            if (!cv.empty()) try { cr->costImpact = std::stod(cv); } catch(...) {}
            std::string sv = readOpt("Zeitauswirkung Tage (leer=0): ");
            if (!sv.empty()) try { cr->scheduleImpactDays = std::stoi(sv); } catch(...) {}
            cr->scopeImpact   = readOpt("Scope-Auswirkung: ");
            cr->qualityImpact = readOpt("Qualitäts-Auswirkung: ");
            cr->update();
            std::cout << "  >> Auswirkungen gespeichert.\n";
        }

        else if (ch == 4) {
            std::cout << "  Status:\n"
                         "    1.draft  2.submitted  3.under-review  "
                         "4.approved  5.rejected\n"
                         "    6.implemented  7.withdrawn\n";
            int s = readInt("Status", 1, 7);
            static const char* stats[] = {
                "draft","submitted","under-review","approved",
                "rejected","implemented","withdrawn"
            };
            cr->status = stats[s-1];
            if (s == 4) cr->decisionDate = nowIso();
            if (s == 6) cr->implementedDate = nowIso();
            cr->update();
            std::cout << "  >> Status: " << cr->status << "\n";
        }

        else if (ch == 5) {
            std::string rat = readOpt("Genehmigungsbegründung: ");
            if (cr->approve(rat)) std::cout << "  >> Genehmigt.\n";
            else                  std::cout << "  >> Fehler.\n";
        }

        else if (ch == 6) {
            std::string rat = readOpt("Ablehnungsbegründung: ");
            if (cr->reject(rat)) std::cout << "  >> Abgelehnt.\n";
            else                 std::cout << "  >> Fehler.\n";
        }

        else if (ch == 7) {
            addCRAttachment(cr->crId);
        }

        else if (ch == 8) {
            std::string iid = startWfInstanceWizard("change_request", cr->crId);
            if (!iid.empty()) {
                cr->workflowInstanceId = iid;
                cr->update();
                instanceMenu(iid);
            }
        }

        else if (ch == 9) {
            coMenu(cr->crId);
        }
    }
}

// ── Hauptmenü ChangeRequest ───────────────────────────────────
void changeRequestMenu(const std::string& projectId) {
    while (true) {
        auto crs = ChangeRequest::loadForProject(projectId);

        hdr("ÄNDERUNGSANTRÄGE (AEA)  —  Projekt " + projectId.substr(0,20));
        if (crs.empty()) {
            std::cout << "  (keine Änderungsanträge)\n\n";
        } else {
            std::cout << "  " << std::left
                      << std::setw(5)  << "#"
                      << std::setw(26) << "ID"
                      << std::setw(28) << "Titel"
                      << std::setw(14) << "Status"
                      << "Typ\n";
            std::cout << "  " << std::string(76, '-') << "\n";
            int n = 1;
            for (auto& cr : crs) {
                std::cout << "  " << std::left
                          << std::setw(5)  << n++
                          << std::setw(26) << cr->crId.substr(0,24)
                          << std::setw(28) << cr->title.substr(0,26)
                          << std::setw(14) << cr->status
                          << cr->changeType << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "  1.Neu anlegen  2.Öffnen  3.Alle offenen CRs  0.Zurück\n";
        int ch = readInt("Wahl", 0, 3);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string title = readLine("CR-Titel: ");
            if (title.empty()) continue;
            std::cout << "  Typ (general/scope/schedule/budget/technical/regulatory): ";
            std::string typ = readOpt("");
            if (typ.empty()) typ = "general";
            auto cr = ChangeRequest::create(projectId, title, typ);
            cr->description  = readOpt("Beschreibung (optional): ");
            cr->justification= readOpt("Begründung (optional): ");
            cr->raisedBy     = readOpt("Gestellt von (Person-ID, leer=system): ");
            cr->raisedDate   = nowIso();
            if (cr->save()) {
                std::cout << "  >> CR angelegt: " << cr->crId << "\n";
                crDetailMenu(cr);
            } else {
                std::cout << "  >> Fehler beim Speichern.\n";
            }
        }

        else if (ch == 2) {
            if (crs.empty()) { std::cout << "  Keine CRs vorhanden.\n"; continue; }
            int n = readInt("Nummer", 1, (int)crs.size());
            crDetailMenu(crs[n-1]);
        }

        else if (ch == 3) {
            auto open = ChangeRequest::loadOpen();
            hdr("ALLE OFFENEN CRs (" + std::to_string(open.size()) + ")");
            if (open.empty()) { std::cout << "  (keine)\n\n"; continue; }
            int n = 1;
            for (auto& cr : open)
                std::cout << "  " << n++ << ". [" << cr->status << "] "
                          << cr->title.substr(0,30) << "  " << cr->projectId << "\n";
            std::string pick = readOpt("Nummer zum Öffnen (leer=zurück): ");
            if (!pick.empty()) {
                int idx = std::stoi(pick)-1;
                if (idx >= 0 && idx < (int)open.size())
                    crDetailMenu(open[idx]);
            }
        }
    }
}

} // namespace CLI

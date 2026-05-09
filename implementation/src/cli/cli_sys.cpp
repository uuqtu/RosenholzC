// ============================================================
// cli_sys.cpp  —  System-Befehle: status, backup, mfs, log, search
//
// Public functions:
//   cmdStatus()            — show DB record counts and paths
//   cmdBackup()            — run full database + MFS backup
//   cmdMfs(args)           — rebuild MFS tree or write single entity
//   cmdLog(level)          — set log verbosity at runtime
//   cmdSearch(query)       — global search wrapper
//   globalSearch(query)    — search across all entity types
// ============================================================
#include "cli_common.h"
#include "../core/Stats.h"
#include "../model/WatchPoller.h"
#include "../model/akt/FolderObject.h"
#include "../model/akt/Folder.h"
#include "../model/akt/FolderRevision.h"
using Rosenholz::RevState;
#include "../core/Config.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/BackupManager.h"
#include "../mfs/MFSWriter.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/f18/F18Operation.h"
#include "../model/akt/Folder.h"
#include "../workflow/F77Workflow.h"
#include <algorithm>
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── -status ───────────────────────────────────────────────────

void cmdStatus() {
    auto& cfg = Config::instance();
    auto c = Rosenholz::Stats::load();
    auto row = [](const std::string& label, int n, const std::string& unit) {
        std::cout << "  " << std::left << std::setw(18) << label
                  << std::setw(6) << n << "  " << unit << "\n";
    };
    std::cout << "\n  Rosenholz PM v7 — Datenbankzaehler\n"
              << "  " << cfg.basePath() << "\n"
              << "  " << std::string(42, '-') << "\n";
    row("F16-Karten",    c.f16,       "Projektkarten");
    row("F22-Vorgaenge", c.f22,       "Aufgaben");
    row("F18-Ops",       c.f18,       "Vorgaenge");
    row("Akten",         c.akt,       "Dokumente");
    row("F77-Workflows", c.f77Active, "aktive Workflows");
    row("F77-Tasks",     c.f77Tasks,  "offene Aufgaben");
    row("Notizen",       c.notes,     "Notizen");
    std::cout << "\n";
}

// ── -log ──────────────────────────────────────────────────────

void cmdLog(const std::string& level) {
    std::string l = level;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    if      (l == "debug") Logger::instance().setLevel(LogLevel::DEBUG);
    else if (l == "info")  Logger::instance().setLevel(LogLevel::INFO);
    else if (l == "warn")  Logger::instance().setLevel(LogLevel::WARN);
    else if (l == "error" || l == "err") Logger::instance().setLevel(LogLevel::ERR);
    else { printErr("Unbekannter Log-Level: " + level + "  (debug|info|warn|error)"); return; }
    printOk("  Log-Level: " + level);
}

// ── -backup ───────────────────────────────────────────────────

void cmdBackup() {
    auto& cfg = Config::instance();
    std::cout << "  Backup laeuft...\n";
    int n = BackupManager::backupDatabases(cfg.basePath(), cfg.backup().backupPath, false);
    printOk("  >> " + std::to_string(n) + " Datenbank(en) gesichert.");
    if (BackupManager::backupMFS(cfg.mfsPath(), cfg.backup().backupPath))
        printOk("  >> MFS gesichert.");
}

// ── -mfs ──────────────────────────────────────────────────────

void cmdMfs(const std::vector<std::string>& args) {
    auto& cfg = Config::instance();
    std::string root = cfg.mfsPath();

    if (args.empty()) {
        std::cout << "  MFS-Baum aufbauen unter: " << root << "\n";
        bool ok = MFSWriter::rebuildAll(root);
        printOk(ok ? "  >> MFS-Baum vollstaendig geschrieben." : "  >> Fehler beim MFS-Aufbau.");
        return;
    }

    if (!isId(args[0])) { printErr("Ungültiges Argument: " + args[0] + "  (erwartet ID)"); return; }
    const std::string& id = args[0];

    if (auto p = F16::loadById(id)) {
        bool ok = MFSWriter::writeProject(*p, root);
        for (auto& t : F22::loadForProject(p->projectId))
            ok &= MFSWriter::writeTask(*t, root);
        for (auto& t2 : F22::loadForProject(p->projectId))
            for (auto& v : F18Operation::loadForTask(t2->taskId))
                ok &= MFSWriter::writeF18(*v, root);
        for (auto& d : Folder::loadForEntity("f22", p->projectId))
            ok &= MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> F16 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto t = F22::loadById(id)) {
        bool ok = MFSWriter::writeTask(*t, root);
        for (auto& d : Folder::loadForEntity("f22", id))
            ok &= MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> F22 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto v = F18Operation::loadById(id)) {
        bool ok = MFSWriter::writeF18(*v, root);
        for (auto& d : Folder::loadForEntity("f18", id))
            ok &= MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> F18 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto d = Folder::loadById(id)) {
        bool ok = MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> AKT " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto wf = F77W::loadById(id)) {
        bool ok = MFSWriter::writeF77(wf->workflowId, wf->entityType, wf->templateName, root);
        printOk(ok ? "  >> F77 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    printErr("ID nicht gefunden: " + id); return;
}

// ── cmdSearch ─────────────────────────────────────────────────
//
// Thin wrapper called from dispatch. Joins all remaining args
// into one query string, then delegates to globalSearch.

void cmdSearch(const std::string& query) {
    if (query.empty()) {
        printErr("-search benoetigt einen Suchbegriff"); return;
    }
    globalSearch(query);
}

// ── globalSearch ──────────────────────────────────────────────
//
// Search across all entity types by case-insensitive substring.
// Groups results by type, displays them numbered, then offers
// to jump directly to any found entity's interactive menu.

void globalSearch(const std::string& query) {
    using namespace Rosenholz;
    if (query.empty()) return;

    // matchesPattern: * = any chars, % = exactly one char
    auto match = [&](const std::string& s) {
        return CLI::matchesPattern(s, query);
    };

    hdr("GLOBALE SUCHE: \"" + query + "\"");

    struct Hit { std::string typeCode; std::string id; std::string label; std::string sub; };
    std::vector<Hit> hits;

    // F16 Projects
    auto projs = F16::loadRecent(200);
    for (auto& p : projs)
        if (match(p->title) || match(p->projectId) || match(p->codename))
            hits.push_back({"F16", p->projectId, p->title, p->archived ? "archiviert" : "aktiv"});

    // F22 Tasks
    auto tasks = F22::loadRecent(200);
    for (auto& t : tasks)
        if (match(t->title) || match(t->taskId))
            hits.push_back({"F22", t->taskId, t->title, entityStatusToString(t->status)});

    // F18 Workflows (all types)
    auto vorgaenge = Rosenholz::F18Operation::loadRecent(200);
    for (auto& v : vorgaenge)
        if (match(v->title) || match(v->operationId))
            hits.push_back({"F18", v->operationId, v->title,
                            v->operationType + "|" + std::string(entityStatusToString(v->status))});

    // DOK Documents
    auto docs = Folder::loadRecent(200);
    for (auto& d : docs)
        if (match(d->title) || match(d->folderId) || match(d->tags))
            hits.push_back({"AKT", d->folderId, d->title, "v"+d->version});

    // F77 Workflows (active)
    auto wfis = F77W::loadActive();
    for (auto& w : wfis)
        if (match(w->templateName) || match(w->workflowId))
            hits.push_back({"F77", w->workflowId, w->templateName, std::string(toString(w->status))});

    if (hits.empty()) {
        std::cout << "  (keine Treffer für \"" << query << "\")\n";
        return;
    }

    // Display
    int n = 1;
    for (auto& h : hits) {
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << "[" << std::left << std::setw(4) << h.typeCode << "] "
                  << std::setw(26) << h.id.substr(0,24)
                  << "  " << std::setw(28) << h.label.substr(0,26)
                  << "  " << h.sub.substr(0,14) << "\n";
    }
    std::cout << "\n  " << hits.size() << " Treffer gefunden.\n";

    // Jump to entity
    std::cout << "  Nummer zum Öffnen (leer=zurück): ";
    std::string pick; std::getline(std::cin, pick);
    if (pick.empty()) return;

    int idx = 0;
    try { idx = std::stoi(pick)-1; } catch(...) { return; }
    if (idx < 0 || idx >= (int)hits.size()) return;

    auto& h = hits[idx];
    if (h.typeCode == "F16") {
        auto p = F16::loadById(h.id);
        if (p) { projectMenu(p); }
    } else if (h.typeCode == "F22") {
        auto t = F22::loadById(h.id);
        if (t) taskMenu(t);
    } else if (h.typeCode == "F18") {
        auto v = Rosenholz::F18Operation::loadById(h.id);
        if (v) f18Menu(v);
    } else if (h.typeCode == "AKT") {
        auto d = Folder::loadById(h.id);
        if (d) documentMenu(d, "");
    } else if (h.typeCode == "WFI") {
        instanceMenu(h.id);
    }
}


// ── indexDokFolders ───────────────────────────────────────────────────────────
// Scans all revision MFS folders for files not yet registered as FolderObjects.
// For each unregistered file found in an in_work revision, the user is asked
// whether to add it to the document.
//
// Import modes:
//   A. Titel eingeben → ID automatisch vergeben  (normal docObj ID)
//   B. ID manuell eingeben → Titel eingeben       (normal docObj ID, custom display)
//   C. Originaldateiname verwenden               (no ID — filename IS the identifier)
// ─────────────────────────────────────────────────────────────────────────────
void cmdIndexDokFolders() {
    using namespace Rosenholz;
    hdr("INDEX AKT-ORDNER — Nicht registrierte Dateien suchen");

    // Load all documents
    auto docs = Folder::loadRecent(500);
    if (docs.empty()) {
        std::cout << "  (keine Akten vorhanden)\n";
        return;
    }

    int totalFound = 0, totalAdded = 0;

    for (auto& doc : docs) {
        auto candidates = doc->indexMfsFolders();
        if (candidates.empty()) continue;

        for (auto& [revNum, filePath] : candidates) {
            // Load the revision to check its state
            auto rev = FolderRevision::loadByRev(doc->folderId, revNum);
            if (!rev) continue;

            std::cout << "\n  Dokument : " << doc->folderId << "  "" << doc->title << ""\n"
                      << "  Revision : " << revNum << "  [" << rev->revState << "]\n"
                      << "  Datei    : " << filePath << "\n";
            totalFound++;

            if (rev->revState != RevState::IN_WORK) {
                std::cout << "  (Revision ist nicht in_work — übersprungen)\n";
                continue;
            }

            if (!yesno("  Zum Dokument hinzufügen?")) continue;

            // Choose import mode
            std::cout << "  Importmodus:\n"
                      << "    A. Titel eingeben, ID automatisch vergeben (Standard)\n"
                      << "    B. Titel eingeben, ID manuell eingeben\n"
                      << "    C. Originaldateiname als Bezeichner verwenden (keine ID)\n";
            std::string mode = readOpt("Modus (A/B/C, leer=A): ");
            if (mode.empty()) mode = "A";
            for (char& c : mode) c = std::toupper(c);

            OperationResult res = OperationResult::OPERATION_ACK;

            if (mode == "C") {
                // Use original filename — no object ID, import as-is
                auto obj = FolderObject::importFile(doc->folderId, revNum, filePath, res);
                if (opOk(res) && obj) {
                    FolderObject::writeKeyFile(doc->folderId, revNum, doc->title);
                    std::cout << "  >> Importiert (Dateiname): " << obj->displayName() << "\n";
                    totalAdded++;
                } else {
                    std::cout << "  >> " << opResultMessage(res) << "\n";
                }
            } else {
                // Modes A and B: generate normal document object ID
                std::string title;
                std::string customId;

                if (mode == "A") {
                    title = readLine("  Bezeichnung/Titel: ");
                } else { // B
                    title   = readLine("  Bezeichnung/Titel: ");
                    customId = readOpt("  Objekt-ID (5 Zeichen, leer=automatisch): ");
                    if (customId.size() > 5) customId = customId.substr(0, 5);
                    for (char& c : customId) c = std::toupper(c);
                }

                auto obj = FolderObject::importFile(doc->folderId, revNum, filePath, res);
                if (opOk(res) && obj) {
                    if (!title.empty())    obj->originalName = title;
                    if (!customId.empty()) {
                        // Override objectId (stored as docId + ":" + customId)
                        obj->objectId = doc->folderId + ":" + customId;
                    }
                    opOk(obj->update());
                    FolderObject::writeKeyFile(doc->folderId, revNum, doc->title);
                    std::cout << "  >> Importiert: " << obj->objectId
                              << "  "" << obj->displayName() << ""\n";
                    totalAdded++;
                } else {
                    std::cout << "  >> " << opResultMessage(res) << "\n";
                }
            }
        }
    }

    std::cout << "\n  Gesamt: " << totalFound << " Datei(en) gefunden, "
              << totalAdded << " hinzugefügt.\n";
}


void cmdHist() {
    auto hist = Rosenholz::HistoryLog::instance().recent(20);
    if (hist.empty()) { std::cout << "  (kein Verlauf)\n"; return; }
    std::cout << "\n  Zuletzt geoeffnet:\n";
    for (int i = 0; i < (int)hist.size(); i++) {
        auto& r = hist[i];
        std::cout << "  " << std::setw(3) << (i+1) << ". "
                  << std::left << std::setw(6) << Rosenholz::entityTypeLabel(r.type)
                  << "  " << std::setw(28) << r.id
                  << "  " << r.displayName.substr(0,30) << "\n";
    }
    std::cout << "\n  cd <ID>  zum Navigieren\n\n";
}

void cmdGo(const std::vector<std::string>& args) {
    if (args.empty()) { CLI::printErr("go <ID>"); return; }
    CLI::cmdCd(args);
}
} // namespace CLI

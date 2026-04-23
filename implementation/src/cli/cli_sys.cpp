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
#include "../core/Config.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/BackupManager.h"
#include "../mfs/MFSWriter.h"
#include "../model/f16/ProjectF16.h"
#include "../model/f22/TaskF22.h"
#include "../model/f18/F18Operation.h"
#include "../model/dok/Document.h"
#include "../workflow/F77Workflow.h"
#include <algorithm>
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── -status ───────────────────────────────────────────────────

void cmdStatus() {
    auto& cfg = Config::instance();
    auto row = [](const std::string& label, int n, const std::string& unit) {
        std::cout << "  " << std::left << std::setw(18) << label
                  << std::setw(6) << n << "  " << unit << "\n";
    };
    std::cout << "\n  Rosenholz PM  —  System-Status\n";
    std::cout << "  " << std::string(42,'-') << "\n";
    std::cout << "  Basispfad : " << cfg.basePath() << "\n\n";

    if (auto* db = DatabasePool::instance().get("f16"))
        row("F16 Projekte:", db->rowCount("projects"), "Projekte");
    if (auto* db = DatabasePool::instance().get("f22"))
        row("F22 Aufgaben:", db->rowCount("tasks"), "Aufgaben");
    if (auto* db = DatabasePool::instance().get("f18")) {
        row("F18 Ops:",       db->rowCount("f18_operations"),     "Operationen");
        row("F18 Schritte:",  db->rowCount("f18_operation_steps"),"Schritte");
        row("Comms:",         db->rowCount("communications"),     "Communications");
    }
    if (auto* db = DatabasePool::instance().get("f77")) {
        row("F77 Workflows:", db->rowCount("f77_workflows"),          "laufend");
        row("F77 Templates:", db->rowCount("f77_workflow_templates"), "Vorlagen");
    }
    if (auto* db = DatabasePool::instance().get("dok"))
        row("Dokumente:",     db->rowCount("documents"), "Dokumente");
    if (auto* db = DatabasePool::instance().get("core")) {
        row("Personen:",      db->rowCount("persons"),  "Personen");
        row("Teams:",         db->rowCount("teams"),    "Diensteinheiten");
    }
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
    else die("Unbekannter Log-Level: " + level + "  (debug|info|warn|error)");
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

    if (!isId(args[0])) die("Ungültiges Argument: " + args[0] + "  (erwartet ID)");
    const std::string& id = args[0];

    if (auto p = ProjectF16::loadById(id)) {
        bool ok = MFSWriter::writeProject(*p, root);
        for (auto& t : TaskF22::loadForProject(p->projectId))
            ok &= MFSWriter::writeTask(*t, root);
        for (auto& v : F18Operation::loadForProject(p->projectId))
            ok &= MFSWriter::writeF18(*v, root);
        for (auto& d : Document::loadForProject(p->projectId))
            ok &= MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> F16 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto t = TaskF22::loadById(id)) {
        bool ok = MFSWriter::writeTask(*t, root);
        for (auto& d : Document::loadForEntity("f22", id))
            ok &= MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> F22 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto v = F18Operation::loadById(id)) {
        bool ok = MFSWriter::writeF18(*v, root);
        for (auto& d : Document::loadForEntity("f18", id))
            ok &= MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> F18 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto d = Document::loadById(id)) {
        bool ok = MFSWriter::writeDocument(*d, root);
        printOk(ok ? "  >> DOK " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    if (auto wf = F77_Workflow::loadById(id)) {
        bool ok = MFSWriter::writeF77(wf->workflowId, wf->entityType, wf->templateName, root);
        printOk(ok ? "  >> F77 " + id + " geschrieben." : "  >> Fehler.");
        return;
    }
    die("ID nicht gefunden: " + id);
}

// ── cmdSearch ─────────────────────────────────────────────────
//
// Thin wrapper called from dispatch. Joins all remaining args
// into one query string, then delegates to globalSearch.

void cmdSearch(const std::string& query) {
    if (query.empty()) {
        die("-search benoetigt einen Suchbegriff");
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

    std::string lq = query;
    for (char& c : lq) c = (char)std::tolower(c);

    auto match = [&](const std::string& s) {
        std::string ls = s;
        for (char& c : ls) c = (char)std::tolower(c);
        return ls.find(lq) != std::string::npos;
    };

    hdr("GLOBALE SUCHE: \"" + query + "\"");

    struct Hit { std::string typeCode; std::string id; std::string label; std::string sub; };
    std::vector<Hit> hits;

    // F16 Projects
    auto projs = ProjectF16::loadRecent(200);
    for (auto& p : projs)
        if (match(p->title) || match(p->projectId) || match(p->codename))
            hits.push_back({"F16", p->projectId, p->title, p->status});

    // F22 Tasks
    auto tasks = TaskF22::loadRecent(200);
    for (auto& t : tasks)
        if (match(t->title) || match(t->taskId))
            hits.push_back({"F22", t->taskId, t->title, t->status});

    // F18 Workflows (all types)
    auto vorgaenge = Rosenholz::F18Operation::loadRecent(200);
    for (auto& v : vorgaenge)
        if (match(v->title) || match(v->vorgangId))
            hits.push_back({"F18", v->vorgangId, v->title,
                            v->vorgangType + "|" + v->status});

    // DOK Documents
    auto docs = Document::loadRecent(200);
    for (auto& d : docs)
        if (match(d->title) || match(d->documentId) || match(d->tags))
            hits.push_back({"DOK", d->documentId, d->title, "v"+d->version});

    // F77 Workflows (active)
    auto wfis = F77_Workflow::loadActive();
    for (auto& w : wfis)
        if (match(w->templateName) || match(w->workflowId))
            hits.push_back({"F77", w->workflowId, w->templateName, w->status});

    if (hits.empty()) {
        std::cout << "  (keine Treffer für \"" << query << "\")\n\n";
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
        auto p = ProjectF16::loadById(h.id);
        if (p) { p->loadQTCSLinks(); projectMenu(p); }
    } else if (h.typeCode == "F22") {
        auto t = TaskF22::loadById(h.id);
        if (t) taskMenu(t);
    } else if (h.typeCode == "F18") {
        auto v = Rosenholz::F18Operation::loadById(h.id);
        if (v) f18Menu(v);
    } else if (h.typeCode == "DOK") {
        auto d = Document::loadById(h.id);
        if (d) documentMenu(d);
    } else if (h.typeCode == "WFI") {
        instanceMenu(h.id);
    }
}

} // namespace CLI

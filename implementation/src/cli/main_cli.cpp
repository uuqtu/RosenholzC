// ============================================================
// main_cli.cpp  —  Interactive CLI entry point and main loop
//
// run() drives the top-level menu loop (options 0–17)
// Startup: AppController::init → MigrationEngine::runAll
//          → WorkflowEngine::createStandardTemplates
// Clean shutdown via AppController::shutdown()
// ============================================================
// ============================================================
// main_cli.cpp  —  Interactive CLI entry point
// ============================================================
#include "cli_common.h"
#include "../app/AppController.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include "../core/Migration.h"
#include "../mfs/MFSWriter.h"
#include "../core/BackupManager.h"
#include "../workflow/WorkflowEngine.h"
using namespace Rosenholz;
#include <cstring>

namespace CLI {
void run() {
    hdr("ROSENHOLZ PM  —  INTERACTIVE CONSOLE");
    std::cout << "\n"
              << "  Welcome to the Rosenholz PM interactive shell.\n"
              << "  Enter a number to choose an action.\n"
              << "  All data is stored in your configured base path.\n\n";

    while (true) {
        std::cout << "\n  MAIN MENU\n";
        hr();
        std::cout << "  PROJECTS (F16)\n"
                  << "    1.  List all projects\n"
                  << "    2.  Create new project\n"
                  << "    3.  Open project by number\n"
                  << "    4.  Open project by ID\n"
                  << "\n  TASKS (F22)\n"
                  << "    5.  Open task by ID\n"
                  << "\n  PERSONS\n"
                  << "    6.  List all persons\n"
                  << "    7.  Create new person\n"
                  << "\n  DOCUMENTS\n"
                  << "    8.  Create / register document\n"
                  << "    9.  Browse all documents\n"
                  << "   10.  Archive URL as document\n"
                  << "\n  WORKFLOWS\n"
                  << "   11.  Workflow browser\n"
                  << "\n  SYSTEM\n"
                  << "   12.  Rebuild MFS tree\n"
                  << "   13.  Run backup now\n"
                  << "   14.  Show config / status\n"
                  << "   15.  ID abbreviation table\n"
                  << "   16.  Set log verbosity\n"
                  << "   17.  Diensteinheiten (Teams)\n"
                  << "\n    0.  Exit\n";
        hr();

        int ch = readInt("Choice", 0, 17);

        if (ch == 0) {
            std::cout << "\n  Auf Wiedersehen.\n\n";
            break;
        }

        // ── LIST PROJECTS ────────────────────────────────────
        else if (ch == 1) {
            listProjects();
        }

        // ── CREATE PROJECT ───────────────────────────────────
        else if (ch == 2) {
            auto p = createProjectWizard();
            if (p) {
                std::cout << "  Open the new project now? (y/n): ";
                std::string ans; std::getline(std::cin, ans);
                if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y'))
                    projectMenu(p);
            }
        }

        // ── OPEN PROJECT BY NUMBER ────────────────────────────
        else if (ch == 3) {
            auto all = Rosenholz::ProjectF16::loadAll();
            if (all.empty()) { std::cout << "\n  (no projects)\n"; continue; }
            listProjects();
            int n = readInt("Project number", 1, (int)all.size());
            auto p = all[n-1];
            p->loadQTCSLinks();
            projectMenu(p);
        }

        // ── OPEN PROJECT BY ID ────────────────────────────────
        else if (ch == 4) {
            std::string id = readLine("Project ID: ");
            auto p = Rosenholz::ProjectF16::loadById(id);
            if (!p) { std::cout << "  >> Not found.\n"; continue; }
            p->loadQTCSLinks();
            projectMenu(p);
        }

        // ── OPEN TASK BY ID ───────────────────────────────────
        else if (ch == 5) {
            std::string id = readLine("Task ID: ");
            auto t = Rosenholz::TaskF22::loadById(id);
            if (!t) { std::cout << "  >> Not found.\n"; continue; }
            taskMenu(t);
        }

        // ── LIST PERSONS ──────────────────────────────────────
        else if (ch == 6) {
            listPersons();
        }

        // ── CREATE PERSON ─────────────────────────────────────
        else if (ch == 7) {
            createPersonWizard();
        }

        // ── CREATE DOCUMENT ──────────────────────────────────
        else if (ch == 8) {
            createDocumentWizard();
        }

        // ── BROWSE ALL DOCUMENTS ──────────────────────────────
        else if (ch == 9) {
            documentBrowserMenu();
        }

        // ── ARCHIVE URL ───────────────────────────────────────
        else if (ch == 10) {
            std::string url = readLine("URL to archive: ");
            std::string pid = readOpt("Attach to project-ID (optional): ");
            std::cout << "  >> Downloading and archiving...\n";
            auto doc = Rosenholz::Document::archiveFromUrl(url, pid);
            if (doc) {
                std::cout << "  >> Archived: " << doc->title << "\n"
                          << "     Path   : " << doc->filePath << "\n";
                if (!pid.empty()) doc->attachToEntity("project", pid);
                auto& cfg = Rosenholz::Config::instance();
                if (cfg.mfs().enabled) Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
                std::cout << "  Open document now? (y/n): ";
                std::string ans; std::getline(std::cin, ans);
                if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y'))
                    documentMenu(doc);
            } else {
                std::cout << "  >> Archive failed (check network / URL).\n";
            }
        }

        // ── WORKFLOW BROWSER ──────────────────────────────────
        else if (ch == 11) {
            workflowMenu();
        }

        // ── REBUILD MFS ───────────────────────────────────────
        else if (ch == 12) {
            auto& cfg = Rosenholz::Config::instance();
            std::cout << "  Rebuilding MFS tree at " << cfg.mfsPath() << " ...\n";
            bool ok = Rosenholz::MFSWriter::rebuildAll(cfg.mfsPath());
            std::cout << "  >> " << (ok ? "Done." : "FAILED.") << "\n";
        }

        // ── BACKUP ────────────────────────────────────────────
        else if (ch == 13) {
            auto& cfg = Rosenholz::Config::instance();
            std::cout << "  Running backup to " << cfg.backupDestPath() << " ...\n";
            bool ok = Rosenholz::BackupManager::runFull(
                cfg.basePath(), cfg.backupDestPath(), cfg.backup().maxCopies);
            std::cout << "  >> " << (ok ? "Backup complete." : "Backup FAILED.") << "\n";
        }

        // ── STATUS ────────────────────────────────────────────
        else if (ch == 14) {
            Rosenholz::AppController::instance().printStatus();
            auto* db = Rosenholz::DatabasePool::instance().get("projects");
            if (db) {
                std::cout << "\n  DB row counts:\n"
                          << "    projects : " << db->rowCount("projects")  << "\n"
                          << "    tasks    : " << db->rowCount("tasks")     << "\n"
                          << "    incidents: " << db->rowCount("incidents") << "\n";
            }
            auto* cdb = Rosenholz::DatabasePool::instance().get("core");
            if (cdb) {
                std::cout << "    persons  : " << cdb->rowCount("persons")      << "\n"
                          << "    teams    : " << cdb->rowCount("teams")        << "\n"
                          << "    members  : " << cdb->rowCount("team_members") << "\n";
            }
            auto* ddb = Rosenholz::DatabasePool::instance().get("documents");
            if (ddb) {
                std::cout << "    documents: " << ddb->rowCount("documents")     << "\n"
                          << "    attached : " << ddb->rowCount("entity_documents") << "\n";
            }
            auto* tdb = Rosenholz::DatabasePool::instance().get("tracking");
            if (tdb) {
                std::cout << "    CRs (AEA) : " << tdb->rowCount("change_requests") << "\n"
                          << "    COs       : " << tdb->rowCount("change_objects") << "\n";
            }
            auto* rdb = Rosenholz::DatabasePool::instance().get("reporting");
            if (rdb) {
                std::cout << "    Risiken   : " << rdb->rowCount("risks") << "\n"
                          << "    Massnahmen: " << rdb->rowCount("measures") << "\n"
                          << "    QT-Tore   : " << rdb->rowCount("quality_gates") << "\n"
                          << "    LE        : " << rdb->rowCount("lessons_learned") << "\n"
                          << "    ENT-Log   : " << rdb->rowCount("decision_log") << "\n";
            }
            auto* prdb = Rosenholz::DatabasePool::instance().get("projects");
            if (prdb) {
                std::cout << "    Meilensteine: " << prdb->rowCount("milestones") << "\n"
                          << "    Besprechungen: " << prdb->rowCount("meetings") << "\n";
            }
            auto* wfdb = Rosenholz::DatabasePool::instance().get("workflow");
            if (wfdb) {
                std::cout << "    WF-Instanzen: " << wfdb->rowCount("workflow_instances") << "\n"
                          << "    WF-Actions  : " << wfdb->rowCount("workflow_actions") << "\n";
            }
        }

        // ── ID ABBREVIATION TABLE ─────────────────────────────────
        else if (ch == 15) {
            hdr("ID-Abkuerzungsverzeichnis (DDR-Stil)");
            auto& de = Rosenholz::Config::instance().registratur().diensteinheitKuerzel;
            std::cout << "  Aktuelle Diensteinheit : " << de << "\n\n";
            std::cout << "  " << std::left
                      << std::setw(10) << "Kuerzel"
                      << std::setw(14) << "MFS-Ordner"
                      << "Bezeichnung\n";
            hr();
            struct {const char* code; const char* folder; const char* name;} abbrevs[] = {
                {"F16", "F16/",  "Vorgangskartei (Projekt)"},
                {"F22", "F22/",  "Aufgabenkartei (Untervorgang)"},
                {"F18", "F18/",  "Vorfallkartei"},
                {"RSK", "RSK/",  "Risiko-Akte"},
                {"MSN", "MSN/",  "Massnahmen-Akte"},
                {"QT",  "QT/",   "Qualitaetstor"},
                {"KPI", "KPI/",  "Kennzahl (Key Performance Indicator)"},
                {"LE",  "LE/",   "Lernerkenntnis (Lessons Learned)"},
                {"ENT", "ENT/",  "Entscheidungsprotokoll"},
                {"AEA", "AEA/",  "Aenderungsantrag"},
                {"ABE", "ABE/",  "Annahmen und Beschraenkungen"},
                {"BSP", "BSP/",  "Besprechungsprotokoll"},
                {"MEI", "MEI/",  "Meilensteinblatt"},
                {"DOK", "DOK/",  "Dokument (allgemein)"},
                {"PER", "PER/",  "Personenkartei"},
                {"DE",  "DE/",   "Diensteinheit / Team"},
                {"WFD", "—",     "Workflow-Definition"},
                {"WFI", "—",     "Workflow-Instanz"},
                {"WFA", "—",     "Workflow-Aktion"},
                {nullptr, nullptr, nullptr}
            };
            for (auto& a : abbrevs) {
                if (!a.code) break;
                std::cout << "  " << std::left
                          << std::setw(10) << a.code
                          << std::setw(14) << a.folder
                          << a.name << "\n";
            }
            std::cout << "\n  Beispiel-ID: " << de << "/F16/0042/2026\n\n";
        }

        // ── LOG VERBOSITY ─────────────────────────────────────
        else if (ch == 17) {
            teamMenu();
        }

        else if (ch == 16) {
            std::cout << "  1. DEBUG  2. INFO  3. WARN  4. ERROR\n";
            int lv = readInt("Level", 1, 4);
            static const Rosenholz::LogLevel lvls[] = {
                Rosenholz::LogLevel::DEBUG, Rosenholz::LogLevel::INFO,
                Rosenholz::LogLevel::WARN,  Rosenholz::LogLevel::ERR };
            Rosenholz::Logger::instance().setLevel(lvls[lv-1]);
            std::cout << "  >> Log level set.\n";
        }
    }
}

} // namespace CLI

int main(int argc, char* argv[]) {
    std::string settingsPath = "settings.json";
    std::string basePath;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i],"--settings")==0 && i+1<argc) settingsPath=argv[++i];
        if (std::strcmp(argv[i],"--basepath")==0 && i+1<argc) basePath=argv[++i];
    }

    auto& cfg = Config::instance();
    if (!basePath.empty()) cfg.setBasePath(basePath);

    auto& app = AppController::instance();
    if (!app.init(settingsPath, "", AppMode::CLI)) {
        std::cerr << "FATAL: Initialization failed\n";
        return 1;
    }

    // Run schema migrations on startup
    Rosenholz::MigrationEngine::runAll();
    // Seed standard workflow templates if not present
    Rosenholz::WorkflowEngine::createStandardTemplates();

    Logger::instance().setLevel(LogLevel::WARN);
    CLI::run();
    app.shutdown();
    return 0;
}

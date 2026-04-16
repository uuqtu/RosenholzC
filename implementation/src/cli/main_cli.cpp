// ============================================================
// main_cli.cpp  —  Rosenholz PM Hauptmenü
//
// Entry: run()
// Alle Entitätszugänge über Projekte (F16) — kein Standalone-Dokument.
// ============================================================
#include "cli_common.h"
#include "../app/AppController.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include "../core/Database.h"
#include "../core/FileOps.h"
#include "../core/BackupManager.h"
#include "../model/ProjectF16.h"
#include "../model/Person.h"
#include <iostream>
#include <iomanip>

namespace CLI {

void run() {
    std::cout << "\n"
              << "  ╔══════════════════════════════════════╗\n"
              << "  ║   Rosenholz PM — Interaktive Shell   ║\n"
              << "  ╚══════════════════════════════════════╝\n\n";

    while (true) {
        hdr("HAUPTMENÜ");
        std::cout
            << "  PROJEKTE (F16)\n"
            << "    1.  Alle Projekte anzeigen\n"
            << "    2.  Neues Projekt anlegen\n"
            << "    3.  Projekt öffnen (nach Nummer)\n"
            << "    4.  Projekt öffnen (nach ID)\n"
            << "\n  PERSONEN & TEAMS\n"
            << "    5.  Alle Personen anzeigen\n"
            << "    6.  Neue Person anlegen\n"
            << "    7.  Diensteinheiten (Teams)\n"
            << "\n  WORKFLOWS\n"
            << "    8.  Workflow-Browser\n"
            << "\n  SYSTEM\n"
            << "    9.  Backup jetzt ausführen\n"
            << "   10.  Konfiguration / Status\n"
            << "   11.  ID-Kurzzeichentabelle\n"
            << "   12.  Log-Verbosity setzen\n"
            << "   13.  Globale Suche\n"
            << "\n    0.  Beenden\n";
        hr();

        int ch = readInt("Wahl", 0, 13);
        if (ch == 0) { std::cout << "\n  Auf Wiedersehen.\n\n"; break; }

        if (ch == 1) {
            listProjects();

        } else if (ch == 2) {
            auto p = createProjectWizard();
            if (p) {
                std::cout << "  Projekt jetzt öffnen? (j/n): ";
                std::string yn; std::getline(std::cin, yn);
                if (yn=="j"||yn=="J"||yn=="y") projectMenu(p);
            }

        } else if (ch == 3) {
            auto all = Rosenholz::ProjectF16::loadAll();
            if (all.empty()) { std::cout << "  (keine Projekte)\n"; continue; }
            int n=1;
            for (auto& p : all)
                std::cout << "  " << std::setw(3) << n++ << ". " << p->regNumber.toString()
                          << "  " << p->title.substr(0,40)
                          << "  [" << p->status << "]\n";
            int pick = readInt("Nummer",1,(int)all.size());
            projectMenu(all[pick-1]);

        } else if (ch == 4) {
            std::string id = readLine("Projekt-ID: ");
            auto p = Rosenholz::ProjectF16::loadById(id);
            if (!p) { std::cout << "  >> Nicht gefunden.\n"; continue; }
            projectMenu(p);

        } else if (ch == 5) {
            listPersons();

        } else if (ch == 6) {
            createPersonWizard();

        } else if (ch == 7) {
            teamMenu();

        } else if (ch == 8) {
            workflowMenu();

        } else if (ch == 9) {
            std::cout << "  Backup läuft...\n";
            auto& cfg = Rosenholz::Config::instance();
            Rosenholz::BackupManager::backupDatabases(cfg.basePath(), cfg.backup().backupPath, false);
            std::cout << "  >> Backup abgeschlossen.\n";

        } else if (ch == 10) {
            auto& cfg = Rosenholz::Config::instance();
            hdr("KONFIGURATION");
            std::cout << "  Basispfad : " << cfg.basePath() << "\n";
            auto* prdb = Rosenholz::DatabasePool::instance().get("projects");
            auto* wfdb = Rosenholz::DatabasePool::instance().get("workflow");
            auto* ddb  = Rosenholz::DatabasePool::instance().get("documents");
            if (prdb) std::cout << "  Projekte  : " << prdb->rowCount("projects")
                                << " F16, " << prdb->rowCount("tasks") << " F22\n";
            if (wfdb) std::cout << "  Workflows : " << wfdb->rowCount("workflow_instances") << " WFI, "
                                << wfdb->rowCount("workflow_actions") << " WFA\n";
            if (ddb)  std::cout << "  Dokumente : " << ddb->rowCount("documents") << "\n";

        } else if (ch == 11) {
            hdr("ID-KURZZEICHEN");
            std::cout << "  F16  = Projekt (Vorgang)\n"
                      << "  F22  = Aufgabe (Task)\n"
                      << "  F18  = Vorgang (Incident/Risk/Measure/...)\n"
                      << "  DOK  = Dokument\n"
                      << "  WFI  = Workflow-Instanz\n"
                      << "  WFA  = Workflow-Aktion (Schritt)\n"
                      << "  WFS  = F18-Workflow-Schritt\n"
                      << "  PER  = Person\n"
                      << "  TEA  = Diensteinheit (Team)\n";

        } else if (ch == 12) {
            std::cout << "  1.DEBUG  2.INFO  3.WARN  4.ERROR\n";
            int lv = readInt("Level",1,4);
            static const Rosenholz::LogLevel lvls[] = {
                Rosenholz::LogLevel::DEBUG, Rosenholz::LogLevel::INFO,
                Rosenholz::LogLevel::WARN,  Rosenholz::LogLevel::ERR};
            Rosenholz::Logger::instance().setLevel(lvls[lv-1]);
            std::cout << "  >> Log-Level gesetzt.\n";

        } else if (ch == 13) {
            std::string q = readLine("Suche: ");
            if (!q.empty()) globalSearch(q);
        }
    }
}

} // namespace CLI

// ── Entry point ───────────────────────────────────────────────
int main(int argc, char** argv) {
    std::string settingsPath = "settings.json";
    std::string rhFile;
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if ((a == "--settings" || a == "-s") && i+1 < argc)
            settingsPath = argv[++i];
        else if ((a == "--basepath" || a == "-b") && i+1 < argc) {
            // Override basepath via environment for quick testing
            setenv("RH_BASEPATH", argv[++i], 1);
        } else if (a.size() > 3 && a.substr(a.size()-3) == ".rh")
            rhFile = a;
    }

    // Bootstrap application
    bool ok = Rosenholz::AppController::instance()
                  .init(settingsPath, rhFile, Rosenholz::AppMode::CLI);
    if (!ok) {
        std::cerr << "ERROR: AppController init failed.\n";
        return 1;
    }

    CLI::run();

    Rosenholz::AppController::instance().shutdown();
    return 0;
}

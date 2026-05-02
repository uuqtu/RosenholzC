// ============================================================
// main_cli.cpp  —  Rosenholz PM  Unix-style CLI — Einstiegspunkt
//
// This file contains only:
//   printHelp()  — full usage text
//   dispatch()   — routes the command to the right cli_*.cpp module
//   main()       — arg parsing, AppController bootstrap, shutdown
//
// All command implementations live in:
//   cli_f16.cpp  cli_f22.cpp  cli_f18.cpp  cli_dok.cpp
//   cli_f77.cpp  cli_per.cpp  cli_de.cpp
//   cli_sys.cpp  cli_comm.cpp cli_utils.cpp
// ============================================================
#include "cli_common.h"
#include "cli_registry.h"
#include "../app/AppController.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <csignal>
#include "../model/NavigationContext.h"
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

using namespace Rosenholz;

// ── printHelp ─────────────────────────────────────────────────
//
// Full usage reference. Printed when no command is given or
// when -h / --help is requested. Does not initialise the DB.

static void printHelp() {
    std::cout <<
"Rosenholz PM v7  |  rh <BEFEHL> [ARGS]\n""\n""Navigation (Linux-Stil):\n""  cd <ID>   In Entitaet navigieren (F16/F22/F18/AKT)\n""  ls        Inhalt der aktuellen Ebene auflisten\n""  lo / -h   Kontextabhaengige Optionen anzeigen\n""  ..        Eine Ebene zurueck\n""\n""\n""F16: -f16 F16-Karten  | -f16 -n Neu | -f16 -o Auswahl | -f16 <id> Öffnen | -f16 -s <q> Suche\n""F22: -f22 F22-Vorgänge | -f22 -n Neu | -f22 <id> Öffnen | -f22 <f16id> Liste\n""F18: -f18 Vorgänge    | -f18 -n Neu | -f18 <id> Öffnen\n""AKT: -akt Akten   | -akt -n Neu | -akt <id> Öffnen | -akt -s <q> Suche\n"
"     -tasks       | Meine Workflow-Aufgaben (F77-Tasks)\n""F77: -f77 Hinweise    | -f77 -start <id> [Zielzustand] | -f77 -tpl Vorlagen\n""PER: -per Personen    | -per -n Neu | -per <id> Karte | -per -s <q> Suche\n""DE:  -de  Diensteinheiten-Browser\n""\n""SYS  -search <q>  Globale Suche (F16/F22/F18/AKT/F77)\n""     -status      Datensatz-Zählungen    -backup Backup    -mfs [id] MFS neu\n""     -log <level> Verbosität: debug|info|warn|error\n""     -go <ref>    Direkt öffnen (ID / Typ:N / Seq#)\n""     -ctx [ref]   Kontext setzen (oder 'clear')  ..=zurück\n""     -hist        Verlauf zuletzt geöffneter Entitäten\n""     -tree [f16id] Hierarchiebaum F16→F22→AKT/F18\n""     -watch [N]   Polling: Benachrichtigung bei Task-Änderungen (N=Sek, Standard=30)\n""     -note <id> [Text]  Schnellnotiz ohne Menü\n""     -cal               Kalenderansicht geplanter Start-/Enddaten\n""\n""IDs enthalten /  z.B. XV/F16/0001/26\n""Flags: -s <settings.json>  -b <basispfad>\n"
;
}


// Forward declaration so runShell can call dispatch
static void dispatch(const std::string& cmd,
                     const std::vector<std::string>& rest)
{
    if (cmd.empty()) return;

    // ── Context-sensitive -h ─────────────────────────────────────────────
    if (cmd == "-h" || cmd == "--help") {
        auto cur = Rosenholz::NavigationStack::instance().current();
        if (cur.valid()) { CLI::printContextHelp(); return; }
        printHelp(); return;
    }

    // ── Exit ─────────────────────────────────────────────────────────────
    if (cmd == "exit" || cmd == "quit" || cmd == "q") {
        std::cout << "  Auf Wiedersehen.\n\n";
        AppController::instance().shutdown();
        std::exit(0);
    }

    // ── Navigate back ─────────────────────────────────────────────────────
    if (cmd == "..") {
        auto& nav = Rosenholz::NavigationStack::instance();
        nav.pop();
        CLI::cmdLs({});
        return;
    }

    // ── Log level ─────────────────────────────────────────────────────────
    if (cmd == "-log") {
        if (!rest.empty()) {
            auto& logger = Rosenholz::Logger::instance();
            if      (rest[0] == "debug") logger.setLevel(Rosenholz::LogLevel::DEBUG);
            else if (rest[0] == "info")  logger.setLevel(Rosenholz::LogLevel::INFO);
            else if (rest[0] == "warn")  logger.setLevel(Rosenholz::LogLevel::WARN);
            else if (rest[0] == "error") logger.setLevel(Rosenholz::LogLevel::ERR);
            std::cout << "  >> Log-Level: " << rest[0] << "\n";
        }
        return;
    }

    // ── Context set / clear ───────────────────────────────────────────────
    if (cmd == "-ctx") {
        auto& nav = Rosenholz::NavigationStack::instance();
        if (!rest.empty() && rest[0] == "clear") { nav.clear(); return; }
        if (!rest.empty()) CLI::cmdCd(rest);
        return;
    }

    // ── Registry dispatch (handles all other commands) ────────────────────
    if (CLI::dispatch(cmd, rest)) return;

    // ── Unknown ───────────────────────────────────────────────────────────
    std::cout << "  >> Unbekannter Befehl: " << Color::red(cmd) << "\n"
              << "     lo  oder  -h  = Optionen  |  Tab = Vervollstaendigung\n";
}

// main_cli.cpp  —  Rosenholz PM  Unix-style CLI — Einstiegspunkt
//
// This file contains only:
//   printHelp()  — full usage text
//   dispatch()   — routes the command to the right cli_*.cpp module
//   main()       — arg parsing, AppController bootstrap, shutdown
//
// All command implementations live in:
//   cli_f16.cpp  cli_f22.cpp  cli_f18.cpp  cli_dok.cpp
//   cli_f77.cpp  cli_per.cpp  cli_de.cpp
//   cli_sys.cpp  cli_comm.cpp cli_utils.cpp
// ============================================================
#include "cli_common.h"
#include "cli_registry.h"
#include "../app/AppController.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <csignal>
#include "../model/NavigationContext.h"
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

using namespace Rosenholz;

// ── printHelp ─────────────────────────────────────────────────
//
// Full usage reference. Printed when no command is given or
// when -h / --help is requested. Does not initialise the DB.

// Forward declaration so runShell can call dispatch
// ── Tab-completion ─────────────────────────────────────────────
//
// Context-aware: completes command tokens at the start of the line,
// and entity IDs when after a command that takes an <id> argument.
//
// Completion mapping:
//   First token     → command tokens (-f16, -f22, …)
//   -f16  <id>      → all F16 registration numbers
//   -f22  <id>      → F16 reg-nrs + recent F22 reg-nrs
//   -f18  <id>      → F16 reg-nrs + recent F18 vorgang-IDs
//   -dok  <id>      → F16 reg-nrs + recent DOK document-IDs
//   -f77  <id>      → active F77 workflow-IDs
//   -f77  -start <id> → F16 + F22 + F18 + DOK ids
//   -per  <id>      → all Person reg-nrs
//   -mfs  <id>      → all entity types
//   -search <text>  → no completion (free text)

static const char* const kCommands[] = {
    "-f16", "-f22", "-f18", "-akt", "-f77",
    "-per", "-de",  "-search", "-backup", "-status", "-tasks",
    "-mfs", "-log", "-go", "-ctx", "-h", "--help",
    "cd", "ls", "lo",
    "f16", "f22", "f18", "akt", "f77",
    "rev", "kom", "note", "tsk", "srch",
    "sts", "bak", "wch", "tree", "cal", "his",
    "exit", "quit", "help",
    nullptr
};

// Holds the candidate list between generator calls within one Tab press
static std::vector<std::string> g_candidates;

static char* candidateGenerator(const char* text, int state) {
    static std::size_t idx;
    static std::size_t len;
    if (state == 0) { idx = 0; len = std::strlen(text); }
    while (idx < g_candidates.size()) {
        const std::string& s = g_candidates[idx++];
        if (s.compare(0, len, text) == 0)
            return strdup(s.c_str());
    }
    return nullptr;
}

// Parse the tokens already on the readline line buffer (before cursor)
static std::vector<std::string> currentLineTokens() {
    std::vector<std::string> tokens;
    if (!rl_line_buffer) return tokens;
    std::istringstream ss(std::string(rl_line_buffer, rl_point));
    std::string t;
    while (ss >> t) tokens.push_back(t);
    return tokens;
}

static char** rhCompletion(const char* text, int /*start*/, int /*end*/) {
    rl_attempted_completion_over = 1;   // no fallback to filename completion
    g_candidates.clear();

    auto tokens = currentLineTokens();

    // ── First token: complete command names ───────────────────
    if (tokens.empty() || (tokens.size() == 1 && !tokens[0].empty() &&
                            rl_line_buffer[rl_point - 1] != ' ')) {
        for (int i = 0; kCommands[i]; ++i)
            g_candidates.push_back(kCommands[i]);
        return rl_completion_matches(text, candidateGenerator);
    }

    // ── After a command: complete entity IDs ─────────────────
    // Determine which command is active
    std::string cmd = tokens.empty() ? "" : tokens[0];

    // Helper: add F16 reg-nrs
    auto addF16 = [&]() {
        try {
            auto all = Rosenholz::F16::loadAll();
            for (auto& p : all) g_candidates.push_back(p->regNumber.toString());
        } catch (...) {}
    };
    // Helper: add recent F22 reg-nrs
    auto addF22 = [&]() {
        try {
            auto all = Rosenholz::F22::loadRecent(50);
            for (auto& t : all) g_candidates.push_back(t->regNumber.toString());
        } catch (...) {}
    };
    // Helper: add recent F18 ids
    auto addF18 = [&]() {
        try {
            auto all = Rosenholz::F18Operation::loadRecent(50);
            for (auto& v : all) g_candidates.push_back(v->operationId);
        } catch (...) {}
    };
    // Helper: add recent DOK ids
    auto addDok = [&]() {
        try {
            auto all = Rosenholz::Folder::loadRecent(50);
            for (auto& d : all) g_candidates.push_back(d->folderId);
        } catch (...) {}
    };
    // Helper: add active F77 workflow ids
    auto addF77 = [&]() {
        try {
            auto all = Rosenholz::F77W::loadActive();
            for (auto& w : all) g_candidates.push_back(w->workflowId);
        } catch (...) {}
    };
    // Helper: add person reg-nrs
    auto addPer = [&]() {
        try {
            auto all = Rosenholz::Person::loadAll();
            for (auto& p : all) g_candidates.push_back(p->regNumber.toString());
        } catch (...) {}
    };

    if (cmd == "cd") {
        // Complete with context children: "ID (Title)" format
        try {
            auto children = CLI::getContextChildren();
            for (auto& [id, title] : children) {
                // Format: "ID" for completion, display "ID (Title)"
                g_candidates.push_back(id);
            }
        } catch (...) {}
        return rl_completion_matches(text, candidateGenerator);
    } else if (cmd == "-f16") {
        addF16();
        // Also sub-flags
        g_candidates.insert(g_candidates.end(), {"-n", "-s", "-status"});
    } else if (cmd == "-f22") {
        addF16();   // project-id to list/create under
        addF22();   // task-id to open menu
        g_candidates.insert(g_candidates.end(), {"-n"});
    } else if (cmd == "-f18") {
        addF16();
        addF18();
        g_candidates.insert(g_candidates.end(), {"-n", "-t"});
    } else if (cmd == "-akt") {
        addDok();
        g_candidates.insert(g_candidates.end(), {"-n", "-s", "-f22", "-f18"});
    } else if (cmd == "-f77") {
        if (tokens.size() >= 2 && tokens[1] == "-start") {
            // -f77 -start <any-entity-id>
            addF16(); addF22(); addF18(); addDok();
        } else {
            addF77();
            g_candidates.insert(g_candidates.end(), {"-tpl", "-new", "-start"});
        }
    } else if (cmd == "-per") {
        addPer();
        g_candidates.insert(g_candidates.end(), {"-n", "-s"});
    } else if (cmd == "-mfs") {
        addF16(); addF22(); addF18(); addDok(); addF77();
    } else if (cmd == "-log") {
        g_candidates = {"debug", "info", "warn", "error"};
    }

    if (g_candidates.empty()) return nullptr;
    return rl_completion_matches(text, candidateGenerator);
}


// ── SIGINT handler (Ctrl+C) ──────────────────────────────────────────
static void sigintHandler(int) {
    CLI::cliMarkInterrupted();
    rl_done = 1;
    std::cout << "\n";
}

// ── runShell ──────────────────────────────────────────────────
//
// Interactive command shell. Launched when rh is run without arguments.
//
// Features:
//   Tab        — completes command tokens (-f16, -f22, …)
//   Arrow keys — navigate command history
//   Ctrl+C     — aborts the current wizard / menu, returns to rh> prompt
//   Ctrl+D     — exits the shell (EOF)
//   exit/quit  — exits the shell

static void runShell() {
    // Register Ctrl+C handler — abort current activity, stay in shell
    struct sigaction sa{};
    sa.sa_handler = sigintHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                    // no SA_RESTART so getline is interrupted
    sigaction(SIGINT, &sa, nullptr);

    // Configure readline
    rl_attempted_completion_function = rhCompletion;
    rl_bind_key('\t', rl_complete);

    using_history();

    std::cout << "\n"
              << Color::bold("  Rosenholz PM v7") + "  —  Interaktive Shell\n"
              << "  Tab=Vervollständigung  ↑↓=Verlauf  Ctrl+C=Abbruch  Ctrl+D=Beenden\n"
              << "  cd <ID>=navigieren  ls=inhalt  lo/-h=hilfe  ..=zurueck  exit\n"
              << "  Beispiel:  cd XV/F16/0001/26   ls   -f22 -n   exit\n"
              << "\n";

    while (true) {
        CLI::cliClearInterrupted();

        // Build dynamic prompt with navigation context
        std::string promptSuffix = Rosenholz::NavigationStack::instance().promptSuffix();
        // Show full breadcrumb in prompt
        std::string breadcrumb = Rosenholz::NavigationStack::instance().breadcrumb();
        std::string prompt;
        if (breadcrumb.empty())
            prompt = Color::bold("rh") + " > ";
        else
            prompt = Color::bold("rh") + " " + Color::cyan(breadcrumb) + " > ";
        char* raw = readline(prompt.c_str());
        if (!raw) {
            // EOF (Ctrl+D)
            std::cout << "\n  Auf Wiedersehen.\n\n";
            AppController::instance().shutdown();
            std::exit(0);
        }

        std::string line(raw);
        free(raw);

        // Skip empty lines and Ctrl+C leftovers
        if (line.empty()) continue;

        // Add to history
        add_history(line.c_str());

        // Tokenise
        std::vector<std::string> tokens;
        std::istringstream ss(line);
        std::string tok;
        while (ss >> tok) tokens.push_back(tok);
        if (tokens.empty()) continue;

        // ".." — navigate back (pop navigation stack)
        if (tokens[0] == "..") {
            auto& nav = Rosenholz::NavigationStack::instance();
            nav.pop();
            CLI::cmdLs({});  // show new location's children
            continue;
        }

        // Exit keywords — controlled shutdown via AppController
        if (tokens[0] == "exit" || tokens[0] == "quit" || tokens[0] == "q") {
            std::cout << "  Auf Wiedersehen.\n\n";
            // Controlled shutdown: flush logs, close DBs, run final backup if enabled
            AppController::instance().shutdown();
            std::exit(0);  // clean exit after shutdown
        }

        // Help shortcut
        if (tokens[0] == "help" || tokens[0] == "h") { printHelp(); continue; }

        // Dispatch
        std::string cmd = tokens[0];
        std::vector<std::string> rest(tokens.begin() + 1, tokens.end());
        dispatch(cmd, rest);

        // If a wizard was aborted with Ctrl+C, the flag is set — clear it
        // so the next command starts fresh.
        if (CLI::cliIsInterrupted()) {
            CLI::cliClearInterrupted();
            std::cout << "\n";
        }
    }

    // Restore default SIGINT so the process can be killed normally afterwards
    signal(SIGINT, SIG_DFL);
}

// ── dispatch ──────────────────────────────────────────────────
//
// Routes the command string to the correct module function.
// All cmd* functions are declared in cli_common.h and defined
// in their respective cli_*.cpp files.

// ── main ──────────────────────────────────────────────────────
//
// Argument parsing:
//   1. Global flags (-s settings, -b basepath) must appear BEFORE
//      the command. As soon as the first command token is seen,
//      global pre-scanning stops and all remaining args are passed
//      verbatim to the command handler as 'rest'.
//   2. No command → printHelp without DB init.
//   3. -h / --help → printHelp without DB init.
//   4. All other commands → init AppController, dispatch, shutdown.

int main(int argc, char** argv) {
    std::string settingsPath = "settings.json";
    std::vector<std::string> positional;
    bool commandSeen = false;

    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);

        if (!commandSeen && (a == "-s" || a == "--settings") && i + 1 < argc) {
            settingsPath = argv[++i];
        } else if (!commandSeen && (a == "-b" || a == "--basepath") && i + 1 < argc) {
            setenv("RH_BASEPATH", argv[++i], 1);
        } else {
            positional.push_back(a);
            if (!commandSeen && !a.empty()) commandSeen = true;
        }
    }

    // -h / --help: print help without initialising the DB
    if (!positional.empty()
        && (positional[0] == "-h" || positional[0] == "--help")) {
        printHelp();
        return 0;
    }

    // Initialise the application (databases, LMDB, config)
    bool initOk = AppController::instance().init(settingsPath, "", AppMode::CLI);
    if (!initOk) {
        std::cerr << "rh: AppController-Initialisierung fehlgeschlagen.\n";
        return 1;
    }

    // No command → interactive shell (useful in CLion debugger and bare invocation)
    if (positional.empty()) {
        runShell();
        AppController::instance().shutdown();
        return 0;
    }

    std::string cmd = positional[0];
    std::vector<std::string> rest(positional.begin() + 1, positional.end());
    dispatch(cmd, rest);

    AppController::instance().shutdown();
    return 0;
}

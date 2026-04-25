// ============================================================
// cli_utils.cpp  —  Shared CLI utilities
//
// Contains:
//   - Input primitives: readLine, readOpt, readInt, yesno
//   - Display primitives: hdr, hr, fval, fdate
//   - Predicate: isId
//   - Output helpers: printOk, die
//   - Entity print functions: printProject, printTask, printDocument
//   - List display: listProjects, listTasks, listPersons
//   - showRecentItems (cross-type recent overview)
// ============================================================
#include "cli_common.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include "../model/f16/ProjectF16.h"
#include "../model/f22/TaskF22.h"
#include "../model/dok/Document.h"
#include "../model/f18/F18Operation.h"
#include "../workflow/F77Workflow.h"
#include <sstream>
#include "../repository/DocumentRevision.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <readline/readline.h>
#include <readline/history.h>

namespace CLI {

using namespace Rosenholz;

// ── Output helpers ────────────────────────────────────────────

void printOk(const std::string& msg) {
    std::cout << msg << "\n";
}

void die(const std::string& msg) {
    std::cerr << "rh: " << msg << "\n";
    std::exit(1);
}

void printErr(const std::string& msg) {
    std::cout << "  >> Fehler: " << msg << "\n";
}

bool isId(const std::string& s) {
    // genId/RegNumber strings contain '/' e.g. XV/F16/0001/2026
    return s.find('/') != std::string::npos;
}

// ── Display primitives ────────────────────────────────────────

void hr() {
    std::cout << "  " << std::string(54, '-') << "\n";
}

void hdr(const std::string& t) {
    std::cout << "\n  +" << std::string(52, '-') << "+\n";
    std::string line = t.size() > 50 ? t.substr(0, 50) : t;
    std::cout << "  |  " << std::left << std::setw(49) << line << "|\n";
    std::cout << "  +" << std::string(52, '-') << "+\n";
}

std::string fval(const std::string& v) {
    return v.empty() ? "—" : v;
}

std::string fdate(const std::string& d) {
    return d.empty() ? "—" : d;
}

// ── Ctrl+C handling ───────────────────────────────────────────
// g_interrupted is set by the SIGINT handler. Input functions check
// it and return empty / default values so the current wizard or menu
// loop can detect the interrupt and unwind back to the shell prompt.
static volatile sig_atomic_t g_interrupted = 0;

void cliMarkInterrupted()  { g_interrupted = 1; }
void cliClearInterrupted() { g_interrupted = 0; }
bool cliIsInterrupted()    { return g_interrupted != 0; }

// ── readline helper ───────────────────────────────────────────
// All interactive input goes through rl_input() so that:
//   - Tab completes common rh command tokens.
//   - Arrow keys navigate history.
//   - Ctrl+C sets g_interrupted and returns "".
//   - EOF (Ctrl+D) also returns "".
static std::string rl_input(const std::string& prompt) {
    char* raw = readline(prompt.c_str());
    if (!raw) {
        // EOF or rl_done set by SIGINT handler
        g_interrupted = 1;
        rl_replace_line("", 0);
        rl_point = 0;
        std::cout << "\n";
        return "";
    }
    std::string s(raw);
    free(raw);
    // Add non-blank lines to history
    if (!s.empty()) add_history(s.c_str());
    return s;
}

static std::string strip(const std::string& s) {
    size_t f = s.find_first_not_of(" \t");
    if (f == std::string::npos) return "";
    size_t l = s.find_last_not_of(" \t");
    return s.substr(f, l - f + 1);
}

// ── Input primitives ──────────────────────────────────────────
// All primitives return a sensible default / empty string when
// g_interrupted is set — callers check cliIsInterrupted() to abort.

std::string readLine(const std::string& prompt) {
    while (true) {
        if (g_interrupted) return "";
        std::string s = strip(rl_input("  " + prompt));
        if (g_interrupted) return "";
        if (s.empty()) {
            std::cout << "  (Bitte Eingabe machen — Ctrl+C zum Abbrechen)\n";
            continue;
        }
        return s;
    }
}

std::string readOpt(const std::string& prompt) {
    if (g_interrupted) return "";
    std::string s = strip(rl_input("  " + prompt));
    if (g_interrupted) return "";
    return s;
}

int readInt(const std::string& prompt, int lo, int hi) {
    while (true) {
        if (g_interrupted) return lo;
        std::string s = strip(rl_input(
            "  " + prompt + " [" + std::to_string(lo) + "-" + std::to_string(hi) + "]: "));
        if (g_interrupted) return lo;
        if (s.empty()) continue;
        try {
            int v = std::stoi(s);
            if (v >= lo && v <= hi) return v;
        } catch (...) {}
        std::cout << "  (Bitte Zahl zwischen " << lo << " und " << hi << " eingeben)\n";
    }
}

bool yesno(const std::string& prompt) {
    while (true) {
        if (g_interrupted) return false;
        std::string s = readOpt(prompt + " (ja/nein): ");
        if (g_interrupted) return false;
        if (s == "ja" || s == "j" || s == "y" || s == "yes") return true;
        if (s == "nein" || s == "n" || s == "no")             return false;
        std::cout << "  (bitte 'ja' oder 'nein')\n";
    }
}

// ── Entity print functions ─────────────────────────────────────

void printProject(const ProjectF16& p) {
    auto row = [](const std::string& k, const std::string& v) {
        std::cout << "  | " << std::left << std::setw(10) << k
                  << std::setw(40) << v << "|\n";
    };
    hdr("F16 — " + p.regNumber.toString() + "  " + p.title.substr(0,30));
    std::cout << "  +" << std::string(52,'-') << "+\n";
    row("ID (rh -f16 <id>)", p.projectId);
    row("Status",   p.status + " / " + p.phase);
    row("Lead",     p.leadId.empty() ? "—" : p.leadId.substr(0,26));
    row("Budget",   std::to_string((int)p.budgetPlanned) + " " + p.currency);
    row("CPI/SPI",  std::to_string(p.cpi).substr(0,4) + " / " + std::to_string(p.spi).substr(0,4));
    row("Start",    p.startDatePlanned.empty() ? "—" : p.startDatePlanned.substr(0,10));
    row("Ende",     p.endDatePlanned.empty()   ? "—" : p.endDatePlanned.substr(0,10));
    if (!p.releaseWorkflowId.empty())
        row("F77 WFI", p.releaseWorkflowId.substr(0,26));
    if (!p.milestones.empty())
        std::cout << "  Meilsteine:\n" << p.milestones.substr(0,120) << "\n";
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}


void printTask(const TaskF22& t) {
    auto row = [](const std::string& k, const std::string& v) {
        std::cout << "  | " << std::left << std::setw(10) << k
                  << std::setw(40) << v << "|\n";
    };
    hdr("F22 — " + t.regNumber.toString() + "  " + t.title.substr(0,30));
    std::cout << "  +" << std::string(52,'-') << "+\n";
    row("ID (rh -f22 <id>)", t.taskId);
    row("Status", t.status + " / " + t.priority);
    row("Projekt",t.projectId.substr(0,26));
    row("Person", t.assigneeId.empty() ? "—" : t.assigneeId.substr(0,26));
    row("Fortsch",std::to_string(t.percentComplete) + "%");
    row("Start",  t.startDatePlanned.empty() ? "—" : t.startDatePlanned.substr(0,10));
    row("Ende",   t.dueDatePlanned.empty()   ? "—" : t.dueDatePlanned.substr(0,10));
    if (!t.releaseWorkflowId.empty())
        row("F77 WFI", t.releaseWorkflowId.substr(0,26));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}


void printDocument(const Document& d) {
    hdr("DOK — " + d.documentId.substr(0,26) + "  " + d.title.substr(0,24));
    // Show current revision state (the authoritative lifecycle state)
    auto curRev = Rosenholz::DocumentRevision::currentRevision(d.documentId);
    std::string revInfo = curRev
        ? "Rev " + std::to_string(curRev->rev) + " [" + curRev->revStateStr() + "]"
        : "(keine Revision)";
    std::cout << "  Revision   : " << revInfo << "\n";
    std::cout << "  Dok-Status : " << revStateToString(d.currentRevisionState()) << "\n";
    std::cout << "  Typ        : " << d.docType << " / " << d.format << "\n";
    std::cout << "  Version    : " << d.version << "\n";
    std::cout << "  Projekt    : " << (d.projectId.empty() ? "—" : d.projectId.substr(0,32)) << "\n";
    if (!d.taskId.empty())
        std::cout << "  Aufgabe    : " << d.taskId.substr(0,32) << "\n";
    if (!d.releaseWorkflowId.empty())
        std::cout << "  Main WFI   : " << d.releaseWorkflowId.substr(0,36) << "\n";
    // Show current revision
    auto cur = DocumentRevision::currentRevision(d.documentId);
    if (cur)
        std::cout << "  Revision   : Rev " << cur->rev
                  << " [" << cur->revStateStr() << "]"
                  << (cur->superseded ? "" : " ← aktiv") << "\n";
    else
        std::cout << "  Revision   : (keine)\n";
    if (!d.filePath.empty())
        std::cout << "  Datei      : " << FileOps::baseName(d.filePath) << "\n";
    std::cout << "\n";
}


void listDocuments(const std::vector<std::shared_ptr<Document>>& docs,
                   const std::string& title) {
    hdr(title.empty() ? "DOKUMENTE" : title);
    if (docs.empty()) { std::cout << "  (keine Dokumente)\n"; return; }
    int n=1;
    for (auto& d : docs) {
        auto cur = DocumentRevision::currentRevision(d->documentId);
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(28) << d->title.substr(0,26)
                  << "  " << std::setw(12) << revStateToString(d->currentRevisionState())
                  << "  v" << d->version;
        if (cur) std::cout << "  [Rev " << cur->rev << " " << cur->revStateStr() << "]";
        std::cout << "\n";
    }
    std::cout << "\n";
}


// ── List display functions ─────────────────────────────────────

void listProjects() {
    auto all = ProjectF16::loadAll();
    if (all.empty()) { std::cout << "  (keine Projekte)\n"; return; }

    std::cout << "  " << std::left
              << std::setw(28) << "PROJEKT-ID"
              << std::setw(32) << "TITEL"
              << std::setw(14) << "STATUS"
              << std::setw(12) << "PHASE"
              << std::setw(8)  << "PRIO"
              << "CPI\n"
              << "  " << std::string(86, '-') << "\n";
    std::cout << "  (ID für rh -f16 <id> verwenden)\n";

    for (auto& p : all) {
        std::string title = p->title.size() > 30 ? p->title.substr(0, 29) + "~" : p->title;
        std::string phase = p->phase.empty()    ? "-" : p->phase;
        std::string prio  = p->priority.empty() ? "-" : p->priority;
        char cpibuf[10];
        snprintf(cpibuf, sizeof(cpibuf), "%.2f", p->cpi);
        std::cout << "  " << std::left
                  << std::setw(28) << p->projectId
                  << std::setw(32) << title
                  << std::setw(14) << p->status
                  << std::setw(12) << phase
                  << std::setw(8)  << prio
                  << cpibuf << "\n";
    }
    std::cout << "  " << all.size() << " Projekt(e)\n";
}

void listTasks(const std::string& projectId) {
    auto tasks = TaskF22::loadForProject(projectId);
    if (tasks.empty()) { std::cout << "  (keine Aufgaben)\n"; return; }

    std::cout << "  " << std::left
              << std::setw(28) << "AUFGABE-ID"
              << std::setw(30) << "TITEL"
              << std::setw(14) << "STATUS"
              << std::setw(6)  << "%"
              << std::setw(10) << "PRIO"
              << "ASSIGNEE\n"
              << "  " << std::string(86, '-') << "\n";
    std::cout << "  (ID für rh -f22 <id> verwenden)\n";

    for (auto& t : tasks) {
        std::string title = t->title.size() > 28 ? t->title.substr(0, 27) + "~" : t->title;
        std::string ass   = t->assigneeId.empty() ? "-" : t->assigneeId.substr(0, 14);
        std::cout << "  " << std::left
                  << std::setw(28) << t->taskId
                  << std::setw(30) << title
                  << std::setw(14) << t->status
                  << std::setw(6)  << t->percentComplete
                  << std::setw(10) << t->priority
                  << ass << "\n";
    }
    std::cout << "  " << tasks.size() << " Aufgabe(n)\n";
}


} // namespace CLI

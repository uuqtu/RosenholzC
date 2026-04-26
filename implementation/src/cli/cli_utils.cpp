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
    // compact mode: no separator line
    (void)0;
}

void hdr(const std::string& t) {
    std::string line = t.size() > 54 ? t.substr(0, 54) : t;
    std::cout << "  -- " << line << " --\n";
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
    hdr("F16 " + p.regNumber.toString() + "  " + p.title.substr(0,38));
    std::cout << "  ID:" << p.projectId
              << "  Status:" << p.status << "/" << p.sizeClass << "\n";
    if (!p.leadId.empty() || p.budgetPlanned > 0)
        std::cout << "  Lead:" << (p.leadId.empty()?"—":p.leadId.substr(0,26))
                  << "  Budget:" << (int)p.budgetPlanned << " " << p.currency << "\n";
    if (!p.startDatePlanned.empty() || !p.endDatePlanned.empty())
        std::cout << "  " << (p.startDatePlanned.empty()?"—":p.startDatePlanned.substr(0,10))
                  << " → " << (p.endDatePlanned.empty()?"—":p.endDatePlanned.substr(0,10)) << "\n";
    if (!p.releaseWorkflowId.empty())
        std::cout << "  WFI:" << p.releaseWorkflowId.substr(0,36) << "\n";
}
void printTask(const TaskF22& t) {
    hdr("F22 " + t.regNumber.toString() + "  " + t.title.substr(0,38));
    std::cout << "  ID:" << t.taskId
              << "  " << t.status << "/" << t.priority
              << "  " << t.percentComplete << "%\n";
    std::cout << "  F16:" << t.projectId.substr(0,26);
    if (!t.assigneeId.empty()) std::cout << "  Person:" << t.assigneeId.substr(0,26);
    std::cout << "\n";
    if (!t.startDatePlanned.empty() || !t.dueDatePlanned.empty())
        std::cout << "  " << (t.startDatePlanned.empty()?"—":t.startDatePlanned.substr(0,10))
                  << " → " << (t.dueDatePlanned.empty()?"—":t.dueDatePlanned.substr(0,10)) << "\n";
    if (!t.releaseWorkflowId.empty())
        std::cout << "  WFI:" << t.releaseWorkflowId.substr(0,36) << "\n";
}
void printDocument(const Document& d) {
    hdr("DOK " + d.documentId.substr(0,26) + "  " + d.title.substr(0,28));
    auto curRev = Rosenholz::DocumentRevision::currentRevision(d.documentId);
    std::cout << "  " << d.docType << "/" << d.format
              << "  Rev:" << (curRev ? std::to_string(curRev->rev) + "[" + curRev->revStateStr() + "]" : "—")
              << "  v" << d.version << "\n";
    if (!d.taskId.empty()) std::cout << "  F22:" << d.taskId.substr(0,26);
    std::cout << "\n";
    if (!d.releaseWorkflowId.empty())
        std::cout << "  WFI:" << d.releaseWorkflowId.substr(0,36) << "\n";
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
                  << "  " << std::setw(12) << d->currentRevisionState()
                  << "  v" << d->version;
        if (cur) std::cout << "  [Rev " << cur->rev << " " << cur->revState << "]";
        std::cout << "\n";
    }
    std::cout << "\n";
}


// ── List display functions ─────────────────────────────────────

void listProjects() {
    auto all = ProjectF16::loadAll();
    if (all.empty()) { std::cout << "  (keine F16-Karten)\n"; return; }

    std::cout << "  " << std::left
              << std::setw(28) << "F16-ID"
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
    std::cout << "  " << all.size() << " F16-Karte(n)\n";
}

void listTasks(const std::string& projectId) {
    auto tasks = TaskF22::loadForProject(projectId);
    if (tasks.empty()) { std::cout << "  (keine F22-Vorgänge)\n"; return; }

    std::cout << "  " << std::left
              << std::setw(28) << "F22-ID"
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
    std::cout << "  " << tasks.size() << " F22\n";
}


} // namespace CLI

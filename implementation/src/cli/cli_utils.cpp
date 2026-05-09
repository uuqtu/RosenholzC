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
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/akt/Folder.h"
#include "../model/f18/F18Operation.h"
#include "../workflow/F77Workflow.h"
#include <sstream>
#include "../model/akt/FolderRevision.h"
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <readline/readline.h>
#include <readline/history.h>

namespace CLI {


using namespace Rosenholz;

// ── Output helpers ────────────────────────────────────────────

void printOk(const std::string& msg) {
    std::cout << Color::green("  >> ") << msg << "\n";
}

void die(const std::string& msg) {
    std::cerr << "rh: " << msg << "\n";
    std::exit(1);
}

void printErr(const std::string& msg) {
    std::cout << Color::red("  >> Fehler: ") << msg << "\n";
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
    std::cout << "  " << Color::bold("-- " + line + " --") << "\n";
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

// ── parseDate: shorthand date input ──────────────────────────────────────────
// "."     → today
// "+Nd"   → today + N days
// "+Nw"   → today + N weeks
// "+Nm"   → today + N months  
// "+Ny"   → today + N years
// "YYYY-MM-DD" → passthrough
// ""      → empty passthrough
// parseDate: shorthand date input → ISO YYYY-MM-DD
//   "."       → today
//   "+Nd"     → today + N days   (any positive integer N)
//   "+Nw"     → today + N weeks
//   "+Nm"     → today + N months
//   "+Ny"     → today + N years
//   "-Nd" etc → subtract (e.g. "-7d" = 1 week ago)
//   "YYYY-MM-DD" → passed through unchanged
//   "" → empty (no date)
//
// Examples: +14d +3m +1y -7d .
std::string parseDate(const std::string& input) {
    if (input.empty()) return "";

    // Already ISO format (basic check):
    if (input.size() == 10 && input[4] == '-' && input[7] == '-') return input;

    std::time_t now = std::time(nullptr);
    std::tm tm_now = *std::localtime(&now);

    // "." → today
    if (input == ".") {
        char buf[16]; std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_now); return buf;
    }

    // "+Nx" or "-Nx" offsets
    if (!input.empty() && (input[0] == '+' || input[0] == '-')) {
        int sign = (input[0] == '-') ? -1 : 1;
        int n = 0;
        std::size_t i = 1;
        // Parse any number of digits:
        while (i < input.size() && std::isdigit((unsigned char)input[i]))
            n = n * 10 + (input[i++] - '0');
        char unit = (i < input.size()) ? std::tolower((unsigned char)input[i]) : 'd';
        n *= sign;

        switch (unit) {
            case 'd': tm_now.tm_mday += n;     break;
            case 'w': tm_now.tm_mday += n * 7; break;
            case 'm': tm_now.tm_mon  += n;     break;
            case 'y': tm_now.tm_year += n;     break;
            default:  return input; // unknown unit → pass through
        }
        std::mktime(&tm_now); // normalize (handles month/year overflow)
        char buf[16]; std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_now); return buf;
    }

    return input; // unrecognised format → pass through
}


// ── readChoice: menu choice with ? for help display ──────────────────────────
// If user types "?", returns -1 (caller shows menu and re-asks).
// Otherwise same as readInt().
int readChoice(const std::string& menuText, int lo, int hi) {
    while (true) {
        // Print compact prompt
        std::cout << "  " << Color::dim("[" + std::to_string(lo) + "-" + std::to_string(hi) + " | ?=Hilfe]") << " Wahl: ";
        std::string input;
        std::getline(std::cin, input);
        if (g_interrupted) { g_interrupted = 0; return lo - 1; }
        if (input == "?") { std::cout << menuText; continue; }
        if (input.empty()) continue;
        try {
            int v = std::stoi(input);
            if (v >= lo && v <= hi) return v;
        } catch (...) {}
        std::cout << "  " << Color::yellow("  Ungültig — bitte Zahl zwischen ")
                  << lo << " und " << hi << " (oder ?)\n";
    }
}


// ── readChar: read a single-char shortcut with alias table ──────────────────
// aliases: {{"h","high"},{"m","medium"},{"l","low"}} etc.
// Returns the mapped value, or the raw input if no match.
// Displays the prompt and waits for a full line (no raw terminal needed).
std::string readChar(const std::string& prompt,
                     const std::vector<std::pair<std::string,std::string>>& aliases,
                     bool optional)
{
    while (true) {
        std::cout << "  " << prompt;
        std::cout.flush();
        std::string raw;
        if (!std::getline(std::cin, raw)) { g_interrupted = 1; return ""; }
        // strip whitespace:
        raw.erase(0, raw.find_first_not_of(" \t"));
        raw.erase(raw.find_last_not_of(" \t") + 1);
        if (raw.empty()) {
            if (optional) return "";
            continue;
        }
        // lowercase for matching:
        std::string lo = raw;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
        for (auto& [key, val] : aliases)
            if (lo == key || lo == val) return val;
        // numeric: try mapping to alias by position
        // unknown input: re-prompt
        std::string hint = "(";
        for (int i = 0; i < (int)aliases.size(); i++) {
            if (i) hint += "/";
            hint += aliases[i].first + "=" + aliases[i].second;
        }
        hint += ")";
        std::cout << "  Ungueltig. " << hint << "\n";
    }
}


// ── Wildcard matching ─────────────────────────────────────────────────────────
// Forwards to Rosenholz::matchesPattern (model/Utils.cpp, in rosenholz_lib).
// CLI namespace wrapper for backward compatibility with existing call sites.
bool matchesPattern(const std::string& text, const std::string& pattern) {
    return Rosenholz::matchesPattern(text, pattern);
}

// patternToSQLLike: convert user pattern to SQLite LIKE pattern
//   User * → SQL %   (any number of chars)
//   User % → SQL _   (exactly one char)
//   No wildcards → %pattern% (substring, backward compat)
std::string patternToSQLLike(const std::string& pattern) {
    bool hasWild = (pattern.find('*') != std::string::npos ||
                    pattern.find('%') != std::string::npos);
    if (!hasWild) return "%" + pattern + "%";

    std::string sql;
    sql.reserve(pattern.size() * 2);
    for (char c : pattern) {
        if      (c == '*') sql += '%';   // * → any number
        else if (c == '%') sql += '_';   // % → exactly one
        else if (c == '_') sql += "\_"; // escape SQL's own single-char wildcard
        else               sql += c;
    }
    return sql;
}


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
        // Trim leading/trailing whitespace:
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
        if (s == "ja" || s == "j" || s == "y" || s == "yes") return true;
        if (s == "nein" || s == "n" || s == "no")             return false;
        std::cout << "  (bitte \'ja\' oder \'nein\')\n";
    }
}

// ── Entity print functions ─────────────────────────────────────

void printProject(const F16& p) {
    hdr("F16 " + p.regNumber.toString() + "  " + p.title.substr(0,38));
    std::cout << "  ID:" << p.projectId
              << "  Typ:" << p.projectType
              << "  " << (p.archived ? Color::dim("archiviert") : Color::green("aktiv")) << "\n";
    if (!p.leadId.empty() || p.budgetPlanned > 0)
        std::cout << "  Lead:" << (p.leadId.empty()?"—":p.leadId.substr(0,26))
                  << "  Budget:" << (int)p.budgetPlanned << " " << p.currency << "\n";
    if (!p.startDatePlanned.empty() || !p.endDatePlanned.empty())
        std::cout << "  " << (p.startDatePlanned.empty()?"—":p.startDatePlanned.substr(0,10))
                  << " → " << (p.endDatePlanned.empty()?"—":p.endDatePlanned.substr(0,10)) << "\n";
}
void printTask(const F22& t) {
    hdr("F22 " + t.regNumber.toString() + "  " + t.title.substr(0,38));
    std::cout << "  ID:" << t.taskId
              << "  " << entityStatusToString(t.status) << "/" << t.priority
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
void printDocument(const Folder& d) {
    hdr("AKT " + d.folderId.substr(0,26) + "  " + d.title.substr(0,28));
    auto curRev = Rosenholz::FolderRevision::currentRevision(d.folderId);
    std::cout << "  " << d.docType << "/" << d.format
              << "  Rev:" << (curRev ? std::to_string(curRev->rev) + "[" + curRev->revStateStr() + "]" : "—")
              << "  v" << d.version << "\n";
    if (!d.taskId.empty()) std::cout << "  F22:" << d.taskId.substr(0,26);
    std::cout << "\n";
    if (!d.workflowId.empty())
        std::cout << "  WFI:" << d.workflowId.substr(0,36) << "\n";

}


void listDocuments(const std::vector<std::shared_ptr<Folder>>& docs,
                   const std::string& title) {
    hdr(title.empty() ? "AKTEN" : title);
    if (docs.empty()) { std::cout << "  (keine Akten)\n"; return; }
    int n=1;
    for (auto& d : docs) {
        auto cur = FolderRevision::currentRevision(d->folderId);
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
    auto all = F16::loadAll();
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
        char cpibuf[10];
        snprintf(cpibuf, sizeof(cpibuf), "%.2f", p->costPerformanceIndex);
        std::cout << "  " << std::left
                  << std::setw(28) << p->projectId
                  << std::setw(32) << title
                  << std::setw(14) << Color::statusColor(p->archived ? "archiviert" : "aktiv")
                  << std::setw(12) << phase
                  << cpibuf << "\n";
    }
    std::cout << "  " << all.size() << " F16-Karte(n)\n";
}

void listTasks(const std::string& projectId) {
    auto tasks = F22::loadForProject(projectId);
    if (tasks.empty()) { std::cout << "  (keine F22-Vorgänge)\n"; return; }

    std::cout << "  " << std::left
              << std::setw(28) << "F22-ID"
              << std::setw(30) << "TITEL"
              << std::setw(14) << "STATUS"
              << std::setw(6)  << "%"
              
              << "ASSIGNEE\n"
              << "  " << std::string(86, '-') << "\n";
    std::cout << "  (ID für rh -f22 <id> verwenden)\n";

    for (auto& t : tasks) {
        std::string title = t->title.size() > 28 ? t->title.substr(0, 27) + "~" : t->title;
        std::string ass   = t->assigneeId.empty() ? "-" : t->assigneeId.substr(0, 14);
        std::cout << "  " << std::left
                  << std::setw(28) << t->taskId
                  << std::setw(30) << title
                  << std::setw(14) << entityStatusToString(t->status)
                  << std::setw(6)  << t->percentComplete
                  << std::setw(10) << t->priority
                  << ass << "\n";
    }
    std::cout << "  " << tasks.size() << " F22\n";
}


// ── listComms: numbered list of Communications for owner entity ───────────
// Returns the loaded list so the caller can pick by number.
std::vector<std::shared_ptr<Rosenholz::Communication>>
listComms(const std::string& ownerId, const std::string& ownerType) {
    auto items = Rosenholz::Communication::loadForOwner(ownerId, ownerType);
    if (items.empty()) {
        std::cout << "  (keine Kommunikationseintraege)\n";
    } else {
        int n = 1;
        for (auto& c : items)
            std::cout << "  " << std::setw(3) << n++ << ".["
                      << std::left << std::setw(8) << c->commType.substr(0,7) << "] "
                      << std::setw(26) << c->title.substr(0,24)
                      << "  " << fdate(c->scheduledDate)
                      << "  " << commStatusToString(c->status) << "\n";
    }
    return items;
}


// ── notesMenu ─────────────────────────────────────────────────
// Shows existing notes and allows adding new ones.
// Writes _Notizen.txt to the entity's MFS folder when done.
void notesMenu(const std::string& entityType,
                    const std::string& entityId,
                    const std::string& mfsDir)
{
    while (true) {
        auto notes = Rosenholz::Note::loadForEntity(entityType, entityId);
        hdr("NOTIZEN — " + entityType + "/" + entityId.substr(0, 26));
        if (notes.empty()) {
            std::cout << "  (keine Notizen)\n";
        } else {
            for (int i = 0; i < (int)notes.size(); i++) {
                auto& n = notes[i];
                std::cout << "  " << std::setw(3) << (i+1) << ". ["
                          << n->createdAt.substr(0,16) << "]";
                if (!n->author.empty()) std::cout << " " << n->author;
                std::cout << "\n";
                // Wrap body at 72 chars:
                std::string body = n->body;
                for (std::size_t pos = 0; pos < body.size(); pos += 72)
                    std::cout << "       " << body.substr(pos, 72) << "\n";
                std::cout << "\n";
            }
        }
        std::cout << "\n  1.Notiz hinzufuegen  ";
        if (!notes.empty()) std::cout << "2.<#> loeschen  ";
        std::cout << "0.Zurueck\n";
        int hi = notes.empty() ? 1 : 2;
        int ch = readInt("Wahl", 0, hi);
        if (ch == 0) break;
        if (ch == 1) {
            std::string body = readLine("Notiz: ");
            if (body.empty()) continue;
            std::string author = readOpt("Autor/Person-ID (leer=anonym): ");
            auto n = Rosenholz::Note::create(entityType, entityId, body, author);
            if (n) {
                printOk("  >> Notiz gespeichert: " + n->noteId);
                if (!mfsDir.empty())
                    Rosenholz::Note::writeNotesFile(entityType, entityId, mfsDir);
            }
        } else if (ch == 2 && !notes.empty()) {
            int pick = readInt("Notiz # loeschen", 1, (int)notes.size());
            notes[pick-1]->remove();
            if (!mfsDir.empty())
                Rosenholz::Note::writeNotesFile(entityType, entityId, mfsDir);
            printOk("  >> Geloescht.");
        }
    }
}
} // namespace CLI

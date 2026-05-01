#pragma once
// ============================================================
// cli_common.h  —  Shared declarations for all CLI modules
//
// Every cli_*.cpp includes this header. It provides:
//   - All model and workflow includes
//   - Input primitives (readLine, readInt, …)
//   - Display primitives (hdr, hr, fval, fdate)
//   - Helper predicates (isId)
//   - Forward declarations for every public CLI function
// ============================================================

#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/akt/Folder.h"
#include "../model/person/Person.h"
#include "../model/team/Team.h"
#include "../model/f18/F18Operation.h"
#include "../model/f18/F18OperationStep.h"
#include "../model/f18/Communication.h"
#include "../workflow/F77Workflow.h"
#include "../model/NavigationContext.h"
#include "../model/Note.h"
#include "../model/HistoryLog.h"
#include "../model/TreeBuilder.h"
#include "../model/StatusColor.h"
#include "../model/WatchPoller.h"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <memory>
#include <unistd.h>
#include <cstdlib>


// ── ANSI color support ─────────────────────────────────────────────────────
namespace Color {

inline bool enabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* term = std::getenv("TERM");
        cached = ::isatty(STDOUT_FILENO) && term && std::string(term) != "dumb" ? 1 : 0;
    }
    return cached == 1;
}
inline std::string wrap(const std::string& text, const char* code) {
    if (!enabled()) return text;
    return std::string("\033[") + code + "m" + text + "\033[0m";
}
inline std::string bold(const std::string& s)    { return wrap(s, "1"); }
inline std::string dim(const std::string& s)     { return wrap(s, "2"); }
inline std::string red(const std::string& s)     { return wrap(s, "31"); }
inline std::string green(const std::string& s)   { return wrap(s, "32"); }
inline std::string yellow(const std::string& s)  { return wrap(s, "33"); }
inline std::string blue(const std::string& s)    { return wrap(s, "34"); }
inline std::string magenta(const std::string& s) { return wrap(s, "35"); }
inline std::string cyan(const std::string& s)    { return wrap(s, "36"); }

inline std::string statusColor(const std::string& status) {
    if (status == "in_work" || status == "aktiv")       return green(status);
    if (status == "locked"  || status == "gesperrt")    return yellow(status);
    if (status == "released"|| status == "freigegeben") return cyan(status);
    if (status == "closed"  || status == "geschlossen") return dim(status);
    return status;
}
inline std::string stepSymbolColored(Rosenholz::StepStatus s) {
    switch (s) {
        case Rosenholz::StepStatus::APPROVED:    return green("[OK]");
        case Rosenholz::StepStatus::REJECTED:    return red("[X ]");
        case Rosenholz::StepStatus::SKIPPED:     return dim("[~ ]");
        case Rosenholz::StepStatus::IN_PROGRESS: return yellow("[ >]");
        default:                                  return dim("[  ]");
    }
}
} // namespace Color

namespace CLI {

// ── Symbol → ASCII renderers (CLI only; Qt will use icons instead) ────────
inline const char* stepSymbolStr(Rosenholz::StepSymbol s) {
    switch (s) {
        case Rosenholz::StepSymbol::APPROVED:    return "[OK]";
        case Rosenholz::StepSymbol::REJECTED:    return "[X ]";
        case Rosenholz::StepSymbol::SKIPPED:     return "[~ ]";
        case Rosenholz::StepSymbol::IN_PROGRESS: return "[ >]";
        case Rosenholz::StepSymbol::LOCKED:      return "[L ]";
        case Rosenholz::StepSymbol::PENDING:     return "[  ]";
    }
    return "[  ]";
}
inline const char* stepSymbolShortStr(Rosenholz::StepSymbol s) {
    switch (s) {
        case Rosenholz::StepSymbol::APPROVED:    return "OK";
        case Rosenholz::StepSymbol::REJECTED:    return "X";
        case Rosenholz::StepSymbol::SKIPPED:     return "~";
        case Rosenholz::StepSymbol::IN_PROGRESS: return ">";
        case Rosenholz::StepSymbol::LOCKED:      return "L";
        case Rosenholz::StepSymbol::PENDING:     return " ";
    }
    return " ";
}
inline const char* workflowSymbolStr(Rosenholz::WorkflowSymbol s) {
    switch (s) {
        case Rosenholz::WorkflowSymbol::COMPLETED: return "[ABGESCHL]";
        case Rosenholz::WorkflowSymbol::LOCKED:    return "[GESPERRT]";
        case Rosenholz::WorkflowSymbol::CANCELLED: return "[ABGEBR  ]";
        case Rosenholz::WorkflowSymbol::ACTIVE:    return "[AKTIV   ]";
    }
    return "[?]";
}
inline const char* f18StepSymbolStr(Rosenholz::F18StepSymbol s) {
    switch (s) {
        case Rosenholz::F18StepSymbol::DONE:        return "OK";
        case Rosenholz::F18StepSymbol::SKIPPED:     return "--";
        case Rosenholz::F18StepSymbol::IN_PROGRESS: return " >";
        case Rosenholz::F18StepSymbol::PENDING:     return "  ";
    }
    return "  ";
}


// ── Input / output primitives (cli_utils.cpp) ─────────────────
std::string readLine(const std::string& prompt);
std::string readOpt(const std::string& prompt);
int         readInt(const std::string& prompt, int lo, int hi);
int         readChoice(const std::string& menuText, int lo, int hi); ///< ? shows menu
bool        yesno(const std::string& prompt);
std::string fval(const std::string& v);
std::string parseDate(const std::string& input); ///< "." "+1d" "+2w" "+3m" "+1y" shortcuts
std::string fdate(const std::string& d);
void        hdr(const std::string& title);
void        hr();

// Returns true when s contains '/' — the marker of a genId/RegNumber string.
bool isId(const std::string& s);

// Ctrl+C interrupt state — set by SIGINT handler, cleared by runShell after each command.
void cliMarkInterrupted();
void cliClearInterrupted();
bool cliIsInterrupted();

// Print a one-line confirmation. Writes to stdout.
void printOk(const std::string& msg);

// Print error to stderr and exit(1).
void die(const std::string& msg);
void printErr(const std::string& msg);

/// Show notes for any entity (list + add + delete + write _Notizen.txt).
void notesMenu(const std::string& entityType,
               const std::string& entityId,
               const std::string& mfsDir = "");  ///< Non-fatal error — print and return.

// ── F16 project commands (cli_f16.cpp) ────────────────────────
void cmdF16(const std::vector<std::string>& args);
void listProjects();
void printProject(const Rosenholz::F16& p);
void projectMenu(std::shared_ptr<Rosenholz::F16> p);
std::shared_ptr<Rosenholz::F16> createProjectWizard();

// ── F22 task commands (cli_f22.cpp) ───────────────────────────
void cmdF22(const std::vector<std::string>& args);
void listTasks(const std::string& projectId);
void printTask(const Rosenholz::F22& t);
void taskMenu(std::shared_ptr<Rosenholz::F22> t);
std::shared_ptr<Rosenholz::F22> createTaskWizard(const std::string& projectId);

// Guided wizard: asks user to pick F16 from list, then creates F22.
std::shared_ptr<Rosenholz::F22> createTaskWizardGuided();

// ── F18 operation commands (cli_f18.cpp) ──────────────────────
void cmdF18(const std::vector<std::string>& args);
void f18BrowserMenu(const std::string& taskId     = "",
                    const std::string& typeFilter = "");
std::shared_ptr<Rosenholz::F18Operation> createF18Wizard(
    const std::string& projectId = "",
    const std::string& taskId    = "",
    const std::string& type      = "");

// Guided wizard: asks user to pick F16 (and optionally F22), then creates F18.

// ── Document commands (cli_dok.cpp) ───────────────────────────
void cmdAkt(const std::vector<std::string>& args);
void printDocument(const Rosenholz::Folder& d);
void listDocuments(const std::vector<std::shared_ptr<Rosenholz::Folder>>& docs,
                   const std::string& title = "AKTEN");
void documentMenu(std::shared_ptr<Rosenholz::Folder> doc);
void documentBrowserMenu(const std::string& taskId  = "",
                         const std::string& f18OpId = "");

// Create document with known parent.
std::shared_ptr<Rosenholz::Folder> createDocumentWizard(
    const std::string& taskId  = "",
    const std::string& f18OpId = "");

// Guided: asks user which F16/F22/F18 to attach to.
std::shared_ptr<Rosenholz::Folder> createDocumentWizardGuided();

// Attach-or-create dialog (called from entity menus).
std::shared_ptr<Rosenholz::Folder> attachDocumentDialog(
    const std::string& projectId = "",
    const std::string& taskId    = "");

// ── F77 workflow commands (cli_f77.cpp) ───────────────────────
void cmdF77(const std::vector<std::string>& args);
void workflowMenu();
void instanceMenu(const std::string& workflowId);
void listWfInstances(const std::string& entityType = "",
                     const std::string& entityId   = "");
std::string startWfInstanceWizard(const std::string& entityType = "",
                                  const std::string& entityId   = "");

// ── Person commands (cli_per.cpp) ─────────────────────────────
void cmdPer(const std::vector<std::string>& args);
void listPersons();
void printPerson(const Rosenholz::Person& p);
std::shared_ptr<Rosenholz::Person> createPersonWizard();

// ── Team / Diensteinheit commands (cli_de.cpp) ────────────────
void cmdDe(const std::vector<std::string>& args);
void cmdTasks(const std::vector<std::string>& args);

// ── Navigation commands (cli_nav.cpp) ─────────────────────────────────────
void cmdCf(const std::vector<std::string>& args);   ///< change folder — navigate into entity
void cmdLf(const std::vector<std::string>& args);   ///< list folder — show children
void cmdLo(const std::vector<std::string>& args);   ///< list options — context-sensitive help

/// Returns (id, title) pairs for tab completion of cf command
std::vector<std::pair<std::string,std::string>> getContextChildren();

void teamMenu();

// ── Communication menu (cli_comm.cpp) ────────────────────────
void communicationMenu(const std::string& ownerId,
                       const std::string& ownerType);
void commDetailMenu(std::shared_ptr<Rosenholz::Communication> c);
std::vector<std::shared_ptr<Rosenholz::Communication>>
    listComms(const std::string& ownerId, const std::string& ownerType);

// ── System commands (cli_sys.cpp) ─────────────────────────────
void cmdStatus();
void cmdBackup();
void cmdMfs(const std::vector<std::string>& args);
void cmdLog(const std::string& level);
void cmdSearch(const std::string& query);
void globalSearch(const std::string& query);

    // ── F18 functions (defined in cli_f18.cpp) ──────────────
    void f18Menu(std::shared_ptr<Rosenholz::F18Operation> v);
    void printF18Operation(const Rosenholz::F18Operation& v);
    std::shared_ptr<Rosenholz::F18Operation> createF18WizardGuided();
} // namespace CLI



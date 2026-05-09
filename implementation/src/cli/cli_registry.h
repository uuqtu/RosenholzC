#pragma once
// ============================================================
// cli_registry.h  —  Command Registry (single source of truth)
//
// ARCHITECTURE
// ════════════
// Every CLI command is defined ONCE in kCommands[] (cli_registry.cpp).
// Dispatch, lo(), and Tab-completion all read from that single table.
//
// HOW TO ADD A COMMAND
// ════════════════════
//  1. Add ONE entry to kCommands[] in cli_registry.cpp:
//       { "cmd", "-cmd", ET::NONE, "hint text",
//         {"-sub1", "-sub2"},          // Tab-completable sub-args
//         ctx("cmd"),                  // contextHandler → cmdContextual("cmd",args)
//         global(cmdFoo) }             // globalHandler  → called for -cmd
//
//  2. Add the handler in cmdContextual (cli_nav.cpp):
//       if (cmd == "cmd") {
//           // CONTEXT GUARD at top:
//           if (!args.empty() && needsCtx && cur.type != EntityType::XXX) {
//               printErr("…erfordert XXX-Kontext"); return;
//           }
//           // Self-commands (operate on current entity):
//           if (sub == "-e") { /* call model */ return; }
//           // Global pass-through: cmdFoo(args); return;
//           // Unknown sub-arg:
//           printErr("Unbekannter Befehl: " + sub); return;
//       }
//
//  3. Add lo() hint to printContextHelp() for the relevant context.
//
// CONTEXT RULES
// ═════════════
//  plain cmd      → contextHandler → cmdContextual(cmd, args)  [context-aware]
//  -dash cmd      → globalHandler  → cmdXxx(args)              [always global]
//  "." cmd        → expands to current entity type's cmd       [context-aware]
//
//  Self sub-commands (-e, -v, -arc, -f77 etc.) require the matching context.
//  Global sub-commands (-n, -o, -so, -s) work from any context.
//  Unknown sub-commands → printErr()
//
// BUSINESS LOGIC RULE
// ═══════════════════
//  NO business logic in CLI. CLI only:
//    - reads user input
//    - calls model methods (save, update, archive, etc.)
//    - prints results
//  All state transitions, validation, and persistence live in model/.
// ============================================================
// cli_registry.h  —  Command Registry (single source of truth)
//
// ARCHITECTURE
// ════════════
// Every CLI command is ONE entry in kCommands[] in cli_registry.cpp.
// Dispatch, lo(), and Tab-completion all read from this table.
//
// CONTEXT VALIDITY
// ════════════════
// Each command defines which entity contexts accept it via validIn bitmask.
// CTX_NONE   = works anywhere (global commands and nav commands)
// CTX_F16    = only inside F16 entity context
// CTX_F22    = only inside F22 entity context
// CTX_F18    = only inside F18 entity context
// CTX_AKT    = only inside AKT entity context
// CTX_ANY    = any entity context (but NOT top level)
// Combine with |: CTX_F22|CTX_F18 = valid in F22 and F18 contexts.
//
// When a plain command is called outside its validIn contexts,
// dispatch() prints "unbekannter Befehl" and returns false.
// Dash commands (-form) bypass context validation (always global).
//
// CliCommand FIELDS
// ═════════════════
//   name           plain form: "f22"
//   dashName       dash form or nullptr: "-f22"
//   validIn        context bitmask (see above)
//   loHint         one-line help string shown by lo() in matching context
//   subHints       tab-completable sub-arguments (for "." and the command)
//   contextHandler called for plain "f22 -n"  — context-aware
//   globalHandler  called for dash "-f22 -n"  — always global, no context
//                  nullptr → falls back to contextHandler
//
// ADDING A COMMAND
// ════════════════
// 1. Add one CliCommand entry to kCommands[] in cli_registry.cpp
// 2. Set validIn to the contexts that make sense
// 3. Write contextHandler (no business logic — just call model layer)
// 4. lo() and tab-completion work automatically
// ============================================================
#include "../model/NavigationContext.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace CLI {

using Handler = std::function<void(const std::vector<std::string>&)>;

// Context validity bitmask — combine with |
using CtxMask = uint8_t;
constexpr CtxMask CTX_NONE   = 0x00;  ///< No context required (global)
constexpr CtxMask CTX_F16    = 0x01;  ///< Valid in F16 context
constexpr CtxMask CTX_F22    = 0x02;  ///< Valid in F22 context
constexpr CtxMask CTX_F18    = 0x04;  ///< Valid in F18 context
constexpr CtxMask CTX_AKT    = 0x08;  ///< Valid in AKT context
constexpr CtxMask CTX_ANY    = 0xFF;  ///< Valid in any context

/// Returns the CTX bitmask for the current nav entity type.
CtxMask currentCtxMask();

struct CliCommand {
    const char*  name;           ///< plain form: "f22"
    const char*  dashName;       ///< dash form or nullptr: "-f22"
    CtxMask      validIn;        ///< context bitmask for plain form dispatch
    const char*  loHint;         ///< one-line help for lo() (nullptr = hidden)
    std::vector<const char*> subHints; ///< tab-completable sub-commands
    Handler contextHandler;      ///< called for plain "f22 -n" (context-aware)
    Handler globalHandler;       ///< called for dash "-f22 -n" (global)
                                 ///< nullptr → contextHandler used for both
};

/// Returns the full command registry.
const std::vector<CliCommand>& registry();

/// Dispatch a command. Returns false when unknown or wrong context.
/// Plain form → contextHandler (checked against validIn).
/// Dash form  → globalHandler  (no context check).
bool dispatch(const std::string& cmd, const std::vector<std::string>& args);

/// Print context-sensitive help (lo / -h).
void printContextHelp();

/// Tab-completion candidates.
std::vector<std::string> completions(const std::string& prefix,
                                     const std::string& prev);

/// Returns context children for 'cd' Tab-completion.
std::vector<std::pair<std::string,std::string>> getContextChildren();

} // namespace CLI

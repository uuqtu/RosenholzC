#pragma once
// ============================================================
// cli_registry.h  —  Command Registry (Dispatcher)
//
// EXTENSIBILITY: To add a command, add ONE entry to kCommands[]
// in cli_registry.cpp. Dispatch, lo(), and Tab-completion all
// read from this single table automatically.
//
// Entry semantics:
//   name           short form (no dash): "f22", "tsk"
//   dashName       with dash or nullptr: "-f22", nullptr
//   context        EntityType filter for lo() display (NONE = always shown)
//   loHint         one-line help string in lo()
//   subHints       tab-completable sub-arguments
//   contextHandler called for plain form: "f22 -n"  (context-aware)
//   globalHandler  called for dash form:  "-f22 -n" (global, no context)
//                  nullptr → falls back to contextHandler
//
// RULE: dash = global (no context used), no-dash = contextual.
// dispatch() enforces this automatically from the handlers.
// ============================================================
#include "../model/NavigationContext.h"
#include <functional>
#include <string>
#include <vector>

namespace CLI {

using Handler = std::function<void(const std::vector<std::string>&)>;

struct CliCommand {
    const char*  name;           ///< plain form: "f22"
    const char*  dashName;       ///< dash form or nullptr: "-f22"
    Rosenholz::EntityType context; ///< ET::NONE = show in lo() everywhere
    const char*  loHint;         ///< one-line help for lo()
    std::vector<const char*> subHints; ///< tab-completable sub-commands
    Handler contextHandler;      ///< called for plain "f22 ..." (context-aware)
    Handler globalHandler;       ///< called for dash "-f22 ..." (global); nullptr→contextHandler
};

/// Returns the full command registry (single source of truth).
const std::vector<CliCommand>& registry();

/// Dispatch a command. Returns false if unknown.
/// Automatically routes: dash→globalHandler, plain→contextHandler.
bool dispatch(const std::string& cmd, const std::vector<std::string>& args);

/// Print context-sensitive help (lo / -h in context).
void printContextHelp();

/// Tab-completion candidates for a given prefix and previous token.
std::vector<std::string> completions(const std::string& prefix,
                                     const std::string& prev);

/// Returns context children for 'cd' Tab-completion.
std::vector<std::pair<std::string,std::string>> getContextChildren();

} // namespace CLI

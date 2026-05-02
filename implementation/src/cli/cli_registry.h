#pragma once
// ============================================================
// cli_registry.h  —  Command Registry (Dispatcher)
//
// EXTENSIBILITY: To add a new command, add ONE entry to the
// kCommands[] table in cli_registry.cpp. The dispatcher,
// lo() help display, and tab completion all read from this
// table automatically. No other files need to change.
//
// Entry fields:
//   name       short form, no dash: "f22", "tsk", "note"
//   dashName   with dash or nullptr: "-f22", nullptr
//   context    EntityType the command requires, or NONE for all
//   loHint     one-line help shown in lo() for that context
//   subHints   tab-completable sub-arguments: {"-n","-e","-v"}
//   handler    the actual implementation function
// ============================================================
#include "../model/NavigationContext.h"
#include <functional>
#include <string>
#include <vector>

namespace CLI {

struct CliCommand {
    const char*  name;       ///< short form without dash: "f22"
    const char*  dashName;   ///< with dash or nullptr: "-f22"
    Rosenholz::EntityType context; ///< EntityType::NONE = available everywhere
    const char*  loHint;     ///< one-line shown in lo() when in this context
    std::vector<const char*> subHints; ///< tab-completable sub-commands
    std::function<void(const std::vector<std::string>&)> handler;
};

/// Returns the full command registry.
const std::vector<CliCommand>& registry();

/// Dispatch: try to find and run a command.
/// Returns true if handled, false if unknown.
bool dispatch(const std::string& cmd, const std::vector<std::string>& args);

/// Print context-sensitive help (lo).
void printContextHelp();

/// Returns tab-completion candidates for a given prefix.
std::vector<std::string> completions(const std::string& prefix,
                                     const std::string& prev);

/// Context children for tab completion (cd command)
std::vector<std::pair<std::string,std::string>> getContextChildren();
} // namespace CLI

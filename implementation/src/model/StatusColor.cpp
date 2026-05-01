// ============================================================
// StatusColor.cpp  —  ANSI terminal color dispatch
// ============================================================
#include "StatusColor.h"
#include "Utils.h"
#include <unordered_map>
#include <unistd.h>

namespace Rosenholz {

static bool s_colorsEnabled = false;

// Color table — single place to change any color.
// Keyed on the canonical string value as stored in DB.
static const std::unordered_map<std::string, const char*> kColorMap = {
    {"released",     "\033[32m"},   // green   — authoritative
    {"completed",    "\033[32m"},   // green
    {"approved",     "\033[32m"},   // green
    {"done",         "\033[32m"},   // green
    {"in_work",      "\033[0m"},    // default (white)
    {"pending",      "\033[0m"},    // default
    {"active",       "\033[0m"},    // default
    {"scheduled",    "\033[0m"},    // default
    {"open",         "\033[0m"},    // default
    {"in_progress",  "\033[36m"},   // cyan    — in motion
    {"pre_released", "\033[36m"},   // cyan
    {"locked",       "\033[33m"},   // yellow  — blocked/waiting
    {"rejected",     "\033[31m"},   // red     — failure
    {"cancelled",    "\033[31m"},   // red
    {"inactive",     "\033[90m"},   // grey    — terminal/inactive
    {"closed",       "\033[90m"},   // grey
    {"skipped",      "\033[90m"},   // grey
};

void StatusColor::init() {
    s_colorsEnabled = (isatty(STDOUT_FILENO) == 1);
}

std::string StatusColor::ansi(const std::string& status) {
    if (!s_colorsEnabled) return "";
    auto it = kColorMap.find(status);
    return it != kColorMap.end() ? std::string(it->second) : "";
}

std::string StatusColor::reset() {
    return s_colorsEnabled ? "\033[0m" : "";
}

std::string StatusColor::colored(const std::string& status) {
    std::string a = ansi(status);
    return a.empty() ? status : a + status + reset();
}

} // namespace Rosenholz

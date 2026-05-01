// ============================================================
// StatusColor.cpp
// ============================================================
#include "StatusColor.h"
#include <unistd.h>   // isatty, STDOUT_FILENO

namespace Rosenholz {

static bool s_colorsEnabled = false;

void StatusColor::init() {
    s_colorsEnabled = (isatty(STDOUT_FILENO) == 1);
}

std::string StatusColor::ansi(const std::string& status) {
    if (!s_colorsEnabled) return "";
    // Colors follow a traffic-light model:
    if (status == "released")     return "\033[32m";  // green
    if (status == "completed")    return "\033[32m";  // green
    if (status == "approved")     return "\033[32m";  // green
    if (status == "in_work")      return "\033[0m";   // default/white
    if (status == "pending")      return "\033[0m";   // default
    if (status == "active")       return "\033[0m";   // default
    if (status == "in_progress")  return "\033[36m";  // cyan
    if (status == "pre_released") return "\033[36m";  // cyan
    if (status == "locked")       return "\033[33m";  // yellow
    if (status == "rejected")     return "\033[31m";  // red
    if (status == "cancelled")    return "\033[31m";  // red
    if (status == "closed")       return "\033[90m";  // dark grey
    if (status == "skipped")      return "\033[90m";  // dark grey
    return "";
}

std::string StatusColor::reset() {
    return s_colorsEnabled ? "\033[0m" : "";
}

std::string StatusColor::colored(const std::string& status) {
    std::string a = ansi(status);
    if (a.empty()) return status;
    return a + status + reset();
}

} // namespace Rosenholz

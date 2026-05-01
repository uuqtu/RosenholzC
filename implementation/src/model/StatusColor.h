#pragma once
// ============================================================
// StatusColor.h  —  ANSI terminal color for entity status
//
// Returns ANSI escape codes for known status values.
// Automatically disabled when stdout is not a TTY.
// ============================================================
#include <string>

namespace Rosenholz {

class StatusColor {
public:
    // Initialize once (checks isatty):
    static void init();

    // Returns ANSI color prefix for a status string.
    // Returns "" when colors are disabled or status is unknown.
    static std::string ansi(const std::string& status);

    // Reset sequence (always returns "" when colors disabled).
    static std::string reset();

    // Returns colored status string ready for display:
    //   color(status) → "\033[32mreleased\033[0m" or just "released"
    static std::string colored(const std::string& status);
};

} // namespace Rosenholz

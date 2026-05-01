#pragma once
// ============================================================
// WatchPoller.h  —  Passive polling for workflow task changes
//
// Runs a polling loop that compares open F77 task counts.
// Notifies to stdout when changes are detected.
// All detection logic in model; CLI only renders the output.
// ============================================================
#include <functional>
#include <string>

namespace Rosenholz {

class WatchPoller {
public:
    /// Start a blocking polling loop.
    /// onEvent: called with a description string when something changes.
    /// intervalSeconds: how often to poll (default 30).
    /// Exits when user presses Ctrl+C.
    static void run(std::function<void(const std::string&)> onEvent,
                    int intervalSeconds = 30);
};

} // namespace Rosenholz

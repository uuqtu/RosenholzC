#pragma once
// ============================================================
// WatchPoller.h  —  Background polling + MFS sync
//
// - Polls F77-Task changes and notifies via callback
// - Detects entity DB changes (updatedAt) and rewrites MFS files
// ============================================================
#include <functional>
#include <string>

namespace Rosenholz {

class WatchPoller {
public:
    /// Blocking polling loop. Notifies on F77-Task changes and runs MFS sync.
    /// Exits on Ctrl+C.
    static void run(std::function<void(const std::string&)> onEvent,
                    int intervalSeconds = 30);

    /// One-shot MFS sync: check all entities and rewrite changed MFS files.
    static void syncMFSNow(std::function<void(const std::string&)> onEvent);
};

} // namespace Rosenholz

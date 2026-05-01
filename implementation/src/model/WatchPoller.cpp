// ============================================================
// WatchPoller.cpp
// ============================================================
#include "WatchPoller.h"
#include "Utils.h"
#include "Note.h"
#include "../workflow/F77Task.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <csignal>
#include <atomic>

namespace Rosenholz {

static std::atomic<bool> s_watchStop{false};

static void watchSigint(int) { s_watchStop.store(true); }

void WatchPoller::run(std::function<void(const std::string&)> onEvent,
                      int intervalSeconds)
{
    s_watchStop.store(false);
    signal(SIGINT, watchSigint);

    // Baseline: current open task count
    int prevOpen = (int)F77_Task::loadOpen().size();

    onEvent("Watch gestartet. Pruefe alle " +
            std::to_string(intervalSeconds) + "s. Ctrl+C zum Beenden.");

    while (!s_watchStop.load()) {
        // Sleep in small chunks to respond quickly to Ctrl+C:
        for (int i = 0; i < intervalSeconds * 10 && !s_watchStop.load(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (s_watchStop.load()) break;

        int nowOpen = (int)F77_Task::loadOpen().size();
        if (nowOpen != prevOpen) {
            std::ostringstream oss;
            int delta = nowOpen - prevOpen;
            if (delta > 0)
                oss << "  ▲ " << delta << " neue F77-Aufgabe(n) — " << nowOpen << " offen";
            else
                oss << "  ✓ " << (-delta) << " Aufgabe(n) erledigt — " << nowOpen << " offen";
            onEvent(oss.str());
            prevOpen = nowOpen;
        }
    }
    signal(SIGINT, SIG_DFL);
    onEvent("Watch beendet.");
}

} // namespace Rosenholz

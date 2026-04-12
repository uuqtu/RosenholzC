#pragma once
// ============================================================
// AppController.h  —  Application mode switch (CLI / UI)
//
// This is the central initialisation point. It:
//   1. Loads config (settings.json or .rh project file)
//   2. Initialises all 6 databases
//   3. Sets up logging
//   4. Ensures directory structure exists
//   5. Runs backup if due
//
// For CLI mode: run() returns after completing the command.
// For Qt/QML mode: the same init() will be called, then the
//   Qt event loop takes over. The model layer is unchanged.
//
// NO Qt headers are included here. When building with Qt,
// the app's main.cpp will call QApplication before init().
// ============================================================

#include <string>
#include <functional>

namespace RH {

enum class AppMode { CLI, UI };

class AppController {
public:
    static AppController& instance();

    AppController(const AppController&)            = delete;
    AppController& operator=(const AppController&) = delete;

    // ── Startup / shutdown ─────────────────────────────────
    /// Call this once before anything else.
    /// settingsPath: path to settings.json (or "" for auto-detect)
    /// rhFile:       path to .rh project file (overrides settingsPath base path)
    bool init(const std::string& settingsPath = "settings.json",
              const std::string& rhFile       = "",
              AppMode            mode         = AppMode::CLI);

    /// Graceful shutdown: flush, backup if needed, close DBs.
    void shutdown();

    AppMode mode() const { return m_mode; }

    // ── CLI mode ───────────────────────────────────────────
    /// Run an interactive console test session.
    /// Returns 0 on clean exit.
    int runCLI();

    // ── Status / diagnostics ───────────────────────────────
    bool isReady()    const { return m_ready; }
    void printStatus() const;

    // ── Qt/QML hook (call from QML engine, no Qt headers here)
    /// Set a function that Qt will call to emit a signal from logger output.
    /// This allows model log events to surface in the QML console.
    void setQtLogCallback(std::function<void(int level, const std::string&)> cb);

private:
    AppController() = default;

    bool m_ready { false };
    AppMode m_mode { AppMode::CLI };

    void setupLogging();
    void ensureDirectoryStructure();
    void runBackupIfDue();
};

} // namespace RH

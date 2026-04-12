// AppController.cpp
#include "AppController.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include "../core/Database.h"
#include "../core/FileOps.h"
#include "../core/BackupManager.h"
#include <iostream>

namespace RH {

AppController& AppController::instance() {
    static AppController inst;
    return inst;
}

bool AppController::init(const std::string& settingsPath,
                          const std::string& rhFile,
                          AppMode mode_)
{
    m_mode = mode_;

    // 1 ── Load configuration
    auto& cfg = Config::instance();
    cfg.load(settingsPath);
    if (!rhFile.empty()) cfg.loadProjectFile(rhFile);

    // 2 ── Set up logging (level from config)
    setupLogging();

    LOG_INFO("========================================");
    LOG_INFO("  Rosenholz PM — starting up");
    LOG_INFO("  Mode: " + std::string(mode_ == AppMode::CLI ? "CLI" : "UI"));
    LOG_INFO("  Base: " + cfg.basePath());
    LOG_INFO("========================================");

    // 3 ── Ensure directory structure
    ensureDirectoryStructure();

    // 4 ── Initialise all databases
    bool dbOk = DatabasePool::instance().initAll(
        cfg.basePath(),
        cfg.db().walMode,
        cfg.db().cacheSize);

    if (!dbOk) {
        LOG_ERROR("Database initialisation failed — cannot continue");
        return false;
    }

    // 5 ── Run backup if due
    if (cfg.backup().enabled) runBackupIfDue();

    m_ready = true;
    LOG_INFO("AppController ready");
    return true;
}

void AppController::shutdown() {
    LOG_INFO("AppController shutting down...");

    // Final backup on exit if enabled
    auto& cfg = Config::instance();
    if (cfg.backup().enabled) {
        LOG_INFO("Running shutdown backup");
        BackupManager::runFull(cfg.basePath(), cfg.backupDestPath(), cfg.backup().maxCopies);
    }

    DatabasePool::instance().closeAll();
    LOG_INFO("Shutdown complete.");
}

void AppController::setupLogging() {
    auto& cfg = Config::instance();
    auto& log = Logger::instance();

    std::string lvl = cfg.logLevel();
    if      (lvl == "DEBUG") log.setLevel(LogLevel::DEBUG);
    else if (lvl == "WARN")  log.setLevel(LogLevel::WARN);
    else if (lvl == "ERROR") log.setLevel(LogLevel::ERR);
    else                     log.setLevel(LogLevel::INFO);

    if (!cfg.logFile().empty())
        log.setLogFile(FileOps::joinPath(cfg.basePath(), cfg.logFile()));
}

void AppController::ensureDirectoryStructure() {
    auto& cfg = Config::instance();
    // Core directories
    FileOps::makeDirs(cfg.basePath());
    FileOps::makeDirs(FileOps::joinPath(cfg.basePath(), "db"));
    FileOps::makeDirs(FileOps::joinPath(cfg.basePath(), "documents"));
    FileOps::makeDirs(FileOps::joinPath(cfg.basePath(), "documents", "archived"));
    FileOps::makeDirs(FileOps::joinPath(cfg.basePath(), "backup"));

    // MFS tree
    if (cfg.mfs().enabled)
        FileOps::ensureMFSTree(cfg.mfsPath());

    LOG_DEBUG("Directory structure ready");
}

void AppController::runBackupIfDue() {
    auto& cfg = Config::instance();
    if (BackupManager::isDue(cfg.backupDestPath(), cfg.backup().intervalHours)) {
        LOG_INFO("Backup is due — running now");
        BackupManager::runFull(cfg.basePath(), cfg.backupDestPath(), cfg.backup().maxCopies);
    } else {
        LOG_DEBUG("Backup not yet due");
    }
}

void AppController::printStatus() const {
    auto& cfg = Config::instance();
    std::cout << "\n=== Rosenholz Status ===\n"
              << "  Ready:    " << (m_ready ? "YES" : "NO") << "\n"
              << "  Mode:     " << (m_mode == AppMode::CLI ? "CLI" : "UI") << "\n"
              << "  BasePath: " << cfg.basePath() << "\n"
              << "  LogLevel: " << cfg.logLevel() << "\n"
              << "  Backup:   " << (cfg.backup().enabled ? "enabled" : "disabled") << "\n"
              << "  MFS:      " << (cfg.mfs().enabled ? "enabled" : "disabled") << "\n"
              << "========================\n";
}

void AppController::setQtLogCallback(std::function<void(int, const std::string&)> cb) {
    // Bridge: wrap the typed callback into Logger's callback
    Logger::instance().setCallback([cb](LogLevel lvl, const std::string& msg) {
        cb(static_cast<int>(lvl), msg);
    });
}

// ── CLI test runner ──────────────────────────────────────────
int AppController::runCLI() {
    std::cout << "\nRosenholz PM — Console test session\n"
              << "Type 'help' for commands, 'exit' to quit\n\n";

    std::string line;
    while (true) {
        std::cout << "rh> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line == "status") { printStatus(); continue; }
        if (line == "help") {
            std::cout << "  status       — print system status\n"
                      << "  exit         — quit\n";
            continue;
        }
        std::cout << "Unknown command: " << line
                  << " (use main.cpp test suite for full testing)\n";
    }
    return 0;
}

} // namespace RH

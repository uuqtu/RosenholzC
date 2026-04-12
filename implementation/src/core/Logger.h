#pragma once
// ============================================================
// Logger.h  —  Verbosity-controlled logging subsystem
// Levels: DEBUG < INFO < WARN < ERROR
// Only messages at or above the current level are emitted.
// Thread-safe via mutex. Singleton access via Logger::instance().
// ============================================================

#include <string>
#include <fstream>
#include <mutex>
#include <functional>
#include <chrono>

namespace Rosenholz {

enum class LogLevel {
    DEBUG = 0,  ///< Finest detail — internal state, flow tracing
    INFO  = 1,  ///< Normal operational events
    WARN  = 2,  ///< Recoverable issues, degraded operation
    ERR   = 3   ///< Errors that require attention
};

class Logger {
public:
    // ── Singleton ──────────────────────────────────────────
    static Logger& instance();

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    // ── Configuration ──────────────────────────────────────
    /// Set minimum level — messages below this are discarded
    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    /// Also write to a log file (append mode)
    void setLogFile(const std::string& path);

    /// Optional callback — e.g. to push logs to QML
    void setCallback(std::function<void(LogLevel, const std::string&)> cb);

    // ── Emit ───────────────────────────────────────────────
    void debug(const std::string& msg, const std::string& ctx = "");
    void info (const std::string& msg, const std::string& ctx = "");
    void warn (const std::string& msg, const std::string& ctx = "");
    void error(const std::string& msg, const std::string& ctx = "");

    /// Generic emit — used internally and by macros
    void log(LogLevel level, const std::string& msg, const std::string& ctx = "");

private:
    Logger();
    ~Logger();

    LogLevel    m_level { LogLevel::INFO };
    std::ofstream m_file;
    std::mutex  m_mutex;
    std::function<void(LogLevel, const std::string&)> m_callback;

    std::string levelTag(LogLevel l) const;
    std::string timestamp() const;
};

// ── Convenience macros (include file/line in DEBUG) ────────
#define LOG_DEBUG(msg) Rosenholz::Logger::instance().debug(msg, __FUNCTION__)
#define LOG_INFO(msg)  Rosenholz::Logger::instance().info (msg, __FUNCTION__)
#define LOG_WARN(msg)  Rosenholz::Logger::instance().warn (msg, __FUNCTION__)
#define LOG_ERROR(msg) Rosenholz::Logger::instance().error(msg, __FUNCTION__)

} // namespace Rosenholz

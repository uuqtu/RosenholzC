// ============================================================
// Logger.cpp  —  Implementation of the verbosity-controlled logger
// ============================================================

#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace RH {

// ── Singleton ────────────────────────────────────────────────
Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() = default;
Logger::~Logger() {
    if (m_file.is_open())
        m_file.close();
}

// ── Configuration ────────────────────────────────────────────
void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_level = level;
}

LogLevel Logger::getLevel() const {
    return m_level;
}

void Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_file.is_open()) m_file.close();
    m_file.open(path, std::ios::app);
    if (!m_file.is_open())
        std::cerr << "[Logger] Could not open log file: " << path << "\n";
}

void Logger::setCallback(std::function<void(LogLevel, const std::string&)> cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_callback = std::move(cb);
}

// ── Convenience wrappers ─────────────────────────────────────
void Logger::debug(const std::string& msg, const std::string& ctx) { log(LogLevel::DEBUG, msg, ctx); }
void Logger::info (const std::string& msg, const std::string& ctx) { log(LogLevel::INFO,  msg, ctx); }
void Logger::warn (const std::string& msg, const std::string& ctx) { log(LogLevel::WARN,  msg, ctx); }
void Logger::error(const std::string& msg, const std::string& ctx) { log(LogLevel::ERR,   msg, ctx); }

// ── Core emit ────────────────────────────────────────────────
void Logger::log(LogLevel level, const std::string& msg, const std::string& ctx) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (level < m_level) return;

    std::ostringstream oss;
    oss << timestamp() << " " << levelTag(level);
    if (!ctx.empty()) oss << " [" << ctx << "]";
    oss << " " << msg;

    const std::string line = oss.str();
    std::cout << line << "\n";
    if (m_file.is_open()) m_file << line << "\n";
    if (m_callback) m_callback(level, line);
}

// ── Helpers ──────────────────────────────────────────────────
std::string Logger::levelTag(LogLevel l) const {
    switch (l) {
        case LogLevel::DEBUG: return "[DEBUG]";
        case LogLevel::INFO:  return "[INFO ]";
        case LogLevel::WARN:  return "[WARN ]";
        case LogLevel::ERR:   return "[ERROR]";
    }
    return "[?????]";
}

std::string Logger::timestamp() const {
    auto now   = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace RH

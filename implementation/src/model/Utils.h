#pragma once
// ============================================================
// Utils.h  —  Shared model utilities
// Included by every model .cpp to avoid code duplication.
// ============================================================
#include "../core/Database.h"
#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>

namespace RH {

/// Return current time as ISO-8601 string
inline std::string nowIso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream o;
    o << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return o.str();
}

/// Generate a random hex ID with given prefix
inline std::string genId(const std::string& prefix) {
    static std::mt19937_64 rng(std::random_device{}());
    std::ostringstream o;
    o << prefix << "_" << std::hex << rng();
    return o.str();
}

/// Return NULL BindParam when string is empty, TEXT otherwise.
/// Prevents FOREIGN KEY constraint failures on soft references.
inline BindParam ton(const std::string& s) {
    return s.empty() ? BindParam::null() : BindParam::text(s);
}

/// Safe string-to-int with default
inline int safeInt(const std::string& s, int def = 0) {
    return s.empty() ? def : std::stoi(s);
}

/// Safe string-to-double with default
inline double safeDbl(const std::string& s, double def = 0.0) {
    return s.empty() ? def : std::stod(s);
}

/// Get value from Row map safely
inline std::string rowGet(const Row& r, const std::string& k) {
    auto it = r.find(k);
    return it != r.end() ? it->second : "";
}

} // namespace RH

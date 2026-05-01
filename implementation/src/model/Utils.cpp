// ============================================================
// Utils.cpp  —  Shared model utility implementations
// ============================================================
#include "Utils.h"
#include "../core/RegNumber.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>

namespace Rosenholz {

// ── genId ─────────────────────────────────────────────────────
// Generates the next typed registration number.
// typePart maps to RegDept constants: "F16", "F22", "AKT", etc.
std::string genId(const std::string& typePart) {
    return RegNumberGenerator::next(typePart).toString();
}

// ── nowIso ────────────────────────────────────────────────────
// Returns current local time as ISO-8601: "2026-01-01T12:00:00"
std::string nowIso() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val{};
#ifdef _WIN32
    localtime_s(&tm_val, &t);
#else
    localtime_r(&t, &tm_val);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

// ── sanitiseRegNr ─────────────────────────────────────────────
// Replaces '/' with '_' for use as a filesystem path component.
// XV/F22/0001/26 → XV_F22_0001_26
std::string sanitiseRegNr(const std::string& regNr) {
    std::string s = regNr;
    std::replace(s.begin(), s.end(), '/', '_');
    return s;
}

} // namespace Rosenholz

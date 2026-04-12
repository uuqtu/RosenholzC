#pragma once
// ============================================================
// Utils.h  —  Shared model utilities
//
// ID FORMAT (DDR MfS inspired, from settings.json):
//   <Diensteinheit>/<Typ>/<Seq>/<Jahr>
//   e.g.  XV/F16/0042/2026   XV/F22/0017/2026
//
// The Diensteinheit Kuerzel (Roman numeral dept code) comes
// from Config::instance().registratur().diensteinheitKuerzel
// ============================================================
#include "../core/Database.h"
#include "../core/Config.h"
#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <mutex>

namespace Rosenholz {

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

/// Return current 4-digit year
inline int currentYear() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm.tm_year + 1900;
}

// ── DDR-style ID generation ─────────────────────────────────
// Format: <DE-Kuerzel>/<Typ>/<Seq0000>/<Jahr>
// e.g.  XV/F16/0042/2026
// Sequence is per-process monotonic (persisted via reg_number_sequences in core.db
// for entity types that use RegNumberGenerator; for internal IDs use this counter).

namespace detail {
    inline std::atomic<uint32_t>& seqCounter() {
        static std::atomic<uint32_t> s{1};
        return s;
    }
}

/// Generate a DDR-style internal ID.
/// typeCode: F16, F22, F18, DOK, MSR, QG, KPI, LL, DL, CR, AC, MTG, MS, RISK, TRK, PER, DE, etc.
inline std::string genId(const std::string& typeCode) {
    uint32_t seq = detail::seqCounter().fetch_add(1);
    int year = currentYear();

    // Get department code from config (may not be loaded yet → fallback "XX")
    std::string de = "XX";
    try {
        const std::string& cfg = Config::instance().registratur().diensteinheitKuerzel;
        if (!cfg.empty()) de = cfg;
    } catch (...) {}

    std::ostringstream o;
    o << de << "/" << typeCode << "/" << std::setw(4) << std::setfill('0') << seq
      << "/" << year;
    return o.str();
}

/// Return NULL BindParam when string is empty, TEXT otherwise.
/// Prevents FK constraint failures on soft references.
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

// ── MFS physical folder path helpers ───────────────────────
// Every item that gets materialised to the MFS tree lives under:
//   <mfsRoot>/<Typ>/<DE-Kuerzel>/<RegNr-Jahr>/
// e.g.  mfs/F16/XV/2026/F16_XV_0042_2026.txt
//       mfs/F22/XV/2026/F22_XV_0017_2026.txt
//       mfs/F16/XV/2026/F16_XV_0042_2026/DOK/DOK_XV_0001_2026_Projektcharter.txt
//
// Documents are always filed UNDER their parent entity's folder.
// This mirrors the physical Ablage: a Vorgang has a Hängeregister,
// all Dokumente go inside it — never loose in a shared folder.

inline std::string mfsEntityDir(const std::string& mfsRoot,
                                 const std::string& typeCode,
                                 const std::string& regNr) {
    // regNr format: DE/TYPE/SEQ/YEAR  → use year for folder
    // Fallback: just use the regNr directly sanitised
    std::string de   = "XX";
    std::string year = "0000";
    try {
        auto sl1 = regNr.find('/');
        auto sl3 = regNr.rfind('/');
        if (sl1 != std::string::npos) de   = regNr.substr(0, sl1);
        if (sl3 != std::string::npos) year = regNr.substr(sl3+1);
    } catch (...) {}

    // Build path: mfsRoot/TypeCode/DE/Year
    std::string path = mfsRoot;
    for (auto& seg : {typeCode, de, year})
        path += "/" + seg;
    return path;
}

/// Sanitise a reg number for use as filename component (replace / with _)
inline std::string sanitiseRegNr(const std::string& regNr) {
    std::string s = regNr;
    for (char& c : s) if (c == '/') c = '_';
    return s;
}

} // namespace Rosenholz

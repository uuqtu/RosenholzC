#pragma once
// ============================================================
// RegNumber.h  —  DDR-style registration number generator
//
// Format: {DE-Kuerzel}/{TypeCode}/{seq:04d}/{year}
// Example: XV/F16/0042/2026
// Sequences are per-type, stored in core.db.
// DE-Kuerzel comes from settings.json registratur block.
// ============================================================
// ============================================================
// RegNumber.h  —  Structured registration number generator
//
// Format: DEPT/SEQUENCE/YEAR  e.g. "XV/1234/2024"
//
// The sequence is persisted in core.db so it survives restarts
// and is safe under concurrent access via SQLite transactions.
//
// F16 projects, F22 tasks, and F18 incidents all use this
// system with different department codes.
// ============================================================

#include <string>
#include <cstdint>

namespace Rosenholz {

struct RegNumber {
    std::string dept;      ///< Roman numeral department code
    int64_t     sequence;  ///< Sequential number within year
    int         year;      ///< 4-digit year

    /// Formatted as DEPT/SEQ/YEAR
    std::string toString() const;

    /// Parse from string "DEPT/SEQ/YEAR"
    static RegNumber fromString(const std::string& s);

    bool isValid() const;
};

// ── Department codes (Registrierbereiche) ────────────────────
namespace RegDept {
    // Original MfS codes mapped to PM domains
    constexpr const char* PROJECT   = "F16";  ///< Projects (F16 Vorgang)
    constexpr const char* TASK      = "F22";  ///< Tasks (F22 Vorgangskartei)
    constexpr const char* INCIDENT  = "F18";  ///< Incidents (F18 Verhaftungskartei)
    constexpr const char* ARCHIVE   = "AU";   ///< Archived (Untersuchungsvorgang)
    constexpr const char* PERSON    = "HVA";  ///< Person registry
    constexpr const char* TEAM      = "DE";   ///< Diensteinheit
    constexpr const char* OPERATION = "F18";  ///< F18 Operation (vormals Incident)
    constexpr const char* RELEASE   = "F77";  ///< F77 Freigabe-Workflow (lifecycle)
}

class RegNumberGenerator {
public:
    /// Generate next reg number for given dept, persisted in core.db
    /// Thread-safe — uses SQLite transaction
    static RegNumber next(const std::string& dept);

    /// Preview without consuming (for display)
    static RegNumber peek(const std::string& dept);

    /// Format a reg number string
    static std::string format(const std::string& dept, int64_t seq, int year);

    /// Validate format
    static bool isValid(const std::string& regStr);

private:
    /// Returns current year
    static int currentYear();
};

} // namespace Rosenholz

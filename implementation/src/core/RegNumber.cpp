// ============================================================
// RegNumber.cpp  —  Structured registration number generator
// ============================================================

#include "RegNumber.h"
#include <iomanip>
#include <sstream>
#include <vector>
#include "Config.h"
#include "Database.h"
#include "Logger.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <stdexcept>

namespace Rosenholz {

// ── RegNumber struct ─────────────────────────────────────────
std::string RegNumber::toString() const {
    // Format: {de-kürzel}/{type}/{seq:04d}/{year2}
    // e.g.   XV/F16/0001/26
    // de-kürzel comes from Config; dept holds the type code (F16, F22, etc.)
    const std::string& de = Config::instance().registratur().diensteinheitKuerzel;
    std::ostringstream oss;
    oss << de << "/" << dept << "/"
        << std::setw(6) << std::setfill('0') << sequence
        << "/" << (year % 100);
    return oss.str();
}

RegNumber RegNumber::fromString(const std::string& s) {
    // Accepts both formats:
    //   Old: XV/0042/2026 (dept=XV, seq=42, year=2026)
    //   New: XV/F16/0001/26 (de=XV, dept=F16, seq=1, year2=26 → 2000+26)
    RegNumber rn;
    std::vector<std::string> parts;
    std::string tok; std::istringstream ss(s);
    while (std::getline(ss, tok, '/')) parts.push_back(tok);
    if (parts.size() == 4) {
        // New format: de / type / seq / year2
        rn.dept     = parts[1];
        rn.sequence = std::stoll(parts[2]);
        int y2      = std::stoi(parts[3]);
        rn.year     = (y2 < 100) ? 2000 + y2 : y2;
    } else if (parts.size() == 3) {
        // Old format: dept / seq / year
        rn.dept     = parts[0];
        rn.sequence = std::stoll(parts[1]);
        rn.year     = std::stoi(parts[2]);
    }
    return rn;
}

bool RegNumber::isValid() const {
    return !dept.empty() && sequence > 0 && year >= 2000;
}

// ── RegNumberGenerator ───────────────────────────────────────
int RegNumberGenerator::currentYear() {
    auto now  = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm.tm_year + 1900;
}

RegNumber RegNumberGenerator::next(const std::string& dept) {
    LOG_DEBUG("Generating next RegNumber for dept: " + dept);

    auto* db = DatabasePool::instance().get("core");
    if (!db) {
        LOG_ERROR("RegNumber::next — core DB not available");
        return {};
    }

    int year = currentYear();

    // Upsert sequence row and atomically increment
    db->beginTransaction();

    // Ensure row exists
    db->exec(
        "INSERT OR IGNORE INTO reg_number_sequences (reg_dept, reg_year, next_seq) VALUES (?,?,1);",
        {BindParam::text(dept), BindParam::int64(year)});

    // Read current
    auto scalar = db->queryScalar(
        "SELECT next_seq FROM reg_number_sequences WHERE reg_dept=? AND reg_year=?;",
        {BindParam::text(dept), BindParam::int64(year)});

    int64_t seq = scalar.empty() ? 1 : std::stoll(scalar);

    // Increment
    db->exec(
        "UPDATE reg_number_sequences SET next_seq=next_seq+1 WHERE reg_dept=? AND reg_year=?;",
        {BindParam::text(dept), BindParam::int64(year)});

    db->commitTransaction();

    RegNumber rn;
    rn.dept     = dept;
    rn.sequence = seq;
    rn.year     = year;

    LOG_INFO("RegNumber generated: " + rn.toString());
    return rn;
}

RegNumber RegNumberGenerator::peek(const std::string& dept) {
    auto* db = DatabasePool::instance().get("core");
    int year = currentYear();
    RegNumber rn;
    rn.dept = dept;
    rn.year = year;

    if (!db) { rn.sequence = 0; return rn; }

    auto scalar = db->queryScalar(
        "SELECT next_seq FROM reg_number_sequences WHERE reg_dept=? AND reg_year=?;",
        {BindParam::text(dept), BindParam::int64(year)});

    rn.sequence = scalar.empty() ? 1 : std::stoll(scalar);
    return rn;
}

std::string RegNumberGenerator::format(const std::string& dept, int64_t seq, int year) {
    const std::string& de = Config::instance().registratur().diensteinheitKuerzel;
    std::ostringstream oss;
    oss << de << "/" << dept << "/"
        << std::setw(6) << std::setfill('0') << seq
        << "/" << (year % 100);
    return oss.str();
}

bool RegNumberGenerator::isValid(const std::string& regStr) {
    return RegNumber::fromString(regStr).isValid();
}

} // namespace Rosenholz

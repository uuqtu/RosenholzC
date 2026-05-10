// ============================================================
// Utils.cpp  —  Shared model utility implementations
// ============================================================
#include "Utils.h"
#include <vector>
#include <algorithm>
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


// ── EntityStatus ──────────────────────────────────────────────────────────
const char* entityStatusToString(EntityStatus s) {
    switch (s) {
        case EntityStatus::IN_WORK:      return "in_work";
        case EntityStatus::LOCKED:       return "locked";
        case EntityStatus::PRE_RELEASED: return "pre_released";
        case EntityStatus::RELEASED:     return "released";
        case EntityStatus::CLOSED:       return "closed";
        case EntityStatus::CANCELLED:    return "cancelled";
    }
    return "in_work";
}
EntityStatus entityStatusFrom(const std::string& s) {
    if (s == "locked")       return EntityStatus::LOCKED;
    if (s == "pre_released") return EntityStatus::PRE_RELEASED;
    if (s == "released")     return EntityStatus::RELEASED;
    if (s == "closed")       return EntityStatus::CLOSED;
    if (s == "cancelled")    return EntityStatus::CANCELLED;
    return EntityStatus::IN_WORK;
}

// ── F24StepStatus ─────────────────────────────────────────────────────────
const char* f24StepStatusToString(F24StepStatus s) {
    switch (s) {
        case F24StepStatus::PENDING:     return "pending";
        case F24StepStatus::IN_PROGRESS: return "in_progress";
        case F24StepStatus::WAITING:     return "waiting";
        case F24StepStatus::BLOCKED:     return "blocked";
        case F24StepStatus::DONE:        return "done";
        case F24StepStatus::SKIPPED:     return "skipped";
        case F24StepStatus::APPROVED:    return "approved";
        case F24StepStatus::REJECTED:    return "rejected";
    }
    return "pending";
}
F24StepStatus f24StepStatusFrom(const std::string& s) {
    if (s == "in_progress") return F24StepStatus::IN_PROGRESS;
    if (s == "waiting")     return F24StepStatus::WAITING;
    if (s == "blocked")     return F24StepStatus::BLOCKED;
    if (s == "done")        return F24StepStatus::DONE;
    if (s == "skipped")     return F24StepStatus::SKIPPED;
    if (s == "approved")    return F24StepStatus::APPROVED;
    if (s == "rejected")    return F24StepStatus::REJECTED;
    return F24StepStatus::PENDING;
}

// ── CommType ──────────────────────────────────────────────────────────────
const char* commTypeToString(CommType t) {
    switch (t) {
        case CommType::MEETING: return "meeting";
        case CommType::MESSAGE: return "message";
        case CommType::CALL:    return "call";
        case CommType::EMAIL:   return "email";
        case CommType::REPORT:  return "report";
    }
    return "meeting";
}
CommType commTypeFrom(const std::string& s) {
    if (s == "message") return CommType::MESSAGE;
    if (s == "call")    return CommType::CALL;
    if (s == "email")   return CommType::EMAIL;
    if (s == "report")  return CommType::REPORT;
    return CommType::MEETING;
}

// ── CommStatus ────────────────────────────────────────────────────────────
const char* commStatusToString(CommStatus s) {
    switch (s) {
        case CommStatus::SCHEDULED:  return "scheduled";
        case CommStatus::COMPLETED:  return "completed";
        case CommStatus::CANCELLED:  return "cancelled";
    }
    return "scheduled";
}
CommStatus commStatusFrom(const std::string& s) {
    if (s == "completed")  return CommStatus::COMPLETED;
    if (s == "cancelled")  return CommStatus::CANCELLED;
    return CommStatus::SCHEDULED;
}

// ── TemplateStatus ────────────────────────────────────────────────────────
const char* templateStatusToString(TemplateStatus s) {
    return s == TemplateStatus::INACTIVE ? "inactive" : "active";
}
TemplateStatus templateStatusFrom(const std::string& s) {
    return s == "inactive" ? TemplateStatus::INACTIVE : TemplateStatus::ACTIVE;
}


std::string patternToSQLLike(const std::string& pattern) {
    bool hasWild = (pattern.find('*') != std::string::npos ||
                    pattern.find('%') != std::string::npos);
    if (!hasWild) {
        // No user wildcards → substring search.
        // Escape any literal SQL metachar _ in the pattern:
        std::string escaped;
        for (char c : pattern) {
            if (c == '_') escaped += "\_";
            else          escaped += c;
        }
        return "%" + escaped + "%";
    }
    std::string sql;
    sql.reserve(pattern.size() * 2);
    for (char c : pattern) {
        if      (c == '*') sql += '%';
        else if (c == '%') sql += '_';
        else if (c == '_') sql += "\_";
        else               sql += c;
    }
    return sql;
}

bool matchesPattern(const std::string& text, const std::string& pattern) {
    std::string t = text, p = pattern;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    std::transform(p.begin(), p.end(), p.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (p.find('*') == std::string::npos && p.find('%') == std::string::npos)
        return t.find(p) != std::string::npos;

    const std::size_t pn = p.size(), tn = t.size();
    std::vector<bool> prev(tn + 1, false), curr(tn + 1, false);
    prev[0] = true;
    for (std::size_t i = 1; i <= pn; ++i) {
        curr.assign(tn + 1, false);
        curr[0] = (p[i-1] == '*') && prev[0];
        for (std::size_t j = 1; j <= tn; ++j) {
            if      (p[i-1] == '*') curr[j] = prev[j] || curr[j-1];
            else if (p[i-1] == '%') curr[j] = prev[j-1];
            else                    curr[j] = prev[j-1] && (p[i-1] == t[j-1]);
        }
        prev = curr;
    }
    return prev[tn];
}

} // namespace Rosenholz

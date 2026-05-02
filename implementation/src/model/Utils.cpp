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

// ── F18StepStatus ─────────────────────────────────────────────────────────
const char* f18StepStatusToString(F18StepStatus s) {
    switch (s) {
        case F18StepStatus::PENDING:     return "pending";
        case F18StepStatus::IN_PROGRESS: return "in_progress";
        case F18StepStatus::WAITING:     return "waiting";
        case F18StepStatus::BLOCKED:     return "blocked";
        case F18StepStatus::DONE:        return "done";
        case F18StepStatus::SKIPPED:     return "skipped";
        case F18StepStatus::APPROVED:    return "approved";
        case F18StepStatus::REJECTED:    return "rejected";
    }
    return "pending";
}
F18StepStatus f18StepStatusFrom(const std::string& s) {
    if (s == "in_progress") return F18StepStatus::IN_PROGRESS;
    if (s == "waiting")     return F18StepStatus::WAITING;
    if (s == "blocked")     return F18StepStatus::BLOCKED;
    if (s == "done")        return F18StepStatus::DONE;
    if (s == "skipped")     return F18StepStatus::SKIPPED;
    if (s == "approved")    return F18StepStatus::APPROVED;
    if (s == "rejected")    return F18StepStatus::REJECTED;
    return F18StepStatus::PENDING;
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
    // If pattern has no user wildcards, wrap for substring match:
    bool hasWild = (pattern.find('*') != std::string::npos ||
                    pattern.find('%') != std::string::npos);
    if (!hasWild) return "%" + pattern + "%";

    std::string sql;
    sql.reserve(pattern.size() * 2);
    for (char c : pattern) {
        if      (c == '*') sql += '%';    // * → any number of chars in SQL
        else if (c == '%') sql += '_';    // % → exactly one char in SQL
        else if (c == '_') sql += "\_";  // escape SQL's own single-char wildcard
        else               sql += c;
    }
    return sql;
}
} // namespace Rosenholz

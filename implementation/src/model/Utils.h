#pragma once
// ============================================================
// Utils.h  —  Shared model utilities
//
// Provides genId, nowIso, sanitiseRegNr, and transitively
// includes the core infrastructure headers that all models need.
// ============================================================
#include <string>
#include <ostream>
#include <cstdint>
#include "../core/RegNumber.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/OperationResult.h"
#include "../core/Repository.h"

namespace Rosenholz {

// ── EntityStatus ──────────────────────────────────────────────────────────
// Shared lifecycle status for F16, F22, F18 entities.
// Mirrors DocumentRevision::RevState but for non-document entities.
// Stored as string in SQLite; convert with entityStatusToString / entityStatusFrom.
enum class EntityStatus {
    IN_WORK = 0,
    LOCKED,
    PRE_RELEASED,
    RELEASED,
    CLOSED,
    CANCELLED,
};
const char* entityStatusToString(EntityStatus s);
EntityStatus entityStatusFrom(const std::string& s);

// ── F18StepSymbol ──────────────────────────────────────────────────────────
// Display symbol for F18OperationStep — returned by model, mapped by UI.
enum class F18StepSymbol {
    PENDING,      ///< not yet started
    IN_PROGRESS,  ///< being worked
    WAITING,      ///< waiting on external input
    BLOCKED,      ///< blocked by predecessor or constraint
    DONE,         ///< terminal — completed
    SKIPPED,      ///< terminal — skipped/cancelled
};

// ── F18StepStatus ──────────────────────────────────────────────────────────
// Status of a single F18OperationStep.
enum class F18StepStatus {
    PENDING = 0,
    IN_PROGRESS,
    WAITING,      ///< blocked on external input (resumable)
    BLOCKED,      ///< blocked by predecessor/constraint (resumable)
    DONE,
    SKIPPED,
    APPROVED,
    REJECTED,
};
const char* f18StepStatusToString(F18StepStatus s);
F18StepStatus f18StepStatusFrom(const std::string& s);

// ── CommType ──────────────────────────────────────────────────────────────
// Type of a Communication entry.
enum class CommType {
    MEETING = 0,
    MESSAGE,
    CALL,
    EMAIL,
    REPORT,
};
const char* commTypeToString(CommType t);
CommType commTypeFrom(const std::string& s);

// ── CommStatus ────────────────────────────────────────────────────────────
enum class CommStatus { SCHEDULED = 0, COMPLETED, CANCELLED };
const char* commStatusToString(CommStatus s);
CommStatus commStatusFrom(const std::string& s);

// ── TemplateStatus ────────────────────────────────────────────────────────
// Status of an F77_WorkflowTemplate.
enum class TemplateStatus { ACTIVE = 0, INACTIVE };
const char* templateStatusToString(TemplateStatus s);
TemplateStatus templateStatusFrom(const std::string& s);


/// Generate a typed registration ID: genId("F22") → "XV/F22/0001/26"
std::string genId(const std::string& typePart);

/// Current UTC time as ISO-8601 string: "2026-01-01T12:00:00"
std::string nowIso();

/// Sanitise a registration number for filesystem use:
/// XV/F22/0001/26 → XV_F22_0001_26
std::string sanitiseRegNr(const std::string& regNr);


inline std::ostream& operator<<(std::ostream& os, TemplateStatus s) {
    return os << templateStatusToString(s);
}
} // namespace Rosenholz

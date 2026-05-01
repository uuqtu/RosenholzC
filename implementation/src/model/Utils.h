#pragma once
// ============================================================
// Utils.h  —  Shared model utilities
//
// Provides genId, nowIso, sanitiseRegNr, and transitively
// includes the core infrastructure headers that all models need.
// ============================================================
#include <string>
#include <cstdint>
#include "../core/RegNumber.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/OperationResult.h"
#include "../core/Repository.h"

namespace Rosenholz {

/// Generate a typed registration ID: genId("F22") → "XV/F22/0001/26"
std::string genId(const std::string& typePart);

/// Current UTC time as ISO-8601 string: "2026-01-01T12:00:00"
std::string nowIso();

/// Sanitise a registration number for filesystem use:
/// XV/F22/0001/26 → XV_F22_0001_26
std::string sanitiseRegNr(const std::string& regNr);

} // namespace Rosenholz

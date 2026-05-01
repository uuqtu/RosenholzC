#pragma once
// ============================================================
// HistoryLog.h  —  Persistent navigation history (session-crossing)
//
// Appends a line to basePath/history.log whenever an entity is opened.
// Survives restarts. NavigationStack calls record() on each push.
// ============================================================
#include "NavigationContext.h"
#include <vector>
#include <string>

namespace Rosenholz {

class HistoryLog {
public:
    static HistoryLog& instance();

    /// Record that the user opened this entity.
    void record(const EntityRef& ref);

    /// Return the N most recently opened unique entity refs.
    std::vector<EntityRef> recent(int n = 20) const;

    /// Set the file path (called once after config is loaded).
    void setPath(const std::string& path);

private:
    std::string logPath_;
};

} // namespace Rosenholz

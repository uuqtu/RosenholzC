#include "Utils.h"
// ============================================================
// HistoryLog.cpp
// ============================================================
#include "HistoryLog.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Rosenholz {

HistoryLog& HistoryLog::instance() {
    static HistoryLog h;
    return h;
}

void HistoryLog::setPath(const std::string& path) {
    logPath_ = path;
}

void HistoryLog::record(const EntityRef& ref) {
    if (logPath_.empty() || !ref.valid()) return;
    // line: timestamp|type|id|displayName
    std::ofstream f(logPath_, std::ios::app);
    if (!f) return;
    f << nowIso() << "|"
      << entityTypeLabel(ref.type) << "|"
      << ref.id << "|"
      << ref.displayName << "\n";
}

std::vector<EntityRef> HistoryLog::recent(int n) const {
    if (logPath_.empty()) return {};
    std::ifstream f(logPath_);
    if (!f) return {};

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) lines.push_back(line);
    }

    // Walk backwards, deduplicate by id, collect up to n:
    std::vector<EntityRef> result;
    std::vector<std::string> seen;
    for (int i = (int)lines.size() - 1; i >= 0 && (int)result.size() < n; --i) {
        std::istringstream ss(lines[i]);
        std::string ts, typeStr, id, name;
        if (!std::getline(ss, ts, '|'))   continue;
        if (!std::getline(ss, typeStr, '|')) continue;
        if (!std::getline(ss, id, '|'))   continue;
        if (!std::getline(ss, name))      name = id;

        if (std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
        seen.push_back(id);
        EntityRef ref;
        ref.type        = entityTypeFromString(typeStr);
        ref.id          = id;
        ref.displayName = name;
        ref.regNr       = id;
        if (ref.valid()) result.push_back(ref);
    }
    return result;
}

} // namespace Rosenholz

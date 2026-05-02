// ============================================================
// Stats.cpp  —  Datenbankzaehler
// ============================================================
#include "Stats.h"
#include "Database.h"
#include "../model/f16/F16.h"
#include <sqlite3.h>

namespace Rosenholz {

EntityCounts Stats::load() {
    EntityCounts c;
    auto& pool = DatabasePool::instance();

    auto count = [&](const char* db, const char* table) -> int {
        auto* d = pool.get(db);
        if (!d) return -1;
        auto rows = d->query("SELECT COUNT(*) AS n FROM " + std::string(table) + ";", {});
        if (!rows.empty()) { try { return std::stoi(rows[0].at("n")); } catch(...) {} }
        return 0;
    };

    c.f16      = count("f16",     "projects");
    c.f22      = count("f22",     "tasks");
    c.f18      = count("f18",     "f18_operations");
    c.akt      = count("akt",     "folders");
    c.f77Active= count("f77",     "f77_workflows");
    c.f77Tasks = count("f77task", "f77_tasks");
    c.notes    = count("f99",     "f99_entries");
    return c;
}

} // namespace Rosenholz

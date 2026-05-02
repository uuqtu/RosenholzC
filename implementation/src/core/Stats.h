#pragma once
// ============================================================
// Stats.h  —  Datenbankzaehler (Model-Schicht)
// ============================================================
#include <string>
#include <map>

namespace Rosenholz {

struct EntityCounts {
    int f16 = 0;
    int f22 = 0;
    int f18 = 0;
    int akt = 0;
    int f77Active = 0;
    int f77Tasks = 0;
    int notes = 0;
};

class Stats {
public:
    /// Query all entity counts from their respective databases.
    static EntityCounts load();
};

} // namespace Rosenholz

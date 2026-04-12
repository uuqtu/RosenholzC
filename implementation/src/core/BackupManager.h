#pragma once
// ============================================================
// BackupManager.h  —  Configurable backup of all DBs and files
//
// Backs up all 6 SQLite databases and the MFS tree.
// Keeps at most maxCopies backup sets.
// Can be triggered manually or on a time interval.
// ============================================================
#include <string>

namespace Rosenholz {

class BackupManager {
public:
    BackupManager() = delete;

    /// Backup all databases under basePath/db/ to backupDest/
    /// Returns number of files successfully backed up.
    static int backupDatabases(const std::string& basePath,
                               const std::string& backupDest,
                               int maxCopies = 7);

    /// Backup MFS tree (zip or copy entire folder)
    static bool backupMFS(const std::string& mfsRoot,
                          const std::string& backupDest,
                          int maxCopies = 7);

    /// Run a full backup (databases + MFS)
    static bool runFull(const std::string& basePath,
                        const std::string& backupDest,
                        int maxCopies = 7);

    /// Check if a backup is due based on last backup time and interval
    static bool isDue(const std::string& backupDest, int intervalHours);
};

} // namespace Rosenholz

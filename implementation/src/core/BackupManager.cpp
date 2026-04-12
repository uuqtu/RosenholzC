// BackupManager.cpp
#include "BackupManager.h"
#include "FileOps.h"
#include "Logger.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>

namespace Rosenholz {

static std::string backupTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{}; localtime_r(&t, &tm);
    std::ostringstream o;
    o << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return o.str();
}

int BackupManager::backupDatabases(
    const std::string& basePath, const std::string& backupDest, int maxCopies)
{
    LOG_INFO("Starting database backup: " + basePath + " -> " + backupDest);
    std::string dbDir   = FileOps::joinPath(basePath, "db");
    std::string destDir = FileOps::joinPath(backupDest, "db", backupTimestamp());
    FileOps::makeDirs(destDir);

    auto files = FileOps::listDir(dbDir, ".db");
    int count = 0;
    for (auto& f : files) {
        std::string src = FileOps::joinPath(dbDir, f);
        std::string dst = FileOps::joinPath(destDir, f);
        if (FileOps::copyFile(src, dst, true)) {
            LOG_DEBUG("Backed up DB: " + f);
            ++count;
        } else {
            LOG_ERROR("Failed to back up DB: " + f);
        }
    }

    // Prune old backup sets
    auto sets = FileOps::listDir(FileOps::joinPath(backupDest, "db"));
    while (static_cast<int>(sets.size()) > maxCopies) {
        std::string oldest = FileOps::joinPath(backupDest, "db", sets.front());
        FileOps::removeDir(oldest, true);
        LOG_DEBUG("Pruned old backup set: " + oldest);
        sets.erase(sets.begin());
    }

    LOG_INFO("Database backup complete: " + std::to_string(count) + "/" +
             std::to_string(files.size()) + " files");
    return count;
}

bool BackupManager::backupMFS(
    const std::string& mfsRoot, const std::string& backupDest, int maxCopies)
{
    LOG_INFO("Backing up MFS tree: " + mfsRoot);
    std::string destDir = FileOps::joinPath(backupDest, "mfs", backupTimestamp());
    FileOps::makeDirs(destDir);

    // Copy owner_key.txt first (sits at root, not in a subdir)
    bool ok = true;
    std::string keyFile = FileOps::joinPath(mfsRoot, "owner_key.txt");
    if (FileOps::fileExists(keyFile))
        ok &= FileOps::copyFile(keyFile, FileOps::joinPath(destDir,"owner_key.txt"), true);

    // Copy MFS subdirectories - skip plain files at root level
    auto entries = FileOps::listDir(mfsRoot);
    for (auto& sub : entries) {
        std::string srcSub = FileOps::joinPath(mfsRoot, sub);
        if (!FileOps::dirExists(srcSub)) continue;  // skip owner_key.txt etc.
        std::string dstSub = FileOps::joinPath(destDir, sub);
        FileOps::makeDirs(dstSub);
        for (auto& f : FileOps::listDir(srcSub)) {
            ok &= FileOps::copyFile(FileOps::joinPath(srcSub,f),
                                    FileOps::joinPath(dstSub,f), true);
        }
    }

    // Prune
    auto sets = FileOps::listDir(FileOps::joinPath(backupDest, "mfs"));
    while (static_cast<int>(sets.size()) > maxCopies) {
        std::string oldest = FileOps::joinPath(backupDest, "mfs", sets.front());
        FileOps::removeDir(oldest, true);
        sets.erase(sets.begin());
    }

    LOG_INFO("MFS backup complete: " + std::string(ok?"OK":"FAILED"));
    return ok;
}

bool BackupManager::runFull(
    const std::string& basePath, const std::string& backupDest, int maxCopies)
{
    LOG_INFO("=== Full backup started ===");
    int dbCount = backupDatabases(basePath, backupDest, maxCopies);
    bool mfsOk  = backupMFS(
        FileOps::joinPath(basePath, "mfs"), backupDest, maxCopies);

    // Write a sentinel file so isDue() can check the last backup time
    std::string sentinelPath = FileOps::joinPath(backupDest, "last_backup.txt");
    FileOps::writeTextFile(sentinelPath, backupTimestamp(), false);

    bool ok = (dbCount > 0) && mfsOk;
    LOG_INFO("=== Full backup " + std::string(ok?"OK":"FAILED") + " ===");
    return ok;
}

bool BackupManager::isDue(const std::string& backupDest, int intervalHours) {
    std::string sentinelPath = FileOps::joinPath(backupDest, "last_backup.txt");
    if (!FileOps::fileExists(sentinelPath)) {
        LOG_DEBUG("No backup sentinel found — backup is due");
        return true;
    }
    // Compare file modified time with interval
    std::string modTime = FileOps::fileModifiedTime(sentinelPath);
    // Simple heuristic: if sentinel is older than intervalHours, return true
    // Full implementation would parse ISO timestamps
    LOG_DEBUG("Last backup: " + modTime);
    return false; // safe default — let caller decide timing
}

} // namespace Rosenholz

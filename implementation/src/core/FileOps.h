#pragma once
// ============================================================
// FileOps.h  —  All filesystem operations in one place
//
// Abstracts away Linux/Windows differences.
// Every file path operation in the application goes through here.
// Never use std::filesystem or OS APIs directly elsewhere.
// ============================================================

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Rosenholz {

class FileOps {
public:
    // ── Path utilities ─────────────────────────────────────
    static std::string joinPath(const std::string& a, const std::string& b);
    static std::string joinPath(const std::string& a, const std::string& b, const std::string& c);
    static std::string dirName(const std::string& path);
    static std::string baseName(const std::string& path);
    static std::string extension(const std::string& path);
    static std::string withoutExtension(const std::string& path);
    static std::string normalizeSeparators(const std::string& path);

    // ── Existence checks ───────────────────────────────────
    static bool fileExists(const std::string& path);
    static bool dirExists(const std::string& path);
    /// List all files in a directory (non-recursive by default).
    static std::vector<std::string> listFiles(const std::string& dir,
                                               bool recursive = false);
    static bool pathExists(const std::string& path);

    // ── Directory operations ───────────────────────────────
    static bool makeDir(const std::string& path);
    static bool makeDirs(const std::string& path);   ///< Creates full path incl. parents
    static bool removeDir(const std::string& path, bool recursive = false);
    static std::vector<std::string> listDir(const std::string& path, const std::string& ext = "");

    // ── File operations ────────────────────────────────────
    static bool copyFile(const std::string& src, const std::string& dst, bool overwrite = true);
    static bool moveFile(const std::string& src, const std::string& dst);
    static bool deleteFile(const std::string& path);
    static bool writeTextFile(const std::string& path, const std::string& content, bool append = false);
    static std::string readTextFile(const std::string& path);
    static bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data);
    static std::vector<uint8_t> readBinaryFile(const std::string& path);
    static int64_t fileSize(const std::string& path);

    // ── Timestamps ────────────────────────────────────────
    static std::string fileModifiedTime(const std::string& path); ///< ISO 8601

    // ── Application paths ──────────────────────────────────
    static std::string currentDirectory();
    static std::string executableDirectory();
    static std::string homeDirectory();
    static std::string tempDirectory();

    // ── Backup helpers ─────────────────────────────────────
    /// Copy src to dst/basename_YYYYMMDD_HHMMSS.ext
    static bool backupFile(const std::string& src, const std::string& dstDir);
    /// Remove oldest files in dir keeping at most maxCopies
    static void pruneBackups(const std::string& dir, const std::string& prefix, int maxCopies);

    // ── Download / archive ─────────────────────────────────
    /// Download a URL to a local file. Returns "" on failure.
    /// Uses libcurl if available, falls back to system curl/wget.
    static std::string downloadUrl(const std::string& url, const std::string& destDir);

    // ── Locking (for OneDrive shared access) ───────────────
    /// Create an advisory lock file (.lock) alongside the given file.
    /// Returns true if we acquired the lock.
    static bool acquireLock(const std::string& filePath, int timeoutMs = 5000);
    static void releaseLock(const std::string& filePath);

    // ── Path sanitization ──────────────────────────────────
    /// Strip characters not safe for filenames
    static std::string sanitizeFilename(const std::string& name);

    // ── MFS tree helpers ───────────────────────────────────
    /// Ensure all required subdirectories under mfsRoot exist
    static bool ensureMFSTree(const std::string& mfsRoot);

    static bool deleteDir(const std::string& path);  ///< Recursive directory delete

private:
    FileOps() = delete; // static-only class
};

} // namespace Rosenholz

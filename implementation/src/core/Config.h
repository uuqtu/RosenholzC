#pragma once
// ============================================================
// Config.h  —  Application configuration manager
//
// Reads/writes settings.json in the application directory.
// Also handles .rh project files (used for double-click open).
//
// Settings structure (JSON):
// {
//   "basePath":       "/path/to/onedrive/rosenholz",
//   "logLevel":       "INFO",
//   "logFile":        "rosenholz.log",
//   "backup": {
//     "enabled":      true,
//     "intervalHours":24,
//     "maxCopies":    7,
//     "backupPath":   "/path/to/backup"
//   },
//   "github": {
//     "enabled":      false,
//     "repoUrl":      "",
//     "branch":       "main",
//     "token":        ""
//   },
//   "mfs": {
//     "enabled":      true,
//     "rootFolder":   "mfs"
//   },
//   "db": {
//     "walMode":      true,
//     "cacheSize":    -64000
//   }
// }
// ============================================================

#include <string>
#include <nlohmann/json.hpp>

namespace RH {

struct BackupConfig {
    bool        enabled       { false };
    int         intervalHours { 24 };
    int         maxCopies     { 7 };
    std::string backupPath;
};

struct GitHubConfig {
    bool        enabled { false };
    std::string repoUrl;
    std::string branch  { "main" };
    std::string token;
};

struct MFSConfig {
    bool        enabled    { true };
    std::string rootFolder { "mfs" };
};

struct DBConfig {
    bool walMode   { true };
    int  cacheSize { -64000 }; // negative = kibibytes
};

class Config {
public:
    static Config& instance();

    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;

    // ── Load / Save ────────────────────────────────────────
    /// Load from settings.json next to the executable (or given path)
    bool load(const std::string& settingsPath = "settings.json");

    /// Persist current settings back to disk
    bool save(const std::string& settingsPath = "settings.json") const;

    /// Load a .rh project file (overrides basePath and project name)
    bool loadProjectFile(const std::string& rhPath);

    /// Create a new .rh project file at basePath
    bool saveProjectFile(const std::string& rhPath) const;

    // ── Accessors ──────────────────────────────────────────
    const std::string& basePath()    const { return m_basePath; }
    const std::string& logLevel()    const { return m_logLevel; }
    const std::string& logFile()     const { return m_logFile; }
    const BackupConfig& backup()     const { return m_backup; }
    const GitHubConfig& github()     const { return m_github; }
    const MFSConfig&   mfs()         const { return m_mfs; }
    const DBConfig&    db()          const { return m_db; }
    const std::string& projectName() const { return m_projectName; }

    void setBasePath(const std::string& p)    { m_basePath = p; }
    void setLogLevel(const std::string& l)    { m_logLevel = l; }
    void setProjectName(const std::string& n) { m_projectName = n; }

    // ── Derived paths ──────────────────────────────────────
    /// Full path to a named database file under basePath/db/
    std::string dbPath(const std::string& dbName) const;

    /// Full path to the MFS root folder
    std::string mfsPath() const;

    /// Full path to the backup destination
    std::string backupDestPath() const;

private:
    Config() = default;

    void applyDefaults();
    void fromJson(const nlohmann::json& j);
    nlohmann::json toJson() const;

    std::string m_basePath;
    std::string m_logLevel    { "INFO" };
    std::string m_logFile     { "rosenholz.log" };
    std::string m_projectName { "Unnamed" };
    BackupConfig m_backup;
    GitHubConfig m_github;
    MFSConfig    m_mfs;
    DBConfig     m_db;
    std::string  m_settingsPath { "settings.json" };
};

} // namespace RH

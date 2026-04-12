// ============================================================
// Config.cpp  —  Application configuration manager
// ============================================================

#include "Config.h"
#include "Logger.h"
#include "FileOps.h"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace RH {

Config& Config::instance() {
    static Config inst;
    return inst;
}

// ── Load / Save ──────────────────────────────────────────────
bool Config::load(const std::string& settingsPath) {
    m_settingsPath = settingsPath;
    LOG_INFO("Loading config from: " + settingsPath);

    std::ifstream f(settingsPath);
    if (!f.is_open()) {
        LOG_WARN("settings.json not found — applying defaults and saving.");
        applyDefaults();
        save(settingsPath);
        return false;
    }

    try {
        json j;
        f >> j;
        fromJson(j);
        LOG_INFO("Config loaded. basePath=" + m_basePath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse settings.json: " + std::string(e.what()));
        applyDefaults();
        return false;
    }
}

bool Config::save(const std::string& settingsPath) const {
    LOG_DEBUG("Saving config to: " + settingsPath);
    std::ofstream f(settingsPath);
    if (!f.is_open()) {
        LOG_ERROR("Cannot write settings.json: " + settingsPath);
        return false;
    }
    f << toJson().dump(4);
    LOG_INFO("Config saved.");
    return true;
}

bool Config::loadProjectFile(const std::string& rhPath) {
    LOG_INFO("Loading .rh project file: " + rhPath);
    std::ifstream f(rhPath);
    if (!f.is_open()) {
        LOG_ERROR("Cannot open .rh file: " + rhPath);
        return false;
    }
    try {
        json j;
        f >> j;
        if (j.contains("basePath"))    m_basePath    = j["basePath"];
        if (j.contains("projectName")) m_projectName = j["projectName"];
        LOG_INFO("Project loaded: " + m_projectName + " at " + m_basePath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse .rh file: " + std::string(e.what()));
        return false;
    }
}

bool Config::saveProjectFile(const std::string& rhPath) const {
    json j;
    j["basePath"]    = m_basePath;
    j["projectName"] = m_projectName;
    j["version"]     = "1.0";
    j["appId"]       = "rosenholz";

    std::ofstream f(rhPath);
    if (!f.is_open()) {
        LOG_ERROR("Cannot write .rh file: " + rhPath);
        return false;
    }
    f << j.dump(4);
    LOG_INFO("Project file saved: " + rhPath);
    return true;
}

// ── Derived paths ────────────────────────────────────────────
std::string Config::dbPath(const std::string& dbName) const {
    return m_basePath + "/db/" + dbName;
}

std::string Config::mfsPath() const {
    return m_basePath + "/" + m_mfs.rootFolder;
}

std::string Config::backupDestPath() const {
    return m_backup.backupPath.empty()
        ? m_basePath + "/backup"
        : m_backup.backupPath;
}

// ── Private helpers ──────────────────────────────────────────
void Config::applyDefaults() {
    m_basePath    = FileOps::currentDirectory() + "/rosenholz_data";
    m_logLevel    = "INFO";
    m_logFile     = "rosenholz.log";
    m_projectName = "Unnamed";
    m_backup      = BackupConfig{};
    m_github      = GitHubConfig{};
    m_mfs         = MFSConfig{};
    m_db          = DBConfig{};
    LOG_INFO("Applied default configuration. basePath=" + m_basePath);
}

void Config::fromJson(const json& j) {
    if (j.contains("basePath"))    m_basePath    = j["basePath"];
    if (j.contains("logLevel"))    m_logLevel    = j["logLevel"];
    if (j.contains("logFile"))     m_logFile     = j["logFile"];
    if (j.contains("projectName")) m_projectName = j["projectName"];

    if (j.contains("backup")) {
        auto& b = j["backup"];
        if (b.contains("enabled"))       m_backup.enabled       = b["enabled"];
        if (b.contains("intervalHours")) m_backup.intervalHours = b["intervalHours"];
        if (b.contains("maxCopies"))     m_backup.maxCopies     = b["maxCopies"];
        if (b.contains("backupPath"))    m_backup.backupPath     = b["backupPath"];
    }
    if (j.contains("github")) {
        auto& g = j["github"];
        if (g.contains("enabled")) m_github.enabled = g["enabled"];
        if (g.contains("repoUrl")) m_github.repoUrl = g["repoUrl"];
        if (g.contains("branch"))  m_github.branch  = g["branch"];
        if (g.contains("token"))   m_github.token   = g["token"];
    }
    if (j.contains("mfs")) {
        auto& m = j["mfs"];
        if (m.contains("enabled"))    m_mfs.enabled    = m["enabled"];
        if (m.contains("rootFolder")) m_mfs.rootFolder = m["rootFolder"];
    }
    if (j.contains("db")) {
        auto& d = j["db"];
        if (d.contains("walMode"))   m_db.walMode   = d["walMode"];
        if (d.contains("cacheSize")) m_db.cacheSize = d["cacheSize"];
    }
}

nlohmann::json Config::toJson() const {
    json j;
    j["basePath"]    = m_basePath;
    j["logLevel"]    = m_logLevel;
    j["logFile"]     = m_logFile;
    j["projectName"] = m_projectName;
    j["backup"]  = { {"enabled", m_backup.enabled},
                     {"intervalHours", m_backup.intervalHours},
                     {"maxCopies", m_backup.maxCopies},
                     {"backupPath", m_backup.backupPath} };
    j["github"]  = { {"enabled", m_github.enabled},
                     {"repoUrl", m_github.repoUrl},
                     {"branch", m_github.branch},
                     {"token", m_github.token} };
    j["mfs"]     = { {"enabled", m_mfs.enabled},
                     {"rootFolder", m_mfs.rootFolder} };
    j["db"]      = { {"walMode", m_db.walMode},
                     {"cacheSize", m_db.cacheSize} };
    return j;
}

} // namespace RH

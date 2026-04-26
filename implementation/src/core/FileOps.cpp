// ============================================================
// FileOps.cpp  —  Cross-platform filesystem operations
// Supports Linux and Windows (Win32 API / POSIX)
// ============================================================

#include "FileOps.h"
#include "Logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <chrono>
#include <thread>
#include <filesystem>

#ifdef _WIN32
  #include <windows.h>
  #include <shlobj.h>
  #include <direct.h>
  #define PATH_SEP '\\'
  #define PATH_SEP_STR "\\"
#else
  #include <unistd.h>
  #include <dirent.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <pwd.h>
  #include <libgen.h>
  #define PATH_SEP '/'
  #define PATH_SEP_STR "/"
#endif

namespace Rosenholz {

// ── Path utilities ───────────────────────────────────────────
std::string FileOps::joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '/' || last == '\\')
        return normalizeSeparators(a + b);
    return normalizeSeparators(a + PATH_SEP_STR + b);
}

std::string FileOps::joinPath(const std::string& a, const std::string& b, const std::string& c) {
    return joinPath(joinPath(a, b), c);
}

std::string FileOps::normalizeSeparators(const std::string& path) {
    std::string out = path;
#ifdef _WIN32
    std::replace(out.begin(), out.end(), '/', '\\');
#else
    std::replace(out.begin(), out.end(), '\\', '/');
#endif
    return out;
}

std::string FileOps::dirName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

std::string FileOps::baseName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

std::string FileOps::extension(const std::string& path) {
    std::string base = baseName(path);
    size_t dot = base.find_last_of('.');
    if (dot == std::string::npos) return "";
    return base.substr(dot);
}

std::string FileOps::withoutExtension(const std::string& path) {
    size_t dot = path.find_last_of('.');
    size_t sep = path.find_last_of("/\\");
    if (dot == std::string::npos) return path;
    if (sep != std::string::npos && dot < sep) return path;
    return path.substr(0, dot);
}

// ── Existence checks ─────────────────────────────────────────
bool FileOps::fileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st{};
    return (stat(path.c_str(), &st) == 0) && S_ISREG(st.st_mode);
#endif
}

bool FileOps::dirExists(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st{};
    return (stat(path.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
#endif
}

bool FileOps::pathExists(const std::string& path) {
    return fileExists(path) || dirExists(path);
}

// ── Directory operations ─────────────────────────────────────
bool FileOps::makeDir(const std::string& path) {
    if (dirExists(path)) return true;
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), nullptr) != 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

bool FileOps::makeDirs(const std::string& path) {
    if (dirExists(path)) return true;
    // Create each component
    std::string cur;
    for (char c : normalizeSeparators(path)) {
        cur += c;
        if (c == PATH_SEP || c == '/') {
            if (!cur.empty() && !dirExists(cur))
                makeDir(cur);
        }
    }
    return makeDir(path);
}

bool FileOps::removeDir(const std::string& path, bool recursive) {
    if (!dirExists(path)) return true;
    if (recursive) {
        auto entries = listDir(path);
        for (auto& e : entries) {
            std::string full = joinPath(path, e);
            if (dirExists(full)) removeDir(full, true);
            else deleteFile(full);
        }
    }
#ifdef _WIN32
    return RemoveDirectoryA(path.c_str()) != 0;
#else
    return rmdir(path.c_str()) == 0;
#endif
}

std::vector<std::string> FileOps::listDir(const std::string& path, const std::string& ext) {
    std::vector<std::string> result;
#ifdef _WIN32
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA((path + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return result;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        if (!ext.empty() && extension(name) != ext) continue;
        result.push_back(name);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) return result;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        if (!ext.empty() && extension(name) != ext) continue;
        result.push_back(name);
    }
    closedir(dir);
#endif
    std::sort(result.begin(), result.end());
    return result;
}

// ── File operations ──────────────────────────────────────────
bool FileOps::copyFile(const std::string& src, const std::string& dst, bool overwrite) {
    if (!overwrite && fileExists(dst)) return false;
    makeDirs(dirName(dst));
#ifdef _WIN32
    return CopyFileA(src.c_str(), dst.c_str(), !overwrite) != 0;
#else
    std::ifstream in(src, std::ios::binary);
    if (!in) { LOG_ERROR("copyFile: cannot open src: " + src); return false; }
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out) { LOG_ERROR("copyFile: cannot open dst: " + dst); return false; }
    out << in.rdbuf();
    return true;
#endif
}

bool FileOps::moveFile(const std::string& src, const std::string& dst) {
    makeDirs(dirName(dst));
    if (std::rename(src.c_str(), dst.c_str()) == 0) return true;
    // rename across filesystems — copy + delete
    if (copyFile(src, dst, true)) return deleteFile(src);
    return false;
}

bool FileOps::deleteFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

bool FileOps::writeTextFile(const std::string& path, const std::string& content, bool append) {
    makeDirs(dirName(path));
    std::ofstream f(path, append ? (std::ios::app) : (std::ios::trunc));
    if (!f.is_open()) { LOG_ERROR("writeTextFile: cannot open: " + path); return false; }
    f << content;
    return true;
}

std::string FileOps::readTextFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) { LOG_WARN("readTextFile: not found: " + path); return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool FileOps::writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    makeDirs(dirName(path));
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { LOG_ERROR("writeBinaryFile: cannot open: " + path); return false; }
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return true;
}

std::vector<uint8_t> FileOps::readBinaryFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { LOG_WARN("readBinaryFile: not found: " + path); return {}; }
    return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

int64_t FileOps::fileSize(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &d)) return -1;
    LARGE_INTEGER sz;
    sz.HighPart = d.nFileSizeHigh;
    sz.LowPart  = d.nFileSizeLow;
    return sz.QuadPart;
#else
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return -1;
    return st.st_size;
#endif
}

// ── Timestamps ───────────────────────────────────────────────
std::string FileOps::fileModifiedTime(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &d)) return "";
    SYSTEMTIME st{};
    FileTimeToSystemTime(&d.ftLastWriteTime, &st);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
#else
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return "";
    std::tm* tm = std::localtime(&st.st_mtime);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    return buf;
#endif
}

// ── Application paths ────────────────────────────────────────
std::string FileOps::currentDirectory() {
    char buf[4096];
#ifdef _WIN32
    _getcwd(buf, sizeof(buf));
#else
    if (getcwd(buf, sizeof(buf)) == nullptr) buf[0] = '\0';
#endif
    return normalizeSeparators(buf);
}

std::string FileOps::homeDirectory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, buf);
    return normalizeSeparators(buf);
#else
    const char* home = getenv("HOME");
    if (home) return home;
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/tmp";
#endif
}

std::string FileOps::executableDirectory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return dirName(normalizeSeparators(buf));
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (len == -1) return currentDirectory();
    buf[len] = '\0';
    return dirName(normalizeSeparators(buf));
#endif
}

std::string FileOps::tempDirectory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    return normalizeSeparators(buf);
#else
    const char* tmp = getenv("TMPDIR");
    return tmp ? tmp : "/tmp";
#endif
}

// ── Backup helpers ───────────────────────────────────────────
bool FileOps::backupFile(const std::string& src, const std::string& dstDir) {
    if (!fileExists(src)) { LOG_WARN("backupFile: src not found: " + src); return false; }
    makeDirs(dstDir);

    // Build timestamped destination name
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char ts[20];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm);

    std::string stem = withoutExtension(baseName(src));
    std::string ext  = extension(src);
    std::string dst  = joinPath(dstDir, stem + "_" + ts + ext);

    LOG_DEBUG("Backing up " + src + " -> " + dst);
    return copyFile(src, dst, true);
}

void FileOps::pruneBackups(const std::string& dir, const std::string& prefix, int maxCopies) {
    auto files = listDir(dir);
    std::vector<std::string> matching;
    for (auto& f : files)
        if (f.substr(0, prefix.size()) == prefix)
            matching.push_back(f);

    std::sort(matching.begin(), matching.end()); // oldest first by timestamp
    while (static_cast<int>(matching.size()) > maxCopies) {
        std::string oldest = joinPath(dir, matching.front());
        LOG_DEBUG("Pruning old backup: " + oldest);
        deleteFile(oldest);
        matching.erase(matching.begin());
    }
}

// ── Download ─────────────────────────────────────────────────
std::string FileOps::downloadUrl(const std::string& url, const std::string& destDir) {
    makeDirs(destDir);

    // Derive a safe filename from the URL
    std::string fname = url;
    size_t q = fname.find('?');
    if (q != std::string::npos) fname = fname.substr(0, q);
    fname = sanitizeFilename(baseName(fname));
    if (fname.empty() || fname == "/") fname = "download";

    std::string dest = joinPath(destDir, fname);
    LOG_INFO("Downloading: " + url + " -> " + dest);

    // Try libcurl via system command (robust fallback)
#ifdef _WIN32
    std::string cmd = "curl -L -s -o \"" + dest + "\" \"" + url + "\" 2>nul";
#else
    std::string cmd = "curl -L -s -o \"" + dest + "\" \"" + url + "\" 2>/dev/null";
    if (system(("which curl >/dev/null 2>&1") ) != 0)
        cmd = "wget -q -O \"" + dest + "\" \"" + url + "\" 2>/dev/null";
#endif
    int rc = system(cmd.c_str());
    if (rc != 0 || !fileExists(dest)) {
        LOG_ERROR("Download failed for: " + url);
        return "";
    }
    LOG_INFO("Download complete: " + dest + " (" + std::to_string(fileSize(dest)) + " bytes)");
    return dest;
}

// ── Locking ──────────────────────────────────────────────────
bool FileOps::acquireLock(const std::string& filePath, int timeoutMs) {
    std::string lockPath = filePath + ".lock";
    int elapsed = 0;
    const int interval = 100; // ms

    while (fileExists(lockPath) && elapsed < timeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        elapsed += interval;
        LOG_DEBUG("Waiting for lock: " + lockPath);
    }

    if (fileExists(lockPath)) {
        LOG_WARN("Lock timeout on: " + lockPath);
        return false;
    }

    // Write our PID into the lock file
#ifdef _WIN32
    std::string pid = std::to_string(GetCurrentProcessId());
#else
    std::string pid = std::to_string(getpid());
#endif
    return writeTextFile(lockPath, pid, false);
}

void FileOps::releaseLock(const std::string& filePath) {
    std::string lockPath = filePath + ".lock";
    if (fileExists(lockPath)) {
        deleteFile(lockPath);
        LOG_DEBUG("Released lock: " + lockPath);
    }
}

// ── Sanitize ─────────────────────────────────────────────────
std::string FileOps::sanitizeFilename(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == ' ')
            out += c;
        else
            out += '_';
    }
    return out;
}

// ── MFS tree ─────────────────────────────────────────────────
bool FileOps::ensureMFSTree(const std::string& mfsRoot) {
    LOG_INFO("Ensuring MFS directory tree at: " + mfsRoot);
    // Only F16/ is the top-level filing structure.
    // All other entities (F22, F18, DOK, Persons) are filed
    // UNDER their parent F16 Hängeregister — no standalone folders.
    // Everything is cross-referenced; no folder makes sense alone.
    bool ok = makeDirs(joinPath(mfsRoot, "F16"));
    // owner_key.txt is written on demand — it maps all IDs to real names
    return ok;
}

}
namespace Rosenholz {
std::vector<std::string> FileOps::listFiles(const std::string& dir, bool recursive) {
    std::vector<std::string> result;
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return result;
    if (recursive) {
        for (auto& e : fs::recursive_directory_iterator(dir, ec))
            if (e.is_regular_file(ec)) result.push_back(e.path().string());
    } else {
        for (auto& e : fs::directory_iterator(dir, ec))
            if (e.is_regular_file(ec)) result.push_back(e.path().string());
    }
    return result;
}


bool FileOps::deleteDir(const std::string& path) {
    if (path.empty()) return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(path, ec)) return true;  // already gone
    fs::remove_all(path, ec);
    return !ec;
}


} // namespace Rosenholz (listFiles)

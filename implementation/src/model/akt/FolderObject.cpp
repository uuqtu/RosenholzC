// ============================================================
// FolderObject.cpp
// ============================================================
#include <regex>
#include "FolderObject.h"
#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#endif
#include <set>
#include "../Utils.h"
#include "../../core/FileOps.h"
#include "../../core/Config.h"
#include "../../model/akt/ArchiveStore.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace Rosenholz {

// ── Internal helpers ─────────────────────────────────────────
static auto t_(const std::string& s) { return BindParam::text(s); }
static auto i_(int64_t v)             { return BindParam::int64(v); }

Database* FolderObject::db() {
    return DatabasePool::instance().get("akt");
}

// ── CRUD + internal helpers ─────────────────────────────────────────────────
std::string FolderObject::sanitiseForFilename(const std::string& s) {
    return FileOps::sanitizeFilename(s);
}

// ── CRUD ─────────────────────────────────────────────────────
void FolderObject::fromRow(const Row& r) {
    auto g = [&](const char* k) -> std::string {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    auto gb = [&](const char* k) -> bool { return g(k) == "1"; };
    objectId      = g("object_id");
    folderId    = g("folder_id");
    rev           = g("rev").empty() ? 1u : (uint32_t)std::stoi(g("rev"));
    originalName  = g("original_name");
    storedFileName   = g("stored_file_name");
    filePath       = g("file_path");
    contentHash   = g("content_hash");
    contentSize   = g("content_size").empty() ? 0 : std::stoll(g("content_size"));
    format        = g("format");
    committed     = gb("committed");
    createdAt     = g("created_at");
    updatedAt     = g("updated_at");
}

OperationResult FolderObject::save() const {
    auto* d = db();
    if (!d) return OperationResult::DB_ERROR;
    bool ok = d->exec(R"(
        INSERT OR REPLACE INTO folder_objects
        (object_id,folder_id,rev,original_name,stored_file_name,file_path,
         content_hash,content_size,format,source_url,display_name,description,committed,created_at,updated_at)
        VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);)",
        {BindParam::text(objectId), BindParam::text(folderId),
         BindParam::int64(rev), BindParam::text(originalName),
         BindParam::text(storedFileName), BindParam::text(filePath),
         BindParam::text(contentHash), BindParam::int64(contentSize),
         BindParam::text(format), BindParam::nullOrText(sourceUrl),
         BindParam::nullOrText(displayName_), BindParam::nullOrText(description),
         BindParam::int64(committed ? 1 : 0),
         BindParam::text(createdAt), BindParam::text(nowIso())});
    return ok ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult FolderObject::update() const { return save(); }

OperationResult FolderObject::remove() const {
    auto* d = db();
    if (!d) return OperationResult::DB_ERROR;
    bool ok = d->exec("DELETE FROM folder_objects WHERE object_id=? AND folder_id=?;",
                      {t_(objectId), t_(folderId)});
    return ok ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

// ── Display ─────────────────────────────────────────────────────────────────
std::string FolderObject::summary() const {
    std::ostringstream ss;
    ss << std::left << std::setw(7) << objectId
       << "  " << std::setw(40) << originalName.substr(0,38)
       << "  " << (committed ? "[LMDB]" : "[MFS] ")
       << "  " << (contentSize/1024) << " KB";
    return ss.str();
}

// ── Queries ──────────────────────────────────────────────────
std::shared_ptr<FolderObject> FolderObject::loadByDocAndId(
    const std::string& docId, const std::string& oid)
{
    auto* d = db();
    if (!d) return nullptr;
    // objectId is stored globally unique as docId+":"+oid in DB PK
    std::string pk = docId + ":" + oid;
    auto rows = d->query(
        "SELECT * FROM folder_objects WHERE object_id=?;", {t_(pk)});
    if (rows.empty()) return nullptr;
    auto o = std::make_shared<FolderObject>(); o->fromRow(rows[0]);
    return o;
}

std::vector<std::shared_ptr<FolderObject>> FolderObject::loadForRevision(
    const std::string& docId, uint32_t revNum)
{
    auto* d = db();
    std::vector<std::shared_ptr<FolderObject>> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM folder_objects "
        "WHERE folder_id=? AND rev=? ORDER BY original_name;",
        {t_(docId), i_(revNum)});
    for (auto& r : rows) {
        auto o = std::make_shared<FolderObject>(); o->fromRow(r);
        result.push_back(o);
    }
    return result;
}

// ── ID generation ────────────────────────────────────────────
// 5-char alphanumeric (uppercase), unique per document.
// Uses a counter stored in the DB: max seq +1.
// Encoded as base-36: 0-9 A-Z
// ── Private helpers (filename, objectId generation) ─────────────────────────
std::string FolderObject::nextObjectId(const std::string& docId) {
    auto* d = db();
    if (!d) return "XXXXX";

    // Count existing objects for this document (across all revisions)
    auto rows = d->query(
        "SELECT COUNT(*) as n FROM folder_objects WHERE folder_id=?;",
        {t_(docId)});
    int64_t seq = 1;
    if (!rows.empty()) {
        auto it = rows[0].find("n");
        if (it != rows[0].end() && !it->second.empty())
            seq = std::stoll(it->second) + 1;
    }

    // Encode as 5-char base-36 uppercase
    static const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[6]; buf[5] = '\0';
    int64_t v = seq;
    for (int i = 4; i >= 0; --i) {
        buf[i] = chars[v % 36];
        v /= 36;
    }
    return std::string(buf);
}

// ── MFS helpers ──────────────────────────────────────────────
std::string FolderObject::mfsRevDir(const std::string& docId, uint32_t revNum) {
    const std::string& mfsRoot = Config::instance().mfsPath();
    std::string sane = docId;
    for (char& c : sane) if (c == '/') c = '_';
    char revbuf[8]; std::snprintf(revbuf, sizeof(revbuf), "%03u", revNum);
    std::string folderName = sane + "_" + revbuf;
    return FileOps::joinPath(FileOps::joinPath(mfsRoot, "AKT"), folderName);
}

std::string FolderObject::buildMfsFilename(
    const std::string& docRegNr,
    const std::string& oid,
    uint32_t            revNum,
    const std::string& ext)
{
    // {sanitisedDocRegNr}_{objectId}_r{revNr:03d}.{ext}
    // e.g.  XV_DOK_0018_2026_A1B2C_r001.xls
    std::string saneReg = docRegNr;
    for (char& c : saneReg) if (c == '/') c = '_';
    char revbuf[8]; std::snprintf(revbuf, sizeof(revbuf), "r%03u", revNum);
    std::string name = saneReg + "_" + oid + "_" + revbuf;
    if (!ext.empty()) name += "." + ext;
    return name;
}

// ── Factory ──────────────────────────────────────────────────
// ── Factory ─────────────────────────────────────────────────────────────────
std::shared_ptr<FolderObject> FolderObject::importFile(
    const std::string& docId,
    uint32_t            revNum,
    const std::string& srcPath,
    OperationResult&   result,
    const std::string& label,
    const std::string& desc)
{
    // docRegNr is always identical to folderId — derive it here
    const std::string& docRegNr = docId;
    if (!FileOps::fileExists(srcPath)) {
        LOG_ERROR("[DocObject] importFile: not found: " + srcPath);
        result = OperationResult::DOC_FILE_NOT_FOUND;
        return nullptr;
    }

    std::string revDir = mfsRevDir(docId, revNum);
    FileOps::makeDirs(revDir);

    // Generate unique 5-char ID for this document
    std::string oid = nextObjectId(docId);

    // Detect extension
    std::string ext = FileOps::extension(srcPath);
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

    // Build MFS filename and path
    std::string mfsFname = buildMfsFilename(docRegNr, oid, revNum, ext);
    std::string destPath = FileOps::joinPath(revDir, mfsFname);

    if (!FileOps::copyFile(srcPath, destPath, true)) {
        LOG_ERROR("[DocObject] importFile: copy failed " + srcPath + " → " + destPath);
        result = OperationResult::IO_ERROR;
        return nullptr;
    }

    // Store original filename (for displayName)
    std::string origName = FileOps::baseName(srcPath);

    // PK in DB: folderId + ":" + objectId
    std::string pk = docId + ":" + oid;

    auto obj = std::make_shared<FolderObject>();
    obj->objectId     = pk;
    obj->folderId   = docId;
    obj->rev          = revNum;
    obj->originalName = origName;
    obj->storedFileName  = mfsFname;
    obj->filePath      = destPath;
    obj->format       = ext;
    obj->displayName_ = label;
    obj->description  = desc;
    obj->committed    = false;
    obj->createdAt    = nowIso();
    obj->updatedAt    = obj->createdAt;

    auto saveRes = obj->save();
    if (!opOk(saveRes)) {
        LOG_ERROR("[DocObject] importFile: DB save failed for " + pk);
        FileOps::deleteFile(destPath);
        result = saveRes;
        return nullptr;
    }

    LOG_INFO("[DocObject] Imported: " + origName + " → " + destPath + "  id=" + oid);
    result = OperationResult::OPERATION_ACK;

    // Update key file
    writeKeyFile(docId, revNum);

    return obj;
}

// ── Key file ─────────────────────────────────────────────────
// ── MFS key-file ─────────────────────────────────────────────────────────────
OperationResult FolderObject::writeKeyFile(
    const std::string& docId,
    uint32_t            revNum,
    const std::string& docTitle)
{
    std::string revDir = mfsRevDir(docId, revNum);
    FileOps::makeDirs(revDir);
    std::string keyPath = FileOps::joinPath(revDir, "_SCHLUESSEL.txt");

    auto objs = loadForRevision(docId, revNum);

    std::ostringstream ss;
    ss << "ROSENHOLZ PM — SCHLÜSSELDATEI\n"
       << "==============================\n"
       << "Akten-ID  : " << docId << "\n";
    if (!docTitle.empty())
        ss << "Bezeichnung  : " << docTitle << "\n";
    ss << "Revision     : " << revNum << "\n"
       << "Erstellt     : " << nowIso() << "\n"
       << "\n"
       << "Diese Datei ermöglicht die Entschlüsselung der Objekte ohne Rosenholz PM.\n"
       << "Jede Datei in diesem Ordner trägt eine interne ID statt des Originalnamens.\n"
       << "\n"
       << std::left
       << std::setw(8) << "OBJ-ID"
       << "  " << std::setw(42) << "ORIGINAL-DATEINAME"
       << "  " << std::setw(50) << "MFS-DATEINAME"
       << "  STATUS\n"
       << std::string(120, '-') << "\n";

    for (auto& o : objs) {
        // Extract the 5-char ID from the PK (docId:oid)
        std::string shortId = o->objectId;
        auto col = shortId.rfind(':');
        if (col != std::string::npos) shortId = shortId.substr(col+1);

        std::string dispName = o->displayName_.empty() ? o->originalName : o->displayName_;
        ss << std::left
           << std::setw(8) << shortId
           << "  " << std::setw(42) << dispName.substr(0,40)
           << "  " << std::setw(50) << o->storedFileName.substr(0,48)
           << "  " << (o->committed ? "LMDB (dauerhaft)" : "MFS  (in_work)") << "\n";
        if (!o->description.empty())
            ss << "          Beschr.: " << o->description.substr(0,72) << "\n";
    }

    if (objs.empty())
        ss << "(keine Objekte in dieser Revision)\n";

    ss << "\n"
       << "OBJ-ID  : Interne 5-stellige ID (alphanumerisch), eindeutig pro Dokument\n"
       << "LMDB    : Dauerhaft im Langzeitspeicher gespeichert\n"
       << "MFS     : Nur im Dateisystem vorhanden (in_work — noch nicht freigegeben)\n";

    bool ok = FileOps::writeTextFile(keyPath, ss.str(), false);
    return ok ? OperationResult::OPERATION_ACK : OperationResult::KEY_FILE_WRITE_FAILED;
}

// ── LMDB commit ──────────────────────────────────────────────
// ── LMDB archive ─────────────────────────────────────────────────────────────
OperationResult FolderObject::commitToLMDB() {
    if (filePath.empty() || !FileOps::fileExists(filePath)) {
        LOG_ERROR("[DocObject] commitToLMDB: MFS file not found: " + filePath);
        return OperationResult::DOC_FILE_NOT_FOUND;
    }
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    if (!store.isOpen()) {
        LOG_ERROR("[DocObject] commitToLMDB: ArchiveStore not open");
        return OperationResult::DOC_COMMIT_FAILED;
    }
    std::string stagePath;
    auto ref = store.stageContent(filePath, stagePath);
    if (!ref.valid()) {
        LOG_ERROR("[DocObject] commitToLMDB: stage failed for " + filePath);
        return OperationResult::DOC_COMMIT_FAILED;
    }
    // LMDB key: objectId (which includes docId+":"+shortId)
    if (!store.commitContent(stagePath, ref, objectId, 0)) {
        LOG_ERROR("[DocObject] commitToLMDB: commit failed for " + objectId);
        return OperationResult::DOC_COMMIT_FAILED;
    }
    contentHash = ref.sha256;
    contentSize = (int64_t)ref.size;
    committed   = true;
    auto res = update();
    if (!opOk(res)) return res;

    // Update key file to reflect committed status
    writeKeyFile(folderId, rev);

    LOG_INFO("[DocObject] Committed: " + objectId
             + " (" + std::to_string(contentSize) + " bytes)");
    return OperationResult::OPERATION_ACK;
}

std::string FolderObject::extractFromLMDB(const std::string& destDir) {
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    std::string outDir = destDir.empty() ? mfsRevDir(folderId, rev) : destDir;
    FileOps::makeDirs(outDir);
    std::string destPath = FileOps::joinPath(outDir, storedFileName);
    if (!store.retrieveContent(objectId, 0, destPath)) {
        LOG_ERROR("[DocObject] extractFromLMDB: failed for " + objectId);
        return "";
    }
    filePath = destPath;
    update();
    return destPath;
}


// ── Object-level checkout ──────────────────────────────────────
// ── Checkout / Checkin / Revert / Open ──────────────────────────────────────
std::string FolderObject::checkoutObject(const std::string& destDir) {
    if (checkedOut) {
        LOG_WARN("[DocObject] checkoutObject: already checked out: " + objectId);
        return checkoutPath;
    }
    // Resolve destination
    std::string outDir = destDir.empty() ? mfsRevDir(folderId, rev) : destDir;
    FileOps::makeDirs(outDir);
    std::string destPath = FileOps::joinPath(outDir, storedFileName.empty() ? originalName : storedFileName);

    bool ok = false;
    if (committed && !contentHash.empty()) {
        // Extract from LMDB
        auto& store = Rosenholz::Archive::ArchiveStore::instance();
        std::string lmdbKey = folderId + ":r" + std::to_string(rev) + ":" + originalName;
        ok = store.retrieveContent(lmdbKey, 0, destPath);
    } else if (!filePath.empty() && FileOps::fileExists(filePath)) {
        ok = FileOps::copyFile(filePath, destPath, true);
    }
    if (!ok) {
        LOG_ERROR("[DocObject] checkoutObject: failed for " + objectId);
        return "";
    }
    checkedOut   = true;
    checkoutPath = destPath;
    filePath      = destPath;
    update();
    LOG_INFO("[DocObject] Checked out: " + destPath);
    return destPath;
}

// ── Object-level checkin ──────────────────────────────────────
OperationResult FolderObject::checkinObject(const std::string& srcPath) {
    // Fall back: use filePath if neither srcPath nor checkoutPath is set
    std::string path = srcPath.empty() ? checkoutPath : srcPath;
    if (path.empty() && !filePath.empty() && FileOps::fileExists(filePath))
        path = filePath; // use current MFS location as source
    if (path.empty() || !FileOps::fileExists(path)) {
        LOG_ERROR("[DocObject] checkinObject: file not found: " + path);
        return OperationResult::DOC_FILE_NOT_FOUND;
    }
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    std::string stagePath;
    auto ref = store.stageContent(path, stagePath);
    if (!ref.valid()) return OperationResult::DOC_COMMIT_FAILED;

    std::string lmdbKey = folderId + ":r" + std::to_string(rev) + ":" + originalName;
    if (!store.commitContent(stagePath, ref, lmdbKey, 0))
        return OperationResult::DOC_COMMIT_FAILED;

    contentHash = ref.sha256;
    contentSize = (int64_t)ref.size;
    committed   = true;
    checkedOut  = false;
    checkoutPath.clear();
    auto res = update();
    writeKeyFile(folderId, rev);
    LOG_INFO("[DocObject] Checked in: " + lmdbKey);
    return res;
}

// ── Object-level revert ──────────────────────────────────────
OperationResult FolderObject::revertObject(uint32_t parentRev) {
    if (parentRev == 0) return OperationResult::DOC_NO_PARENT_REV;

    // Find same objectId in parent revision
    auto parentObjs = loadForRevision(folderId, parentRev);
    std::shared_ptr<FolderObject> parentObj;
    std::string shortId = objectId;
    auto colon = shortId.rfind(':');
    if (colon != std::string::npos) shortId = shortId.substr(colon + 1);

    for (auto& o : parentObjs) {
        std::string sid = o->objectId;
        auto c = sid.rfind(':');
        if (c != std::string::npos) sid = sid.substr(c + 1);
        if (sid == shortId) { parentObj = o; break; }
    }
    if (!parentObj) return OperationResult::ENTITY_NOT_FOUND;

    // Extract parent content, overwrite current
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    std::string parentKey = folderId + ":r" + std::to_string(parentRev) + ":" + originalName;
    std::string tmpDir    = FileOps::joinPath(Config::instance().basePath(), "tmp_revert");
    FileOps::makeDirs(tmpDir);
    std::string tmpPath   = FileOps::joinPath(tmpDir, originalName);

    if (!store.retrieveContent(parentKey, 0, tmpPath))
        return OperationResult::DOC_FILE_NOT_FOUND;

    return checkinObject(tmpPath);
}

// ── Object open ──────────────────────────────────────────────
std::string FolderObject::openObject(bool inWork, bool& wasCheckedOut) {
    wasCheckedOut = false;
    auto openPath = [](const std::string& p) {
#ifdef _WIN32
        // Windows: use ShellExecute to open file with default application
        ShellExecuteA(nullptr, "open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        std::system(("open \"" + p + "\" &").c_str());
#else
        (void)std::system(("xdg-open \"" + p + "\" 2>/dev/null &").c_str());
#endif
    };

    // If already checked out — open directly from checkout location
    if (checkedOut && !checkoutPath.empty() && FileOps::fileExists(checkoutPath)) {
        wasCheckedOut = true;
        openPath(checkoutPath);
        return checkoutPath;
    }

    if (!inWork) {
        // Not in_work: always copy to tmp (read-only, changes not tracked)
        std::string tmpDir  = FileOps::joinPath(Config::instance().basePath(), "tmp_view");
        FileOps::makeDirs(tmpDir);
        std::string tmpPath = FileOps::joinPath(tmpDir, storedFileName.empty() ? originalName : storedFileName);
        bool ok = false;
        if (committed) {
            auto& store = Rosenholz::Archive::ArchiveStore::instance();
            std::string key = folderId + ":r" + std::to_string(rev) + ":" + originalName;
            ok = store.retrieveContent(key, 0, tmpPath);
        } else if (!filePath.empty()) {
            ok = FileOps::copyFile(filePath, tmpPath, true);
        }
        if (ok) { openPath(tmpPath); return tmpPath; }
        return "";
    }

    // in_work but not checked out: copy to tmp for viewing (not tracked)
    std::string tmpDir  = FileOps::joinPath(Config::instance().basePath(), "tmp_view");
    FileOps::makeDirs(tmpDir);
    std::string tmpPath = FileOps::joinPath(tmpDir, storedFileName.empty() ? originalName : storedFileName);
    bool ok = false;
    if (!filePath.empty() && FileOps::fileExists(filePath))
        ok = FileOps::copyFile(filePath, tmpPath, true);
    else if (committed) {
        auto& store = Rosenholz::Archive::ArchiveStore::instance();
        std::string key = folderId + ":r" + std::to_string(rev) + ":" + originalName;
        ok = store.retrieveContent(key, 0, tmpPath);
    }
    if (ok) { openPath(tmpPath); return tmpPath; }
    return "";
}

// ── MFS scan for unregistered files ──────────────────────────
// ── MFS folder scanner ───────────────────────────────────────────────────────
std::vector<std::string> FolderObject::scanForUnregisteredFiles(
    const std::string& docId, uint32_t rev)
{
    std::vector<std::string> result;
    std::string revDir = mfsRevDir(docId, rev);
    if (!FileOps::dirExists(revDir)) return result;

    // Collect known MFS paths for this revision
    auto known = loadForRevision(docId, rev);
    std::set<std::string> knownPaths;
    for (auto& o : known) {
        if (!o->filePath.empty()) knownPaths.insert(o->filePath);
        // Also track by filename
        if (!o->storedFileName.empty())
            knownPaths.insert(FileOps::joinPath(revDir, o->storedFileName));
    }

    // List all files in the revision folder (recursive to preserve structure)
    auto files = FileOps::listFiles(revDir, true);
    for (auto& f : files) {
        std::string base = FileOps::baseName(f);
        if (base.empty()) continue;
        // Skip auto-generated key/index files:
        if (base == "_SCHLUESSEL.txt") continue;
        if (base == "owner_key.txt") continue;
        // Skip files that end with the doc-ID pattern (e.g. XV_AKT_0001_26.txt)
        if (base.size() > 4 && base.substr(base.size()-4) == ".txt") {
            // Check if the stem matches a Rosenholz ID pattern (XV_XXX_NNNN_YY)
            static const std::regex rz_id(R"(^[A-Z]{2}_[A-Z]+_\d+_\d+\.txt$)");
            if (std::regex_match(base, rz_id)) continue;
        }
        if (knownPaths.count(f) == 0)
            result.push_back(f);
    }
    return result;
}


// ── FolderObject::loadById ─────────────────────────────────────────────
std::shared_ptr<FolderObject> FolderObject::loadById(const std::string& objectId) {
    auto* db = DatabasePool::instance().get("akt");
    if (!db) return nullptr;
    auto rows = db->query("SELECT * FROM folder_objects WHERE object_id=?;",
                          {BindParam::text(objectId)});
    if (rows.empty()) return nullptr;
    auto obj = std::make_shared<FolderObject>();
    obj->fromRow(rows[0]);
    return obj;
}

// ── FolderObject::updateFromUrl ────────────────────────────────────────
OperationResult FolderObject::updateFromUrl(const std::string& url) {
    std::string target = url.empty() ? sourceUrl : url;
    if (target.empty()) return OperationResult::IO_ERROR;
    if (!url.empty()) sourceUrl = url;

    const std::string& base = Config::instance().basePath();
    std::string tmpDir = FileOps::joinPath(base, "documents", "tmp");
    FileOps::makeDirs(tmpDir);
    std::string downloaded = FileOps::downloadUrl(target, tmpDir);
    if (downloaded.empty()) return OperationResult::IO_ERROR;

    if (!filePath.empty() && FileOps::fileExists(filePath))
        FileOps::deleteFile(filePath);
    if (FileOps::fileExists(downloaded)) {
        if (!filePath.empty())
            FileOps::copyFile(downloaded, filePath);
        FileOps::deleteFile(downloaded);
    }
    return update();
}

} // namespace Rosenholz
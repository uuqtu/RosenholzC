// ============================================================
// DocumentObject.cpp
// ============================================================
#include "DocumentObject.h"
#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#endif
#include <set>
#include "../Utils.h"
#include "../../core/FileOps.h"
#include "../../core/Config.h"
#include "../../repository/ArchiveStore.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace Rosenholz {

// ── Internal helpers ─────────────────────────────────────────
static auto t_(const std::string& s) { return BindParam::text(s); }
static auto i_(int64_t v)             { return BindParam::int64(v); }

Database* DocumentObject::db() {
    return DatabasePool::instance().get("dok");
}

// ── CRUD + internal helpers ─────────────────────────────────────────────────
std::string DocumentObject::sanitiseForFilename(const std::string& s) {
    return FileOps::sanitizeFilename(s);
}

// ── CRUD ─────────────────────────────────────────────────────
void DocumentObject::fromRow(const Row& r) {
    auto g = [&](const char* k) -> std::string {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    auto gb = [&](const char* k) -> bool { return g(k) == "1"; };
    objectId      = g("object_id");
    documentId    = g("document_id");
    rev           = g("rev").empty() ? 1u : (uint32_t)std::stoi(g("rev"));
    originalName  = g("original_name");
    mfsFilename   = g("mfs_filename");
    mfsPath       = g("mfs_path");
    contentHash   = g("content_hash");
    contentSize   = g("content_size").empty() ? 0 : std::stoll(g("content_size"));
    format        = g("format");
    committed     = gb("committed");
    createdAt     = g("created_at");
    updatedAt     = g("updated_at");
}

OperationResult DocumentObject::save() const {
    auto* d = db();
    if (!d) return OperationResult::DB_ERROR;
    bool ok = d->exec(R"(
        INSERT OR REPLACE INTO document_objects
        (object_id,document_id,rev,original_name,mfs_filename,mfs_path,
         content_hash,content_size,format,source_url,committed,created_at,updated_at)
        VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?);)",
        {t_(objectId), t_(documentId), i_(rev), t_(originalName),
         t_(mfsFilename), t_(mfsPath), t_(contentHash), i_(contentSize),
         t_(format), t_(sourceUrl), i_(committed ? 1 : 0), t_(createdAt), t_(nowIso())});
    return ok ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult DocumentObject::update() const { return save(); }

OperationResult DocumentObject::remove() const {
    auto* d = db();
    if (!d) return OperationResult::DB_ERROR;
    bool ok = d->exec("DELETE FROM document_objects WHERE object_id=? AND document_id=?;",
                      {t_(objectId), t_(documentId)});
    return ok ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

// ── Display ─────────────────────────────────────────────────────────────────
std::string DocumentObject::summary() const {
    std::ostringstream ss;
    ss << std::left << std::setw(7) << objectId
       << "  " << std::setw(40) << originalName.substr(0,38)
       << "  " << (committed ? "[LMDB]" : "[MFS] ")
       << "  " << (contentSize/1024) << " KB";
    return ss.str();
}

// ── Queries ──────────────────────────────────────────────────
std::shared_ptr<DocumentObject> DocumentObject::loadByDocAndId(
    const std::string& docId, const std::string& oid)
{
    auto* d = db();
    if (!d) return nullptr;
    // objectId is stored globally unique as docId+":"+oid in DB PK
    std::string pk = docId + ":" + oid;
    auto rows = d->query(
        "SELECT * FROM document_objects WHERE object_id=?;", {t_(pk)});
    if (rows.empty()) return nullptr;
    auto o = std::make_shared<DocumentObject>(); o->fromRow(rows[0]);
    return o;
}

std::vector<std::shared_ptr<DocumentObject>> DocumentObject::loadForRevision(
    const std::string& docId, uint32_t revNum)
{
    auto* d = db();
    std::vector<std::shared_ptr<DocumentObject>> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM document_objects "
        "WHERE document_id=? AND rev=? ORDER BY original_name;",
        {t_(docId), i_(revNum)});
    for (auto& r : rows) {
        auto o = std::make_shared<DocumentObject>(); o->fromRow(r);
        result.push_back(o);
    }
    return result;
}

// ── ID generation ────────────────────────────────────────────
// 5-char alphanumeric (uppercase), unique per document.
// Uses a counter stored in the DB: max seq +1.
// Encoded as base-36: 0-9 A-Z
// ── Private helpers (filename, objectId generation) ─────────────────────────
std::string DocumentObject::nextObjectId(const std::string& docId) {
    auto* d = db();
    if (!d) return "XXXXX";

    // Count existing objects for this document (across all revisions)
    auto rows = d->query(
        "SELECT COUNT(*) as n FROM document_objects WHERE document_id=?;",
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
std::string DocumentObject::mfsRevDir(const std::string& docId, uint32_t revNum) {
    const std::string& mfsRoot = Config::instance().mfsPath();
    std::string sane = docId;
    for (char& c : sane) if (c == '/') c = '_';
    char revbuf[8]; std::snprintf(revbuf, sizeof(revbuf), "%03u", revNum);
    std::string folderName = sane + "_" + revbuf;
    return FileOps::joinPath(FileOps::joinPath(mfsRoot, "DOK"), folderName);
}

std::string DocumentObject::buildMfsFilename(
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
std::shared_ptr<DocumentObject> DocumentObject::importFile(
    const std::string& docId,
    uint32_t            revNum,
    const std::string& srcPath,
    OperationResult&   result)
{
    // docRegNr is always identical to documentId — derive it here
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

    // PK in DB: documentId + ":" + objectId
    std::string pk = docId + ":" + oid;

    auto obj = std::make_shared<DocumentObject>();
    obj->objectId     = pk;
    obj->documentId   = docId;
    obj->rev          = revNum;
    obj->originalName = origName;
    obj->mfsFilename  = mfsFname;
    obj->mfsPath      = destPath;
    obj->format       = ext;
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
OperationResult DocumentObject::writeKeyFile(
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
       << "Dokument-ID  : " << docId << "\n";
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

        ss << std::left
           << std::setw(8) << shortId
           << "  " << std::setw(42) << o->originalName.substr(0,40)
           << "  " << std::setw(50) << o->mfsFilename.substr(0,48)
           << "  " << (o->committed ? "LMDB (dauerhaft)" : "MFS  (in_work)") << "\n";
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
OperationResult DocumentObject::commitToLMDB() {
    if (mfsPath.empty() || !FileOps::fileExists(mfsPath)) {
        LOG_ERROR("[DocObject] commitToLMDB: MFS file not found: " + mfsPath);
        return OperationResult::DOC_FILE_NOT_FOUND;
    }
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    if (!store.isOpen()) {
        LOG_ERROR("[DocObject] commitToLMDB: ArchiveStore not open");
        return OperationResult::DOC_COMMIT_FAILED;
    }
    std::string stagePath;
    auto ref = store.stageContent(mfsPath, stagePath);
    if (!ref.valid()) {
        LOG_ERROR("[DocObject] commitToLMDB: stage failed for " + mfsPath);
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
    writeKeyFile(documentId, rev);

    LOG_INFO("[DocObject] Committed: " + objectId
             + " (" + std::to_string(contentSize) + " bytes)");
    return OperationResult::OPERATION_ACK;
}

std::string DocumentObject::extractFromLMDB(const std::string& destDir) {
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    std::string outDir = destDir.empty() ? mfsRevDir(documentId, rev) : destDir;
    FileOps::makeDirs(outDir);
    std::string destPath = FileOps::joinPath(outDir, mfsFilename);
    if (!store.retrieveContent(objectId, 0, destPath)) {
        LOG_ERROR("[DocObject] extractFromLMDB: failed for " + objectId);
        return "";
    }
    mfsPath = destPath;
    update();
    return destPath;
}


// ── Object-level checkout ──────────────────────────────────────
// ── Checkout / Checkin / Revert / Open ──────────────────────────────────────
std::string DocumentObject::checkoutObject(const std::string& destDir) {
    if (checkedOut) {
        LOG_WARN("[DocObject] checkoutObject: already checked out: " + objectId);
        return checkoutPath;
    }
    // Resolve destination
    std::string outDir = destDir.empty() ? mfsRevDir(documentId, rev) : destDir;
    FileOps::makeDirs(outDir);
    std::string destPath = FileOps::joinPath(outDir, mfsFilename.empty() ? originalName : mfsFilename);

    bool ok = false;
    if (committed && !contentHash.empty()) {
        // Extract from LMDB
        auto& store = Rosenholz::Archive::ArchiveStore::instance();
        std::string lmdbKey = documentId + ":r" + std::to_string(rev) + ":" + originalName;
        ok = store.retrieveContent(lmdbKey, 0, destPath);
    } else if (!mfsPath.empty() && FileOps::fileExists(mfsPath)) {
        ok = FileOps::copyFile(mfsPath, destPath, true);
    }
    if (!ok) {
        LOG_ERROR("[DocObject] checkoutObject: failed for " + objectId);
        return "";
    }
    checkedOut   = true;
    checkoutPath = destPath;
    mfsPath      = destPath;
    update();
    LOG_INFO("[DocObject] Checked out: " + destPath);
    return destPath;
}

// ── Object-level checkin ──────────────────────────────────────
OperationResult DocumentObject::checkinObject(const std::string& srcPath) {
    std::string path = srcPath.empty() ? checkoutPath : srcPath;
    if (path.empty() || !FileOps::fileExists(path)) {
        LOG_ERROR("[DocObject] checkinObject: file not found: " + path);
        return OperationResult::DOC_FILE_NOT_FOUND;
    }
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    std::string stagePath;
    auto ref = store.stageContent(path, stagePath);
    if (!ref.valid()) return OperationResult::DOC_COMMIT_FAILED;

    std::string lmdbKey = documentId + ":r" + std::to_string(rev) + ":" + originalName;
    if (!store.commitContent(stagePath, ref, lmdbKey, 0))
        return OperationResult::DOC_COMMIT_FAILED;

    contentHash = ref.sha256;
    contentSize = (int64_t)ref.size;
    committed   = true;
    checkedOut  = false;
    checkoutPath.clear();
    auto res = update();
    writeKeyFile(documentId, rev);
    LOG_INFO("[DocObject] Checked in: " + lmdbKey);
    return res;
}

// ── Object-level revert ──────────────────────────────────────
OperationResult DocumentObject::revertObject(uint32_t parentRev) {
    if (parentRev == 0) return OperationResult::DOC_NO_PARENT_REV;

    // Find same objectId in parent revision
    auto parentObjs = loadForRevision(documentId, parentRev);
    std::shared_ptr<DocumentObject> parentObj;
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
    std::string parentKey = documentId + ":r" + std::to_string(parentRev) + ":" + originalName;
    std::string tmpDir    = FileOps::joinPath(Config::instance().basePath(), "tmp_revert");
    FileOps::makeDirs(tmpDir);
    std::string tmpPath   = FileOps::joinPath(tmpDir, originalName);

    if (!store.retrieveContent(parentKey, 0, tmpPath))
        return OperationResult::DOC_FILE_NOT_FOUND;

    return checkinObject(tmpPath);
}

// ── Object open ──────────────────────────────────────────────
std::string DocumentObject::openObject(bool inWork, bool& wasCheckedOut) {
    wasCheckedOut = false;
    auto openPath = [](const std::string& p) {
#ifdef _WIN32
        // Windows: use ShellExecute to open file with default application
        ShellExecuteA(nullptr, "open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        std::system(("open \"" + p + "\" &").c_str());
#else
        std::system(("xdg-open \"" + p + "\" 2>/dev/null &").c_str());
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
        std::string tmpPath = FileOps::joinPath(tmpDir, mfsFilename.empty() ? originalName : mfsFilename);
        bool ok = false;
        if (committed) {
            auto& store = Rosenholz::Archive::ArchiveStore::instance();
            std::string key = documentId + ":r" + std::to_string(rev) + ":" + originalName;
            ok = store.retrieveContent(key, 0, tmpPath);
        } else if (!mfsPath.empty()) {
            ok = FileOps::copyFile(mfsPath, tmpPath, true);
        }
        if (ok) { openPath(tmpPath); return tmpPath; }
        return "";
    }

    // in_work but not checked out: copy to tmp for viewing (not tracked)
    std::string tmpDir  = FileOps::joinPath(Config::instance().basePath(), "tmp_view");
    FileOps::makeDirs(tmpDir);
    std::string tmpPath = FileOps::joinPath(tmpDir, mfsFilename.empty() ? originalName : mfsFilename);
    bool ok = false;
    if (!mfsPath.empty() && FileOps::fileExists(mfsPath))
        ok = FileOps::copyFile(mfsPath, tmpPath, true);
    else if (committed) {
        auto& store = Rosenholz::Archive::ArchiveStore::instance();
        std::string key = documentId + ":r" + std::to_string(rev) + ":" + originalName;
        ok = store.retrieveContent(key, 0, tmpPath);
    }
    if (ok) { openPath(tmpPath); return tmpPath; }
    return "";
}

// ── MFS scan for unregistered files ──────────────────────────
// ── MFS folder scanner ───────────────────────────────────────────────────────
std::vector<std::string> DocumentObject::scanForUnregisteredFiles(
    const std::string& docId, uint32_t rev)
{
    std::vector<std::string> result;
    std::string revDir = mfsRevDir(docId, rev);
    if (!FileOps::dirExists(revDir)) return result;

    // Collect known MFS paths for this revision
    auto known = loadForRevision(docId, rev);
    std::set<std::string> knownPaths;
    for (auto& o : known) {
        if (!o->mfsPath.empty()) knownPaths.insert(o->mfsPath);
        // Also track by filename
        if (!o->mfsFilename.empty())
            knownPaths.insert(FileOps::joinPath(revDir, o->mfsFilename));
    }

    // List all files in the revision folder
    auto files = FileOps::listFiles(revDir, false);
    for (auto& f : files) {
        // Skip the key file itself
        if (FileOps::baseName(f) == "_SCHLUESSEL.txt") continue;
        if (knownPaths.count(f) == 0)
            result.push_back(f);
    }
    return result;
}


// ── DocumentObject::loadById ─────────────────────────────────────────────
std::shared_ptr<DocumentObject> DocumentObject::loadById(const std::string& objectId) {
    auto* db = DatabasePool::instance().get("dok");
    if (!db) return nullptr;
    auto rows = db->query("SELECT * FROM document_objects WHERE object_id=?;",
                          {BindParam::text(objectId)});
    if (rows.empty()) return nullptr;
    auto obj = std::make_shared<DocumentObject>();
    obj->fromRow(rows[0]);
    return obj;
}

// ── DocumentObject::updateFromUrl ────────────────────────────────────────
OperationResult DocumentObject::updateFromUrl(const std::string& url) {
    std::string target = url.empty() ? sourceUrl : url;
    if (target.empty()) return OperationResult::IO_ERROR;
    if (!url.empty()) sourceUrl = url;

    const std::string& base = Config::instance().basePath();
    std::string tmpDir = FileOps::joinPath(base, "documents", "tmp");
    FileOps::makeDirs(tmpDir);
    std::string downloaded = FileOps::downloadUrl(target, tmpDir);
    if (downloaded.empty()) return OperationResult::IO_ERROR;

    if (!mfsPath.empty() && FileOps::fileExists(mfsPath))
        FileOps::deleteFile(mfsPath);
    if (FileOps::fileExists(downloaded)) {
        if (!mfsPath.empty())
            FileOps::copyFile(downloaded, mfsPath);
        FileOps::deleteFile(downloaded);
    }
    return update();
}

} // namespace Rosenholz
// Document.cpp
#include "Document.h"
#include <cstdlib>
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#include "../core/Repository.h"
#include "../core/FileOps.h"
#include "../core/Config.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Rosenholz {




// ------------------------------
// create
//
// Parameters:
//   title                : document title (required)
//   docType              : report|specification|contract|…
//   projectId            : owning project (empty = orphan)
//
// Behavior:
//   Generates DDR ID: XV/DOK/{seq}/{year}
//   Sets dateCreated, status=draft, version=1.0
//   Does NOT save — caller must call save()
//
// Returns:
//   Shared pointer to in-memory Document
// ------------------------------
std::shared_ptr<Document> Document::create(
    const std::string& title_, const std::string& type_, const std::string& pid)
{
    auto d = std::make_shared<Document>();
    d->documentId  = genId("DOK"); d->title = title_;
    d->docType     = type_; d->projectId = pid;
    d->status      = "draft"; d->dateCreated = nowIso();
    d->createdAt   = d->dateCreated; d->updatedAt = d->createdAt;
    d->notes       = "{}";
    LOG_INFO("Document created: " + d->documentId + " \"" + title_ + "\"");
    return d;
}

bool Document::save() const {
    auto* db = DatabasePool::instance().get("documents");
    if (!db) { LOG_ERROR("Document::save — documents DB unavailable"); return false; }
    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO documents
        (document_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,author_id,approved_by,doc_type,doc_category,title,version,
         date_created,date_modified,date_approved,date_expires,status,classification,
         volume_number,page_count,language,format,file_path,file_size,file_hash,file_url,
         external_ref,storage_system,tags,summary,links,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(documentId), textOrNull(workflowInstanceId),
        textOrNull(workflowStatus), textOrNull(workflowCurrentState),
        textOrNull(projectId), textOrNull(taskId),
        textOrNull(authorId), textOrNull(approvedBy),
        BindParam::text(docType), textOrNull(docCategory),
        BindParam::text(title), textOrNull(version),
        BindParam::text(dateCreated), textOrNull(dateModified),
        textOrNull(dateApproved), textOrNull(dateExpires),
        BindParam::text(status), textOrNull(classification),
        BindParam::int64(volumeNumber), BindParam::int64(pageCount),
        BindParam::text(language), textOrNull(format),
        textOrNull(filePath), BindParam::int64(fileSize), textOrNull(fileHash), textOrNull(fileUrl),
        textOrNull(externalRef), textOrNull(storageSystem),
        textOrNull(tags), textOrNull(summary),
        textOrNull(links), BindParam::text(notes),
        BindParam::text(createdAt), BindParam::text(nowIso())
    });
    if (ok) LOG_INFO("Document saved: " + documentId);
    else    LOG_ERROR("Document save failed: " + documentId);
    return ok;
}

void Document::fromRow(const Row& r) {
    auto g=[&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
    documentId=g("document_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id");
    authorId=g("author_id"); approvedBy=g("approved_by");
    docType=g("doc_type"); docCategory=g("doc_category");
    title=g("title"); version=g("version");
    dateCreated=g("date_created"); dateModified=g("date_modified");
    dateApproved=g("date_approved"); dateExpires=g("date_expires");
    status=g("status"); classification=g("classification");
    auto gi=[&](const std::string& k){ auto v=g(k); return v.empty()?0:std::stoi(v); };
    volumeNumber=gi("volume_number"); pageCount=gi("page_count");
    language=g("language"); format=g("format");
    filePath=g("file_path"); fileUrl=g("file_url");
    { auto sv=g("file_size"); fileSize=sv.empty()?0:std::stoll(sv); } fileHash=g("file_hash");
    externalRef=g("external_ref"); storageSystem=g("storage_system");
    tags=g("tags"); summary=g("summary"); links=g("links"); notes=g("notes");
    createdAt=g("created_at"); updatedAt=g("updated_at");
}
bool Document::load(const std::string& id) {
    auto* db=DatabasePool::instance().get("documents");
    if (!db) return false;
    auto rows=db->query("SELECT * FROM documents WHERE document_id=?;",{BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("Document not found: "+id); return false; }
    fromRow(rows[0]); return true;
}
bool Document::remove() {
    auto* db=DatabasePool::instance().get("documents");
    if (!db) return false;
    db->exec("DELETE FROM entity_documents WHERE document_id=?;",{BindParam::text(documentId)});
    return db->exec("DELETE FROM documents WHERE document_id=?;",{BindParam::text(documentId)});
}
bool Document::update() { updatedAt=nowIso(); dateModified=updatedAt; return save(); }

std::shared_ptr<Document> Document::loadById(const std::string& id) {
    auto d=std::make_shared<Document>(); if(!d->load(id)) return nullptr; return d;
}
std::vector<std::shared_ptr<Document>> Document::loadForProject(const std::string& pid) {
    auto* db=DatabasePool::instance().get("documents");
    std::vector<std::shared_ptr<Document>> result;
    if (!db) return result;
    auto rows=db->query("SELECT * FROM documents WHERE project_id=? ORDER BY date_created DESC;",
                        {BindParam::text(pid)});
    for (auto& r:rows) { auto d=std::make_shared<Document>(); d->fromRow(r); result.push_back(d); }
    return result;
}
std::vector<std::shared_ptr<Document>> Document::loadForEntity(
    const std::string& et, const std::string& eid)
{
    auto* db=DatabasePool::instance().get("documents");
    std::vector<std::shared_ptr<Document>> result;
    if (!db) return result;
    // Join entity_documents -> documents
    auto rows=db->query(R"(
        SELECT d.* FROM documents d
        JOIN entity_documents e ON d.document_id=e.document_id
        WHERE e.entity_type=? AND e.entity_id=? ORDER BY d.date_created DESC;
    )", {BindParam::text(et), BindParam::text(eid)});
    for (auto& r:rows) { auto d=std::make_shared<Document>(); d->fromRow(r); result.push_back(d); }
    return result;
}

// ── URL archiving ─────────────────────────────────────────────
std::shared_ptr<Document> Document::archiveFromUrl(
    const std::string& url, const std::string& pid, const std::string& aid)
{
    LOG_INFO("Archiving URL: " + url);
    const std::string& base = Config::instance().basePath();
    std::string archiveDir  = FileOps::joinPath(base, "documents", "archived");
    FileOps::makeDirs(archiveDir);

    // Determine if it's a website or a file
    bool isWebsite = (url.find("http") == 0) && (url.find('.') != std::string::npos);
    std::string ext = FileOps::extension(url);
    bool isDocFile  = !ext.empty() && ext != ".html" && ext != ".htm" && ext != ".php";

    std::string localPath;

    if (isWebsite && !isDocFile) {
        // Try to archive as PDF first (requires wkhtmltopdf), fallback to HTML
        localPath = archiveWebsite(url, archiveDir);
    } else {
        // Direct file download
        localPath = FileOps::downloadUrl(url, archiveDir);
    }

    if (localPath.empty()) {
        LOG_ERROR("Failed to archive URL: " + url);
        return nullptr;
    }

    // Build Document record
    auto d = create(FileOps::baseName(localPath), "archive", pid);
    d->fileUrl      = url;
    d->filePath     = localPath;
    d->authorId     = aid;
    d->dateCreated  = nowIso();
    d->format       = FileOps::extension(localPath);
    d->storageSystem= "local";
    d->status       = "approved";

    int64_t sz = FileOps::fileSize(localPath);
    d->summary = "Archived from " + url + " (" + std::to_string(sz) + " bytes)";

    d->save();
    LOG_INFO("URL archived as document: " + d->documentId + " path=" + localPath);
    return d;
}

std::string Document::archiveWebsite(const std::string& url, const std::string& destDir) {
    // Derive filename from URL
    std::string safe = FileOps::sanitizeFilename(url);
    if (safe.size() > 80) safe = safe.substr(0, 80);

    // Try wkhtmltopdf -> PDF
    std::string pdfPath = FileOps::joinPath(destDir, safe + ".pdf");
    std::string cmd = "wkhtmltopdf --quiet \"" + url + "\" \"" + pdfPath + "\" 2>/dev/null";
    if (system(cmd.c_str()) == 0 && FileOps::fileExists(pdfPath)) {
        LOG_INFO("Website archived as PDF: " + pdfPath);
        return pdfPath;
    }

    // Fallback: wget single-page HTML
    std::string htmlPath = FileOps::joinPath(destDir, safe + ".html");
    cmd = "wget -q --convert-links -O \"" + htmlPath + "\" \"" + url + "\" 2>/dev/null";
    if (system(cmd.c_str()) == 0 && FileOps::fileExists(htmlPath)) {
        LOG_INFO("Website archived as HTML: " + htmlPath);
        return htmlPath;
    }
    // Final fallback: plain curl
    return FileOps::downloadUrl(url, destDir);
}

// ------------------------------
// attachToEntity
//
// Parameters:
//   entityType           : project|task|incident|workflow_instance|…
//   entityId             : ID of the entity to link to
//   relationship         : attached|mandatory|reference
//
// Behavior:
//   Inserts into entity_documents table in documents.db
//   Idempotent: INSERT OR IGNORE prevents duplicates
//
// Returns:
//   true on success
// ------------------------------
bool Document::attachToEntity(const std::string& et, const std::string& eid, const std::string& rel) {
    auto* db = DatabasePool::instance().get("documents");
    if (!db) return false;
    return db->exec(
        "INSERT OR IGNORE INTO entity_documents(entity_type,entity_id,document_id,relationship) VALUES(?,?,?,?);",
        {BindParam::text(et), BindParam::text(eid), BindParam::text(documentId), BindParam::text(rel)});
}
bool Document::reassignAuthor(const std::string& id) { authorId=id; return update(); }
bool Document::reassignToProject(const std::string& id) { projectId=id; return update(); }
bool Document::reassignToTask(const std::string& id) { taskId=id; return update(); }
nlohmann::json Document::toJson() const {
    return {{"documentId",documentId},{"title",title},{"docType",docType},
            {"format",format},{"status",status},{"fileUrl",fileUrl}};
}

// ══════════════════════════════════════════════════════════════
// DOCUMENT VERSION MANAGEMENT
// ══════════════════════════════════════════════════════════════

// ── SHA-256 via /usr/bin/sha256sum ───────────────────────────
static std::string computeSHA256(const std::string& path) {
    if (!FileOps::fileExists(path)) return "";
    FILE* pipe = popen(("sha256sum \"" + path + "\" 2>/dev/null").c_str(), "r");
    if (!pipe) return "";
    char buf[256] = {};
    fgets(buf, sizeof(buf), pipe);
    pclose(pipe);
    std::string s(buf);
    // sha256sum output: "hash  filename"
    size_t sp = s.find(' ');
    return (sp != std::string::npos) ? s.substr(0, sp) : "";
}

// ── MFS document directory for this document ─────────────────
static std::string docMFSDir(const Document& d) {
    const std::string& mfsRoot = Config::instance().mfsPath();
    // Primary parent: project or task
    std::string parent = d.projectId.empty() ? d.taskId : d.projectId;
    if (parent.empty()) return "";
    std::string sane = sanitiseRegNr(parent);
    // mfs/DOK/{parent_sane}/
    return FileOps::joinPath(mfsRoot, "DOK", sane);
}

// ── MFS canonical filename for a document version ────────────
static std::string mfsDocFilename(const Document& d, const std::string& versionSuffix = "") {
    std::string docSane  = sanitiseRegNr(d.documentId);
    std::string titleSafe = FileOps::sanitizeFilename(d.title);
    if (titleSafe.size() > 40) titleSafe = titleSafe.substr(0, 40);
    std::string ext = d.format.empty() ? "bin" : d.format;
    // Format: {DOK-ID}_{Titel}_v{Version}.{ext}
    std::string ver = versionSuffix.empty() ? d.version : versionSuffix;
    return docSane + "_" + titleSafe + "_v" + ver + "." + ext;
}

bool Document::importLocalFile(const std::string& srcPath) {
    if (!FileOps::fileExists(srcPath)) {
        LOG_ERROR("importLocalFile: not found: " + srcPath);
        return false;
    }

    std::string dir = docMFSDir(*this);
    if (dir.empty()) {
        LOG_ERROR("importLocalFile: document has no project/task reference");
        return false;
    }
    FileOps::makeDirs(dir);

    // Detect format from extension
    if (format.empty()) {
        std::string ext = FileOps::extension(srcPath);
        if (!ext.empty()) format = ext.substr(1); // strip leading dot
    }

    std::string destName = mfsDocFilename(*this);
    std::string destPath = FileOps::joinPath(dir, destName);

    if (!FileOps::copyFile(srcPath, destPath, true)) {
        LOG_ERROR("importLocalFile: copy failed " + srcPath + " → " + destPath);
        return false;
    }

    filePath = destPath;
    fileSize = FileOps::fileSize(destPath);
    fileHash = computeSHA256(destPath);
    LOG_INFO("Document imported to MFS: " + destPath);
    return update();
}

bool Document::refreshFromUrl() {
    if (fileUrl.empty()) {
        LOG_WARN("refreshFromUrl: no URL set");
        return false;
    }

    // Snapshot existing file first
    if (!filePath.empty() && FileOps::fileExists(filePath))
        snapshotVersion("Vor URL-Aktualisierung");

    const std::string& base = Config::instance().basePath();
    std::string tmpDir = FileOps::joinPath(base, "documents", "tmp");
    FileOps::makeDirs(tmpDir);

    // Download
    std::string downloaded = FileOps::downloadUrl(fileUrl, tmpDir);
    if (downloaded.empty()) {
        LOG_ERROR("refreshFromUrl: download failed: " + fileUrl);
        return false;
    }

    // Bump version
    std::string oldVer = version;
    try {
        size_t dot = version.rfind('.');
        if (dot != std::string::npos) {
            int minor = std::stoi(version.substr(dot+1)) + 1;
            version = version.substr(0, dot+1) + std::to_string(minor);
        }
    } catch(...) { version += ".r"; }

    // Import into MFS
    bool ok = importLocalFile(downloaded);
    FileOps::deleteFile(downloaded);
    if (ok) dateModified = nowIso();
    return ok;
}

bool Document::snapshotVersion(const std::string& changeNote, const std::string& createdBy) {
    // Copy current file to versioned name
    std::string versionedPath;
    if (!filePath.empty() && FileOps::fileExists(filePath)) {
        std::string dir = FileOps::dirName(filePath);
        std::string versName = mfsDocFilename(*this, version + "-snap");
        versionedPath = FileOps::joinPath(dir, versName);
        FileOps::copyFile(filePath, versionedPath, true);
    }

    auto* db = DatabasePool::instance().get("documents");
    if (!db) return false;
    std::string vid = genId("VER");
    return db->exec(
        "INSERT INTO document_versions"
        "(version_id,document_id,version_number,file_path,file_size,file_hash,"
        " created_by,change_note,created_at) VALUES(?,?,?,?,?,?,?,?,?);",
        {BindParam::text(vid), BindParam::text(documentId),
         BindParam::text(version), textOrNull(versionedPath),
         BindParam::int64(fileSize), textOrNull(fileHash),
         textOrNull(createdBy), textOrNull(changeNote),
         BindParam::text(nowIso())});
}

std::vector<Document::VersionRecord> Document::loadVersions() const {
    std::vector<VersionRecord> result;
    auto* db = DatabasePool::instance().get("documents");
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM document_versions WHERE document_id=? ORDER BY created_at DESC;",
        {BindParam::text(documentId)});
    for (auto& r : rows) {
        VersionRecord v;
        v.versionId    = rowGet(r,"version_id");
        v.versionNumber= rowGet(r,"version_number");
        v.filePath     = rowGet(r,"file_path");
        v.fileHash     = rowGet(r,"file_hash");
        v.fileSize     = std::stoll(rowGet(r,"file_size").empty() ? "0" : rowGet(r,"file_size"));
        v.createdBy    = rowGet(r,"created_by");
        v.changeNote   = rowGet(r,"change_note");
        v.createdAt    = rowGet(r,"created_at");
        result.push_back(v);
    }
    return result;
}

bool Document::openFile(const std::string& mode) const {
    if (filePath.empty() || !FileOps::fileExists(filePath)) {
        LOG_WARN("openFile: no file at " + filePath);
        return false;
    }

    std::string pathToOpen = filePath;

    if (mode == "read") {
        // Temporary read-only copy in /tmp
        std::string tmpDir = FileOps::joinPath(FileOps::tempDirectory(), "rosenholz_view");
        FileOps::makeDirs(tmpDir);
        std::string tmpFile = FileOps::joinPath(tmpDir, FileOps::baseName(filePath));
        if (!FileOps::copyFile(filePath, tmpFile, true)) return false;
        pathToOpen = tmpFile;
        LOG_INFO("openFile (read): tmp copy → " + tmpFile);
    }
    // else mode == "edit": open original (already versioned by caller)

    // Open with system default application
    std::string cmd;
#ifdef __linux__
    cmd = "xdg-open \"" + pathToOpen + "\" &";
#elif __APPLE__
    cmd = "open \"" + pathToOpen + "\" &";
#else
    cmd = "start \"" + pathToOpen + "\"";
#endif
    int ret = system(cmd.c_str());
    return ret == 0;
}

} // namespace Rosenholz

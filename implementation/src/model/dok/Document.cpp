// Document.cpp
#include "Document.h"
#include "../../repository/ArchiveStore.h"
#include "../../repository/DocumentRevision.h"
#include "../../workflow/F77Workflow.h"
#include "../../repository/DocumentRevision.h"
#include <cstdlib>
#include "../../core/Database.h"
#include "../../workflow/F77Workflow.h"
#include "../../core/Logger.h"
#include "../Utils.h"
#include "../../core/Repository.h"
#include "../../core/FileOps.h"
#include "../../core/Config.h"
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
    auto* db = DatabasePool::instance().get("dok");
    if (!db) { LOG_ERROR("Document::save — documents DB unavailable"); return false; }
    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO documents
        (document_id,workflow_instance_id,workflow_status,workflow_current_state,release_workflow_id,
         project_id,task_id,f18_operation_id,f18_step_id,author_id,approved_by,doc_type,doc_category,title,version,
         date_created,date_modified,date_approved,date_expires,status,classification,
         volume_number,page_count,language,format,file_path,file_size,file_hash,file_url,
         external_ref,storage_system,tags,summary,links,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(documentId), textOrNull(workflowInstanceId),
        textOrNull(workflowStatus), textOrNull(workflowCurrentState),
        textOrNull(releaseWorkflowId),
        textOrNull(projectId), textOrNull(taskId),
        textOrNull(f18OperationId), textOrNull(f18StepId),
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
    workflowStatus=g("workflow_status"); workflowCurrentState=g("workflow_current_state");
    releaseWorkflowId=g("release_workflow_id");
    projectId=g("project_id"); taskId=g("task_id");
    f18OperationId=g("f18_operation_id"); f18StepId=g("f18_step_id");
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
    auto* db=DatabasePool::instance().get("dok");
    if (!db) return false;
    auto rows=db->query("SELECT * FROM documents WHERE document_id=?;",{BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("Document not found: "+id); return false; }
    fromRow(rows[0]); return true;
}
bool Document::remove() {
    auto* db=DatabasePool::instance().get("dok");
    if (!db) return false;
    db->exec("DELETE FROM entity_documents WHERE document_id=?;",{BindParam::text(documentId)});
    return db->exec("DELETE FROM documents WHERE document_id=?;",{BindParam::text(documentId)});
}
bool Document::update() {
    if (isFrozen()) {
        LOG_WARN("[DOK] update() verweigert: Revision ist eingefroren — " + documentId);
        return false;
    } updatedAt=nowIso(); dateModified=updatedAt; return save(); }

std::shared_ptr<Document> Document::loadById(const std::string& id) {
    auto d=std::make_shared<Document>(); if(!d->load(id)) return nullptr; return d;
}
std::vector<std::shared_ptr<Document>> Document::loadForProject(const std::string& pid) {
    auto* db=DatabasePool::instance().get("dok");
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
    auto* db=DatabasePool::instance().get("dok");
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
    auto* db = DatabasePool::instance().get("dok");
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
    if (!fgets(buf, sizeof(buf), pipe)) buf[0] = '\0';
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

    // No snapshot before URL refresh — revise() is the sole entry point for revisions

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



// ── revertChanges ─────────────────────────────────────────────
bool Document::revertChanges() {
    // Restore the document to the state of the PREVIOUS revision.
    //
    // Rules:
    //   - Only works when current active revision is in_work.
    //   - Finds the revision BEFORE the current one (parentRev).
    //   - Restores that revision's content into the current in_work revision
    //     (overwrites its LMDB content). No new revision is created.
    //   - If no previous revision exists (rev == 1 with no parent): does nothing.
    //   - Discards the local checked-out working copy.

    // Must have a checkout in progress
    if (preCheckoutRevId == 0 && checkedOutPath.empty()) {
        LOG_WARN("[Document] revertChanges: kein aktiver Checkout fuer " + documentId);
        return false;
    }

    auto curRev = Rosenholz::DocumentRevision::currentRevision(documentId);
    if (!curRev) {
        LOG_ERROR("[Document] revertChanges: keine aktive Revision fuer " + documentId);
        return false;
    }
    if (curRev->revState != DocRevState::IN_WORK) {
        LOG_ERROR("[Document] revertChanges: aktive Revision ist nicht in_work");
        return false;
    }

    // Find the previous revision via parentRev
    if (curRev->parentRev == 0) {
        // This IS revision 1 — no previous revision to restore from.
        // Just discard the local copy and leave the revision content empty.
        if (!checkedOutPath.empty()) {
            std::remove(checkedOutPath.c_str());
            checkedOutPath.clear();
        }
        preCheckoutRevId = 0;
        LOG_INFO("[Document] revertChanges: erste Revision — kein Vorzustand vorhanden, "
                 "ausgecheckte Datei verworfen");
        return false; // nothing to restore
    }

    auto prevRev = Rosenholz::DocumentRevision::loadByRev(documentId, curRev->parentRev);
    if (!prevRev) {
        LOG_ERROR("[Document] revertChanges: Vorgaenger-Revision " +
                  std::to_string(curRev->parentRev) + " nicht gefunden");
        return false;
    }

    auto& store = Rosenholz::Archive::ArchiveStore::instance();

    // Restore previous revision's content into the current in_work revision
    if (!prevRev->contentHash.empty() && store.isOpen()) {
        std::string tmpDir = FileOps::joinPath(Config::instance().basePath(), "tmp_revert");
        FileOps::makeDirs(tmpDir);
        std::string fname = documentId + (format.empty() ? "" : "." + format);
        std::string tmpPath = FileOps::joinPath(tmpDir, fname);

        if (store.retrieveContent(documentId, prevRev->rev, tmpPath)) {
            std::string stagePath;
            auto ref = store.stageContent(tmpPath, stagePath);
            if (ref.valid()) {
                // Overwrite current in_work revision's content in LMDB
                if (store.commitContent(stagePath, ref, documentId, curRev->rev)) {
                    curRev->contentHash = ref.sha256;
                    curRev->contentSize = (int64_t)ref.size;
                    curRev->update();
                    fileHash = ref.sha256;
                    fileSize = (int64_t)ref.size;
                    update();
                    FileOps::deleteFile(tmpPath);
                    if (!checkedOutPath.empty()) {
                        std::remove(checkedOutPath.c_str());
                        checkedOutPath.clear();
                    }
                    preCheckoutRevId = 0;
                    LOG_INFO("[Document] revertChanges: Inhalt auf Rev " +
                             std::to_string(prevRev->rev) + " zurueckgesetzt fuer " + documentId);
                    return true;
                }
            }
        }
    }

    // Fallback: no LMDB content in previous revision
    if (!checkedOutPath.empty()) {
        std::remove(checkedOutPath.c_str());
        checkedOutPath.clear();
    }
    preCheckoutRevId = 0;
    LOG_WARN("[Document] revertChanges: Vorgaenger-Revision hat keinen LMDB-Inhalt — "
             "Arbeitskopie verworfen, Revision-Inhalt unveraendert");
    return false;
}

std::vector<Document::VersionRecord> Document::loadVersions() const {
    // Returns version history via DocumentRevision (replaces document_versions table).
    std::vector<VersionRecord> result;
    auto revs = DocumentRevision::loadAllRevisions(documentId);
    for (auto& r : revs) {
        VersionRecord v;
        v.versionId     = r->documentId + "/r" + std::to_string(r->rev);
        v.versionNumber = std::to_string(r->rev);
        v.filePath      = filePath;
        v.fileHash      = r->contentHash;
        v.fileSize      = r->contentSize;
        v.createdBy     = r->createdBy;
        v.changeNote    = r->changeNote;
        v.createdAt     = r->createdAt;
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

// ------------------------------
// loadRecent
// Returns the n most recently created Document records.
// Parameters:
//   n : maximum number of results (default 20)
// ------------------------------
std::vector<std::shared_ptr<Document>> Document::loadRecent(int n) {
    std::vector<std::shared_ptr<Document>> result;
    auto* db = DatabasePool::instance().get("dok");
    if (!db) return result;
    auto rows = db->query("SELECT * FROM documents ORDER BY created_at DESC LIMIT ?;", {BindParam::int64(n)});
    for (auto& r : rows) {
        auto obj = std::make_shared<Document>();
        obj->fromRow(r);
        result.push_back(obj);
    }
    return result;
}

// ── State predicates ──────────────────────────────────────────
bool Document::isInWork() const {
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    return cur && cur->revState == DocRevState::IN_WORK;
}

bool Document::isFrozen() const {
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    return cur && cur->revState != DocRevState::IN_WORK;
}

bool Document::canRevise() const {
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    // No revisions yet: can create first revision
    if (!cur) return true;
    // Active revision must be frozen (not in_work) to branch a new one
    return cur->revState != DocRevState::IN_WORK;
}

bool Document::canCheckout() const {
    // Must be in_work and no checkout already open
    return isInWork() && checkedOutPath.empty();
}

bool Document::canCheckin() const {
    // Must be in_work and a checkout must be open
    return isInWork() && !checkedOutPath.empty();
}

bool Document::canRevert() const {
    if (!isInWork() || checkedOutPath.empty()) return false;
    // Must have a parent revision to restore from
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    return cur && cur->parentRev > 0;
}

bool Document::canEdit() const {
    return isInWork();
}


// ── Lifecycle: ensure Main WFI for this document ─────────────
void Document::ensureReleaseWorkflow() {
    if (!releaseWorkflowId.empty()) return;
    auto wf = Rosenholz::F77_Engine::startDefault("dok", documentId);
    if (!wf) return;
    releaseWorkflowId = wf->workflowId;
    status         = "in_work";
    auto* db = DatabasePool::instance().get("dok");
    if (db) db->exec(
        "UPDATE documents SET release_workflow_id=?, status='in_work', updated_at=? "
            "WHERE document_id=?;",
            {BindParam::text(releaseWorkflowId),
             BindParam::text(nowIso()),
             BindParam::text(documentId)});
    LOG_INFO("[F77] Workflow created: " + releaseWorkflowId +
             " for doc " + documentId);
}

// ── revise ────────────────────────────────────────────────────
//
// The sole entry point for creating new document revisions.
// All other code paths that used to call createRevision have
// been consolidated here.
std::shared_ptr<DocumentRevision> Document::revise(
    const std::string& changeNote,
    const std::string& createdBy)
{
    uint32_t latest = DocumentRevision::latestRevNumber(documentId);

    // Case 1: No revisions yet — create the first revision.
    if (latest == 0) {
        auto rev = DocumentRevision::createRevision(
            documentId, 0,
            createdBy.empty() ? authorId : createdBy,
            changeNote.empty() ? "Revision 1 — Initialzustand" : changeNote);
        if (rev) {
            // Sync document status
            auto* db = DatabasePool::instance().get("dok");
            if (db) db->exec(
                "UPDATE documents SET status='in_work', updated_at=? WHERE document_id=?;",
                {BindParam::text(nowIso()), BindParam::text(documentId)});
            status = "in_work";
            LOG_INFO("[Document] Revision 1 angelegt: " + documentId);
        }
        return rev;
    }

    // Case 2: Check the active revision state.
    auto cur = DocumentRevision::currentRevision(documentId);
    if (!cur) {
        LOG_ERROR("[Document] revise: keine aktive Revision gefunden fuer " + documentId);
        return nullptr;
    }

    // Refuse if active revision is still in_work:
    // only one in_work revision is allowed at a time.
    if (cur->revState == DocRevState::IN_WORK) {
        LOG_WARN("[Document] revise: aktive Revision ist noch in_work — "
                 "erst Workflow starten und Revision freigeben, bevor neue angelegt werden kann.");
        return nullptr;
    }

    // Active revision is frozen (pre_released / released / locked / closed):
    // create the next revision branching from the current one.
    auto newRev = DocumentRevision::createRevision(
        documentId, cur->rev,
        createdBy.empty() ? authorId : createdBy,
        changeNote.empty() ? "Neue Revision" : changeNote);
    if (newRev) {
        auto* db = DatabasePool::instance().get("dok");
        if (db) db->exec(
            "UPDATE documents SET status='in_work', updated_at=? WHERE document_id=?;",
            {BindParam::text(nowIso()), BindParam::text(documentId)});
        status = "in_work";
        LOG_INFO("[Document] Revision " + std::to_string(newRev->rev) +
                 " angelegt: " + documentId);
    }
    return newRev;
}


// ── Lifecycle: ensure Revision 1 exists for this document ────

// ── checkout ─────────────────────────────────────────────────
std::string Document::checkout(const std::string& destDir) {
    auto& store = Rosenholz::Archive::ArchiveStore::instance();

    // Record which revision was current at checkout time so revertChanges()
    // can restore from the PREVIOUS revision if needed.
    // No new revision is created here — revise() is the only entry point.
    auto preRev = Rosenholz::DocumentRevision::currentRevision(documentId);
    if (preRev) {
        preCheckoutRevId = preRev->rev;
        LOG_INFO("[Document] checkout: recording preCheckoutRevId=" +
                 std::to_string(preRev->rev) + " for " + documentId);
    }

    // Determine where to put the local file
    std::string dir = destDir.empty()
        ? FileOps::joinPath(FileOps::joinPath(
              Config::instance().basePath(), "checkout"), documentId)
        : destDir;
    FileOps::makeDirs(dir);

    // Derive filename
    std::string fname = documentId;
    if (!format.empty()) fname += "." + format;
    std::string localPath = FileOps::joinPath(dir, fname);

    // Try LMDB first (authoritative content store)
    auto rev = Rosenholz::DocumentRevision::currentRevision(documentId);
    if (rev && store.isOpen() && !rev->contentHash.empty()) {
        if (store.retrieveContent(documentId, rev->rev, localPath)) {
            checkedOutPath = localPath;
            LOG_INFO("[Document] Checked out Rev " + std::to_string(rev->rev) +
                     " → " + localPath);
            return localPath;
        }
    }

    // Fall back to MFS file path
    if (!filePath.empty() && FileOps::fileExists(filePath)) {
        if (FileOps::copyFile(filePath, localPath, true)) {
            checkedOutPath = localPath;
            LOG_INFO("[Document] Checked out from MFS → " + localPath);
            return localPath;
        }
    }

    // Nothing to check out — create an empty file
    FileOps::writeTextFile(localPath, "", false);
    checkedOutPath = localPath;
    LOG_INFO("[Document] Checked out (empty) → " + localPath);
    return localPath;
}

// ── checkin ──────────────────────────────────────────────────
bool Document::checkin(const std::string& srcPath) {
    std::string path = srcPath.empty() ? checkedOutPath : srcPath;
    if (path.empty() || !FileOps::fileExists(path)) {
        LOG_ERROR("[Document] checkin: no file to check in (path=" + path + ")");
        return false;
    }

    auto& store = Rosenholz::Archive::ArchiveStore::instance();

    // Stage to LMDB
    std::string tmpPath;
    auto ref = store.stageContent(path, tmpPath);
    if (!ref.valid()) {
        LOG_ERROR("[Document] checkin: staging failed for " + path);
        return false;
    }

    // Commit content to the CURRENT in_work revision.
    // checkin never creates a new revision — only revise() does that.
    auto curRev = Rosenholz::DocumentRevision::currentRevision(documentId);
    if (!curRev) {
        LOG_ERROR("[Document] checkin: keine aktive Revision fuer " + documentId);
        std::remove(tmpPath.c_str());
        return false;
    }
    if (curRev->revState != DocRevState::IN_WORK) {
        LOG_ERROR("[Document] checkin: aktive Revision ist nicht in_work (ist: "
                  + curRev->revState + ") — nur in_work-Revisionen koennen eingecheckt werden");
        std::remove(tmpPath.c_str());
        return false;
    }

    // Commit content to LMDB under the current revision
    if (!store.commitContent(tmpPath, ref, documentId, curRev->rev)) {
        LOG_ERROR("[Document] checkin: commitContent failed");
        return false;
    }

    // Update revision metadata
    curRev->contentHash = ref.sha256;
    curRev->contentSize = (int64_t)ref.size;
    curRev->update();
    auto newRev = curRev; // alias for code below

    // Update document fields
    fileHash = ref.sha256;
    fileSize = (int64_t)ref.size;
    filePath = path;
    update();

    // Remove local checked-out file
    if (srcPath.empty() && !checkedOutPath.empty())
        std::remove(checkedOutPath.c_str());
    checkedOutPath = "";

    LOG_INFO("[Document] Checked in: " + documentId + " Rev " +
             std::to_string(newRev->rev));
    return true;
}

} // namespace Rosenholz

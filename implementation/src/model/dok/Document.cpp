// Document.cpp
#include "Document.h"
#include "DocumentObject.h"
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
#include <set>
#include <iomanip>
#include <openssl/evp.h>
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
    const std::string& title_, const std::string& type_,
    const std::string& taskId_, const std::string& f18OpId_)
{
    auto d = std::make_shared<Document>();
    d->documentId  = genId("AKT"); d->title = title_;
    d->docType     = type_;
    d->taskId      = taskId_;
    d->f18OperationId = f18OpId_;
    d->dateCreated = nowIso();
    d->createdAt   = d->dateCreated; d->updatedAt = d->createdAt;
    d->notes       = "{}";
    LOG_INFO("Document created: " + d->documentId + " \"" + title_ + "\"");
    return d;
}


// ── MFS folder indexing ────────────────────────────────────────
std::vector<std::pair<uint32_t,std::string>> Document::indexMfsFolders() const {
    std::vector<std::pair<uint32_t,std::string>> result;
    // Load all revisions and scan each folder
    auto revs = Rosenholz::DocumentRevision::loadAllRevisions(documentId);
    for (auto& rev : revs) {
        auto unregistered = Rosenholz::DocumentObject::scanForUnregisteredFiles(
            documentId, rev->rev);
        for (auto& path : unregistered)
            result.emplace_back(rev->rev, path);
    }
    return result;
}


OperationResult Document::save() const {
    auto* db = DatabasePool::instance().get("akt");
    if (!db) { LOG_ERROR("Document::save — documents DB unavailable"); return OperationResult::IO_ERROR; }
    OperationResult ok = db->exec(R"(
        INSERT OR REPLACE INTO akten
        (document_id,release_workflow_id,
         task_id,f18_operation_id,f18_step_id,author_id,approved_by,
         doc_type,doc_category,title,version,
         date_created,date_modified,date_approved,date_expires,classification,
         volume_number,page_count,language,format,file_path,file_size,file_hash,
         external_ref,tags,summary,links,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(documentId),
        BindParam::nullOrText(releaseWorkflowId),
        BindParam::nullOrText(taskId),
        BindParam::nullOrText(f18OperationId), BindParam::nullOrText(f18StepId),
        BindParam::nullOrText(authorId), BindParam::nullOrText(approvedBy),
        BindParam::text(docType), BindParam::nullOrText(docCategory),
        BindParam::text(title), BindParam::nullOrText(version),
        BindParam::text(dateCreated), BindParam::nullOrText(dateModified),
        BindParam::nullOrText(dateApproved), BindParam::nullOrText(dateExpires),
        BindParam::nullOrText(classification),
        BindParam::int64(volumeNumber), BindParam::int64(pageCount),
        BindParam::text(language), BindParam::nullOrText(format),
        BindParam::nullOrText(filePath), BindParam::int64(fileSize), BindParam::nullOrText(fileHash),
        BindParam::nullOrText(externalRef),
        BindParam::nullOrText(tags), BindParam::nullOrText(summary),
        BindParam::nullOrText(links), BindParam::text(notes),
        BindParam::text(createdAt), BindParam::text(nowIso())
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
    return ok;
}

void Document::fromRow(const Row& r) {
    auto g=[&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
    documentId=g("document_id");
    releaseWorkflowId=g("release_workflow_id");
    taskId=g("task_id");
    f18OperationId=g("f18_operation_id"); f18StepId=g("f18_step_id");
    authorId=g("author_id"); approvedBy=g("approved_by");
    docType=g("doc_type"); docCategory=g("doc_category");
    title=g("title"); version=g("version");
    dateCreated=g("date_created"); dateModified=g("date_modified");
    dateApproved=g("date_approved"); dateExpires=g("date_expires");
    classification=g("classification");
    auto gi=[&](const std::string& k){ auto v=g(k); return v.empty()?0:std::stoi(v); };
    volumeNumber=gi("volume_number"); pageCount=gi("page_count");
    language=g("language"); format=g("format");
    filePath=g("file_path");
    { auto sv=g("file_size"); fileSize=sv.empty()?0:std::stoll(sv); } fileHash=g("file_hash");
    externalRef=g("external_ref");
    tags=g("tags"); summary=g("summary"); links=g("links"); notes=g("notes");
    createdAt=g("created_at"); updatedAt=g("updated_at");
}
bool Document::load(const std::string& id) {
    auto* db=DatabasePool::instance().get("akt");
    if (!db) return false;
    auto rows=db->query("SELECT * FROM akten WHERE document_id=?;",{BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("Document not found: "+id); return false; }
    fromRow(rows[0]); return true;
}
OperationResult Document::remove() {
    auto* db=DatabasePool::instance().get("akt");
    if (!db) return OperationResult::IO_ERROR;
    db->exec("DELETE FROM entity_akten WHERE document_id=?;",{BindParam::text(documentId)});
    return db->exec("DELETE FROM akten WHERE document_id=?;",{BindParam::text(documentId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}
OperationResult Document::update() {
    if (!isEditable()) {
        LOG_WARN("[AKT] update() verweigert: Revision ist eingefroren — " + documentId);
        return OperationResult::DOC_REV_NOT_IN_WORK;
    } updatedAt=nowIso(); dateModified=updatedAt; return save(); }

std::shared_ptr<Document> Document::loadById(const std::string& id) {
    auto d=std::make_shared<Document>(); if(!d->load(id)) return nullptr; return d;
}std::vector<std::shared_ptr<Document>> Document::loadForEntity(
    const std::string& et, const std::string& eid)
{
    auto* db=DatabasePool::instance().get("akt");
    std::vector<std::shared_ptr<Document>> result;
    if (!db) return result;

    // Primary: documents with task_id / project_id set directly
    if (et == "f22") {
        auto rows = db->query(
            "SELECT * FROM akten WHERE task_id=? ORDER BY date_created DESC;",
            {BindParam::text(eid)});
        for (auto& r : rows) {
            auto d = std::make_shared<Document>(); d->fromRow(r); result.push_back(d);
        }
    }

    // Also pick up polymorphically-attached documents via entity_akten
    std::set<std::string> seen;
    for (auto& d : result) seen.insert(d->documentId);

    auto rows2 = db->query(R"(
        SELECT d.* FROM akten d
        JOIN entity_akten e ON d.document_id=e.document_id
        WHERE e.entity_type=? AND e.entity_id=? ORDER BY d.date_created DESC;
    )", {BindParam::text(et), BindParam::text(eid)});
    for (auto& r : rows2) {
        auto d = std::make_shared<Document>(); d->fromRow(r);
        if (!seen.count(d->documentId)) { result.push_back(d); seen.insert(d->documentId); }
    }
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
    d->filePath     = localPath;
    d->authorId     = aid;
    d->dateCreated  = nowIso();
    d->format       = FileOps::extension(localPath);

    int64_t sz = FileOps::fileSize(localPath);
    d->summary = "Archived from " + url + " (" + std::to_string(sz) + " bytes)";

    d->save();
    LOG_INFO("URL archived as document: " + d->documentId + " path=" + localPath);
    return d;
}

std::string Document::archiveWebsite(const std::string& url, const std::string& destDir) {
#ifdef _WIN32
    return FileOps::downloadUrl(url, destDir);
#else

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
#endif // _WIN32
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
//   Inserts into entity_akten table in documents.db
//   Idempotent: INSERT OR IGNORE prevents duplicates
//
// Returns:
//   true on success
// ------------------------------
OperationResult Document::attachToEntity(const std::string& et, const std::string& eid, const std::string& rel) {
    auto* db = DatabasePool::instance().get("akt");
    if (!db) return OperationResult::DB_ERROR;
    return db->exec(
        "INSERT OR IGNORE INTO entity_akten(entity_type,entity_id,document_id,relationship) VALUES(?,?,?,?);",
        {BindParam::text(et), BindParam::text(eid), BindParam::text(documentId), BindParam::text(rel)})
        ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}
OperationResult Document::reassignAuthor(const std::string& id) { authorId=id; return update(); }
OperationResult Document::reassignToTask(const std::string& id) { taskId=id; return update(); }
nlohmann::json Document::toJson() const {
    return {{"documentId",documentId},{"title",title},{"docType",docType},
            {"format",format},{"status",currentRevisionState()}};
}

// ══════════════════════════════════════════════════════════════
// DOCUMENT VERSION MANAGEMENT
// ══════════════════════════════════════════════════════════════

// ── SHA-256 via /usr/bin/sha256sum ───────────────────────────
static std::string computeSHA256(const std::string& filePath) {
    if (!FileOps::fileExists(filePath)) return "";
    std::ifstream in(filePath, std::ios::binary);
    if (!in) return "";
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx); return ""; }
    char buf[65536];
    while (in.read(buf, sizeof(buf)) || in.gcount() > 0)
        EVP_DigestUpdate(ctx, buf, static_cast<size_t>(in.gcount()));
    unsigned char hash[EVP_MAX_MD_SIZE]; unsigned int hashLen = 0;
    EVP_DigestFinal_ex(ctx, hash, &hashLen); EVP_MD_CTX_free(ctx);
    std::ostringstream oss;
    for (unsigned int i = 0; i < hashLen; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    return oss.str();
}

// ── MFS document directory for this document ─────────────────
static std::string docMFSDir(const Document& d) {
    const std::string& mfsRoot = Config::instance().mfsPath();
    // Parent: F22 task or F18 operation
    std::string parent = d.taskId.empty() ? d.f18OperationId : d.taskId;
    if (parent.empty()) return "";
    std::string sane = sanitiseRegNr(parent);
    // mfs/DOK/{parent_sane}/
    return FileOps::joinPath(mfsRoot, "AKT", sane);
}

// ── MFS canonical filename for a document version ────────────
static std::string mfsDocFilename(const Document& d, int revNumber = 0) {
    std::string docSane   = sanitiseRegNr(d.documentId);
    std::string titleSafe = FileOps::sanitizeFilename(d.title);
    if (titleSafe.size() > 40) titleSafe = titleSafe.substr(0, 40);
    std::string ext = d.format.empty() ? "bin" : d.format;
    // Format: {DOK-ID}_{Titel}_r{NNN}.{ext}  (revision-based, no version field)
    int rev = (revNumber > 0) ? revNumber
                               : (int)DocumentRevision::latestRevNumber(d.documentId);
    if (rev <= 0) rev = 1;
    char revbuf[8];
    std::snprintf(revbuf, sizeof(revbuf), "r%03d", rev);
    return docSane + "_" + titleSafe + "_" + revbuf + "." + ext;
}

OperationResult Document::importLocalFile(const std::string& srcPath) {
    if (!FileOps::fileExists(srcPath)) {
        LOG_ERROR("importLocalFile: not found: " + srcPath);
        return OperationResult::IO_ERROR;
    }

    std::string dir = docMFSDir(*this);
    if (dir.empty()) {
        LOG_ERROR("importLocalFile: document has no task/f18 reference");
        return OperationResult::DB_ERROR;
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
        return OperationResult::DB_ERROR;
    }

    filePath = destPath;
    fileSize = FileOps::fileSize(destPath);
    fileHash = computeSHA256(destPath);
    LOG_INFO("Document imported to MFS: " + destPath);
    return update();
}




// ── revertChanges ─────────────────────────────────────────────
// Restore content to the previous revision's state.
// Helper: discard local checkout without touching LMDB.
static void discardCheckout(std::string& coPath, uint32_t& preRevId) {
    if (!coPath.empty()) { std::remove(coPath.c_str()); coPath.clear(); }
    preRevId = 0;
}

// Helper: copy previous revision content into current in_work revision in LMDB.
static bool restoreContentFromLMDB(
    Rosenholz::Archive::ArchiveStore& store,
    const std::string& documentId,
    const std::string& format,
    std::shared_ptr<Rosenholz::DocumentRevision> prevRev,
    std::shared_ptr<Rosenholz::DocumentRevision> curRev)
{
    if (prevRev->contentHash.empty() || !store.isOpen()) return false;

    std::string tmpDir  = FileOps::joinPath(Config::instance().basePath(), "tmp_revert");
    FileOps::makeDirs(tmpDir);
    std::string tmpPath = FileOps::joinPath(tmpDir,
        documentId + (format.empty() ? "" : "." + format));

    if (!store.retrieveContent(documentId, prevRev->rev, tmpPath)) return false;

    std::string stagePath;
    auto ref = store.stageContent(tmpPath, stagePath);
    if (!ref.valid()) return false;
    if (!store.commitContent(stagePath, ref, documentId, curRev->rev)) return false;

    curRev->contentHash = ref.sha256;
    curRev->contentSize = static_cast<int64_t>(ref.size);
    curRev->update();
    FileOps::deleteFile(tmpPath);
    return true;
}

OperationResult Document::revertChanges() {
    // Guard clauses — all failure conditions up front, success path runs flat.
    if (preCheckoutRevId == 0 && checkedOutPath.empty()) {
        LOG_WARN("[Document] revertChanges: kein aktiver Checkout fuer " + documentId);
        return OperationResult::IO_ERROR;
    }

    auto curRev = Rosenholz::DocumentRevision::currentRevision(documentId);
    if (!curRev) {
        LOG_ERROR("[Document] revertChanges: keine aktive Revision"); return OperationResult::DB_ERROR;
    }
    if (curRev->revState != RevState::IN_WORK) {
        LOG_ERROR("[Document] revertChanges: Revision ist nicht in_work"); return OperationResult::DB_ERROR;
    }
    if (curRev->parentRev == 0) {
        // Rev 1: no parent — discard checkout and return
        discardCheckout(checkedOutPath, preCheckoutRevId);
        LOG_INFO("[Document] revertChanges: erste Revision — keine Vorgaenger");
        return OperationResult::DOC_NO_PARENT_REV;
    }

    auto prevRev = Rosenholz::DocumentRevision::loadByRev(documentId, curRev->parentRev);
    if (!prevRev) {
        LOG_ERROR("[Document] revertChanges: Vorgaenger-Revision nicht gefunden");
        return OperationResult::DB_ERROR;
    }

    // Success path — flat, no deep nesting
    auto& store = Rosenholz::Archive::ArchiveStore::instance();
    bool restored = restoreContentFromLMDB(store, documentId, format, prevRev, curRev);

    if (restored) {
        fileHash = curRev->contentHash;
        fileSize = curRev->contentSize;
        update();
        LOG_INFO("[Document] revertChanges: auf Rev " +
                 std::to_string(prevRev->rev) + " zurueckgesetzt");
    } else {
        LOG_WARN("[Document] revertChanges: kein LMDB-Inhalt in Vorgaenger-Revision");
    }

    discardCheckout(checkedOutPath, preCheckoutRevId);
    return restored ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
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

bool Document::openFile(const std::string& mode, const std::string& pathOverride) const {
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
std::vector<std::shared_ptr<Document>> Document::loadRecent(int n,
    DocLoadRule /*rule*/, const std::string& /*targetDate*/) {
    std::vector<std::shared_ptr<Document>> result;
    auto* db = DatabasePool::instance().get("akt");
    if (!db) return result;
    auto rows = db->query("SELECT * FROM akten ORDER BY created_at DESC LIMIT ?;", {BindParam::int64(n)});
    for (auto& r : rows) {
        auto obj = std::make_shared<Document>();
        obj->fromRow(r);
        result.push_back(obj);
    }
    return result;
}

// ── State predicates ──────────────────────────────────────────
// OperationResult versions — authoritative






// ── State predicates (direct implementation) ──────────────────

RevState Document::currentRevisionState() const {
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    return cur ? cur->revState : RevState::IN_WORK;
}

bool Document::isEditable() const {
    return currentRevisionState() == RevState::IN_WORK;
}
bool Document::canRevise() const {
    // No revision yet: can create first. Otherwise: state must not be in_work.
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    return !cur || currentRevisionState() != RevState::IN_WORK;
}
bool Document::canCheckout() const {
    return isEditable() && checkedOutPath.empty();
}
bool Document::canCheckin() const {
    return isEditable() && !checkedOutPath.empty();
}
bool Document::canRevert() const {
    if (!isEditable() || checkedOutPath.empty()) return false;
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    return cur && cur->parentRev > 0;
}
bool Document::canEdit() const { return isEditable(); }


// ── Lifecycle: ensure Main WFI for this document ─────────────
void Document::ensureReleaseWorkflow() {
    if (!releaseWorkflowId.empty()) return;
    // startDefault creates the WF and calls storeWorkflowId (one place, in F77Engine).
    auto wf = Rosenholz::F77_Engine::startDefault("akt", documentId);
    if (!wf) return;
    releaseWorkflowId = wf->workflowId;
    LOG_INFO("[F77] Workflow ensured: " + releaseWorkflowId + " for akt/" + documentId);
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
            auto* db = DatabasePool::instance().get("akt");
            if (db) db->exec(
                "UPDATE akten SET updated_at=? WHERE document_id=?;",
                {BindParam::text(nowIso()), BindParam::text(documentId)});
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
    if (cur->revState == RevState::IN_WORK) {
        LOG_WARN("[Document] revise: aktive Revision ist noch in_work — "
                 "erst Workflow starten und Revision freigeben, bevor neue angelegt werden kann.");
        return nullptr;
    }

    // Active revision is frozen — create next revision.
    // Clear the releaseWorkflowId: WFs are revision-specific.
    // The new revision can start a fresh F77 workflow.
    auto newRev = DocumentRevision::createRevision(
        documentId, cur->rev,
        createdBy.empty() ? authorId : createdBy,
        changeNote.empty() ? "Neue Revision" : changeNote);
    if (newRev) {
        auto* db = DatabasePool::instance().get("akt");
        if (db) {
            // Clear WF reference — new revision gets a clean slate.
            db->exec(
                "UPDATE akten SET release_workflow_id=NULL, updated_at=?"
                " WHERE document_id=?;",
                {BindParam::text(nowIso()), BindParam::text(documentId)});
            releaseWorkflowId = ""; // sync in-memory too
        }
        LOG_INFO("[Document] Revision " + std::to_string(newRev->rev) +
                 " angelegt, WF-Referenz zurückgesetzt: " + documentId);
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
    if (curRev->revState != RevState::IN_WORK) {
        LOG_ERROR("[Document] checkin: aktive Revision ist nicht in_work (ist: "
                  + curRev->revStateStr() + ") — nur in_work-Revisionen koennen eingecheckt werden");
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


std::string Document::mfsSchluesselText() const {
    auto cur = Rosenholz::DocumentRevision::currentRevision(documentId);
    std::ostringstream s;
    // Compact reg-nr key (slash→underscore) and revision label
    std::string saneId = documentId;
    for (char& c : saneId) if (c == '/') c = '_';
    uint32_t revNum = cur ? cur->rev : 0;
    char revbuf[8]; std::snprintf(revbuf, sizeof(revbuf), "%03u", revNum);
    std::string revLabel = saneId + "_r" + revbuf;

    s << "  AKTEN-ID         : " << documentId << "  [" << (cur ? cur->revStateStr() : "?") << "]\n"
      << "  TITEL            : " << title << "  [" << docType << "]\n";
    if (!taskId.empty())         s << "  F22-VORGANG      : " << taskId << "\n";
    if (!f18OperationId.empty()) s << "  F18-OPERATION    : " << f18OperationId << "\n";

    if (cur) {
        auto objs = Rosenholz::DocumentObject::loadForRevision(documentId, revNum);
        if (!objs.empty()) {
            s << "\n"
              << "  " << std::left
              << std::setw(10) << "OBJ-ID"
              << std::setw(44) << "BEZEICHNUNG / ORIGINAL"
              << std::setw(52) << "MFS-DATEINAME"
              << "STATUS\n"
              << "  " << std::string(116, '-') << "\n";
            for (auto& o : objs) {
                std::string shortId = o->objectId;
                auto col = shortId.rfind(':');
                if (col != std::string::npos) shortId = shortId.substr(col+1);
                std::string dispName = o->displayName().substr(0,42);
                s << "  " << std::left
                  << std::setw(10) << shortId
                  << std::setw(44) << dispName
                  << std::setw(52) << o->mfsFilename.substr(0,50)
                  << (o->committed ? "LMDB" : "MFS") << "\n";
                if (!o->description.empty())
                    s << "             Beschr.: " << o->description.substr(0,72) << "\n";
            }
        }
    }
    s << "\n";
    return s.str();
}


// ── Document::knownMfsPaths ───────────────────────────────────────────────
// All MFS file paths registered as DocumentObjects for a parent entity.
// Used by scanners to exclude already-registered files from "loose" lists.
std::set<std::string>
Document::knownMfsPaths(const std::string& entityType,
                          const std::string& entityId)
{
    std::set<std::string> known;
    auto docs = loadForEntity(entityType, entityId);
    for (auto& d : docs) {
        auto cur = Rosenholz::DocumentRevision::currentRevision(d->documentId);
        if (!cur) continue;
        auto objs = Rosenholz::DocumentObject::loadForRevision(d->documentId, cur->rev);
        for (auto& o : objs) {
            if (!o->mfsPath.empty())     known.insert(o->mfsPath);
            if (!o->mfsFilename.empty() && !d->mfsDir().empty())
                known.insert(Rosenholz::FileOps::joinPath(d->mfsDir(), o->mfsFilename));
        }
    }
    return known;
}

// ── Document::mfsDir ─────────────────────────────────────────────────────
std::string Document::mfsDir() const {
    const std::string& root = Config::instance().mfsPath();
    if (root.empty() || documentId.empty()) return "";
    std::string sane = documentId;
    for (char& c : sane) if (c == '/') c = '_';
    return FileOps::joinPath(FileOps::joinPath(root, "AKT"), sane);
}

// ── Document::ensureWorkingRevision ──────────────────────────────────────
std::shared_ptr<DocumentRevision>
Document::ensureWorkingRevision() {
    auto cur = DocumentRevision::currentRevision(documentId);
    if (cur && cur->revState == RevState::IN_WORK) return cur;
    if (!cur) return DocumentRevision::createRevision(documentId, 1);
    return nullptr; // released/locked — caller decides
}

} // namespace Rosenholz

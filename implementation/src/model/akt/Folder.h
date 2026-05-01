// ============================================================
// Folder.h  —  Document entity (DOK)
//
// DDR-Registriernummer: XV/DOK/{seq}/{year}
// Mandatory: projectId (MFS filing requires a project).
// Optional: taskId for task-scoped documents.
//
// A Document is a CONTAINER for FolderObjects (physical files).
// State is governed by FolderRevision (5-state machine).
// All field mutations are guarded by canEdit() / update().
// ============================================================
#pragma once
#include <set>
#include "../../core/OperationResult.h"
#include "../Utils.h"
#include <string>
#include <vector>
#include <memory>
#include "../../core/Database.h"
#include <nlohmann/json.hpp>
#include "../../model/akt/FolderRevision.h"

namespace Rosenholz {

// DocLoadRule — how to select the "current" revision when browsing documents.
// Passed to loadForProject(), loadRecent() to control which revision
// of a document is considered active for display purposes.
enum class DocLoadRule {
    LATEST_RELEASED,  // (1) Neueste released-Revision (Standard)
    LATEST_WORKING,   // (2) Neueste nicht-closed Revision (any state)
    DATE_RELEASED,    // (3) Neueste released-Revision zum Stichtag
};

class Folder {
public:
    std::string folderId;
    std::string workflowId;           ///< F77W controlling this folder lifecycle
    std::string taskId;          ///< Filing parent — F22 (or empty for F18-scoped)
    std::string f18OperationId;  ///< F18 Operation (vorgang) reference
    std::string f18StepId;       ///< F18 Operation Step reference
    std::string authorId, approvedBy;

    std::string docType;        // report|specification|contract|correspondence|evidence|archive
    std::string docCategory;
    std::string title;
    std::string version         { "1.0" };
    std::string dateCreated, dateModified, dateApproved, dateExpires;
    std::string classification;
    int         volumeNumber    { 1 };
    int         pageCount       { 0 };
    std::string language        { "EN" };
    std::string format;         // pdf|docx|xlsx|html|txt|image|archive
    std::string filePath;       // local path on disk (in MFS)
    int64_t     fileSize    { 0 };  // bytes
    std::string fileHash;       // SHA-256
    std::string externalRef;
    std::string tags;           // JSON array
    std::string summary;
    std::string links;
    std::string notes;          // JSON
    std::string createdAt, updatedAt;


    // ── Status (computed from active revision) ────────────────
    // status field is kept for backward compat with SQL queries,
    // but the authoritative value is the active revision's revState.
    // Call currentRevisionState() when you need the real state.
    RevState currentRevisionState() const;

    // ── State predicates ──────────────────────────────────────
    // All UI layers call these. Never inspect revState directly.
    // Returns OPERATION_ACK if the operation is allowed.
    // Returns a specific error code explaining why it is not.

    /// OPERATION_ACK if the active revision is in_work.
    OperationResult checkInWork()    const;

    /// OPERATION_ACK if a new revision may be created via revise().
    OperationResult checkCanRevise() const;

    /// OPERATION_ACK if checkout() may be called.
    OperationResult checkCanCheckout() const;

    /// OPERATION_ACK if checkin() may be called.
    OperationResult checkCanCheckin() const;

    /// OPERATION_ACK if revertChanges() may be called.
    OperationResult checkCanRevert()  const;

    /// OPERATION_ACK if fields may be edited.
    OperationResult checkCanEdit()    const;

    // ── Bool convenience wrappers (for internal/legacy use) ───
    // Revision state predicates — use currentRevisionState() for full state.
    // isEditable: revision is IN_WORK (can edit, checkout, add objects)
    bool isEditable()   const; ///< replaces isInWork() in v5
    bool canRevise()    const;

    /// Returns the current in_work revision, creating one (rev=1) if none exists.
    /// Returns nullptr only if the document is locked/released and cannot be revised.
    std::shared_ptr<FolderRevision> ensureWorkingRevision();
    bool canCheckout()  const;
    bool canCheckin()   const;
    bool canRevert()    const;
    bool canEdit()      const;

    // ── MFS folder indexing ───────────────────────────────────
    /// Scan ALL revision folders of this document for files that are
    /// not yet registered as FolderObjects.
    /// Returns: list of (revNumber, fullFilePath) pairs for unregistered files.
    std::vector<std::pair<uint32_t,std::string>> indexMfsFolders() const;

    // ── CRUD ──────────────────────────────────────────────
    OperationResult save() const;
    void ensureReleaseWorkflow();  ///< Creates Main WFI on first save

    bool load(const std::string& id);
    OperationResult remove();
    OperationResult update();

    // ── Factory ───────────────────────────────────────────
    // ------------------------------
    // Factory: create a new Document record (not yet saved).
    //
    // Parameters:
    //   title     : human-readable document title (required)
    //   docType   : "report"|"specification"|"contract"|"correspondence"|
    //               "evidence"|"plan"|"minutes"|"archive"|"other"
    //   projectId : owning project ID (may be empty for orphans,
    //               but MFS filing will be refused without one)
    //
    // Returns:
    //   Shared pointer to in-memory Document; call save() to persist.
    // ------------------------------
    static int count();
    static std::shared_ptr<Folder> create(
        const std::string& title,
        const std::string& docType     = "report",
        const std::string& taskId      = "",
        const std::string& f18OpId     = "");

    static std::vector<std::shared_ptr<Folder>> loadRecent(
        int n = 20,
        DocLoadRule rule   = DocLoadRule::LATEST_RELEASED,
        const std::string& targetDate = "");
    static std::shared_ptr<Folder> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Folder>> loadForProject(
        const std::string& projectId,
        DocLoadRule rule   = DocLoadRule::LATEST_RELEASED,
        const std::string& targetDate = "");
    static std::vector<std::shared_ptr<Folder>> loadForEntity(
        const std::string& entityType, const std::string& entityId);

    // ── URL download and archive ──────────────────────────
    /// Download URL, detect type, archive locally, return Document.
    /// Websites archived as PDF (via wkhtmltopdf/wget) or HTML.
    // ------------------------------
    // Download a URL and register it as an archived Document.
    //
    // Parameters:
    //   url       : HTTP/HTTPS URL to download or archive
    //   projectId : owning project ID (required for MFS filing)
    //   authorId  : Person-ID of the author (optional)
    //
    // Behavior:
    //   - For web pages (non-file URLs): attempts PDF conversion
    //     via wkhtmltopdf, falls back to HTML snapshot
    //   - For direct file URLs (.pdf, .docx, etc.): downloads directly
    //   - Stores in {basePath}/documents/archived/
    //   - Creates and saves a Document record
    //
    // Returns:
    //   nullptr on download failure
    // ------------------------------
    static std::shared_ptr<Folder> archiveFromUrl(
        const std::string& url,
        const std::string& projectId  = "",
        const std::string& authorId   = "");

    // ── Attach to any entity ──────────────────────────────
    OperationResult attachToEntity(const std::string& entityType, const std::string& entityId,
                                   const std::string& relationship = "attached");

    // ── Reassign ─────────────────────────────────────────
    OperationResult reassignAuthor(const std::string& newAuthorId);
    OperationResult reassignToTask(const std::string& newTaskId);

    nlohmann::json toJson() const;

    // ── Version management ────────────────────────────────
    struct VersionRecord {
        std::string versionId, versionNumber, filePath, fileHash;
        int64_t     fileSize{0};
        std::string createdBy, changeNote, createdAt;
    };

    // ── revise ────────────────────────────────────────────────
    // The SOLE entry point for creating new document revisions.
    //
    // Rules enforced:
    //   1. If no revisions exist yet: creates Revision 1 (in_work).
    //   2. If a revision exists and is in_work: refuses — only one
    //      in_work revision is allowed at a time.
    //   3. If the active revision is NOT in_work (i.e. it was frozen
    //      by completing a workflow): creates the next revision (in_work).
    //
    // All other revision-creating operations (ensureRevision1,
    // snapshotVersion, checkout) have been removed. revise() is the
    // only method that calls FolderRevision::createRevision.
    //
    // Parameters:
    //   changeNote : description of why this revision was created
    //   createdBy  : Person-ID (optional)
    // Returns: new revision, or nullptr if rules prevent creation.
    std::shared_ptr<FolderRevision> revise(
        const std::string& changeNote = "",
        const std::string& createdBy  = "");


    /// List all saved versions (newest first).
    // ------------------------------
    // Load all historical versions of this document.
    //
    // Returns:
    //   Vector of VersionRecord, ordered newest-first.
    //   Each record contains: version number, file path, size,
    //   SHA-256 hash, author, change note, and timestamp.
    // ------------------------------
    std::vector<VersionRecord> loadVersions() const;
    /// Import a local file: copy into MFS directory, update filePath/hash/size.
    // ------------------------------
    // Copy a local file into the MFS document directory.
    //
    // Parameters:
    //   srcPath : absolute path to the source file
    //
    // Behavior:
    //   - Detects format from file extension if not already set
    //   - Constructs MFS destination:
    //       mfs/DOK/{parent-sane}/{DOK-ID}_{Title}_v{Version}.{ext}
    //   - Copies the file (overwrites if same name exists)
    //   - Updates filePath, fileSize, and fileHash (SHA-256)
    //   - Calls update() to persist changes to DB
    //
    // Returns:
    //   true on success; false if src not found, no parent ref, or copy fails
    // ------------------------------
    // ── Checkout / Checkin ──────────────────────────────────
    // checkout: copies current LMDB content to a local working directory.
    //   Returns the local path. Sets checkedOutPath on this object.
    //   The file is editable locally until checkin() is called.
    std::string checkout(const std::string& destDir = "");

    // checkin: stages+commits the local file to LMDB, creates a new
    //   FolderRevision (in_work). The local copy is removed after commit.
    //   Returns false if no checked-out path or commit fails.
    bool checkin(const std::string& srcPath = "");

    // checkedOutPath: local path of the currently checked-out file.
    // Empty if not checked out.
    std::string checkedOutPath;

    // preCheckoutRevId: revision number that was current when checkout()
    // was called.  Used by revertChanges() to restore the pre-edit state.
    // Zero means no checkout is in progress.
    uint32_t preCheckoutRevId { 0 };

    // revertChanges: restore document to the state recorded at checkout time.
    // Creates a new revision from the pre-checkout snapshot so history is
    // preserved.  Discards the current checked-out working copy.
    // Returns false if no checkout was in progress or the snapshot is missing.
    OperationResult revertChanges();

    OperationResult importLocalFile(const std::string& srcPath);
    /// Open file. mode="read" → temp copy; mode="edit" → original in place.
    // ------------------------------
    // Open the document's physical file using the system viewer.
    //
    // Parameters:
    //   mode : "read"  — opens a temporary copy (original unchanged)
    //          "edit"  — opens the original MFS file directly
    //                    (caller should call snapshotVersion first)
    //
    // Behavior:
    //   - "read": copies filePath to /tmp/rosenholz_view/, opens copy
    //   - "edit": opens filePath directly via xdg-open (Linux) or open (macOS)
    //   - Uses system default application for the file format
    //
    // Returns:
    //   true if the open command returned 0; false if file missing or error
    // ------------------------------
    bool openFile(const std::string& mode = "read",
                  const std::string& pathOverride = "") const;

    std::string mfsSchluesselText() const;
    /// Canonical MFS folder for this Akte (mfs/AKT/<sanitised-id>/)
    std::string mfsDir() const;
    /// Collect all MFS paths of currently registered objects for a given parent entity.
    /// Used by F22/F18 scan to know which files are already registered.
    static std::set<std::string> knownMfsPaths(
        const std::string& entityType, const std::string& entityId);
private:
    void fromRow(const Row& r);

    /// Try to convert website URL to PDF using system tools
    static std::string archiveWebsite(const std::string& url, const std::string& destDir);
};

} // namespace Rosenholz

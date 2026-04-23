// ============================================================
// Document.h  —  Document entity (DOK)
//
// DDR-Aktenzeichen: XV/DOK/{seq}/{year}
// Documents require a projectId for MFS filing
// Versioning via document_versions table
// Physical files live in mfs/DOK/{parent-sane}/
// ============================================================
#pragma once
#include "../Utils.h"
// ============================================================
// Document.h  —  Document entity with URL download + archiving
//
// When a URL link is encountered anywhere in the system,
// DocumentManager::archiveFromUrl() downloads the file and
// creates a full Document record with all attributes.
// Websites are archived as HTML or PDF via system tools.
// Stored in documents.db.
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include "../../core/Database.h"
#include <nlohmann/json.hpp>
#include "../../repository/DocumentRevision.h"

namespace Rosenholz {

class Document {
public:
    std::string documentId;
    std::string workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string releaseWorkflowId;    ///< Main WFI controlling this doc lifecycle
    std::string projectId;
    std::string taskId;
    std::string f18OperationId;  ///< F18 Operation (vorgang) reference
    std::string f18StepId;       ///< F18 Operation Step reference
    std::string authorId, approvedBy;

    std::string docType;        // report|specification|contract|correspondence|evidence|archive
    std::string docCategory;
    std::string title;
    std::string version         { "1.0" };
    std::string dateCreated, dateModified, dateApproved, dateExpires;
    std::string status  { "in_work" }; // in_work|pre_released|released|locked|closed
    std::string classification;
    int         volumeNumber    { 1 };
    int         pageCount       { 0 };
    std::string language        { "EN" };
    std::string format;         // pdf|docx|xlsx|html|txt|image|archive
    std::string filePath;       // local path on disk (in MFS)
    std::string fileStagedPath; // temp path before MFS copy
    int64_t     fileSize    { 0 };  // bytes
    std::string fileHash;       // SHA-256
    std::string fileUrl;        // original source URL
    std::string externalRef;
    std::string storageSystem;
    std::string tags;           // JSON array
    std::string summary;
    std::string links;
    std::string notes;          // JSON
    std::string createdAt, updatedAt;


    // ── State predicates ──────────────────────────────────────
    // The CLI and any other UI must call these — never inspect
    // revState or status directly to make business decisions.

    /// True if the active revision is in_work (the only mutable state).
    bool isInWork() const;

    /// True if the active revision is in a frozen state
    /// (pre_released, released, locked, or closed).
    bool isFrozen() const;

    /// True if a new revision may be created via revise().
    /// Requires: active revision exists and is NOT in_work.
    bool canRevise() const;

    /// True if checkout() may be called.
    /// Requires: active revision is in_work and no checkout is already open.
    bool canCheckout() const;

    /// True if checkin() may be called.
    /// Requires: active revision is in_work and a checkout is open.
    bool canCheckin() const;

    /// True if revertChanges() may be called.
    /// Requires: active revision is in_work, a checkout is open,
    ///           and a parent revision exists.
    bool canRevert() const;

    /// True if the document may be edited (fields, metadata).
    /// Requires: active revision is in_work.
    bool canEdit() const;

    // ── CRUD ──────────────────────────────────────────────
    bool save() const;
    void ensureReleaseWorkflow();  ///< Creates Main WFI on first save

    bool load(const std::string& id);
    bool remove();
    bool update();

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
    static std::shared_ptr<Document> create(
        const std::string& title,
        const std::string& docType   = "report",
        const std::string& projectId = "");

    static std::vector<std::shared_ptr<Document>> loadRecent(int n = 20);
    static std::shared_ptr<Document> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Document>> loadForProject(const std::string& projectId);
    static std::vector<std::shared_ptr<Document>> loadForEntity(
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
    static std::shared_ptr<Document> archiveFromUrl(
        const std::string& url,
        const std::string& projectId  = "",
        const std::string& authorId   = "");

    // ── Attach to any entity ──────────────────────────────
    bool attachToEntity(const std::string& entityType, const std::string& entityId,
                        const std::string& relationship = "attached");

    // ── Reassign ─────────────────────────────────────────
    bool reassignAuthor(const std::string& newAuthorId);
    bool reassignToProject(const std::string& newProjectId);
    bool reassignToTask(const std::string& newTaskId);

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
    // only method that calls DocumentRevision::createRevision.
    //
    // Parameters:
    //   changeNote : description of why this revision was created
    //   createdBy  : Person-ID (optional)
    // Returns: new revision, or nullptr if rules prevent creation.
    std::shared_ptr<DocumentRevision> revise(
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
    //   DocumentRevision (in_work). The local copy is removed after commit.
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
    bool revertChanges();

    bool importLocalFile(const std::string& srcPath);
    /// Re-download fileUrl, snapshot current, update filePath.
    // ------------------------------
    // Re-download the document from fileUrl, replacing the current file.
    //
    // Behavior:
    //   - Calls snapshotVersion() first to preserve the current file
    //   - Downloads to a temporary directory
    //   - Imports via importLocalFile() which updates MFS path + hash
    //   - Bumps the minor version number (e.g. "1.0" → "1.1")
    //   - Deletes the temp download after import
    //   - No-op (returns false) if fileUrl is empty
    //
    // Returns:
    //   true on successful download + import; false on any failure
    // ------------------------------
    bool refreshFromUrl();
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
    bool openFile(const std::string& mode = "read") const;

private:
    void fromRow(const Row& r);

    /// Try to convert website URL to PDF using system tools
    static std::string archiveWebsite(const std::string& url, const std::string& destDir);
};

} // namespace Rosenholz

// ============================================================
// Document.h  —  Document entity (DOK)
//
// DDR-Aktenzeichen: XV/DOK/{seq}/{year}
// Documents require a projectId for MFS filing
// Versioning via document_versions table
// Physical files live in mfs/DOK/{parent-sane}/
// ============================================================
#pragma once
#include "Utils.h"
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
#include "../core/Database.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

class Document {
public:
    std::string documentId;
    std::string workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, taskId;
    std::string authorId, approvedBy;

    std::string docType;        // report|specification|contract|correspondence|evidence|archive
    std::string docCategory;
    std::string title;
    std::string version         { "1.0" };
    std::string dateCreated, dateModified, dateApproved, dateExpires;
    std::string status;         // draft|review|approved|superseded|archived
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

    // ── CRUD ──────────────────────────────────────────────
    bool save() const;
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

    /// Snapshot current file as a new version before editing.
    // ------------------------------
    // Snapshot the current file version before making changes.
    //
    // Parameters:
    //   changeNote : description of why this version was created
    //   createdBy  : Person-ID who triggered the change (optional)
    //
    // Behavior:
    //   - Copies current filePath to a versioned name
    //     ({DOK-ID}_{Title}_v{Version}-snap.{ext}) in the same MFS dir
    //   - Inserts a document_versions record with version number,
    //     file path, size, and hash
    //
    // Returns:
    //   true if the DB insert succeeded; false on error or missing file
    // ------------------------------
    bool snapshotVersion(const std::string& changeNote = "",
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

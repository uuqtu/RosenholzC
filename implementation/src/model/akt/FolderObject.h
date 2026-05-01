// ============================================================
// FolderObject.h  —  Physical file inside a FolderRevision
//
// A Document is a CONTAINER; FolderObject is one physical file
// inside that container at a specific revision.
//
// Responsibilities (one place each):
//   CRUD        — save, update, remove, fromRow
//   Factory     — importFile (3 sources: local/url/empty)
//   Queries     — loadForRevision, mfsRevDir
//   LMDB        — commitToLMDB (write to archive)
//   MFS         — writeKeyFile, scanForUnregisteredFiles
//   Checkout    — checkoutObject, checkinObject, revertObject, openObject
// ============================================================
#pragma once
#include "../../core/OperationResult.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include <string>
#include <vector>
#include <memory>
#include <set>

namespace Rosenholz {

class FolderObject {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string objectId;       ///< 5-char alphanumeric, unique per document
    std::string folderId;     ///< XV/AKT/0001/26
    uint32_t    rev         {0};
    std::string originalName;   ///< filename as uploaded (display name)
    std::string storedFileName;    ///< MFS filename: {docRegNr}_{objectId}_r{rev}.{ext}
    std::string filePath;        ///< full file path (valid in in_work state)
    std::string contentHash;    ///< SHA-256 in LMDB (set when committed=true)
    int64_t     contentSize {0};
    std::string format;         ///< file extension without dot
    std::string sourceUrl;      ///< Original URL if object was downloaded; empty for local
    std::string displayName_;   ///< Human-readable label shown in Schlüssel and listings
    std::string description;    ///< Optional descriptive text stored in Schlüssel
    bool        committed   {false};
    std::string createdAt;
    std::string updatedAt;

    // ── Display ───────────────────────────────────────────────
    std::string displayName() const {
        return displayName_.empty() ? originalName : displayName_;
    }
    std::string summary() const;

    // ── CRUD ──────────────────────────────────────────────────
    void fromRow(const Row& r);
    OperationResult save()   const;
    OperationResult update() const;
    OperationResult remove() const;

    // ── Factory ───────────────────────────────────────────────
    // Single entry point for all import sources (local file, URL-downloaded
    // tmp file, or freshly created empty file). docRegNr derived from docId.
    static std::shared_ptr<FolderObject> importFile(
        const std::string& folderId,
        uint32_t            rev,
        const std::string& srcPath,
        OperationResult&   result,
        const std::string& label       = "",
        const std::string& description = "");

    // ── Queries ───────────────────────────────────────────────
    static std::vector<std::shared_ptr<FolderObject>> loadForRevision(
        const std::string& folderId, uint32_t rev);

    // MFS directory for a given document revision
    static std::string mfsRevDir(const std::string& folderId, uint32_t rev);

    // ── LMDB commit ───────────────────────────────────────────
    // Commit file content to the LMDB archive (SHA-256 keyed).
    // Sets committed=true and contentHash on success.
    OperationResult commitToLMDB();
    OperationResult updateFromUrl(const std::string& url = "");

    static std::shared_ptr<FolderObject> loadById(const std::string& objectId);

    // ── MFS key-file ──────────────────────────────────────────
    // Write/rewrite the _SCHLUESSEL.txt for a revision folder.
    static OperationResult writeKeyFile(
        const std::string& folderId,
        uint32_t            rev,
        const std::string& docTitle = "");

    // Scan a revision MFS folder for files not yet registered as objects.
    // Returns full paths of unregistered files.
    static std::vector<std::string> scanForUnregisteredFiles(
        const std::string& folderId, uint32_t rev);

    // ── Checkout ──────────────────────────────────────────────
    // checkedOut=true means the object is locally checked out for editing.
    bool        checkedOut   {false};
    std::string checkoutPath;

    // Extract to destDir (or MFS rev folder if empty). Sets checkedOut=true.
    std::string checkoutObject(const std::string& destDir = "");

    // Commit the checked-out file back to LMDB. Clears checkedOut.
    OperationResult checkinObject(const std::string& srcPath = "");

    // Reset this object to the content of the same objectId in parentRev.
    OperationResult revertObject(uint32_t parentRev);

    // Open for viewing/editing — respects in_work state and checkout status.
    std::string openObject(bool inWork, bool& wasCheckedOut);

private:
    // Internal helpers — not part of the public API
    static Database*     db();
    static std::string   sanitiseForFilename(const std::string& s);
    static std::string   nextObjectId(const std::string& folderId);
    static std::string   buildMfsFilename(const std::string& docRegNr,
                                           const std::string& objectId,
                                           uint32_t rev,
                                           const std::string& ext);
    std::string          extractFromLMDB(const std::string& destDir = "");
    static std::shared_ptr<FolderObject> loadByDocAndId(
        const std::string& docId, const std::string& objId);
};

} // namespace Rosenholz

// ============================================================
// DocumentObject.h  —  Physical file inside a DocumentRevision
//
// A Document is a CONTAINER; DocumentObject is one physical file
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

class DocumentObject {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string objectId;       ///< 5-char alphanumeric, unique per document
    std::string documentId;     ///< XV/DOK/0001/26
    uint32_t    rev         {0};
    std::string originalName;   ///< filename as uploaded (display name)
    std::string mfsFilename;    ///< MFS filename: {docRegNr}_{objectId}_r{rev}.{ext}
    std::string mfsPath;        ///< full MFS path (valid in in_work state)
    std::string contentHash;    ///< SHA-256 in LMDB (set when committed=true)
    int64_t     contentSize {0};
    std::string format;         ///< file extension without dot
    bool        committed   {false};
    std::string createdAt;
    std::string updatedAt;

    // ── Display ───────────────────────────────────────────────
    const std::string& displayName() const { return originalName; }
    std::string summary() const;

    // ── CRUD ──────────────────────────────────────────────────
    void fromRow(const Row& r);
    OperationResult save()   const;
    OperationResult update() const;
    OperationResult remove() const;

    // ── Factory ───────────────────────────────────────────────
    // Single entry point for all import sources (local file, URL-downloaded
    // tmp file, or freshly created empty file). docRegNr derived from docId.
    static std::shared_ptr<DocumentObject> importFile(
        const std::string& documentId,
        uint32_t            rev,
        const std::string& srcPath,
        OperationResult&   result);

    // ── Queries ───────────────────────────────────────────────
    static std::vector<std::shared_ptr<DocumentObject>> loadForRevision(
        const std::string& documentId, uint32_t rev);

    // MFS directory for a given document revision
    static std::string mfsRevDir(const std::string& documentId, uint32_t rev);

    // ── LMDB commit ───────────────────────────────────────────
    // Commit file content to the LMDB archive (SHA-256 keyed).
    // Sets committed=true and contentHash on success.
    OperationResult commitToLMDB();

    // ── MFS key-file ──────────────────────────────────────────
    // Write/rewrite the _SCHLUESSEL.txt for a revision folder.
    static OperationResult writeKeyFile(
        const std::string& documentId,
        uint32_t            rev,
        const std::string& docTitle = "");

    // Scan a revision MFS folder for files not yet registered as objects.
    // Returns full paths of unregistered files.
    static std::vector<std::string> scanForUnregisteredFiles(
        const std::string& documentId, uint32_t rev);

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
    static std::string   nextObjectId(const std::string& documentId);
    static std::string   buildMfsFilename(const std::string& docRegNr,
                                           const std::string& objectId,
                                           uint32_t rev,
                                           const std::string& ext);
    std::string          extractFromLMDB(const std::string& destDir = "");
    static std::shared_ptr<DocumentObject> loadByDocAndId(
        const std::string& docId, const std::string& objId);
};

} // namespace Rosenholz

#pragma once
// ============================================================
// DocumentObject.h  —  A physical file inside a DocumentRevision
//
// A DocumentRevision is a CONTAINER for DocumentObjects.
// All revision metadata (state, superseded, author, workflow)
// applies to ALL objects in the container.
//
// Object identity
// ───────────────
// objectId  : 5-char uppercase alphanumeric, unique within the document.
//             Generated per-document. Same ID can appear in different documents.
//             Format: e.g. "A1B2C"
//
// MFS filename
// ────────────
//   {docRegNr}_{objectId}_r{revNr}.{ext}
//   e.g.  XV_DOK_0018_2026_A1B2C_r001.xls
//
// MFS folder per revision
// ────────────────────────
//   mfs/DOK/{sanitisedDocId}_{revNr:03d}/
//   e.g.  mfs/DOK/XV_DOK_0018_2026_001/
//
// Key file
// ────────
// Every revision folder contains _SCHLUESSEL.txt:
//   objectId | originalFilename | mfsFilename | committed
// Updated whenever an object is added. Allows understanding the
// contents without the Rosenholz PM application.
//
// Metadata
// ────────
// original_name: the original filename as uploaded/imported.
//               Used by displayName() for human-readable output.
// ============================================================
#pragma once
#include <string>
#include <memory>
#include <vector>
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../../core/OperationResult.h"

namespace Rosenholz {

struct DocumentObject {
    std::string objectId;       // 5-char alphanumeric, unique per document (e.g. "A1B2C")
    std::string documentId;     // XV/DOK/0018/2026
    uint32_t    rev       {1};  // revision number
    std::string originalName;   // original filename as uploaded (e.g. "tests-example.xls")
    std::string mfsFilename;    // MFS filename: {docRegNr}_{objectId}_r{revNr}.{ext}
    std::string mfsPath;        // full MFS path (valid in in_work state)
    std::string contentHash;    // SHA-256 in LMDB (valid when committed=true)
    int64_t     contentSize {0};
    std::string format;         // file extension without dot (e.g. "xls")
    bool        committed   {false};
    std::string createdAt;
    std::string updatedAt;

    // ── Display ──────────────────────────────────────────────
    /// Human-readable: returns originalName (the original filename).
    /// Use this everywhere the user sees the object.
    const std::string& displayName() const { return originalName; }

    /// One-line summary: "A1B2C  tests-example.xls  [LMDB]  42 KB"
    std::string summary() const;

    // ── CRUD ─────────────────────────────────────────────────
    OperationResult save()   const;
    OperationResult update() const;
    OperationResult remove() const;

    void fromRow(const Row& r);

    // ── Factory ──────────────────────────────────────────────
    /// Import a file into the revision MFS folder.
    ///   documentId  : e.g. "XV/DOK/0018/2026"
    ///   rev         : current revision number
    ///   srcPath     : full path to source file
    ///   docRegNr    : same as documentId (used for MFS filename prefix)
    ///   result      : filled with OPERATION_ACK or error code
    /// Returns the created object on success, nullptr on failure.
    static std::shared_ptr<DocumentObject> importFile(
        const std::string& documentId,
        uint32_t            rev,
        const std::string& srcPath,
        OperationResult&   result);

    // ── Queries ──────────────────────────────────────────────
    /// Load by composite key: documentId + ":" + objectId

    /// All objects for one (documentId, rev) pair.
    static std::vector<std::shared_ptr<DocumentObject>> loadForRevision(
        const std::string& documentId, uint32_t rev);

    // ── LMDB commit ──────────────────────────────────────────
    /// Copy MFS file into LMDB.
    /// Sets committed=true, contentHash, contentSize.
    /// Returns OPERATION_ACK on success, error code on failure.
    OperationResult commitToLMDB();


    // ── MFS helpers ──────────────────────────────────────────
    /// Revision folder in MFS.
    /// Format: {mfsRoot}/DOK/{sanitisedDocId}_{revNr:03d}/
    static std::string mfsRevDir(const std::string& documentId, uint32_t rev);


    // ── Key file ─────────────────────────────────────────────
    /// Write (or update) _SCHLUESSEL.txt in the revision MFS folder.
    /// Lists all objects: objectId | originalName | mfsFilename | status.
    /// Called whenever an object is added, committed, or removed.
    static OperationResult writeKeyFile(
        const std::string& documentId,
        uint32_t            rev,
        const std::string& docTitle = "");



    // ── Checkout state ────────────────────────────────────────
    bool        checkedOut    {false};  ///< True when this object is currently checked out
    std::string checkoutPath;           ///< Local path of checked-out file (when checkedOut=true)

    // ── Object-level checkout / checkin / revert ─────────────
    /// Checkout this object to destDir (or MFS rev folder if empty).
    /// Only allowed when revision is in_work and not already checked out.
    /// Returns the local path, empty string on failure.
    std::string checkoutObject(const std::string& destDir = "");

    /// Checkin: commit the checked-out file back into LMDB and update DB.
    /// Clears checkedOut and checkoutPath.
    OperationResult checkinObject(const std::string& srcPath = "");

    /// Revert this object to the content of the same objectId in the
    /// previous revision (parentRev). Does nothing if this is rev 1.
    /// Returns DOC_NO_PARENT_REV if no previous revision exists.
    OperationResult revertObject(uint32_t parentRev);

    /// Open this object for viewing/editing.
    /// If in_work and not checked out: offer checkout or tmp copy.
    /// If not in_work: always copy to tmp (read-only, not tracked).
    /// Returns the path that was opened.
    std::string openObject(bool inWork, bool& wasCheckedOut);

    // ── MFS folder scanner ────────────────────────────────────
    /// Scan a revision MFS folder for files that are not yet registered
    /// as DocumentObjects (unknown files = candidates for import).
    /// Returns full paths of unregistered files.
    static std::vector<std::string> scanForUnregisteredFiles(
        const std::string& documentId,
        uint32_t            rev);

private:
    static Database* db();
    static std::string sanitiseForFilename(const std::string& s);
    static std::string nextObjectId(const std::string& documentId);
    static std::string buildMfsFilename(const std::string& r, const std::string& id, uint32_t rev, const std::string& ext);
    std::string extractFromLMDB(const std::string& destDir = "");
    static std::shared_ptr<DocumentObject> loadByDocAndId(const std::string& docId, const std::string& objId);
};

} // namespace Rosenholz

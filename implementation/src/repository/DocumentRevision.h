#pragma once
// ============================================================
// DocumentRevision.h  —  Document revision model
//
// Each Document has N revisions identified by (documentId, rev).
// One revision per documentId holds superseded = false — that is
// the "current active revision".
//
// State machine:
//   in_work (0x00)  →  pre_released, locked, closed
//   pre_released (0x01)  →  released, locked, closed, in_work
//   released (0x02)  →  locked, closed  [immutable — no edits]
//   locked (0x03)  →  pre_released (only if newest rev), closed
//   closed (0x04)  →  [terminal — no transitions]
//
// Superseded priority (who holds false):
//   1. Latest released revision
//   2. Latest non-locked/non-closed revision (if no released exists)
//   3. Latest in_work revision (fallback)
// ============================================================

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "../core/Database.h"

namespace Rosenholz {

// ------------------------------
// DocRevState — five states
// ------------------------------
namespace DocRevState {
    constexpr const char* IN_WORK      = "in_work";      // 0x00 mutable
    constexpr const char* PRE_RELEASED = "pre_released"; // 0x01 read-only
    constexpr const char* RELEASED     = "released";     // 0x02 immutable
    constexpr const char* LOCKED       = "locked";       // 0x03 frozen
    constexpr const char* CLOSED       = "closed";       // 0x04 terminal
}

// ============================================================
// DocumentRevision
// ============================================================
class DocumentRevision {
public:
    // ── Identity ──────────────────────────────────────────────
    std::string documentId;     // XV/DOK/…  (same as parent document)
    uint32_t    rev         {1}; // revision counter, starts at 1

    // ── Lineage ───────────────────────────────────────────────
    uint32_t    parentRev   {0}; // 0 = initial revision (no parent)

    // ── State ─────────────────────────────────────────────────
    std::string revState { DocRevState::IN_WORK };
    // Exactly one revision per documentId holds superseded = false
    bool        superseded { true };

    // ── Content reference ─────────────────────────────────────
    // SHA-256 of the committed file chunk in LMDB (empty until commitContent)
    std::string contentHash;
    int64_t     contentSize {0};

    // ── Metadata ──────────────────────────────────────────────
    std::string createdBy;
    std::string changeNote;
    std::string createdAt;
    std::string updatedAt;

    // ── CRUD ──────────────────────────────────────────────────
    bool save()   const;
    bool update();
    bool remove() const;

    // ── Factory ───────────────────────────────────────────────
    // ------------------------------
    // createRevision
    //
    // Creates a new revision for docId based on baseRev.
    // New revision starts in in_work. Records parentRev.
    // Does NOT copy file content — content is staged separately.
    //
    // Parameters:
    //   documentId : owning document
    //   baseRev    : revision to branch from (0 = initial creation)
    //   createdBy  : person ID
    //   note       : change note
    // Returns: saved DocumentRevision, or nullptr on error
    // ------------------------------
    static std::shared_ptr<DocumentRevision> createRevision(
        const std::string& documentId,
        uint32_t           baseRev,
        const std::string& createdBy = "",
        const std::string& note      = "");

    static std::shared_ptr<DocumentRevision> loadByRev(
        const std::string& documentId, uint32_t rev);

    static std::vector<std::shared_ptr<DocumentRevision>> loadAllRevisions(
        const std::string& documentId);

    // ------------------------------
    // currentRevision
    // Returns the revision with superseded = false for this docId.
    // ------------------------------
    static std::shared_ptr<DocumentRevision> currentRevision(
        const std::string& documentId);

    // ── State machine ─────────────────────────────────────────
    // ------------------------------
    // transitionState
    //
    // Attempts to move this revision to targetState.
    // Validates the transition is allowed.
    // Updates superseded flag atomically (in same SQLite transaction).
    //
    // Returns false if transition is not allowed or DB write fails.
    // ------------------------------
    bool transitionState(const std::string& targetState);

    // ------------------------------
    // isTransitionAllowed
    // Returns true if moving from revState → target is valid.
    // ------------------------------
    static bool isTransitionAllowed(const std::string& from,
                                    const std::string& to);

    // ------------------------------
    // latestRevNumber
    // Returns the highest rev number for a documentId, or 0 if none.
    // ------------------------------
    static uint32_t latestRevNumber(const std::string& documentId);

private:
    void fromRow(const Row& r);
    static Database* db();

    // Atomically update superseded flags for the whole docId.
    // Called inside the same transaction as state changes.
    bool recomputeSuperseded();
};

} // namespace Rosenholz

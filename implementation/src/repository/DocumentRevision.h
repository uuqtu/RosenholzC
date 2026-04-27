#pragma once
// ============================================================
// DocumentRevision.h  —  Document revision model
//
// Each Document has N revisions identified by (documentId, rev).
// One revision per documentId holds superseded = false — that is
// the "current active revision".
//
// Standard state transitions (see isTransitionAllowed() for enforcement):
//   in_work      → locked, pre_released, released, closed
//   locked       → released, pre_released, in_work (unlock), closed
//   pre_released → released, closed
//   released     → closed only
//   closed       → terminal, no transitions
//
// Superseded priority (superseded=false = active revision):
//   1. Latest RELEASED revision
//   2. Latest LOCKED revision (no released exists)
//   3. Latest PRE_RELEASED revision
//   4. Latest IN_WORK revision
//   Fallback: all CLOSED → latest closed is active
// ============================================================

#include <string>
#include <ostream>
#include <vector>
#include <cstdint>
#include <memory>
#include "../core/Database.h"

namespace Rosenholz {

// RevState — five states of a DocumentRevision
// Stored as string in SQLite; convert with revStateToString / revStateFromString.
enum class RevState {
    IN_WORK,        // Mutable — objects in MFS, checkout allowed
    LOCKED,         // Frozen — unlockable via workflow
    PRE_RELEASED,   // Read-only — under review
    RELEASED,       // Immutable — released and valid
    CLOSED,         // Terminal — invalid, no further transitions
};

inline const char* revStateToString(RevState s) {
    switch (s) {
    case RevState::IN_WORK:      return "in_work";
    case RevState::LOCKED:       return "locked";
    case RevState::PRE_RELEASED: return "pre_released";
    case RevState::RELEASED:     return "released";
    case RevState::CLOSED:       return "closed";
    }
    return "in_work";  // unreachable, silence compiler
}

inline RevState revStateFromString(const std::string& s) {
    if (s == "locked")       return RevState::LOCKED;
    if (s == "pre_released") return RevState::PRE_RELEASED;
    if (s == "released")     return RevState::RELEASED;
    if (s == "closed")       return RevState::CLOSED;
    return RevState::IN_WORK; // default and for "in_work"
}

// Stream operator — write RevState directly to ostream without calling revStateStr()
// Enables: std::cout << rev->revState instead of << rev->revState
inline std::ostream& operator<<(std::ostream& os, RevState s) {
    return os << revStateToString(s);
}


// Keep DocRevState aliases for backward compatibility during migration
// These will be removed once all callers use RevState directly.
namespace DocRevState {
    constexpr const char* IN_WORK      = "in_work";
    constexpr const char* PRE_RELEASED = "pre_released";
    constexpr const char* RELEASED     = "released";
    constexpr const char* LOCKED       = "locked";
    constexpr const char* CLOSED       = "closed";
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
    RevState    revState { RevState::IN_WORK };
    // Convenience: string representation for SQL persistence
    std::string revStateStr() const { return revStateToString(revState); }
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
    // Primary: type-safe transition. Returns false if not allowed or DB fails.
    /// Transition to target state.
    /// If f77Gated=false (default), will refuse transition to released/closed
    /// if an active F77 workflow exists for this document — the F77 engine must do it.
    bool transitionState(RevState target, bool f77Gated = false);
    // String overload: converts via revStateFromString, then delegates.
    // Kept for SQL-origin callers (workflow engine, tests).
    bool transitionState(const std::string& target) {
        return transitionState(revStateFromString(target));
    }

    // ------------------------------
    // isTransitionAllowed
    // Returns true if moving from revState → target is valid.
    // ------------------------------
    static bool isTransitionAllowed(RevState from,
                                    RevState to,
                                    bool adminMode = false);

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

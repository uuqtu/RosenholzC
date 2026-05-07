#pragma once
// ============================================================
// Note.h  —  F99 Notizen (ehemals "Notes")
//
// F99-Notizen sind entitaetsagnostisch: jede Entitaet (F16,
// F22, F18, AKT, F77, PER) kann beliebig viele F99-Notizen haben.
// Gespeichert in f99.db; im MFS als _F99_Notizen.txt.
//
// RegNumber: XV/F99/000001/26  (6-stellige Sequenz)
// ============================================================
#include "../model/Utils.h"
#include <string>
#include <vector>
#include <memory>

namespace Rosenholz {

struct Note {
    std::string noteId;       ///< XV/F99/000001/26
    std::string entityType;   ///< f16|f22|f18|akt|f77|per
    std::string entityId;     ///< full entity ID
    std::string createdAt;    ///< ISO-8601
    std::string author;       ///< person_id or name (optional)
    std::string body;         ///< the note text

    OperationResult save()   const;
    OperationResult update() const;   ///< update body in-place
    OperationResult remove() const;
    void fromRow(const Row& r);

    // ── Factory ───────────────────────────────────────────────
    static std::shared_ptr<Note> create(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& body,
        const std::string& author = "");

    // ── Queries ───────────────────────────────────────────────
    static std::vector<std::shared_ptr<Note>> loadForEntity(
        const std::string& entityType,
        const std::string& entityId);

    static std::shared_ptr<Note> loadById(const std::string& noteId);

    /// Full-text search across all F99 bodies.
    /// If entityType given, restrict to that type.
    /// Returns (note, entityTitle) pairs.
    struct SearchResult {
        std::shared_ptr<Note> note;
        std::string entityTitle;  ///< resolved title of owning entity
        std::string entityPath;   ///< compact path, e.g. "F16:0001/26 > F22:0001/26"
    };
    static std::vector<SearchResult> search(
        const std::string& query,
        const std::string& entityType = "");

    /// Load N most recent notes globally (no entity filter).
    static std::vector<SearchResult> loadRecent(int limit = 30);

    // ── MFS export ────────────────────────────────────────────
    static void writeNotesFile(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& mfsDir);

private:
    static Database* db();
};

} // namespace Rosenholz

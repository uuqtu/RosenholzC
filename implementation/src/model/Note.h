#pragma once
// ============================================================
// Note.h  —  Structured timestamped note for any entity
//
// Notes are entity-agnostic: any entity (F16, F22, F18, AKT,
// F77, PER) can have any number of notes.
// Stored in a shared notes DB; written to _Notizen.txt in MFS.
// ============================================================
#include "../model/Utils.h"
#include <string>
#include <vector>
#include <memory>

namespace Rosenholz {

struct Note {
    std::string noteId;       ///< XV/NOTE/0001/26
    std::string entityType;   ///< f16|f22|f18|akt|f77|per
    std::string entityId;     ///< full entity ID
    std::string createdAt;    ///< ISO-8601
    std::string author;       ///< person_id or name (optional)
    std::string body;         ///< the note text

    OperationResult save()   const;
    OperationResult remove() const;
    void fromRow(const Row& r);

    // ── Factory ───────────────────────────────────────────────
    /// Create and save a new note. Returns nullptr on failure.
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

    // ── MFS export ───────────────────────────────────────────
    /// Write all notes for an entity to a _Notizen.txt file.
    /// Full-text search across note bodies.
    static std::vector<std::shared_ptr<Note>> search(
        const std::string& query,
        const std::string& entityType = "");

    static void writeNotesFile(
        const std::string& entityType,
        const std::string& entityId,
        const std::string& mfsDir);

private:
    static Database* db();
};

} // namespace Rosenholz

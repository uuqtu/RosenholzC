#pragma once
// ============================================================
// MFSWriter.h  —  Centralised MFS-style plaintext file output
//
// Writes one plaintext file per entity into the MFS root folder
// structure, in the style the East German MfS (Stasi) used for
// their physical filing system.
//
// Security model:
//   - Each entity file contains ONLY its own reg number and
//     opaque cross-references (no human-readable names).
//   - The OWNER KEY file (owner_key.txt, owner-only readable)
//     maps reg numbers to real names and full relationships.
//   - Without the owner key, an observer can enumerate files
//     but cannot connect them to real entities.
//
// All files written with owner-only (chmod 600) permissions
// on Linux. On Windows, DACL-based restriction is applied.
// ============================================================

#include <string>
#include <vector>
#include <map>

namespace Rosenholz {
class F18Operation;  // forward declaration


// Forward declarations — avoids pulling in all model headers
class ProjectF16;
class TaskF22;
class Document;
class Person;
class Team;

class MFSWriter {
public:
    MFSWriter() = delete;

    // ── Entity writers ─────────────────────────────────────
    // ------------------------------
    // Write a plaintext index card for a project into MFS.
    //
    // Creates: mfs/F16/{DE}/{year}/{sane-ID}/{sane-ID}_{title}.txt
    // Content: all project metadata, ONLY reg-numbers for cross-refs
    //          (human names are kept only in owner_key.txt)
    // ------------------------------
    static bool writeProject (const ProjectF16&   p,   const std::string& mfsRoot);
    static bool writeTask    (const TaskF22&       t,   const std::string& mfsRoot);
    // ------------------------------
    // Write a plaintext index card for a document into MFS.
    //
    // Requirements:
    //   - Document must have projectId or taskId set;
    //     orphan documents are refused (returns false)
    //
    // Creates: mfs/DOK/{parent-sane}/{DOK-ID}_{title}.txt
    // Also copies the physical file (d.filePath) into the same
    // directory if the file exists on disk.
    // ------------------------------
    static bool writeDocument(const Document&       d,   const std::string& mfsRoot);
    static bool writeF18     (const F18Operation&    v,   const std::string& mfsRoot);
    static bool writeF77     (const std::string& wfiId, const std::string& entityType,
                              const std::string& entityTitle, const std::string& mfsRoot);
    static bool writePerson  (const Person&         p,   const std::string& mfsRoot);
    static bool writeTeam    (const Team&           t,   const std::string& mfsRoot);

    // ── Owner key file ─────────────────────────────────────
    /// Append an entry to the owner key file.
    /// owner_key.txt maps reg_number -> real entity name + full FK list.
    /// This file is owner-readable only and is the ONLY place
    /// where real names appear in the MFS tree.
    // ------------------------------
    // Append one entry to the owner key file.
    //
    // The owner key is the ONLY file that maps reg-numbers to
    // real entity names and relationships.  It is written with
    // owner-only permissions (chmod 600).
    //
    // Parameters:
    //   regNumber   : the entity's registration number
    //   realTitle   : the human-readable title / name
    //   connections : map of label -> linked reg-number
    //   mfsRoot     : base path of the MFS tree
    // ------------------------------
    static bool appendOwnerKey(
        const std::string& regNumber,
        const std::string& realTitle,
        const std::map<std::string, std::string>& connections,
        const std::string& mfsRoot);

    // ── Batch rebuild ──────────────────────────────────────
    /// Re-generate the entire MFS tree from the database.
    // ------------------------------
    // Regenerate the entire MFS tree from the database.
    //
    // Iterates all projects, tasks, incidents, risks, documents,
    // persons, and teams and writes their index cards.
    //
    // Called from:
    //   - main_cli.cpp option 12 ("MFS-Baum aufbauen")
    //   - Useful after bulk imports or migrations
    // ------------------------------
    static bool rebuildAll(const std::string& mfsRoot);

    // Schlüssel-file maintenance
    // Called after each write to keep the index files current.
    static void rebuildTypeSchluessel(const std::string& mfsRoot,
                                       const std::string& typeCode);

private:
    static bool ownerOnlyWrite(const std::string& path, const std::string& content);
};

} // namespace Rosenholz

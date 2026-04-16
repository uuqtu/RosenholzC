#pragma once
// ============================================================
// ArchiveStore.h  —  LMDB-backed binary content store
//
// Stores document file content ONLY. All metadata stays in SQLite.
//
// Design rules:
//   • LMDB used exclusively for file bytes (chunks keyed by SHA-256)
//   • One writer at a time (internal mutex)
//   • Free-space guard: if env < 1 GB free, mdb_env_set_mapsize doubles capacity
//   • Content committed to LMDB only after a successful state operation
//     (files land in a temp dir first via stageContent, then commitContent)
//   • Single LMDB environment; two named databases:
//       "chunks"   : SHA-256(hex) → raw bytes
//       "revfiles" : docId:rev    → SHA-256(hex) of the file chunk
// ============================================================

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <memory>
#include <lmdb.h>

namespace Rosenholz {
namespace Archive {

// ------------------------------
// ChunkRef
// Lightweight handle returned by stageContent / lookupContent.
// ------------------------------
struct ChunkRef {
    std::string sha256;   // hex SHA-256 — stable content key
    uint64_t    size{0};  // byte count
    bool        valid() const { return !sha256.empty(); }
};

// ============================================================
// ArchiveStore
//
// Thread-safe singleton wrapping an LMDB environment.
// Use init() once at startup, then stageContent / commitContent
// / retrieveContent for all file I/O.
// ============================================================
class ArchiveStore {
public:
    // ------------------------------
    // instance — singleton accessor
    // ------------------------------
    static ArchiveStore& instance();

    // Non-copyable
    ArchiveStore(const ArchiveStore&)            = delete;
    ArchiveStore& operator=(const ArchiveStore&) = delete;

    // ------------------------------
    // init
    //
    // Opens or creates the LMDB environment at basePath/archive.lmdb.
    // Sets initial map size to initialMapBytes (default 8 GB).
    // Must be called once before any other method.
    //
    // Parameters:
    //   basePath       : application data root
    //   initialMapBytes: starting LMDB map size (grows automatically)
    // ------------------------------
    bool init(const std::string& basePath,
              size_t initialMapBytes = 256ULL * 1024 * 1024);  // 256 MB; grows auto

    // ------------------------------
    // stageContent
    //
    // Copies srcPath into a temp staging area and computes its SHA-256.
    // The file is NOT yet written to LMDB.
    // Returns a ChunkRef with the hash and size.
    // Call commitContent() to persist after a successful state transition.
    //
    // Parameters:
    //   srcPath : path to the source file (local upload / download)
    //   outTmp  : set to the temp staging path (caller can use for state op)
    // ------------------------------
    ChunkRef stageContent(const std::string& srcPath,
                          std::string&       outTmp);

    // ------------------------------
    // commitContent
    //
    // Writes a staged file (at tmpPath) into LMDB under its SHA-256 key.
    // Stores the docId:rev → sha256 mapping.
    // No-op if the chunk already exists (content-addressed dedup).
    //
    // Parameters:
    //   tmpPath : path returned by stageContent
    //   ref     : ChunkRef from stageContent
    //   docId   : document ID (XV/DOK/…)
    //   rev     : revision number
    // ------------------------------
    bool commitContent(const std::string& tmpPath,
                       const ChunkRef&    ref,
                       const std::string& docId,
                       uint32_t           rev);

    // ------------------------------
    // retrieveContent
    //
    // Reads the file bytes for docId:rev out of LMDB into destPath.
    // Returns false if no content found.
    //
    // Parameters:
    //   docId   : document ID
    //   rev     : revision number
    //   destPath: where to write the bytes (created / overwritten)
    // ------------------------------
    bool retrieveContent(const std::string& docId,
                         uint32_t           rev,
                         const std::string& destPath) const;

    // ------------------------------
    // chunkExists
    // Returns true if a chunk with this SHA-256 is already in LMDB.
    // Used to skip re-uploading identical content.
    // ------------------------------
    bool chunkExists(const std::string& sha256) const;

    // ------------------------------
    // lookupRevChunk
    // Returns the SHA-256 stored for docId:rev, or "" if not found.
    // ------------------------------
    std::string lookupRevChunk(const std::string& docId, uint32_t rev) const;

    // ------------------------------
    // deleteRevMapping
    // Removes the docId:rev → sha256 mapping (not the chunk itself —
    // chunks are ref-counted and GC'd separately).
    // Called when a revision is hard-deleted.
    // ------------------------------
    bool deleteRevMapping(const std::string& docId, uint32_t rev);

    // ------------------------------
    // ensureFreeSpace
    //
    // If the LMDB map has less than 1 GB of unused space,
    // doubles the map size (mdb_env_set_mapsize).
    // Called automatically before every write transaction.
    // Returns false only if resizing itself fails.
    // ------------------------------
    bool ensureFreeSpace();

    // ------------------------------
    // stagingDir — path of the temp staging area
    // ------------------------------
    const std::string& stagingDir() const { return stagingDir_; }

    bool isOpen() const { return env_ != nullptr; }

private:
    ArchiveStore() = default;
    ~ArchiveStore();

    // Internal write helper — acquires mutex, checks free space, runs txn
    bool writeChunk(const std::string& sha256,
                    const uint8_t*     data,
                    size_t             len);
    bool writeRevMapping(const std::string& docId,
                         uint32_t           rev,
                         const std::string& sha256);

    static std::string revKey(const std::string& docId, uint32_t rev);

    mutable std::mutex  mtx_;
    MDB_env*            env_       { nullptr };
    MDB_dbi             dbiChunks_ { 0 };
    MDB_dbi             dbiRevs_   { 0 };
    std::string         stagingDir_;
    size_t              mapSize_   { 0 };

    static constexpr size_t MIN_FREE_BYTES = 1ULL * 1024 * 1024 * 1024; // 1 GB
};

} // namespace Archive
} // namespace Rosenholz

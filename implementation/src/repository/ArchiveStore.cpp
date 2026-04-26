// ============================================================
// ArchiveStore.cpp  —  LMDB file-content store implementation
// ============================================================
#include "ArchiveStore.h"
#include <openssl/evp.h>
#include <iomanip>
#include "../core/Logger.h"
#include "../core/FileOps.h"

#include <lmdb.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <stdexcept>


// Portable SHA-256 via OpenSSL EVP (replaces popen("sha256sum") on Linux).
// Requires: OpenSSL — available on both Linux and Windows.
static std::string computeSHA256(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx); return "";
    }

    char buf[65536];
    while (in.read(buf, sizeof(buf)) || in.gcount() > 0)
        EVP_DigestUpdate(ctx, buf, static_cast<size_t>(in.gcount()));

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int  hashLen = 0;
    EVP_DigestFinal_ex(ctx, hash, &hashLen);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hashLen; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}
namespace Rosenholz {
namespace Archive {

// ── Helpers ──────────────────────────────────────────────────


static void lmdbCheck(int rc, const char* op) {
    if (rc != MDB_SUCCESS)
        LOG_ERROR(std::string("[LMDB] ") + op + ": " + mdb_strerror(rc));
}

// ── Singleton ────────────────────────────────────────────────
ArchiveStore& ArchiveStore::instance() {
    static ArchiveStore s;
    return s;
}

ArchiveStore::~ArchiveStore() {
    if (env_) {
        mdb_env_close(env_);
        env_ = nullptr;
    }
}

// ── init ──────────────────────────────────────────────────────
bool ArchiveStore::init(const std::string& basePath, size_t initialMapBytes) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (env_) return true;  // already open

    std::string dbPath = FileOps::joinPath(basePath, "archive.lmdb");
    FileOps::makeDirs(dbPath);

    stagingDir_ = FileOps::joinPath(basePath, "archive_staging");
    FileOps::makeDirs(stagingDir_);

    int rc = mdb_env_create(&env_);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "env_create"); return false; }

    mdb_env_set_maxdbs(env_, 2);
    mdb_env_set_mapsize(env_, initialMapBytes);
    mapSize_ = initialMapBytes;

    // Standard LMDB mode: dbPath is a DIRECTORY containing data.mdb + lock.mdb
    rc = mdb_env_open(env_, dbPath.c_str(),
                      MDB_NOSYNC,   // NOSYNC: OS page cache is sufficient
                      0755);
    if (rc != MDB_SUCCESS) {
        lmdbCheck(rc, "env_open");
        mdb_env_close(env_); env_ = nullptr;
        return false;
    }

    // Open named databases inside a write transaction
    MDB_txn* txn = nullptr;
    rc = mdb_txn_begin(env_, nullptr, 0, &txn);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "txn_begin(init)"); return false; }

    rc = mdb_dbi_open(txn, "chunks", MDB_CREATE, &dbiChunks_);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "dbi_open(chunks)"); mdb_txn_abort(txn); return false; }

    rc = mdb_dbi_open(txn, "revfiles", MDB_CREATE, &dbiRevs_);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "dbi_open(revfiles)"); mdb_txn_abort(txn); return false; }

    rc = mdb_txn_commit(txn);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "txn_commit(init)"); return false; }

    LOG_INFO("[ArchiveStore] Opened at " + dbPath +
             " mapSize=" + std::to_string(initialMapBytes / (1024*1024)) + " MB");
    return true;
}

// ── ensureFreeSpace ───────────────────────────────────────────
bool ArchiveStore::ensureFreeSpace() {
    // Called from inside locked sections — don't re-lock
    MDB_envinfo info;
    int rc = mdb_env_info(env_, &info);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "env_info"); return false; }

    MDB_stat stat;
    rc = mdb_env_stat(env_, &stat);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "env_stat"); return false; }

    size_t usedPages  = info.me_last_pgno + 1;
    size_t pageSize   = stat.ms_psize;
    size_t usedBytes  = usedPages * pageSize;
    size_t totalBytes = mapSize_;
    size_t freeBytes  = (totalBytes > usedBytes) ? (totalBytes - usedBytes) : 0;

    if (freeBytes < MIN_FREE_BYTES) {
        // Double the map size
        size_t newSize = mapSize_ * 2;
        rc = mdb_env_set_mapsize(env_, newSize);
        if (rc != MDB_SUCCESS) {
            lmdbCheck(rc, "set_mapsize");
            return false;
        }
        mapSize_ = newSize;
        LOG_INFO("[ArchiveStore] Map expanded to " +
                 std::to_string(newSize / (1024*1024)) + " MB (was " +
                 std::to_string((newSize/2) / (1024*1024)) + " MB, free was " +
                 std::to_string(freeBytes / (1024*1024)) + " MB)");
    }
    return true;
}

// ── stageContent ─────────────────────────────────────────────
ChunkRef ArchiveStore::stageContent(const std::string& srcPath,
                                    std::string&       outTmp) {
    ChunkRef ref;
    if (!FileOps::fileExists(srcPath)) {
        LOG_ERROR("[ArchiveStore] stageContent: source not found: " + srcPath);
        return ref;
    }

    // Compute SHA-256 first (cheap, no LMDB access)
    ref.sha256 = computeSHA256(srcPath);
    if (ref.sha256.empty()) {
        LOG_ERROR("[ArchiveStore] stageContent: SHA-256 failed for " + srcPath);
        return ref;
    }

    // Copy to staging dir (named by hash to make duplicates obvious)
    std::string stageName = ref.sha256.substr(0, 16) + "_" +
                            FileOps::baseName(srcPath);
    outTmp = FileOps::joinPath(stagingDir_, stageName);

    if (!FileOps::copyFile(srcPath, outTmp, /*overwrite=*/true)) {
        LOG_ERROR("[ArchiveStore] stageContent: copy to staging failed");
        ref.sha256.clear();
        return ref;
    }

    ref.size = (uint64_t)FileOps::fileSize(outTmp);
    LOG_INFO("[ArchiveStore] Staged: " + ref.sha256.substr(0,16) +
             "… size=" + std::to_string(ref.size));
    return ref;
}

// ── commitContent ────────────────────────────────────────────
bool ArchiveStore::commitContent(const std::string& tmpPath,
                                 const ChunkRef&    ref,
                                 const std::string& docId,
                                 uint32_t           rev) {
    if (!ref.valid() || !FileOps::fileExists(tmpPath)) {
        LOG_ERROR("[ArchiveStore] commitContent: invalid ref or missing tmpPath");
        return false;
    }

    std::lock_guard<std::mutex> lk(mtx_);
    if (!ensureFreeSpace()) return false;

    // Read file bytes
    std::ifstream f(tmpPath, std::ios::binary | std::ios::ate);
    if (!f) {
        LOG_ERROR("[ArchiveStore] commitContent: cannot open " + tmpPath);
        return false;
    }
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data((size_t)sz);
    f.read((char*)data.data(), sz);

    // Write transaction covering both named databases atomically
    MDB_txn* txn = nullptr;
    int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "commitContent txn_begin"); return false; }

    bool ok = true;

    // 1. Store chunk (SHA-256 → bytes) — skip if already exists (content dedup)
    MDB_val kChunk, vChunk;
    kChunk.mv_data = (void*)ref.sha256.data();
    kChunk.mv_size = ref.sha256.size();
    MDB_val existing; existing.mv_data = nullptr; existing.mv_size = 0;
    rc = mdb_get(txn, dbiChunks_, &kChunk, &existing);
    if (rc == MDB_NOTFOUND) {
        vChunk.mv_data = data.data();
        vChunk.mv_size = data.size();
        rc = mdb_put(txn, dbiChunks_, &kChunk, &vChunk, 0);
        if (rc != MDB_SUCCESS) { lmdbCheck(rc, "put(chunk)"); ok = false; }
    }

    // 2. Store rev mapping (docId:rev → SHA-256)
    if (ok) {
        std::string rk = revKey(docId, rev);
        MDB_val kRev, vRev;
        kRev.mv_data = (void*)rk.data();
        kRev.mv_size = rk.size();
        vRev.mv_data = (void*)ref.sha256.data();
        vRev.mv_size = ref.sha256.size();
        rc = mdb_put(txn, dbiRevs_, &kRev, &vRev, 0);
        if (rc != MDB_SUCCESS) { lmdbCheck(rc, "put(revMapping)"); ok = false; }
    }

    if (ok) {
        rc = mdb_txn_commit(txn);
        ok = (rc == MDB_SUCCESS);
        if (!ok) lmdbCheck(rc, "commitContent txn_commit");
    } else {
        mdb_txn_abort(txn);
    }

    // Clean up staging file regardless
    if (FileOps::fileExists(tmpPath))
        std::remove(tmpPath.c_str());

    if (ok) LOG_INFO("[ArchiveStore] Committed: " + docId + " rev=" +
                     std::to_string(rev) + " sha=" + ref.sha256.substr(0,16));
    return ok;
}

// ── retrieveContent ───────────────────────────────────────────
bool ArchiveStore::retrieveContent(const std::string& docId,
                                   uint32_t           rev,
                                   const std::string& destPath) const {
    // Read-only transaction — no mutex needed (LMDB MVCC allows concurrent reads)
    MDB_txn* txn = nullptr;
    int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "retrieveContent txn_begin"); return false; }

    // Look up SHA-256 for this docId:rev
    std::string rk = revKey(docId, rev);
    MDB_val kRev, vRev;
    kRev.mv_data = (void*)rk.data();
    kRev.mv_size = rk.size();
    rc = mdb_get(txn, dbiRevs_, &kRev, &vRev);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        if (rc != MDB_NOTFOUND) lmdbCheck(rc, "retrieveContent get(rev)");
        return false;
    }
    std::string sha256((char*)vRev.mv_data, vRev.mv_size);

    // Look up chunk
    MDB_val kChunk, vChunk;
    kChunk.mv_data = (void*)sha256.data();
    kChunk.mv_size = sha256.size();
    rc = mdb_get(txn, dbiChunks_, &kChunk, &vChunk);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        if (rc != MDB_NOTFOUND) lmdbCheck(rc, "retrieveContent get(chunk)");
        return false;
    }

    // Write to destPath
    FileOps::makeDirs(FileOps::dirName(destPath));
    std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
    bool ok = out.good();
    if (ok) out.write((const char*)vChunk.mv_data, (std::streamsize)vChunk.mv_size);
    ok = ok && out.good();

    mdb_txn_abort(txn);  // read txns always aborted
    return ok;
}

// ── chunkExists ───────────────────────────────────────────────
bool ArchiveStore::chunkExists(const std::string& sha256) const {
    MDB_txn* txn = nullptr;
    if (mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn) != MDB_SUCCESS) return false;
    MDB_val k, v;
    k.mv_data = (void*)sha256.data(); k.mv_size = sha256.size();
    int rc = mdb_get(txn, dbiChunks_, &k, &v);
    mdb_txn_abort(txn);
    return rc == MDB_SUCCESS;
}

// ── lookupRevChunk ────────────────────────────────────────────
std::string ArchiveStore::lookupRevChunk(const std::string& docId,
                                          uint32_t           rev) const {
    MDB_txn* txn = nullptr;
    if (mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn) != MDB_SUCCESS) return "";
    std::string rk = revKey(docId, rev);
    MDB_val k, v;
    k.mv_data = (void*)rk.data(); k.mv_size = rk.size();
    int rc = mdb_get(txn, dbiRevs_, &k, &v);
    std::string result;
    if (rc == MDB_SUCCESS) result.assign((char*)v.mv_data, v.mv_size);
    mdb_txn_abort(txn);
    return result;
}

// ── deleteRevMapping ──────────────────────────────────────────
bool ArchiveStore::deleteRevMapping(const std::string& docId, uint32_t rev) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!ensureFreeSpace()) return false;

    MDB_txn* txn = nullptr;
    int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
    if (rc != MDB_SUCCESS) { lmdbCheck(rc, "deleteRevMapping txn_begin"); return false; }

    std::string rk = revKey(docId, rev);
    MDB_val k;
    k.mv_data = (void*)rk.data(); k.mv_size = rk.size();
    rc = mdb_del(txn, dbiRevs_, &k, nullptr);
    bool ok = (rc == MDB_SUCCESS || rc == MDB_NOTFOUND);
    if (ok) mdb_txn_commit(txn); else mdb_txn_abort(txn);
    return ok;
}

// ── revKey ────────────────────────────────────────────────────
std::string ArchiveStore::revKey(const std::string& docId, uint32_t rev) {
    return docId + ":" + std::to_string(rev);
}

} // namespace Archive
} // namespace Rosenholz

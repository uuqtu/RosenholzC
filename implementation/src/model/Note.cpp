#include <iomanip>
// ============================================================
// Note.cpp  —  F99 Notizen (entity-agnostic notes)
//
// DB:    f99.db  (table: f99_entries)
// RegNr: XV/F99/000001/26  (6-digit sequence)
// ============================================================
#include "Note.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/FileOps.h"
#include "../core/Config.h"
#include "../core/RegNumber.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/f18/F18Operation.h"
#include "../model/akt/Folder.h"
#include <sstream>
#include <fstream>

namespace Rosenholz {

Database* Note::db() {
    return DatabasePool::instance().get("f99");
}

void Note::fromRow(const Row& r) {
    auto g = [&](const std::string& k) {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    noteId     = g("f99_id");
    entityType = g("entity_type");
    entityId   = g("entity_id");
    createdAt  = g("created_at");
    author     = g("author");
    body       = g("body");
}

OperationResult Note::save() const {
    auto* d = db(); if (!d) return OperationResult::DB_ERROR;
    return d->exec(
        "INSERT OR REPLACE INTO f99_entries"
        " (f99_id,entity_type,entity_id,created_at,author,body)"
        " VALUES(?,?,?,?,?,?)",
        { BindParam::text(noteId),
          BindParam::text(entityType),
          BindParam::text(entityId),
          BindParam::text(createdAt),
          BindParam::nullOrText(author),
          BindParam::text(body) })
        ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult Note::update() const {
    auto* d = db(); if (!d) return OperationResult::DB_ERROR;
    return d->exec(
        "UPDATE f99_entries SET body=? WHERE f99_id=?;",
        { BindParam::text(body), BindParam::text(noteId) })
        ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult Note::remove() const {
    auto* d = db(); if (!d) return OperationResult::DB_ERROR;
    return d->exec("DELETE FROM f99_entries WHERE f99_id=?;",
                   {BindParam::text(noteId)})
        ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

std::shared_ptr<Note> Note::create(
    const std::string& entityType,
    const std::string& entityId,
    const std::string& body,
    const std::string& author)
{
    if (body.empty()) return nullptr;
    auto n = std::make_shared<Note>();
    // 6-digit sequence: genId stores dept "F99", toString() uses setw(4).
    // We override the formatting by using a dedicated 6-digit generator:
    auto rn = RegNumberGenerator::next("F99");
    // Build 6-digit string manually:
    const std::string& de = Config::instance().registratur().diensteinheitKuerzel;
    std::ostringstream oss;
    oss << de << "/F99/"
        << std::setw(6) << std::setfill('0') << rn.sequence
        << "/" << (rn.year % 100);
    n->noteId     = oss.str();
    n->entityType = entityType;
    n->entityId   = entityId;
    n->createdAt  = nowIso();
    n->author     = author;
    n->body       = body;
    if (!opOk(n->save())) {
        LOG_ERROR("[F99] create failed for " + entityType + "/" + entityId);
        return nullptr;
    }
    LOG_INFO("[F99] Created: " + n->noteId + " for " + entityType + "/" + entityId);
    return n;
}

std::vector<std::shared_ptr<Note>> Note::loadForEntity(
    const std::string& entityType, const std::string& entityId)
{
    auto* d = db();
    std::vector<std::shared_ptr<Note>> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM f99_entries"
        " WHERE entity_type=? AND entity_id=?"
        " ORDER BY created_at ASC;",
        { BindParam::text(entityType), BindParam::text(entityId) });
    for (auto& r : rows) {
        auto n = std::make_shared<Note>(); n->fromRow(r); result.push_back(n);
    }
    return result;
}

std::shared_ptr<Note> Note::loadById(const std::string& noteId) {
    auto* d = db(); if (!d) return nullptr;
    auto rows = d->query(
        "SELECT * FROM f99_entries WHERE f99_id=? LIMIT 1;",
        { BindParam::text(noteId) });
    if (rows.empty()) return nullptr;
    auto n = std::make_shared<Note>(); n->fromRow(rows[0]); return n;
}

// ── resolveEntityTitle: load title for any entity type ──────────────────────
static std::string resolveTitle(const std::string& etype, const std::string& eid) {
    if (etype == "f16") {
        auto p = F16::loadById(eid);
        return p ? p->title : eid;
    }
    if (etype == "f22") {
        auto t = F22::loadById(eid);
        return t ? t->title : eid;
    }
    if (etype == "f18") {
        auto v = F18Operation::loadById(eid);
        return v ? v->title : eid;
    }
    if (etype == "akt") {
        auto d = Folder::loadById(eid);
        return d ? d->title : eid;
    }
    return eid;
}

// ── resolveEntityPath: compact breadcrumb path ───────────────────────────────
static std::string resolvePath(const std::string& etype, const std::string& eid) {
    if (etype == "f22") {
        auto t = F22::loadById(eid);
        if (!t) return "F22:" + eid;
        auto p = F16::loadById(t->projectId);
        std::string path = p ? ("F16:" + p->regNumber.toString()) : "";
        return (path.empty() ? "" : path + " > ") + "F22:" + t->regNumber.toString();
    }
    if (etype == "f18") {
        auto v = F18Operation::loadById(eid);
        if (!v) return "F18:" + eid;
        auto t = F22::loadById(v->taskId);
        if (!t) return "F18:" + eid;
        auto p = F16::loadById(t->projectId);
        std::string path = p ? ("F16:" + p->regNumber.toString() + " > ") : "";
        path += "F22:" + t->regNumber.toString() + " > F18:" + v->operationId;
        return path;
    }
    if (etype == "akt") {
        auto d = Folder::loadById(eid);
        if (!d) return "AKT:" + eid;
        return "AKT:" + d->folderId;
    }
    if (etype == "f16") {
        auto p = F16::loadById(eid);
        return p ? ("F16:" + p->regNumber.toString()) : "F16:" + eid;
    }
    return etype + ":" + eid;
}

std::vector<Note::SearchResult> Note::search(
    const std::string& query, const std::string& entityType)
{
    auto* d = db();
    std::vector<SearchResult> result;
    if (!d || query.empty()) return result;

    std::string sql = "SELECT * FROM f99_entries WHERE body LIKE ?";
    std::vector<BindParam> params;
    params.push_back(BindParam::text(patternToSQLLike(query)));
    if (!entityType.empty()) {
        sql += " AND entity_type=?";
        params.push_back(BindParam::text(entityType));
    }
    sql += " ORDER BY created_at DESC LIMIT 100;";

    for (auto& r : d->query(sql, params)) {
        auto n = std::make_shared<Note>(); n->fromRow(r);
        SearchResult sr;
        sr.note        = n;
        sr.entityTitle = resolveTitle(n->entityType, n->entityId);
        sr.entityPath  = resolvePath(n->entityType, n->entityId);
        result.push_back(sr);
    }
    return result;
}

void Note::writeNotesFile(
    const std::string& entityType,
    const std::string& entityId,
    const std::string& mfsDir)
{
    auto notes = loadForEntity(entityType, entityId);
    std::string path = FileOps::joinPath(mfsDir, "_F99_Notizen.txt");

    if (notes.empty()) {
        if (FileOps::fileExists(path)) FileOps::deleteFile(path);
        return;
    }

    std::ostringstream oss;
    oss << "F99-NOTIZEN — " << entityType << "/" << entityId << "\n";
    oss << std::string(60, '=') << "\n\n";
    for (auto& n : notes) {
        oss << "[" << n->createdAt << "]";
        if (!n->author.empty()) oss << "  " << n->author;
        oss << "  ID:" << n->noteId << "\n";
        oss << n->body << "\n";
        oss << std::string(40, '-') << "\n\n";
    }

    FileOps::makeDirs(mfsDir);
    FileOps::writeTextFile(path, oss.str());
    LOG_DEBUG("[F99] _F99_Notizen.txt written: " + path);
}

} // namespace Rosenholz

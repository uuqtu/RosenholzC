// ============================================================
// Note.cpp  —  Structured entity notes implementation
// ============================================================
#include "Note.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "../core/FileOps.h"
#include <sstream>
#include <fstream>

namespace Rosenholz {

Database* Note::db() {
    return DatabasePool::instance().get("notes");
}

void Note::fromRow(const Row& r) {
    auto g = [&](const std::string& k) {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    noteId     = g("note_id");
    entityType = g("entity_type");
    entityId   = g("entity_id");
    createdAt  = g("created_at");
    author     = g("author");
    body       = g("body");
}

OperationResult Note::save() const {
    auto* d = db(); if (!d) return OperationResult::DB_ERROR;
    return d->exec(
        "INSERT OR REPLACE INTO note_entries"
        " (note_id,entity_type,entity_id,created_at,author,body)"
        " VALUES(?,?,?,?,?,?)",
        { BindParam::text(noteId),
          BindParam::text(entityType),
          BindParam::text(entityId),
          BindParam::text(createdAt),
          BindParam::nullOrText(author),
          BindParam::text(body) })
        ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult Note::remove() const {
    auto* d = db(); if (!d) return OperationResult::DB_ERROR;
    return d->exec("DELETE FROM note_entries WHERE note_id=?;",
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
    n->noteId     = genId("NOTE");
    n->entityType = entityType;
    n->entityId   = entityId;
    n->createdAt  = nowIso();
    n->author     = author;
    n->body       = body;
    if (!opOk(n->save())) {
        LOG_ERROR("[Note] create failed for " + entityType + "/" + entityId);
        return nullptr;
    }
    LOG_INFO("[Note] Created: " + n->noteId + " for " + entityType + "/" + entityId);
    return n;
}

std::vector<std::shared_ptr<Note>> Note::loadForEntity(
    const std::string& entityType, const std::string& entityId)
{
    auto* d = db();
    std::vector<std::shared_ptr<Note>> result;
    if (!d) return result;
    auto rows = d->query(
        "SELECT * FROM note_entries"
        " WHERE entity_type=? AND entity_id=?"
        " ORDER BY created_at ASC;",
        { BindParam::text(entityType), BindParam::text(entityId) });
    for (auto& r : rows) {
        auto n = std::make_shared<Note>();
        n->fromRow(r);
        result.push_back(n);
    }
    return result;
}

std::shared_ptr<Note> Note::loadById(const std::string& noteId) {
    auto* d = db(); if (!d) return nullptr;
    auto rows = d->query(
        "SELECT * FROM note_entries WHERE note_id=? LIMIT 1;",
        { BindParam::text(noteId) });
    if (rows.empty()) return nullptr;
    auto n = std::make_shared<Note>();
    n->fromRow(rows[0]);
    return n;
}

void Note::writeNotesFile(
    const std::string& entityType,
    const std::string& entityId,
    const std::string& mfsDir)
{
    auto notes = loadForEntity(entityType, entityId);
    std::string path = FileOps::joinPath(mfsDir, "_Notizen.txt");

    if (notes.empty()) {
        // Remove old file if no notes exist:
        if (FileOps::fileExists(path)) FileOps::deleteFile(path);
        return;
    }

    std::ostringstream oss;
    oss << "NOTIZEN — " << entityType << "/" << entityId << "\n";
    oss << std::string(60, '=') << "\n\n";
    for (auto& n : notes) {
        oss << "[" << n->createdAt << "]";
        if (!n->author.empty()) oss << "  " << n->author;
        oss << "\n";
        oss << n->body << "\n";
        oss << std::string(40, '-') << "\n\n";
    }

    FileOps::makeDirs(mfsDir);
    FileOps::writeTextFile(path, oss.str());
    LOG_DEBUG("[Note] _Notizen.txt written: " + path);
}

std::vector<std::shared_ptr<Note>> Note::search(
    const std::string& query, const std::string& entityType)
{
    auto* d = db();
    std::vector<std::shared_ptr<Note>> result;
    if (!d || query.empty()) return result;

    std::string sql = "SELECT * FROM note_entries WHERE body LIKE ?";
    std::vector<BindParam> params;
    params.push_back(BindParam::text("%" + query + "%"));
    if (!entityType.empty()) {
        sql += " AND entity_type=?";
        params.push_back(BindParam::text(entityType));
    }
    sql += " ORDER BY created_at DESC;";

    for (auto& r : d->query(sql, params)) {
        auto n = std::make_shared<Note>();
        n->fromRow(r);
        result.push_back(n);
    }
    return result;
}

} // namespace Rosenholz

// Document.cpp
#include "Document.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#include "../core/FileOps.h"
#include "../core/Config.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace RH {




std::shared_ptr<Document> Document::create(
    const std::string& title_, const std::string& type_, const std::string& pid)
{
    auto d = std::make_shared<Document>();
    d->documentId  = genId("doc"); d->title = title_;
    d->docType     = type_; d->projectId = pid;
    d->status      = "draft"; d->dateCreated = nowIso();
    d->createdAt   = d->dateCreated; d->updatedAt = d->createdAt;
    d->notes       = "{}";
    LOG_INFO("Document created: " + d->documentId + " \"" + title_ + "\"");
    return d;
}

bool Document::save() const {
    auto* db = DatabasePool::instance().get("documents");
    if (!db) { LOG_ERROR("Document::save — documents DB unavailable"); return false; }
    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO documents
        (document_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,author_id,approved_by,doc_type,doc_category,title,version,
         date_created,date_modified,date_approved,date_expires,status,classification,
         volume_number,page_count,language,format,file_path,file_url,
         external_ref,storage_system,tags,summary,links,notes,created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(documentId), ton(workflowInstanceId),
        ton(workflowStatus), ton(workflowCurrentState),
        ton(projectId), ton(taskId),
        ton(authorId), ton(approvedBy),
        BindParam::text(docType), ton(docCategory),
        BindParam::text(title), ton(version),
        BindParam::text(dateCreated), ton(dateModified),
        ton(dateApproved), ton(dateExpires),
        BindParam::text(status), ton(classification),
        BindParam::int64(volumeNumber), BindParam::int64(pageCount),
        BindParam::text(language), ton(format),
        ton(filePath), ton(fileUrl),
        ton(externalRef), ton(storageSystem),
        ton(tags), ton(summary),
        ton(links), BindParam::text(notes),
        BindParam::text(createdAt), BindParam::text(nowIso())
    });
    if (ok) LOG_INFO("Document saved: " + documentId);
    else    LOG_ERROR("Document save failed: " + documentId);
    return ok;
}

void Document::fromRow(const Row& r) {
    auto g=[&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
    documentId=g("document_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id");
    authorId=g("author_id"); approvedBy=g("approved_by");
    docType=g("doc_type"); docCategory=g("doc_category");
    title=g("title"); version=g("version");
    dateCreated=g("date_created"); dateModified=g("date_modified");
    dateApproved=g("date_approved"); dateExpires=g("date_expires");
    status=g("status"); classification=g("classification");
    auto gi=[&](const std::string& k){ auto v=g(k); return v.empty()?0:std::stoi(v); };
    volumeNumber=gi("volume_number"); pageCount=gi("page_count");
    language=g("language"); format=g("format");
    filePath=g("file_path"); fileUrl=g("file_url");
    externalRef=g("external_ref"); storageSystem=g("storage_system");
    tags=g("tags"); summary=g("summary"); links=g("links"); notes=g("notes");
    createdAt=g("created_at"); updatedAt=g("updated_at");
}
bool Document::load(const std::string& id) {
    auto* db=DatabasePool::instance().get("documents");
    if (!db) return false;
    auto rows=db->query("SELECT * FROM documents WHERE document_id=?;",{BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("Document not found: "+id); return false; }
    fromRow(rows[0]); return true;
}
bool Document::remove() {
    auto* db=DatabasePool::instance().get("documents");
    if (!db) return false;
    db->exec("DELETE FROM entity_documents WHERE document_id=?;",{BindParam::text(documentId)});
    return db->exec("DELETE FROM documents WHERE document_id=?;",{BindParam::text(documentId)});
}
bool Document::update() { updatedAt=nowIso(); dateModified=updatedAt; return save(); }

std::shared_ptr<Document> Document::loadById(const std::string& id) {
    auto d=std::make_shared<Document>(); if(!d->load(id)) return nullptr; return d;
}
std::vector<std::shared_ptr<Document>> Document::loadForProject(const std::string& pid) {
    auto* db=DatabasePool::instance().get("documents");
    std::vector<std::shared_ptr<Document>> result;
    if (!db) return result;
    auto rows=db->query("SELECT * FROM documents WHERE project_id=? ORDER BY date_created DESC;",
                        {BindParam::text(pid)});
    for (auto& r:rows) { auto d=std::make_shared<Document>(); d->fromRow(r); result.push_back(d); }
    return result;
}
std::vector<std::shared_ptr<Document>> Document::loadForEntity(
    const std::string& et, const std::string& eid)
{
    auto* db=DatabasePool::instance().get("documents");
    std::vector<std::shared_ptr<Document>> result;
    if (!db) return result;
    // Join entity_documents -> documents
    auto rows=db->query(R"(
        SELECT d.* FROM documents d
        JOIN entity_documents e ON d.document_id=e.document_id
        WHERE e.entity_type=? AND e.entity_id=? ORDER BY d.date_created DESC;
    )", {BindParam::text(et), BindParam::text(eid)});
    for (auto& r:rows) { auto d=std::make_shared<Document>(); d->fromRow(r); result.push_back(d); }
    return result;
}

// ── URL archiving ─────────────────────────────────────────────
std::shared_ptr<Document> Document::archiveFromUrl(
    const std::string& url, const std::string& pid, const std::string& aid)
{
    LOG_INFO("Archiving URL: " + url);
    const std::string& base = Config::instance().basePath();
    std::string archiveDir  = FileOps::joinPath(base, "documents", "archived");
    FileOps::makeDirs(archiveDir);

    // Determine if it's a website or a file
    bool isWebsite = (url.find("http") == 0) && (url.find('.') != std::string::npos);
    std::string ext = FileOps::extension(url);
    bool isDocFile  = !ext.empty() && ext != ".html" && ext != ".htm" && ext != ".php";

    std::string localPath;

    if (isWebsite && !isDocFile) {
        // Try to archive as PDF first (requires wkhtmltopdf), fallback to HTML
        localPath = archiveWebsite(url, archiveDir);
    } else {
        // Direct file download
        localPath = FileOps::downloadUrl(url, archiveDir);
    }

    if (localPath.empty()) {
        LOG_ERROR("Failed to archive URL: " + url);
        return nullptr;
    }

    // Build Document record
    auto d = create(FileOps::baseName(localPath), "archive", pid);
    d->fileUrl      = url;
    d->filePath     = localPath;
    d->authorId     = aid;
    d->dateCreated  = nowIso();
    d->format       = FileOps::extension(localPath);
    d->storageSystem= "local";
    d->status       = "approved";

    int64_t sz = FileOps::fileSize(localPath);
    d->summary = "Archived from " + url + " (" + std::to_string(sz) + " bytes)";

    d->save();
    LOG_INFO("URL archived as document: " + d->documentId + " path=" + localPath);
    return d;
}

std::string Document::archiveWebsite(const std::string& url, const std::string& destDir) {
    // Derive filename from URL
    std::string safe = FileOps::sanitizeFilename(url);
    if (safe.size() > 80) safe = safe.substr(0, 80);

    // Try wkhtmltopdf -> PDF
    std::string pdfPath = FileOps::joinPath(destDir, safe + ".pdf");
    std::string cmd = "wkhtmltopdf --quiet \"" + url + "\" \"" + pdfPath + "\" 2>/dev/null";
    if (system(cmd.c_str()) == 0 && FileOps::fileExists(pdfPath)) {
        LOG_INFO("Website archived as PDF: " + pdfPath);
        return pdfPath;
    }

    // Fallback: wget single-page HTML
    std::string htmlPath = FileOps::joinPath(destDir, safe + ".html");
    cmd = "wget -q --convert-links -O \"" + htmlPath + "\" \"" + url + "\" 2>/dev/null";
    if (system(cmd.c_str()) == 0 && FileOps::fileExists(htmlPath)) {
        LOG_INFO("Website archived as HTML: " + htmlPath);
        return htmlPath;
    }
    // Final fallback: plain curl
    return FileOps::downloadUrl(url, destDir);
}

bool Document::attachToEntity(const std::string& et, const std::string& eid, const std::string& rel) {
    auto* db = DatabasePool::instance().get("documents");
    if (!db) return false;
    return db->exec(
        "INSERT OR IGNORE INTO entity_documents(entity_type,entity_id,document_id,relationship) VALUES(?,?,?,?);",
        {BindParam::text(et), BindParam::text(eid), BindParam::text(documentId), BindParam::text(rel)});
}
bool Document::reassignAuthor(const std::string& id) { authorId=id; return update(); }
bool Document::reassignToProject(const std::string& id) { projectId=id; return update(); }
bool Document::reassignToTask(const std::string& id) { taskId=id; return update(); }
nlohmann::json Document::toJson() const {
    return {{"documentId",documentId},{"title",title},{"docType",docType},
            {"format",format},{"status",status},{"fileUrl",fileUrl}};
}

} // namespace RH

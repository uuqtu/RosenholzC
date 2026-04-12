#pragma once
#include "Utils.h"
// ============================================================
// Document.h  —  Document entity with URL download + archiving
//
// When a URL link is encountered anywhere in the system,
// DocumentManager::archiveFromUrl() downloads the file and
// creates a full Document record with all attributes.
// Websites are archived as HTML or PDF via system tools.
// Stored in documents.db.
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include "../core/Database.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

class Document {
public:
    std::string documentId;
    std::string workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, taskId;
    std::string authorId, approvedBy;

    std::string docType;        // report|specification|contract|correspondence|evidence|archive
    std::string docCategory;
    std::string title;
    std::string version         { "1.0" };
    std::string dateCreated, dateModified, dateApproved, dateExpires;
    std::string status;         // draft|review|approved|superseded|archived
    std::string classification;
    int         volumeNumber    { 1 };
    int         pageCount       { 0 };
    std::string language        { "EN" };
    std::string format;         // pdf|docx|xlsx|html|txt|image|archive
    std::string filePath;       // local path on disk
    std::string fileUrl;        // original source URL
    std::string externalRef;
    std::string storageSystem;
    std::string tags;           // JSON array
    std::string summary;
    std::string links;
    std::string notes;          // JSON
    std::string createdAt, updatedAt;

    // ── CRUD ──────────────────────────────────────────────
    bool save() const;
    bool load(const std::string& id);
    bool remove();
    bool update();

    // ── Factory ───────────────────────────────────────────
    static std::shared_ptr<Document> create(
        const std::string& title,
        const std::string& docType   = "report",
        const std::string& projectId = "");

    static std::shared_ptr<Document> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Document>> loadForProject(const std::string& projectId);
    static std::vector<std::shared_ptr<Document>> loadForEntity(
        const std::string& entityType, const std::string& entityId);

    // ── URL download and archive ──────────────────────────
    /// Download URL, detect type, archive locally, return Document.
    /// Websites archived as PDF (via wkhtmltopdf/wget) or HTML.
    static std::shared_ptr<Document> archiveFromUrl(
        const std::string& url,
        const std::string& projectId  = "",
        const std::string& authorId   = "");

    // ── Attach to any entity ──────────────────────────────
    bool attachToEntity(const std::string& entityType, const std::string& entityId,
                        const std::string& relationship = "attached");

    // ── Reassign ─────────────────────────────────────────
    bool reassignAuthor(const std::string& newAuthorId);
    bool reassignToProject(const std::string& newProjectId);
    bool reassignToTask(const std::string& newTaskId);

    nlohmann::json toJson() const;

private:
    void fromRow(const Row& r);

    /// Try to convert website URL to PDF using system tools
    static std::string archiveWebsite(const std::string& url, const std::string& destDir);
};

} // namespace Rosenholz

// ============================================================
// ProjectF16.h  —  Project entity (Vorgangskartei F16)
//
// DDR-Aktenzeichen: XV/F16/{seq}/{year}
// Primary entity: one project per customer engagement
// Supports: Earned Value (CPI/SPI/VAC), QTCS links,
//   MFS filing, and workflow attachment
// ============================================================
#pragma once
#include "../Utils.h"
// ============================================================
// ProjectF16.h  —  Project entity (F16 Vorgang analogy)
//
// The central hub entity. All other entities relate to it.
// QTCS dimensions (Quality/Time/Cost/Scope) are multi-assignable
// and not mandatory — they link via junction tables.
//
// Supports conversion to/from TaskF22 structure.
// Workflow-enabled via workflow_instance_id.
// MFS plaintext output via MFSWriter.
// ============================================================

#include <string>
#include <vector>
#include <memory>
#include "../../core/Database.h"
#include "../../core/RegNumber.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

// ── QTCS dimension stubs (defined in their own files) ────────
struct QualityDimension;
struct TimeDimension;
struct CostDimension;
struct ScopeDimension;

// ── ProjectF16 ────────────────────────────────────────────────
class ProjectF16 {
public:
    // ── Identity / reg number ──────────────────────────────
    std::string projectId;
    RegNumber   regNumber;         ///< Structured dept/seq/year
    std::string title;
    std::string codename;
    std::string projectType;       ///< FK -> project_types.type_code
    std::string sizeClass;         ///< large|medium|small

    // ── Organisational links ───────────────────────────────
    std::string ownerTeamId;       ///< FK -> teams
    std::string leadId;            ///< FK -> persons
    std::string sponsorId;         ///< FK -> persons

    // ── Status / phase ────────────────────────────────────
    std::string status      { "in_work" }; ///< in_work → released (only via Main WFI End step)
    std::string releaseWorkflowId;    ///< WFI ID of the controlling Main Workflow
    std::string phase;
    std::string methodology;       ///< agile|waterfall|kanban
    std::string classification;
    std::string priority;
    std::string complexity;
    std::string strategicAlignment;

    // ── Time (QTCS) ───────────────────────────────────────
    std::string startDatePlanned;
    std::string startDateActual;
    std::string endDatePlanned;
    std::string endDateActual;
    int         durationPlannedDays  { 0 };
    int         durationActualDays   { 0 };
    int         scheduleVarianceDays { 0 };

    // ── Cost / Earned Value (QTCS) ─────────────────────────
    double budgetPlanned    { 0.0 };
    double budgetApproved   { 0.0 };
    double budgetCommitted  { 0.0 };
    double budgetActual     { 0.0 };
    double costVariance     { 0.0 };
    double cpi              { 1.0 };  ///< Cost Performance Index
    double spi              { 1.0 };  ///< Schedule Performance Index
    double earnedValue      { 0.0 };
    double plannedValue     { 0.0 };
    double actualCost       { 0.0 };
    double eac              { 0.0 };  ///< Estimate At Completion
    double etc              { 0.0 };  ///< Estimate To Complete
    double vac              { 0.0 };  ///< Variance At Completion
    std::string currency    { "EUR" };

    // ── Scope (QTCS) ──────────────────────────────────────
    std::string scopeStatement;
    std::string scopeVersion;
    std::string scopeLastChanged;
    std::string scopeChangeReason;
    int         scopeChangeCount { 0 };

    // ── Linked plans ──────────────────────────────────────
    std::string qualityGateId;
    std::string communicationPlanId;
    std::string workflowInstanceId;
    std::string workflowStatus;
    std::string workflowCurrentState;

    // ── External ──────────────────────────────────────────
    std::string externalRef;
    std::string links;
    std::string notes;             ///< JSON
    std::string milestones; ///< Free-text milestone notes (maintainable string)
    std::string createdAt;
    std::string updatedAt;

    // ── Multi-assignable QTCS dimension IDs ───────────────
    /// These are stored in junction tables, not as single FKs
    std::vector<std::string> qualityIds;
    std::vector<std::string> costIds;
    std::vector<std::string> timeIds;
    std::vector<std::string> scopeIds;



    // ── State predicates ──────────────────────────────────────
    // These are the authoritative guards — the CLI and any other UI
    // must rely on these, never re-implement the logic themselves.

    /// True when the project has been released and is now immutable.
    bool isReleased() const { return status == "released"; }

    /// True when the project can still be edited (not yet released).
    bool canEdit()       const { return !isReleased(); }

    /// True when new child entities (F22, F18, DOK) may be added.
    bool canAddChildren() const { return !isReleased(); }

    // ── Guarded update ────────────────────────────────────────
    /// update() refuses silently if the project is released.
    /// Returns false and logs a warning if the project is immutable.
    bool update();

    // ── CRUD ──────────────────────────────────────────────
    bool save() const;
    void ensureReleaseWorkflow();  ///< Creates Main WFI on first save
    bool load(const std::string& id);
    bool remove();

    // ── Factory ───────────────────────────────────────────
    // ------------------------------
    // Factory: create a new ProjectF16 record (not yet saved).
    //
    // Parameters:
    //   title    : project title (required)
    //   leadId   : Person-ID of the project lead (optional)
    //   teamId   : owning team ID (optional)
    //
    // Behavior:
    //   - Generates DDR-style ID: XV/F16/{seq}/{year}
    //   - Sets status to "planned", dates to today
    //   - Does NOT save — call save() explicitly
    // ------------------------------
    static std::shared_ptr<ProjectF16> create(
        const std::string& title,
        const std::string& projectType = "OV",
        const std::string& sizeClass   = "medium",
        const std::string& createdBy   = "");

    static std::vector<std::shared_ptr<ProjectF16>> loadRecent(int n = 20);
    static std::shared_ptr<ProjectF16> loadById(const std::string& projectId);
    static std::vector<std::shared_ptr<ProjectF16>> loadAll();
    static std::vector<std::shared_ptr<ProjectF16>> loadByStatus(const std::string& status);

    // ── QTCS dimension management (multi-assignable) ───────
    bool addQuality(const std::string& qualityId);
    bool addCost   (const std::string& costId);
    bool addTime   (const std::string& timeId);
    bool addScope  (const std::string& scopeId);
    bool removeQuality(const std::string& qualityId);
    bool removeCost   (const std::string& costId);
    bool removeTime   (const std::string& timeId);
    bool removeScope  (const std::string& scopeId);
    void loadQTCSLinks();


    // ── Earned value calculation ───────────────────────────
    void recalcEarnedValue();

    // ── Conversion ────────────────────────────────────────
    /// Convert this project to a Task (F22) — e.g. when a project
    /// is demoted to a sub-task of a larger project.
    /// Returns the new task ID. The project is NOT deleted.
    std::string convertToTask(const std::string& parentProjectId);

    // ── Reassign connections ───────────────────────────────
    bool reassignLead(const std::string& newLeadId);
    bool reassignTeam(const std::string& newTeamId);
    bool reassignSponsor(const std::string& newSponsorId);
    bool reassignWorkflowInstance(const std::string& newInstanceId);

    // ── Serialisation ─────────────────────────────────────
    nlohmann::json toJson() const;
    static std::shared_ptr<ProjectF16> fromJson(const nlohmann::json& j);

    // ── MFS output ────────────────────────────────────────
    /// Write MFS-style plaintext file (owner-only readable)
    bool writeMFSFile(const std::string& mfsRoot) const;

private:
    void fromRow(const Row& r);
};

} // namespace Rosenholz

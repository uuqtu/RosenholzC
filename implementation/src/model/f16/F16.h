// ============================================================
// F16.h  —  Project entity (Vorgangskartei F16)
//
// DDR-Aktenzeichen: XV/F16/{seq}/{year}
// Primary entity: one project per customer engagement
// Supports: Earned Value (CPI/SPI/VAC), QTCS links,
//   MFS filing, and workflow attachment
// ============================================================
#pragma once

#include "../Utils.h"
#include "../../core/OperationResult.h"
// ============================================================
// F16.h  —  Project entity (F16 Vorgang analogy)
//
// The central hub entity. All other entities relate to it.
// QTCS dimensions (Quality/Time/Cost/Scope) are multi-assignable
// and not mandatory — they link via junction tables.
//
// Supports conversion to/from F22 structure.
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

// ── F16 ────────────────────────────────────────────────
class F16 {
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

    // ── Phase / state ─────────────────────────────────────
    // F16 has no lifecycle — it is always editable (IN_WORK).
    // Use the archived flag to soft-delete.
    bool archived { false };          ///< soft-delete; archived projects are hidden by default
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
    double costPerformanceIndex     { 1.0 }; ///< EV / AC  (1.0 = on budget)
    double schedulePerformanceIndex { 1.0 }; ///< EV / PV  (1.0 = on schedule)
    double earnedValue              { 0.0 }; ///< BCWP — budgeted cost of work performed
    double plannedValue             { 0.0 }; ///< BCWS — budgeted cost of work scheduled
    double actualCost               { 0.0 }; ///< ACWP — actual cost of work performed
    double estimateAtCompletion     { 0.0 }; ///< EAC — projected total cost
    double estimateToComplete       { 0.0 }; ///< ETC — remaining projected cost
    double varianceAtCompletion     { 0.0 }; ///< VAC — EAC vs budget at completion
    std::string currency    { "EUR" };

    // ── Scope (QTCS) ──────────────────────────────────────
    std::string scopeStatement;
    std::string scopeVersion;
    std::string scopeLastChanged;
    std::string scopeChangeReason;
    int         scopeChangeCount { 0 };


    // ── External ──────────────────────────────────────────
    std::string externalRef;
    std::string links;
    std::string notes;             ///< JSON
    std::string milestones; ///< Free-text milestone notes (maintainable string)
    std::string createdAt;
    std::string updatedAt;




    // ── State predicates ──────────────────────────────────────
    // F16 has no release lifecycle — it is always editable.
    bool isArchived()     const { return archived; }
    bool canAddChildren() const { return !archived; }

    // ── CRUD ──────────────────────────────────────────────
    OperationResult save() const;
    OperationResult update();
    OperationResult archive();   ///< soft-delete (sets archived=true)
    OperationResult updateScope(const std::string& scopeText); ///< Sets scope + timestamp
    bool load(const std::string& id);
    OperationResult remove();

    // ── Factory ───────────────────────────────────────────
    // ------------------------------
    // Factory: create a new F16 record (not yet saved).
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
    static int count();
    /// Load all projects that have a planned start or end date.
    static std::vector<std::shared_ptr<F16>> loadWithDates();
    static std::shared_ptr<F16> create(
        const std::string& title,
        const std::string& projectType = "OV",
        const std::string& sizeClass   = "medium",
        const std::string& createdBy   = "");

    static std::vector<std::shared_ptr<F16>> loadRecent(int n = 20);
    static std::shared_ptr<F16> loadById(const std::string& projectId);
    static std::vector<std::shared_ptr<F16>> loadAll();
    static std::vector<std::shared_ptr<F16>> loadByStatus(const std::string& status);



    // ── Earned value calculation ───────────────────────────
    void recalcEarnedValue();

    // ── Conversion ────────────────────────────────────────
    /// Convert this project to a Task (F22) — e.g. when a project
    /// is demoted to a sub-task of a larger project.
    /// Returns the new task ID. The project is NOT deleted.
    std::string convertToTask(const std::string& parentProjectId);

    // ── Reassign connections ───────────────────────────────
    OperationResult reassignLead(const std::string& newLeadId);
    OperationResult reassignTeam(const std::string& newTeamId);
    OperationResult reassignSponsor(const std::string& newSponsorId);

    // ── Serialisation ─────────────────────────────────────
    nlohmann::json toJson() const;
    static std::shared_ptr<F16> fromJson(const nlohmann::json& j);

    // ── MFS output ────────────────────────────────────────
    /// Write MFS-style plaintext file (owner-only readable)
    bool writeMFSFile(const std::string& mfsRoot) const;

    std::string mfsSchluesselText() const;
private:
    void fromRow(const Row& r);
};

} // namespace Rosenholz

// ============================================================
// TaskF22.h  —  Task entity (Aufgabenkartei F22)
//
// DDR-Aktenzeichen: XV/F22/{seq}/{year}
// Tasks belong to a project (projectId required)
// Supports: WBS hierarchy (parentTaskId), effort tracking,
//   cost tracking, and workflow attachment
// ============================================================
#pragma once
#include "Utils.h"
// ============================================================
// TaskF22.h  —  Task entity (F22 Vorgangskartei analogy)
//
// Tasks belong to Projects (F16). A task can have:
//   - Child tasks (recursive subtask hierarchy)
//   - Meetings (a task can have MANY meetings)
//   - QTCS dimensions (multi-assignable, not mandatory)
//   - Trackable items
//
// Supports conversion back to ProjectF16 (promotion).
// ============================================================

#include <string>
#include <vector>
#include <memory>
#include "../core/Database.h"
#include "../core/RegNumber.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

class TaskF22 {
public:
    // ── Identity ───────────────────────────────────────────
    std::string taskId;
    RegNumber   regNumber;
    std::string projectId;         ///< Parent project (F16)
    std::string parentTaskId;      ///< Parent task for subtask hierarchy
    std::string taskCode;
    std::string title;
    std::string description;
    std::string taskType;

    // ── Assignment ────────────────────────────────────────
    std::string assigneeId;
    std::string assignedBy;

    // ── Status / workflow ─────────────────────────────────
    std::string status;            ///< draft|active|in-review|done|blocked
    std::string priority;
    std::string workflowInstanceId;
    std::string workflowStatus;
    std::string workflowCurrentState;

    // ── Time (QTCS) ───────────────────────────────────────
    double      effortPlannedHrs   { 0.0 };
    double      effortActualHrs    { 0.0 };
    double      effortRemainingHrs { 0.0 };
    std::string startDatePlanned;
    std::string startDateActual;
    std::string dueDatePlanned;
    std::string dueDateActual;
    int         scheduleVarianceDays { 0 };
    int         percentComplete      { 0 };

    // ── Cost (QTCS) ───────────────────────────────────────
    double costPlanned { 0.0 };
    double costActual  { 0.0 };

    // ── Quality / Scope (QTCS) ────────────────────────────
    std::string qualityCriteria;
    std::string acceptanceCriteria;
    bool        isMilestone { false };
    std::string wbsCode;
    std::string sprintOrPhase;

    // ── QTCS multi-assignable dimension IDs ───────────────
    std::vector<std::string> qualityIds;
    std::vector<std::string> costIds;
    std::vector<std::string> timeIds;
    std::vector<std::string> scopeIds;

    // ── Children ──────────────────────────────────────────
    std::vector<std::shared_ptr<TaskF22>> childTasks;  ///< Lazy loaded
    /// meeting IDs — meetings are loaded separately via Meeting class
    std::vector<std::string> meetingIds;


    // ── External ──────────────────────────────────────────
    std::string externalRef;
    std::string links;
    std::string notes;   ///< JSON
    std::string createdAt;
    std::string updatedAt;

    // ── CRUD ──────────────────────────────────────────────
    bool save() const;
    bool load(const std::string& id);
    bool remove();
    bool update();

    // ── Factory ───────────────────────────────────────────
    static std::shared_ptr<TaskF22> create(
        const std::string& projectId,
        const std::string& title,
        const std::string& assigneeId  = "",
        const std::string& parentTaskId = "");

    static std::shared_ptr<TaskF22> loadById(const std::string& taskId);
    static std::vector<std::shared_ptr<TaskF22>> loadForProject(const std::string& projectId);
    static std::vector<std::shared_ptr<TaskF22>> loadChildren(const std::string& parentTaskId);

    // ── QTCS assignment ───────────────────────────────────
    bool addQuality(const std::string& id);
    bool addCost   (const std::string& id);
    bool addTime   (const std::string& id);
    bool addScope  (const std::string& id);
    void loadQTCSLinks();

    // ── Reassign ─────────────────────────────────────────
    bool reassignTo(const std::string& newAssigneeId);
    bool reassignToProject(const std::string& newProjectId);
    bool reassignParent(const std::string& newParentTaskId);

    // ── Conversion ────────────────────────────────────────
    /// Promote this task to a full Project (F16).
    /// Returns the new project ID. Task is NOT deleted.
    std::string convertToProject(const std::string& projectType = "OV");

    // ── Serialisation ─────────────────────────────────────
    nlohmann::json toJson() const;
    static std::shared_ptr<TaskF22> fromJson(const nlohmann::json& j);

    // ── MFS output ────────────────────────────────────────
    bool writeMFSFile(const std::string& mfsRoot) const;

private:
    void fromRow(const Row& r);
};

} // namespace Rosenholz

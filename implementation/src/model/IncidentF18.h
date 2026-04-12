#pragma once
#include "Utils.h"
// ============================================================
// IncidentF18.h  —  Incident record (F18 Verhaftungskartei analogy)
//
// An incident is something that HAS occurred (past/present),
// distinct from Risk (forward-looking).
// Can link back to the Risk that triggered it.
// QTCS dimensions are multi-assignable, not mandatory.
// ============================================================

#include <string>
#include <vector>
#include <memory>
#include "Trackable.h"
#include "../core/Database.h"
#include "../core/RegNumber.h"
#include <nlohmann/json.hpp>

namespace RH {

class IncidentF18 {
public:
    // ── Identity ───────────────────────────────────────────
    std::string incidentId;
    RegNumber   regNumber;
    std::string projectId;
    std::string taskId;
    std::string riskId;            ///< FK -> risks (optional: risk that triggered this)

    // ── Ownership ─────────────────────────────────────────
    std::string reportedBy;
    std::string ownerId;

    // ── Description ───────────────────────────────────────
    std::string title;
    std::string description;
    std::string incidentType;
    std::string category;
    std::string severity;          ///< critical|high|medium|low
    std::string impactArea;        ///< cost|schedule|scope|quality|resource

    // ── Impact (QTCS) ─────────────────────────────────────
    double costImpact            { 0.0 };
    int    scheduleImpactDays    { 0 };
    std::string scopeImpact;
    std::string qualityImpact;

    // ── Dates ─────────────────────────────────────────────
    std::string occurredDate;
    std::string reportedDate;
    std::string resolvedDate;

    // ── Status / resolution ───────────────────────────────
    std::string status;            ///< open|in-progress|resolved|closed
    std::string rootCause;
    std::string immediateAction;
    std::string correctiveAction;
    std::string resolution;

    // ── Escalation ────────────────────────────────────────
    bool        escalated    { false };
    std::string escalatedTo;

    // ── Workflow ──────────────────────────────────────────
    std::string workflowInstanceId;
    std::string workflowStatus;
    std::string workflowCurrentState;

    // ── QTCS multi-assignable dimension IDs ───────────────
    std::vector<std::string> qualityIds;
    std::vector<std::string> costIds;
    std::vector<std::string> timeIds;
    std::vector<std::string> scopeIds;

    // ── Trackable items ───────────────────────────────────
    std::vector<std::shared_ptr<TrackableItem>> trackables;

    // ── External ──────────────────────────────────────────
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
    static std::shared_ptr<IncidentF18> create(
        const std::string& projectId,
        const std::string& title,
        const std::string& severity    = "medium",
        const std::string& reportedBy  = "");

    static std::shared_ptr<IncidentF18> loadById(const std::string& id);
    static std::vector<std::shared_ptr<IncidentF18>> loadForProject(const std::string& projectId);
    static std::vector<std::shared_ptr<IncidentF18>> loadOpenIncidents();

    // ── QTCS ─────────────────────────────────────────────
    bool addQuality(const std::string& id);
    bool addCost   (const std::string& id);
    bool addTime   (const std::string& id);
    bool addScope  (const std::string& id);
    void loadQTCSLinks();

    // ── Trackable ────────────────────────────────────────
    std::shared_ptr<TrackableItem> addTrackable(
        const std::string& title, const std::string& createdBy = "");

    // ── Reassign ─────────────────────────────────────────
    bool reassignOwner(const std::string& newOwnerId);
    bool reassignToProject(const std::string& newProjectId);
    bool linkToRisk(const std::string& riskId);

    // ── Serialisation ─────────────────────────────────────
    nlohmann::json toJson() const;
    static std::shared_ptr<IncidentF18> fromJson(const nlohmann::json& j);

    // ── MFS output ────────────────────────────────────────
    bool writeMFSFile(const std::string& mfsRoot) const;

private:
    void fromRow(const Row& r);
};

} // namespace RH

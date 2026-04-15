#pragma once
// ============================================================
// ReportingModels.h  —  Measure, QualityGate, ChangeRequest, Meeting
// All stored in reporting.db or tracking.db (per schema).
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include "Utils.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

// ── Measure ──────────────────────────────────────────────────
class Measure {
public:
    std::string measureId, workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string riskId, incidentId, projectId, taskId, ownerId;
    std::string title, description;
    std::string measureType;    // preventive|corrective|detective|directive
    std::string measureCategory;
    std::string status;         // planned|in-progress|completed|verified|cancelled
    std::string plannedDate, actualDate;
    double costPlanned{0}, costActual{0}, effortHrs{0};
    std::string effectiveness, verificationMethod, verifiedDate, verifiedBy, outcome;
    std::string links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<Measure> create(const std::string& projectId, const std::string& title, const std::string& type="corrective");
    static std::shared_ptr<Measure> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Measure>> loadForProject(const std::string& pid);
    static std::vector<std::shared_ptr<Measure>> loadForRisk(const std::string& riskId);
    bool reassignOwner(const std::string& id);
    nlohmann::json toJson() const;
private: void fromRow(const Row& r);
};

// ── QualityGate ──────────────────────────────────────────────
class QualityGate {
public:
    std::string gateId, workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, reviewerId;
    std::string title, phase;
    std::string plannedDate, actualDate;
    std::string criteria, standardsApplied, qualityObjectives, acceptanceCriteria;
    std::string auditSchedule, toolsMethods;
    std::string result;   // passed|failed|conditional|pending
    std::string decision; // proceed|hold|stop
    std::string findings;
    std::string links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<QualityGate> create(const std::string& projectId, const std::string& title, const std::string& phase="");
    static std::shared_ptr<QualityGate> loadById(const std::string& id);
    static std::vector<std::shared_ptr<QualityGate>> loadForProject(const std::string& pid);
    bool recordResult(const std::string& result, const std::string& decision, const std::string& findings="");
    nlohmann::json toJson() const;
private: void fromRow(const Row& r);
};







// ── ChangeRequest ─────────────────────────────────────────────
class ChangeRequest {
public:
    std::string crId, workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, taskId, raisedBy;
    std::string title, description, changeType;
    std::string status;  // draft|submitted|under-review|approved|rejected|implemented|withdrawn
    std::string raisedDate, decisionDate, implementedDate;
    double costImpact{0};
    int    scheduleImpactDays{0};
    std::string scopeImpact, qualityImpact, justification, decisionRationale;
    std::string links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<ChangeRequest> create(const std::string& projectId, const std::string& title, const std::string& type="general");
    static std::shared_ptr<ChangeRequest> loadById(const std::string& id);
    static std::vector<std::shared_ptr<ChangeRequest>> loadForProject(const std::string& pid);
    static std::vector<std::shared_ptr<ChangeRequest>> loadOpen();
    bool approve(const std::string& rationale="");
    bool reject(const std::string& rationale="");
    nlohmann::json toJson() const;
private: void fromRow(const Row& r);
};



// ── Meeting ───────────────────────────────────────────────────
class Meeting {
public:
    std::string meetingId, workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string taskId, projectId, organiserId;
    std::string title, meetingType;
    std::string status;  // scheduled|completed|cancelled
    std::string scheduledDate, actualDate;
    int durationMins{0};
    std::string location, channel;
    std::string agenda, decisions, actions;
    std::string nextMeetingId;
    std::string links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<Meeting> create(const std::string& taskId, const std::string& title, const std::string& scheduledDate="");
    static std::shared_ptr<Meeting> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Meeting>> loadForTask(const std::string& taskId);
    static std::vector<std::shared_ptr<Meeting>> loadForProject(const std::string& projectId);
    bool complete(const std::string& decisions="", const std::string& actions="");
    nlohmann::json toJson() const;
private: void fromRow(const Row& r);
};

} // namespace Rosenholz

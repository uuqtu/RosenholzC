#pragma once
// ============================================================
// ReportingModels.h  —  Measure, QualityGate, KPI,
//                        LessonLearned, DecisionLog,
//                        ChangeRequest, AssumptionConstraint
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

// ── KPI ──────────────────────────────────────────────────────
class KPI {
public:
    std::string kpiId, projectId, taskId, ownerId;
    std::string title, description, category, dimension, unit;
    double targetValue{0}, actualValue{0}, baselineValue{0};
    double thresholdGreen{0}, thresholdAmber{0}, thresholdRed{0};
    std::string ragStatus;  // green|amber|red
    std::string measurementMethod, measurementFrequency;
    std::string lastMeasuredDate, nextMeasurementDate;
    std::string trend;      // up|down|stable
    std::string links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<KPI> create(const std::string& projectId, const std::string& title, const std::string& unit="");
    static std::shared_ptr<KPI> loadById(const std::string& id);
    static std::vector<std::shared_ptr<KPI>> loadForProject(const std::string& pid);
    void updateRAG();  // recalculate rag from thresholds
    bool recordMeasurement(double value, const std::string& date="");
    nlohmann::json toJson() const;
private: void fromRow(const Row& r);
};

// ── LessonLearned ────────────────────────────────────────────
class LessonLearned {
public:
    std::string lessonId, workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, taskId, incidentId;
    std::string submittedBy, reviewedBy;
    std::string title, description, category, dimension;
    std::string identifiedDate, reviewedDate;
    std::string status;  // draft|reviewed|approved|published
    std::string impact, recommendation, actionTaken;
    bool addedToKb{false};
    std::string tags, links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<LessonLearned> create(const std::string& projectId, const std::string& title);
    static std::shared_ptr<LessonLearned> loadById(const std::string& id);
    static std::vector<std::shared_ptr<LessonLearned>> loadForProject(const std::string& pid);
    static std::vector<std::shared_ptr<LessonLearned>> loadKnowledgeBase();
    nlohmann::json toJson() const;
private: void fromRow(const Row& r);
};

// ── DecisionLog ──────────────────────────────────────────────
class DecisionLog {
public:
    std::string decisionId, workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, taskId, meetingId, decidedBy;
    std::string title, description, decisionType;
    std::string status;  // open|implemented|superseded|cancelled
    std::string decisionDate, reviewDate;
    std::string optionsConsidered, rationale;
    std::string impactCost, impactSchedule, impactScope, impactQuality;
    std::string assumptionsMade;
    std::string links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<DecisionLog> create(const std::string& projectId, const std::string& title);
    static std::shared_ptr<DecisionLog> loadById(const std::string& id);
    static std::vector<std::shared_ptr<DecisionLog>> loadForProject(const std::string& pid);
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

// ── AssumptionConstraint ──────────────────────────────────────
class AssumptionConstraint {
public:
    std::string acId, workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, taskId, ownerId;
    std::string title, description;
    std::string acType;      // assumption|constraint
    std::string category, dimension;
    std::string status;      // active|validated|breached|closed
    std::string identifiedDate, reviewDate;
    std::string validationMethod, validatedDate, validatedBy;
    bool breached{false};
    std::string breachedDate, impactIfWrong, mitigation;
    std::string links, notes, createdAt;

    bool save() const; bool load(const std::string& id); bool remove(); bool update();
    static std::shared_ptr<AssumptionConstraint> create(const std::string& projectId, const std::string& title, const std::string& type="assumption");
    static std::shared_ptr<AssumptionConstraint> loadById(const std::string& id);
    static std::vector<std::shared_ptr<AssumptionConstraint>> loadForProject(const std::string& pid);
    bool markBreached(const std::string& date="");
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

#pragma once
#include "Utils.h"
// ============================================================
// Risk.h  —  Risk register entry
// Links to Incidents (F18) that were triggered by this risk.
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include "Trackable.h"
#include "../core/Database.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {
class Risk {
public:
    std::string riskId;
    std::string workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId, taskId, ownerId, identifiedBy;
    std::string title, description, category, subcategory, riskType;
    std::string status;            // open|mitigated|closed|accepted|transferred
    std::string identifiedDate, reviewDate, closedDate;
    int  probabilityScore { 0 };
    int  impactScoreTime  { 0 }, impactScoreCost { 0 };
    int  impactScoreQuality { 0 }, impactScoreScope { 0 };
    int  overallRiskScore  { 0 };
    std::string riskLevel; // critical|high|medium|low
    std::string responseStrategy; // avoid|mitigate|transfer|accept
    std::string contingencyPlan;
    double costReserve           { 0.0 };
    int    scheduleReserveDays   { 0 };
    std::string triggerCondition, earlyWarning, residualRiskLevel;
    bool        escalated        { false };
    std::string escalatedTo;
    std::string links, notes;
    std::string createdAt, updatedAt;

    std::vector<std::shared_ptr<TrackableItem>> trackables;

    bool save() const;
    bool load(const std::string& id);
    bool remove();
    bool update();

    static std::shared_ptr<Risk> create(const std::string& projectId,
        const std::string& title, const std::string& riskLevel = "medium");
    static std::shared_ptr<Risk> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Risk>> loadForProject(const std::string& projectId);
    static std::vector<std::shared_ptr<Risk>> loadHighRisks();

    /// Recalculate overall score from component scores
    void recalcScore();

    bool reassignOwner(const std::string& newOwnerId);
    bool reassignToProject(const std::string& newProjectId);

    std::shared_ptr<TrackableItem> addTrackable(const std::string& title, const std::string& by = "");
    void loadTrackables();

    nlohmann::json toJson() const;

private:
    void fromRow(const Row& r);
};
} // namespace Rosenholz

#pragma once
// ============================================================
// Milestone.h  —  Project milestone (linked to project + optional task)
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include "Utils.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

class Milestone {
public:
    std::string milestoneId;
    std::string workflowInstanceId, workflowStatus, workflowCurrentState;
    std::string projectId;     // required
    std::string taskId;        // optional
    std::string ownerId;
    std::string title;
    std::string description;
    std::string milestoneType; // phase-gate|delivery|payment|contractual|internal
    std::string plannedDate;
    std::string actualDate;
    int         varianceDays  { 0 };
    std::string status;        // pending|achieved|missed|deferred
    std::string criteria;
    bool        contractual    { false };
    bool        paymentTrigger { false };
    std::string links;
    std::string notes;
    std::string createdAt;

    bool save() const;
    bool load(const std::string& id);
    bool remove();
    bool update();

    static std::shared_ptr<Milestone> create(
        const std::string& projectId,
        const std::string& title,
        const std::string& plannedDate = "");
    static std::shared_ptr<Milestone> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Milestone>> loadForProject(const std::string& projectId);
    static std::vector<std::shared_ptr<Milestone>> loadOverdue();

    bool reassignOwner(const std::string& newOwnerId);
    bool markAchieved(const std::string& actualDate = "");

    nlohmann::json toJson() const;
private:
    void fromRow(const Row& r);
};

} // namespace Rosenholz

// ============================================================
// Team.h  —  Team/Diensteinheit entity (DE)
//
// Teams are organizational units (squad/chapter/division)
// Can be hierarchical via parentTeamId
// TeamMember tracks allocation % per person
// ============================================================
#pragma once
#include "../core/OperationResult.h"
#include "Utils.h"
// ============================================================
// Team.h  —  Team (Diensteinheit) and TeamMember
//
// Teams have a self-referential parent for hierarchy.
// TeamMember has extensive categorisation sections matching
// the enriched TEAM_MEMBERS table from the data model.
// Stored in core.db.
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include "../core/RegNumber.h"
#include "../core/Database.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

// ── TeamMember ────────────────────────────────────────────────
struct TeamMember {
    std::string membershipId;
    std::string teamId;
    std::string personId;

    // Role & classification
    std::string role;
    std::string roleCategory;       // leadership|technical|delivery|advisory|support
    std::string seniorityInTeam;    // junior|mid|senior|lead|principal
    std::string memberType;         // internal|contractor|advisor|secondment
    bool isLead           { false };
    bool isDeputy         { false };
    bool isCoreMemember   { false };
    bool isExtendedMember { false };
    bool isObserver       { false };

    // Assignment
    double      allocationPct     { 100.0 };
    double      fteEquivalent     { 1.0 };
    std::string startDate;
    std::string endDate;
    std::string assignmentType;     // permanent|temporary|secondment|project-based

    // Competence
    std::string primarySkill;
    std::string secondarySkills;    // JSON array
    std::string certificationsRelevant; // JSON array
    std::string clearanceLevel;

    // Workload & cost
    double      plannedHoursPerWeek { 0.0 };
    double      actualHoursPerWeek  { 0.0 };
    double      costRate            { 0.0 };
    std::string costCenter;

    // Status
    std::string status;             // active|inactive|onboarding|offboarding
    std::string onboardedDate;
    std::string offboardedDate;
    std::string offboardingReason;
    std::string notes;              // JSON

    OperationResult save() const;
    bool load(const std::string& id);
    OperationResult remove();

    static std::shared_ptr<TeamMember> create(
        const std::string& teamId,
        const std::string& personId,
        const std::string& role         = "",
        const std::string& memberType   = "internal");

    static std::vector<std::shared_ptr<TeamMember>> loadForTeam(const std::string& teamId);
    static std::vector<std::shared_ptr<TeamMember>> loadForPerson(const std::string& personId);

    /// Reassign role within the same team
    OperationResult reassignRole(const std::string& newRole, const std::string& newCategory = "");
    /// Move member to a different team
    OperationResult moveToTeam(const std::string& newTeamId);

    nlohmann::json toJson() const;

private:
    void fromRow(const Row& r);
};

// ── Team ──────────────────────────────────────────────────────
class Team {
public:
    std::string teamId;
    RegNumber   regNumber;
    std::string name;
    std::string abbreviation;
    std::string rosenholzEquiv;     // Diensteinheit equivalent
    std::string parentTeamId;       // FK -> teams (hierarchy)
    std::string leadId;             // FK -> persons
    std::string location;
    std::string type;               // delivery|platform|governance|advisory|ops
    int         headcountPlanned  { 0 };
    int         headcountActual   { 0 };
    double      budgetAllocated   { 0.0 };
    double      budgetConsumed    { 0.0 };
    std::string methodology;
    std::string tools;
    std::string status;
    std::string externalRef;
    std::string links;
    std::string notes;              // JSON
    std::string createdAt;
    std::string updatedAt;

    std::vector<std::shared_ptr<TeamMember>> members;  // lazy loaded

    // ── CRUD ────────────────────────────────────────────
    OperationResult save() const;
    bool load(const std::string& id);
    OperationResult remove();
    OperationResult update();

    // ── Factory ─────────────────────────────────────────
    static std::shared_ptr<Team> create(
        const std::string& name,
        const std::string& type       = "delivery",
        const std::string& parentId   = "");

    static std::shared_ptr<Team> loadById(const std::string& id);
    static std::vector<std::shared_ptr<Team>> loadAll();
    static std::vector<std::shared_ptr<Team>> loadChildren(const std::string& parentId);

    // ── Member management ───────────────────────────────
    std::shared_ptr<TeamMember> addMember(
        const std::string& personId,
        const std::string& role     = "",
        const std::string& type     = "internal");
    void removeMember(const std::string& personId);
    void loadMembers();

    // ── Reassign ────────────────────────────────────────
    OperationResult reassignLead(const std::string& newLeadId);
    OperationResult reassignParent(const std::string& newParentId);

    nlohmann::json toJson() const;

private:
    void fromRow(const Row& r);
};

} // namespace Rosenholz

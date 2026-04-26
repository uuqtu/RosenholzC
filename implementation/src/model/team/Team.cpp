// ============================================================
// Team.cpp  —  Team and TeamMember implementation
// ============================================================
#include "../team/Team.h"
#include "../../core/OperationResult.h"
#include "../../core/Database.h"
#include "../../core/Logger.h"
#include "../Utils.h"
#include "../../core/Repository.h"
#include "../../core/RegNumber.h"

using json = nlohmann::json;
namespace Rosenholz {


// ═════════════════════════════════════════════════════════════
// TeamMember
// ═════════════════════════════════════════════════════════════


std::shared_ptr<TeamMember> TeamMember::create(
    const std::string& teamId_, const std::string& personId_,
    const std::string& role_, const std::string& type_)
{
    LOG_INFO("Adding TeamMember person=" + personId_ + " to team=" + teamId_);
    auto m = std::make_shared<TeamMember>();
    m->membershipId      = genId("DE");
    m->teamId            = teamId_;
    m->personId          = personId_;
    m->role              = role_;
    m->memberType        = type_;
    m->status            = "active";
    m->allocationPct     = 100.0;
    m->fteEquivalent     = 1.0;
    m->onboardedDate     = nowIso();
    m->notes             = "{}";
    return m;
}

OperationResult TeamMember::save() const {
    auto* db = DatabasePool::instance().get("core");
    if (!db) { LOG_ERROR("TeamMember::save — core DB unavailable"); return OperationResult::DB_ERROR; }
    OperationResult ok = db->exec(R"(
        INSERT OR REPLACE INTO team_members
        (membership_id,team_id,person_id,
         role,role_category,seniority_in_team,member_type,
         is_lead,is_deputy,is_core_member,is_extended_member,is_observer,
         allocation_pct,fte_equivalent,start_date,end_date,assignment_type,
         primary_skill,secondary_skills,certifications_relevant,clearance_level,
         planned_hours_per_week,actual_hours_per_week,cost_rate,cost_center,
         status,onboarded_date,offboarded_date,offboarding_reason,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(membershipId),
        BindParam::text(teamId),
        BindParam::text(personId),
        BindParam::nullOrText(role), BindParam::nullOrText(roleCategory), BindParam::nullOrText(seniorityInTeam), BindParam::nullOrText(memberType),
        BindParam::int64(isLead?1:0),
        BindParam::int64(isDeputy?1:0),
        BindParam::int64(isCoreMemember?1:0),
        BindParam::int64(isExtendedMember?1:0),
        BindParam::int64(isObserver?1:0),
        BindParam::real(allocationPct),
        BindParam::real(fteEquivalent),
        BindParam::nullOrText(startDate), BindParam::nullOrText(endDate), BindParam::nullOrText(assignmentType),
        BindParam::nullOrText(primarySkill), BindParam::nullOrText(secondarySkills),
        BindParam::nullOrText(certificationsRelevant), BindParam::nullOrText(clearanceLevel),
        BindParam::real(plannedHoursPerWeek),
        BindParam::real(actualHoursPerWeek),
        BindParam::real(costRate),
        BindParam::nullOrText(costCenter),
        BindParam::text(status.empty() ? "active" : status),
        BindParam::nullOrText(onboardedDate), BindParam::nullOrText(offboardedDate), BindParam::nullOrText(offboardingReason),
        BindParam::text(notes.empty() ? "{}" : notes)
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
    if (!opOk(ok)) LOG_ERROR("TeamMember save failed: " + membershipId);
    return ok;
}

void TeamMember::fromRow(const Row& r) {
    auto g = [&](const std::string& k) -> std::string {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    membershipId     = g("membership_id");
    teamId           = g("team_id");
    personId         = g("person_id");
    role             = g("role");
    roleCategory     = g("role_category");
    seniorityInTeam  = g("seniority_in_team");
    memberType       = g("member_type");
    isLead           = g("is_lead")           == "1";
    isDeputy         = g("is_deputy")         == "1";
    isCoreMemember   = g("is_core_member")    == "1";
    isExtendedMember = g("is_extended_member")== "1";
    isObserver       = g("is_observer")       == "1";
    auto gd = [&](const std::string& k) -> double {
        auto v = g(k); return v.empty() ? 0.0 : std::stod(v);
    };
    allocationPct       = gd("allocation_pct");
    fteEquivalent       = gd("fte_equivalent");
    startDate           = g("start_date");
    endDate             = g("end_date");
    assignmentType      = g("assignment_type");
    primarySkill        = g("primary_skill");
    secondarySkills     = g("secondary_skills");
    certificationsRelevant = g("certifications_relevant");
    clearanceLevel      = g("clearance_level");
    plannedHoursPerWeek = gd("planned_hours_per_week");
    actualHoursPerWeek  = gd("actual_hours_per_week");
    costRate            = gd("cost_rate");
    costCenter          = g("cost_center");
    status              = g("status");
    onboardedDate       = g("onboarded_date");
    offboardedDate      = g("offboarded_date");
    offboardingReason   = g("offboarding_reason");
    notes               = g("notes");
}

bool TeamMember::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM team_members WHERE membership_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) return false;
    fromRow(rows[0]);
    return true;
}

OperationResult TeamMember::remove() {
    auto* db = DatabasePool::instance().get("core");
    return (db && db->exec("DELETE FROM team_members WHERE membership_id=?;",
                           {BindParam::text(membershipId)}))
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

std::vector<std::shared_ptr<TeamMember>> TeamMember::loadForTeam(const std::string& tid) {
    auto* db = DatabasePool::instance().get("core");
    std::vector<std::shared_ptr<TeamMember>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM team_members WHERE team_id=? ORDER BY is_lead DESC, role;",
        {BindParam::text(tid)});
    for (auto& r : rows) {
        auto m = std::make_shared<TeamMember>();
        m->fromRow(r);
        result.push_back(m);
    }
    return result;
}

std::vector<std::shared_ptr<TeamMember>> TeamMember::loadForPerson(const std::string& pid) {
    auto* db = DatabasePool::instance().get("core");
    std::vector<std::shared_ptr<TeamMember>> result;
    if (!db) return result;
    auto rows = db->query("SELECT * FROM team_members WHERE person_id=?;",
                          {BindParam::text(pid)});
    for (auto& r : rows) {
        auto m = std::make_shared<TeamMember>();
        m->fromRow(r);
        result.push_back(m);
    }
    return result;
}

OperationResult TeamMember::reassignRole(const std::string& r, const std::string& cat) {
    role = r;
    if (!cat.empty()) roleCategory = cat;
    LOG_INFO("TeamMember " + membershipId + " role -> " + r);
    return save();
}

OperationResult TeamMember::moveToTeam(const std::string& newTeamId) {
    LOG_INFO("TeamMember " + personId + " -> team " + newTeamId);
    teamId = newTeamId;
    return save();
}

json TeamMember::toJson() const {
    return {
        {"membershipId", membershipId}, {"personId", personId},
        {"role",         role},         {"memberType", memberType},
        {"status",       status},       {"allocationPct", allocationPct}
    };
}

// ═════════════════════════════════════════════════════════════
// Team
// ═════════════════════════════════════════════════════════════


std::shared_ptr<Team> Team::create(
    const std::string& name_, const std::string& type_, const std::string& parentId)
{
    LOG_INFO("Creating Team: " + name_);
    auto t = std::make_shared<Team>();
    t->teamId       = genId("DE");
    t->regNumber    = RegNumber::fromString(t->teamId);
    t->name         = name_;
    t->type         = type_;
    t->parentTeamId = parentId;
    t->status       = "active";
    t->createdAt    = nowIso();
    t->updatedAt    = t->createdAt;
    t->notes        = "{}";
    return t;
}

OperationResult Team::save() const {
    auto* db = DatabasePool::instance().get("core");
    if (!db) { LOG_ERROR("Team::save — core DB unavailable"); return OperationResult::DB_ERROR; }
    OperationResult ok = db->exec(R"(
        INSERT OR REPLACE INTO teams
        (team_id,name,abbreviation,rosenholz_equiv,parent_team_id,lead_id,
         location,type,headcount_planned,headcount_actual,
         budget_allocated,budget_consumed,methodology,tools,
         status,external_ref,links,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(teamId),
        BindParam::text(name),
        BindParam::nullOrText(abbreviation),
        BindParam::nullOrText(rosenholzEquiv),
        BindParam::nullOrText(parentTeamId),   // soft ref — NULL when empty
        BindParam::nullOrText(leadId),
        BindParam::nullOrText(location),
        BindParam::nullOrText(type),
        BindParam::int64(headcountPlanned),
        BindParam::int64(headcountActual),
        BindParam::real(budgetAllocated),
        BindParam::real(budgetConsumed),
        BindParam::nullOrText(methodology),
        BindParam::nullOrText(tools),
        BindParam::text(status.empty() ? "active" : status),
        BindParam::nullOrText(externalRef),
        BindParam::nullOrText(links),
        BindParam::text(notes.empty() ? "{}" : notes)
    }) ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
    return ok;
}

void Team::fromRow(const Row& r) {
    auto g = [&](const std::string& k) -> std::string {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    teamId        = g("team_id");
    name          = g("name");
    abbreviation  = g("abbreviation");
    rosenholzEquiv= g("rosenholz_equiv");
    parentTeamId  = g("parent_team_id");
    leadId        = g("lead_id");
    location      = g("location");
    type          = g("type");
    auto gi = [&](const std::string& k) -> int {
        auto v = g(k); return v.empty() ? 0 : std::stoi(v);
    };
    auto gd = [&](const std::string& k) -> double {
        auto v = g(k); return v.empty() ? 0.0 : std::stod(v);
    };
    headcountPlanned = gi("headcount_planned");
    headcountActual  = gi("headcount_actual");
    budgetAllocated  = gd("budget_allocated");
    budgetConsumed   = gd("budget_consumed");
    methodology  = g("methodology");
    tools        = g("tools");
    status       = g("status");
    externalRef  = g("external_ref");
    links        = g("links");
    notes        = g("notes");
    createdAt    = g("created_at");
    updatedAt    = g("updated_at");
}

bool Team::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM teams WHERE team_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("Team not found: " + id); return false; }
    fromRow(rows[0]);
    return true;
}

OperationResult Team::remove() {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return OperationResult::DB_ERROR;
    LOG_WARN("Removing Team: " + teamId + " " + name);
    db->exec("DELETE FROM team_members WHERE team_id=?;", {BindParam::text(teamId)});
    return db->exec("DELETE FROM teams WHERE team_id=?;", {BindParam::text(teamId)})
           ? OperationResult::OPERATION_ACK : OperationResult::DB_ERROR;
}

OperationResult Team::update() { updatedAt = nowIso(); return save(); }

std::shared_ptr<Team> Team::loadById(const std::string& id) {
    auto t = std::make_shared<Team>();
    if (!t->load(id)) return nullptr;
    return t;
}

std::vector<std::shared_ptr<Team>> Team::loadAll() {
    auto* db = DatabasePool::instance().get("core");
    std::vector<std::shared_ptr<Team>> result;
    if (!db) return result;
    auto rows = db->query("SELECT * FROM teams ORDER BY name;");
    for (auto& r : rows) {
        auto t = std::make_shared<Team>();
        t->fromRow(r);
        result.push_back(t);
    }
    return result;
}

std::vector<std::shared_ptr<Team>> Team::loadChildren(const std::string& parentId) {
    auto* db = DatabasePool::instance().get("core");
    std::vector<std::shared_ptr<Team>> result;
    if (!db) return result;
    auto rows = db->query("SELECT * FROM teams WHERE parent_team_id=? ORDER BY name;",
                          {BindParam::text(parentId)});
    for (auto& r : rows) {
        auto t = std::make_shared<Team>();
        t->fromRow(r);
        result.push_back(t);
    }
    return result;
}

std::shared_ptr<TeamMember> Team::addMember(
    const std::string& personId, const std::string& role_, const std::string& type_)
{
    auto m = TeamMember::create(teamId, personId, role_, type_);
    m->save();
    members.push_back(m);
    headcountActual = static_cast<int>(members.size());
    update();
    return m;
}

void Team::removeMember(const std::string& personId_) {
    for (auto& m : members)
        if (m->personId == personId_) { m->remove(); break; }
    members.erase(std::remove_if(members.begin(), members.end(),
        [&](auto& m){ return m->personId == personId_; }), members.end());
    headcountActual = static_cast<int>(members.size());
    update();
}

void Team::loadMembers() {
    members = TeamMember::loadForTeam(teamId);
}

OperationResult Team::reassignLead(const std::string& id) {
    LOG_INFO("Team " + teamId + " lead -> " + id);
    leadId = id;
    return update();
}

OperationResult Team::reassignParent(const std::string& id) {
    parentTeamId = id;
    return update();
}

json Team::toJson() const {
    return {
        {"teamId",  teamId}, {"name", name}, {"type", type},
        {"leadId",  leadId}, {"status", status},
        {"headcount", headcountActual}
    };
}

} // namespace Rosenholz

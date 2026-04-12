// ============================================================
// IncidentF18.cpp  —  Incident record implementation
// ============================================================

#include "../mfs/MFSWriter.h"
#include "IncidentF18.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include "../core/RegNumber.h"
#include "../core/FileOps.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>

using json = nlohmann::json;

namespace Rosenholz {



std::shared_ptr<IncidentF18> IncidentF18::create(
    const std::string& projectId_,
    const std::string& title_,
    const std::string& severity_,
    const std::string& reportedBy_)
{
    LOG_INFO("Creating IncidentF18: " + title_);
    auto i = std::make_shared<IncidentF18>();
    i->incidentId  = genId("F18");
    i->regNumber   = RegNumberGenerator::next(RegDept::INCIDENT);
    i->projectId   = projectId_;
    i->title       = title_;
    i->severity    = severity_;
    i->reportedBy  = reportedBy_;
    i->status      = "open";
    i->reportedDate= nowIso();
    i->createdAt   = nowIso();
    i->updatedAt   = i->createdAt;
    i->notes       = "{}";
    LOG_INFO("IncidentF18 created: " + i->incidentId + " reg=" + i->regNumber.toString());
    return i;
}

bool IncidentF18::save() const {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) { LOG_ERROR("IncidentF18::save — projects DB not available"); return false; }

    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO incidents (
            incident_id, workflow_instance_id, workflow_status, workflow_current_state,
            project_id, task_id, risk_id, reported_by, owner_id,
            title, description, incident_type, category, severity, impact_area,
            cost_impact, schedule_impact_days, scope_impact, quality_impact,
            occurred_date, reported_date, resolved_date,
            status, root_cause, immediate_action, corrective_action, resolution,
            escalated, escalated_to, links, notes, created_at, updated_at
        ) VALUES (?,?,?,?, ?,?,?,?,?, ?,?,?,?,?,?, ?,?,?,?, ?,?,?, ?,?,?,?,?, ?,?,?,?,?,?)
    )", {
        BindParam::text(incidentId),
        ton(workflowInstanceId),
        ton(workflowStatus),
        ton(workflowCurrentState),
        BindParam::text(projectId),
        ton(taskId),
        ton(riskId),
        ton(reportedBy),
        ton(ownerId),
        BindParam::text(title),
        ton(description),
        ton(incidentType),
        ton(category),
        BindParam::text(severity),
        ton(impactArea),
        BindParam::real(costImpact),
        BindParam::int64(scheduleImpactDays),
        ton(scopeImpact),
        ton(qualityImpact),
        ton(occurredDate),
        ton(reportedDate),
        ton(resolvedDate),
        BindParam::text(status),
        ton(rootCause),
        ton(immediateAction),
        ton(correctiveAction),
        ton(resolution),
        BindParam::int64(escalated ? 1 : 0),
        ton(escalatedTo),
        ton(links),
        BindParam::text(notes),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    });

    if (ok) LOG_INFO("IncidentF18 saved: " + incidentId);
    else    LOG_ERROR("IncidentF18 save failed: " + incidentId);
    return ok;
}

void IncidentF18::fromRow(const Row& r) {
    auto get = [&](const std::string& k){ auto it=r.find(k); return it!=r.end()?it->second:""; };
    incidentId           = get("incident_id");
    workflowInstanceId   = get("workflow_instance_id");
    projectId            = get("project_id");
    taskId               = get("task_id");
    riskId               = get("risk_id");
    reportedBy           = get("reported_by");
    ownerId              = get("owner_id");
    title                = get("title");
    description          = get("description");
    incidentType         = get("incident_type");
    category             = get("category");
    severity             = get("severity");
    impactArea           = get("impact_area");
    auto gd=[&](const std::string& k){ auto v=get(k); return v.empty()?0.0:std::stod(v); };
    auto gi=[&](const std::string& k){ auto v=get(k); return v.empty()?0:std::stoi(v); };
    costImpact           = gd("cost_impact");
    scheduleImpactDays   = gi("schedule_impact_days");
    scopeImpact          = get("scope_impact");
    qualityImpact        = get("quality_impact");
    occurredDate         = get("occurred_date");
    reportedDate         = get("reported_date");
    resolvedDate         = get("resolved_date");
    status               = get("status");
    rootCause            = get("root_cause");
    immediateAction      = get("immediate_action");
    correctiveAction     = get("corrective_action");
    resolution           = get("resolution");
    escalated            = get("escalated") == "1";
    escalatedTo          = get("escalated_to");
    links                = get("links");
    notes                = get("notes");
    createdAt            = get("created_at");
    updatedAt            = get("updated_at");
}

bool IncidentF18::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM incidents WHERE incident_id=?;", {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("IncidentF18 not found: " + id); return false; }
    fromRow(rows[0]);
    loadQTCSLinks();
    return true;
}

bool IncidentF18::remove() {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return false;
    db->exec("DELETE FROM incident_quality WHERE incident_id=?;", {BindParam::text(incidentId)});
    db->exec("DELETE FROM incident_cost    WHERE incident_id=?;", {BindParam::text(incidentId)});
    db->exec("DELETE FROM incident_time    WHERE incident_id=?;", {BindParam::text(incidentId)});
    db->exec("DELETE FROM incident_scope   WHERE incident_id=?;", {BindParam::text(incidentId)});
    return db->exec("DELETE FROM incidents WHERE incident_id=?;", {BindParam::text(incidentId)});
}

bool IncidentF18::update() { updatedAt = nowIso(); return save(); }

std::shared_ptr<IncidentF18> IncidentF18::loadById(const std::string& id) {
    auto i = std::make_shared<IncidentF18>();
    if (!i->load(id)) return nullptr;
    return i;
}

std::vector<std::shared_ptr<IncidentF18>> IncidentF18::loadForProject(const std::string& pid) {
    auto* db = DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<IncidentF18>> result;
    if (!db) return result;
    auto rows = db->query("SELECT * FROM incidents WHERE project_id=? ORDER BY reported_date DESC;",
                          {BindParam::text(pid)});
    for (auto& r : rows) {
        auto i = std::make_shared<IncidentF18>();
        i->fromRow(r);
        result.push_back(i);
    }
    return result;
}

std::vector<std::shared_ptr<IncidentF18>> IncidentF18::loadOpenIncidents() {
    auto* db = DatabasePool::instance().get("projects");
    std::vector<std::shared_ptr<IncidentF18>> result;
    if (!db) return result;
    auto rows = db->query("SELECT * FROM incidents WHERE status='open' ORDER BY severity, reported_date;");
    for (auto& r : rows) {
        auto i = std::make_shared<IncidentF18>();
        i->fromRow(r);
        result.push_back(i);
    }
    LOG_INFO("Loaded " + std::to_string(result.size()) + " open incidents");
    return result;
}

// ── QTCS ─────────────────────────────────────────────────────
bool IncidentF18::addQuality(const std::string& id) {
    qualityIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO incident_quality VALUES (?,?);",
        {BindParam::text(incidentId), BindParam::text(id)}) : false;
}
bool IncidentF18::addCost(const std::string& id) {
    costIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO incident_cost VALUES (?,?);",
        {BindParam::text(incidentId), BindParam::text(id)}) : false;
}
bool IncidentF18::addTime(const std::string& id) {
    timeIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO incident_time VALUES (?,?);",
        {BindParam::text(incidentId), BindParam::text(id)}) : false;
}
bool IncidentF18::addScope(const std::string& id) {
    scopeIds.push_back(id);
    auto* db = DatabasePool::instance().get("projects");
    return db ? db->exec("INSERT OR IGNORE INTO incident_scope VALUES (?,?);",
        {BindParam::text(incidentId), BindParam::text(id)}) : false;
}

void IncidentF18::loadQTCSLinks() {
    auto* db = DatabasePool::instance().get("projects");
    if (!db) return;
    auto loadIds = [&](const std::string& table, const std::string& col) {
        std::vector<std::string> ids;
        auto rows = db->query("SELECT " + col + " FROM " + table + " WHERE incident_id=?;",
                              {BindParam::text(incidentId)});
        for (auto& r : rows) ids.push_back(r.begin()->second);
        return ids;
    };
    qualityIds = loadIds("incident_quality", "quality_id");
    costIds    = loadIds("incident_cost",    "cost_id");
    timeIds    = loadIds("incident_time",    "time_id");
    scopeIds   = loadIds("incident_scope",   "scope_id");
}

// ── Trackable ────────────────────────────────────────────────
std::shared_ptr<TrackableItem> IncidentF18::addTrackable(
    const std::string& title_, const std::string& createdBy_) {
    auto t = TrackableItem::create("incident", incidentId, title_, createdBy_);
    t->save();
    trackables.push_back(t);
    return t;
}

// ── Reassign ─────────────────────────────────────────────────
bool IncidentF18::reassignOwner(const std::string& newOwnerId) {
    LOG_INFO("Reassigning incident " + incidentId + " owner to " + newOwnerId);
    ownerId = newOwnerId; return update();
}
bool IncidentF18::reassignToProject(const std::string& newProjectId) {
    projectId = newProjectId; return update();
}
bool IncidentF18::linkToRisk(const std::string& riskId_) {
    LOG_INFO("Linking incident " + incidentId + " to risk " + riskId_);
    riskId = riskId_; return update();
}

// ── Serialisation ─────────────────────────────────────────────
json IncidentF18::toJson() const {
    json j;
    j["incidentId"]  = incidentId;
    j["regNumber"]   = regNumber.toString();
    j["projectId"]   = projectId;
    j["title"]       = title;
    j["severity"]    = severity;
    j["status"]      = status;
    j["reportedBy"]  = reportedBy;
    j["costImpact"]  = costImpact;
    return j;
}

std::shared_ptr<IncidentF18> IncidentF18::fromJson(const json& j) {
    auto i = std::make_shared<IncidentF18>();
    i->incidentId = j.value("incidentId", "");
    i->projectId  = j.value("projectId",  "");
    i->title      = j.value("title",      "");
    i->severity   = j.value("severity",   "medium");
    i->status     = j.value("status",     "open");
    return i;
}

// ── MFS output ───────────────────────────────────────────────
bool IncidentF18::writeMFSFile(const std::string& mfsRoot) const {
    return MFSWriter::writeIncident(*this, mfsRoot);
}

} // namespace Rosenholz

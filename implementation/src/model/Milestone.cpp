// Milestone.cpp
#include "Milestone.h"
#include "../core/Database.h"
#include "../core/Logger.h"

namespace Rosenholz {

std::shared_ptr<Milestone> Milestone::create(const std::string& pid, const std::string& t, const std::string& pd) {
    LOG_INFO("Creating Milestone: " + t);
    auto m = std::make_shared<Milestone>();
    m->milestoneId  = genId("MEI");
    m->projectId    = pid;
    m->title        = t;
    m->plannedDate  = pd;
    m->status       = "pending";
    m->milestoneType= "internal";
    m->createdAt    = nowIso();
    m->notes        = "{}";
    return m;
}

bool Milestone::save() const {
    auto* db = DatabasePool::instance().get("f16");
    if (!db) return false;
    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO milestones
        (milestone_id,workflow_instance_id,workflow_status,workflow_current_state,
         project_id,task_id,owner_id,title,description,milestone_type,
         planned_date,actual_date,variance_days,status,criteria,
         contractual,payment_trigger,links,notes,created_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(milestoneId), textOrNull(workflowInstanceId), textOrNull(workflowStatus), textOrNull(workflowCurrentState),
        BindParam::text(projectId), textOrNull(taskId), textOrNull(ownerId),
        BindParam::text(title), textOrNull(description), textOrNull(milestoneType),
        textOrNull(plannedDate), textOrNull(actualDate), BindParam::int64(varianceDays),
        BindParam::text(status.empty()?"pending":status), textOrNull(criteria),
        BindParam::int64(contractual?1:0), BindParam::int64(paymentTrigger?1:0),
        textOrNull(links), BindParam::text(notes.empty()?"{}":notes), BindParam::text(createdAt)
    });
    if (ok) LOG_INFO("Milestone saved: " + milestoneId);
    else    LOG_ERROR("Milestone save failed: " + milestoneId);
    return ok;
}

void Milestone::fromRow(const Row& r) {
    auto g=[&](const std::string& k){auto it=r.find(k);return it!=r.end()?it->second:"";};
    milestoneId=g("milestone_id"); workflowInstanceId=g("workflow_instance_id");
    projectId=g("project_id"); taskId=g("task_id"); ownerId=g("owner_id");
    title=g("title"); description=g("description"); milestoneType=g("milestone_type");
    plannedDate=g("planned_date"); actualDate=g("actual_date");
    auto gi=[&](const std::string& k){auto v=g(k);return v.empty()?0:std::stoi(v);};
    varianceDays=gi("variance_days"); status=g("status"); criteria=g("criteria");
    contractual=g("contractual")=="1"; paymentTrigger=g("payment_trigger")=="1";
    links=g("links"); notes=g("notes"); createdAt=g("created_at");
}

bool Milestone::load(const std::string& id) {
    auto* db=DatabasePool::instance().get("f16"); if (!db) return false;
    auto rows=db->query("SELECT * FROM milestones WHERE milestone_id=?;",{BindParam::text(id)});
    if (rows.empty()){LOG_WARN("Milestone not found: "+id);return false;}
    fromRow(rows[0]); return true;
}
bool Milestone::remove() {
    auto* db=DatabasePool::instance().get("f16"); if (!db) return false;
    return db->exec("DELETE FROM milestones WHERE milestone_id=?;",{BindParam::text(milestoneId)});
}
bool Milestone::update() { return save(); }

std::shared_ptr<Milestone> Milestone::loadById(const std::string& id) {
    auto m=std::make_shared<Milestone>(); if(!m->load(id)) return nullptr; return m;
}
std::vector<std::shared_ptr<Milestone>> Milestone::loadForProject(const std::string& pid) {
    auto* db=DatabasePool::instance().get("f16");
    std::vector<std::shared_ptr<Milestone>> result; if (!db) return result;
    auto rows=db->query("SELECT * FROM milestones WHERE project_id=? ORDER BY planned_date;",{BindParam::text(pid)});
    for (auto& r:rows){auto m=std::make_shared<Milestone>();m->fromRow(r);result.push_back(m);}
    return result;
}
std::vector<std::shared_ptr<Milestone>> Milestone::loadOverdue() {
    auto* db=DatabasePool::instance().get("f16");
    std::vector<std::shared_ptr<Milestone>> result; if (!db) return result;
    auto rows=db->query("SELECT * FROM milestones WHERE status='pending' AND planned_date < date('now') ORDER BY planned_date;");
    for (auto& r:rows){auto m=std::make_shared<Milestone>();m->fromRow(r);result.push_back(m);}
    LOG_INFO("Loaded "+std::to_string(result.size())+" overdue milestones");
    return result;
}
bool Milestone::reassignOwner(const std::string& id){ownerId=id;return update();}
bool Milestone::markAchieved(const std::string& d){
    status="achieved"; actualDate=d.empty()?nowIso():d; return update();
}
nlohmann::json Milestone::toJson() const {
    return {{"milestoneId",milestoneId},{"title",title},{"status",status},
            {"plannedDate",plannedDate},{"actualDate",actualDate}};
}

} // namespace Rosenholz

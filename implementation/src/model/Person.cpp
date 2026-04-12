// ============================================================
// Person.cpp
// ============================================================
#include "Person.h"
#include "../core/Database.h"
#include "../core/Logger.h"
#include "Utils.h"
#include "../core/RegNumber.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>

using json = nlohmann::json;
namespace RH {


std::shared_ptr<Person> Person::create(
    const std::string& first, const std::string& last,
    const std::string& email_, const std::string& type)
{
    LOG_INFO("Creating Person: " + first + " " + last);
    auto p = std::make_shared<Person>();
    p->personId   = genId("per");
    p->regNumber  = RegNumberGenerator::next(RegDept::PERSON);
    p->firstName  = first;
    p->lastName   = last;
    p->email      = email_;
    p->personType = type;
    p->status     = "active";
    p->availabilityPct = 100.0;
    p->createdAt  = nowIso();
    p->updatedAt  = p->createdAt;
    p->notes      = "{}";
    return p;
}

// Helper: return null param when string is empty (avoids FK failures)

bool Person::save() const {
    auto* db = DatabasePool::instance().get("core");
    if (!db) { LOG_ERROR("Person::save — core DB unavailable"); return false; }
    bool ok = db->exec(R"(
        INSERT OR REPLACE INTO persons
        (person_id,reg_number,last_name,first_name,preferred_name,email,phone,
         org_unit,department,location,country,role_title,person_type,employment_type,
         seniority_level,skills,certifications,languages,day_rate,monthly_rate,
         availability_pct,availability_from,availability_to,manager_id,
         status,onboard_date,offboard_date,clearance_level,external_ref,links,notes,
         created_at,updated_at)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )", {
        BindParam::text(personId),
        BindParam::text(regNumber.toString()),
        BindParam::text(lastName),
        BindParam::text(firstName),
        ton(preferredName),
        ton(email),
        ton(phone),
        ton(orgUnit),
        ton(department),
        ton(location),
        ton(country),
        ton(roleTitle),
        BindParam::text(personType),
        ton(employmentType),
        ton(seniorityLevel),
        ton(skills),
        ton(certifications),
        ton(languages),
        BindParam::real(dayRate),
        BindParam::real(monthlyRate),
        BindParam::real(availabilityPct),
        ton(availabilityFrom),
        ton(availabilityTo),
        ton(managerId),      // soft ref — NULL when empty, never fails FK
        BindParam::text(status),
        ton(onboardDate),
        ton(offboardDate),
        ton(clearanceLevel),
        ton(externalRef),
        ton(links),
        BindParam::text(notes.empty() ? "{}" : notes),
        BindParam::text(createdAt),
        BindParam::text(nowIso())
    });
    if (ok) LOG_INFO("Person saved: " + personId + " " + fullName());
    else    LOG_ERROR("Person save failed: " + personId + " err=" + db->lastError());
    return ok;
}

void Person::fromRow(const Row& r) {
    auto g = [&](const std::string& k) -> std::string {
        auto it = r.find(k); return it != r.end() ? it->second : "";
    };
    personId       = g("person_id");
    firstName      = g("first_name");   lastName       = g("last_name");
    preferredName  = g("preferred_name"); email         = g("email");
    phone          = g("phone");         orgUnit        = g("org_unit");
    department     = g("department");    location       = g("location");
    country        = g("country");       roleTitle      = g("role_title");
    personType     = g("person_type");   employmentType = g("employment_type");
    seniorityLevel = g("seniority_level"); skills       = g("skills");
    certifications = g("certifications"); languages     = g("languages");
    clearanceLevel = g("clearance_level"); managerId    = g("manager_id");
    status         = g("status");        onboardDate    = g("onboard_date");
    offboardDate   = g("offboard_date"); externalRef    = g("external_ref");
    links          = g("links");         notes          = g("notes");
    createdAt      = g("created_at");    updatedAt      = g("updated_at");
    availabilityFrom = g("availability_from");
    availabilityTo   = g("availability_to");
    auto gd = [&](const std::string& k) -> double {
        auto v = g(k); return v.empty() ? 0.0 : std::stod(v);
    };
    dayRate         = gd("day_rate");
    monthlyRate     = gd("monthly_rate");
    availabilityPct = gd("availability_pct");
    // RegNumber
    auto rn = RegNumber::fromString(g("reg_number"));
    regNumber = rn;
}

bool Person::load(const std::string& id) {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    auto rows = db->query("SELECT * FROM persons WHERE person_id=?;",
                          {BindParam::text(id)});
    if (rows.empty()) { LOG_WARN("Person not found: " + id); return false; }
    fromRow(rows[0]);
    return true;
}

bool Person::remove() {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return false;
    LOG_WARN("Removing Person: " + personId + " " + fullName());
    return db->exec("DELETE FROM persons WHERE person_id=?;",
                    {BindParam::text(personId)});
}

bool Person::update() {
    updatedAt = nowIso();
    return save();
}

std::shared_ptr<Person> Person::loadById(const std::string& id) {
    auto p = std::make_shared<Person>();
    if (!p->load(id)) return nullptr;
    return p;
}

std::shared_ptr<Person> Person::loadByEmail(const std::string& email_) {
    auto* db = DatabasePool::instance().get("core");
    if (!db) return nullptr;
    auto rows = db->query("SELECT * FROM persons WHERE email=?;",
                          {BindParam::text(email_)});
    if (rows.empty()) return nullptr;
    auto p = std::make_shared<Person>();
    p->fromRow(rows[0]);
    return p;
}

std::vector<std::shared_ptr<Person>> Person::loadAll() {
    auto* db = DatabasePool::instance().get("core");
    std::vector<std::shared_ptr<Person>> result;
    if (!db) return result;
    auto rows = db->query(
        "SELECT * FROM persons WHERE status='active' ORDER BY last_name,first_name;");
    for (auto& r : rows) {
        auto p = std::make_shared<Person>();
        p->fromRow(r);
        result.push_back(p);
    }
    LOG_INFO("Loaded " + std::to_string(result.size()) + " persons");
    return result;
}

std::vector<std::shared_ptr<Person>> Person::search(const std::string& frag) {
    auto* db = DatabasePool::instance().get("core");
    std::vector<std::shared_ptr<Person>> result;
    if (!db) return result;
    auto pat = "%" + frag + "%";
    auto rows = db->query(
        "SELECT * FROM persons WHERE last_name LIKE ? OR first_name LIKE ? OR email LIKE ? "
        "ORDER BY last_name;",
        {BindParam::text(pat), BindParam::text(pat), BindParam::text(pat)});
    for (auto& r : rows) {
        auto p = std::make_shared<Person>();
        p->fromRow(r);
        result.push_back(p);
    }
    return result;
}

bool Person::reassignManager(const std::string& id) {
    managerId = id;
    return update();
}

bool Person::setStatus(const std::string& s) {
    LOG_INFO("Person " + personId + " status -> " + s);
    status = s;
    return update();
}

json Person::toJson() const {
    return {
        {"personId",   personId},
        {"firstName",  firstName},
        {"lastName",   lastName},
        {"email",      email},
        {"role",       roleTitle},
        {"status",     status},
        {"personType", personType}
    };
}

std::shared_ptr<Person> Person::fromJson(const json& j) {
    auto p = std::make_shared<Person>();
    p->firstName  = j.value("firstName", "");
    p->lastName   = j.value("lastName",  "");
    p->email      = j.value("email",     "");
    p->personType = j.value("personType","internal");
    return p;
}

} // namespace RH

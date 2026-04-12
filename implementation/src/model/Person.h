#pragma once
#include "Utils.h"
// ============================================================
// Person.h  —  Person entity (Personenkartei)
//
// Persons are referenced by Projects (lead, sponsor),
// Tasks (assignee), Teams (lead, members), Workflow
// participants, and virtually every other entity.
// Stored in core.db for fast cross-entity lookup.
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include "../core/RegNumber.h"
#include "../core/Database.h"
#include <nlohmann/json.hpp>

namespace Rosenholz {

class Person {
public:
    // ── Identity ───────────────────────────────────────────
    std::string personId;
    RegNumber   regNumber;
    std::string lastName;
    std::string firstName;
    std::string preferredName;
    std::string email;
    std::string phone;

    // ── Organisational ─────────────────────────────────────
    std::string orgUnit;
    std::string department;
    std::string location;
    std::string country;
    std::string roleTitle;
    std::string personType;       // internal|external|contractor|advisor
    std::string employmentType;
    std::string seniorityLevel;
    std::string managerId;        // FK -> persons (self-ref)

    // ── Skills / qualifications ────────────────────────────
    std::string skills;           // JSON array
    std::string certifications;   // JSON array
    std::string languages;        // JSON array
    std::string clearanceLevel;

    // ── Availability / cost ────────────────────────────────
    double      dayRate          { 0.0 };
    double      monthlyRate      { 0.0 };
    double      availabilityPct  { 100.0 };
    std::string availabilityFrom;
    std::string availabilityTo;

    // ── Status ────────────────────────────────────────────
    std::string status;           // active|inactive|on-leave|terminated
    std::string onboardDate;
    std::string offboardDate;
    std::string externalRef;
    std::string links;
    std::string notes;            // JSON
    std::string createdAt;
    std::string updatedAt;

    // ── Display helpers ────────────────────────────────────
    std::string fullName() const { return firstName + " " + lastName; }
    std::string displayName() const {
        return preferredName.empty() ? fullName() : preferredName + " " + lastName;
    }

    // ── CRUD ──────────────────────────────────────────────
    bool save() const;
    bool load(const std::string& id);
    bool remove();
    bool update();

    // ── Factory ───────────────────────────────────────────
    static std::shared_ptr<Person> create(
        const std::string& firstName,
        const std::string& lastName,
        const std::string& email       = "",
        const std::string& personType  = "internal");

    static std::shared_ptr<Person> loadById(const std::string& id);
    static std::shared_ptr<Person> loadByEmail(const std::string& email);
    static std::vector<std::shared_ptr<Person>> loadAll();
    static std::vector<std::shared_ptr<Person>> search(const std::string& nameFragment);

    // ── Reassign ─────────────────────────────────────────
    bool reassignManager(const std::string& newManagerId);
    bool setStatus(const std::string& newStatus);

    // ── Serialisation ─────────────────────────────────────
    nlohmann::json toJson() const;
    static std::shared_ptr<Person> fromJson(const nlohmann::json& j);

private:
    void fromRow(const Row& r);
};

} // namespace Rosenholz

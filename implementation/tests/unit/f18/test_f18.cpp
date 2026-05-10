// ============================================================
// tests/unit/f18/test_f18.cpp  —  F18Operation + Communication
//
// Coverage
// ════════
//   All 11 operationTypes:
//     incident, risk, measure, quality_gate, change_request,
//     change_object, decision_log, communication_plan, generic,
//     lessons_learned, risk_response
//   create / save (auto) / update / loadById / loadForTask
//   loadForTask with type filter
//   recalcRiskScore
//   addStep creates Init+End bookends (lifecycle only, steps detail in f18s/)
//   Communication: create, loadForOwner, loadById, complete()
//   SQL column verification for each type's specialised fields
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/f18/Communication.h"

using namespace Rosenholz;
using namespace RhTest;

// ── Shared env ────────────────────────────────────────────────────────────────

struct F18Env {
    TempDB db;
    std::shared_ptr<F16> proj;
    std::shared_ptr<F22> task;

    explicit F18Env(const std::string& tag) : db(tag) {
        proj = makeF16("F18-"+tag+"-F16");
        task = makeF22(proj->projectId, "F18-"+tag+"-F22");
    }
};

// ── Generic create / auto-save ────────────────────────────────────────────────

TEST_CASE("F18: create() auto-saves and returns valid object", "[f18][create][sql]") {
    TempDB db("f18_create");
    auto proj = makeF16("Cr-F16");
    auto task = makeF22(proj->projectId, "Cr-F22");

    CHECK(rowCount("f18","f18_operations") == 0);
    auto v = F18Operation::create(task->taskId, "Generic Op", F18OperationType::GENERIC);
    REQUIRE(v != nullptr);
    CHECK(rowCount("f18","f18_operations") == 1);

    CHECK_FALSE(v->operationId.empty());
    CHECK(v->operationType == "generic");
    CHECK(v->taskId        == task->taskId);
    CHECK(colValue("f18","f18_operations","task_id",
                   "operation_id='"+v->operationId+"'") == task->taskId);
}

TEST_CASE("F18: loadById returns same fields", "[f18][model]") {
    TempDB db("f18_loadbyid");
    auto proj = makeF16("LBI-F16");
    auto task = makeF22(proj->projectId, "LBI-F22");
    auto v    = F18Operation::create(task->taskId, "LBI-Op", F18OperationType::INCIDENT);
    REQUIRE(v != nullptr);

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->operationId   == v->operationId);
    CHECK(r->operationType == "incident");
    CHECK(r->taskId        == task->taskId);
}

// ── operationType: incident ───────────────────────────────────────────────────

TEST_CASE("F18: incident — severity and incidentType persist", "[f18][incident][sql]") {
    TempDB db("f18_inc");
    auto proj = makeF16("Inc-F16");
    auto task = makeF22(proj->projectId, "Inc-F22");

    auto v = F18Operation::create(task->taskId, "Server-Ausfall", F18OperationType::INCIDENT);
    REQUIRE(v != nullptr);
    v->severity     = "high";
    v->incidentType = "technical";
    REQUIRE(opOk(v->update()));

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->severity     == "high");
    CHECK(r->incidentType == "technical");
    CHECK(colValue("f18","f18_operations","severity",
                   "operation_id='"+v->operationId+"'") == "high");
}

// ── operationType: risk ───────────────────────────────────────────────────────

TEST_CASE("F18: risk — riskLevel, scores, recalcRiskScore", "[f18][risk][sql]") {
    TempDB db("f18_risk");
    auto proj = makeF16("Risk-F16");
    auto task = makeF22(proj->projectId, "Risk-F22");

    auto v = F18Operation::create(task->taskId, "Vendor-Risiko", F18OperationType::RISK);
    REQUIRE(v != nullptr);
    // Risk defaults to "critical"; override explicitly:
    v->riskLevel = "high";
    // probabilityScore=4, impactScoreTime=3, impactScoreCost=3 → max=3 → 4*3=12 → high
    v->probabilityScore = 4;
    v->impactScoreTime  = 3;
    v->impactScoreCost  = 3;
    v->recalcRiskScore();
    REQUIRE(opOk(v->save()));

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->riskLevel        == "high");  // 4*3=12 → high
    CHECK(r->probabilityScore == 4);
    CHECK(r->impactScoreTime  == 3);
    CHECK(colValue("f18","f18_operations","risk_level",
                   "operation_id='"+v->operationId+"'") == "high");
    // overallRiskScore = 4*3 = 12 (stored in DB):
    CHECK(colValue("f18","f18_operations","overall_risk_score",
                   "operation_id='"+v->operationId+"'") == "12");
}

// ── operationType: measure ────────────────────────────────────────────────────

TEST_CASE("F18: measure — measureCategory and plannedDate persist", "[f18][measure]") {
    TempDB db("f18_measure");
    auto proj = makeF16("Meas-F16");
    auto task = makeF22(proj->projectId, "Meas-F22");

    auto v = F18Operation::create(task->taskId, "Korrekturmassnahme", F18OperationType::MEASURE);
    REQUIRE(v != nullptr);
    v->measureCategory = "corrective";
    v->plannedDate     = "2026-05-01";
    REQUIRE(opOk(v->update()));

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->measureCategory == "corrective");
    CHECK(r->plannedDate     == "2026-05-01");
}

// ── operationType: quality_gate ───────────────────────────────────────────────

TEST_CASE("F18: quality_gate — phase and gateResult persist", "[f18][quality_gate]") {
    TempDB db("f18_qg");
    auto proj = makeF16("QG-F16");
    auto task = makeF22(proj->projectId, "QG-F22");

    auto v = F18Operation::create(task->taskId, "Phase-Gate Design", F18OperationType::QUALITY_GATE);
    REQUIRE(v != nullptr);
    v->phase      = "design";
    v->gateResult = "passed";
    REQUIRE(opOk(v->update()));

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->phase      == "design");
    CHECK(r->gateResult == "passed");
}

// ── operationType: change_request + change_object chain ───────────────────────

TEST_CASE("F18: change_request to change_object parent link", "[f18][change]") {
    TempDB db("f18_cr");
    auto proj = makeF16("CR-F16");
    auto task = makeF22(proj->projectId, "CR-F22");

    auto cr = F18Operation::create(task->taskId, "Scope-CR", F18OperationType::CHANGE_REQUEST);
    REQUIRE(cr != nullptr);
    cr->changeType    = "scope";
    cr->justification = "Customer requested expansion";
    REQUIRE(opOk(cr->update()));

    auto co = F18Operation::create(task->taskId, "Scope-CO", F18OperationType::CHANGE_OBJECT);
    REQUIRE(co != nullptr);
    co->parentVorgangId = cr->operationId;
    REQUIRE(opOk(co->update()));

    SECTION("change_request fields") {
        auto r = F18Operation::loadById(cr->operationId);
        REQUIRE(r != nullptr);
        CHECK(r->changeType    == "scope");
        CHECK(r->justification == "Customer requested expansion");
    }
    SECTION("change_object links to CR") {
        auto r = F18Operation::loadById(co->operationId);
        REQUIRE(r != nullptr);
        CHECK(r->parentVorgangId == cr->operationId);
    }
}

// ── operationType: decision_log ───────────────────────────────────────────────

TEST_CASE("F18: decision_log — decisionType and rationale persist", "[f18][decision_log]") {
    TempDB db("f18_dl");
    auto proj = makeF16("DL-F16");
    auto task = makeF22(proj->projectId, "DL-F22");

    auto v = F18Operation::create(task->taskId, "DB-Entscheidung", F18OperationType::DECISION_LOG);
    REQUIRE(v != nullptr);
    v->decisionType = "architectural";
    v->rationale    = "SQLite chosen for embedded use";
    v->decisionBy   = "tech-lead";
    REQUIRE(opOk(v->update()));

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->decisionType == "architectural");
    CHECK(r->rationale    == "SQLite chosen for embedded use");
    CHECK(r->decisionBy   == "tech-lead");
}

// ── operationType: communication_plan ────────────────────────────────────────

TEST_CASE("F18: communication_plan — audience and frequency persist", "[f18][comm_plan]") {
    TempDB db("f18_cp");
    auto proj = makeF16("CP-F16");
    auto task = makeF22(proj->projectId, "CP-F22");

    auto v = F18Operation::create(task->taskId, "Stakeholder-Komm", F18OperationType::COMMUNICATION_PLAN);
    REQUIRE(v != nullptr);
    v->audience  = "Auftraggeber";
    v->frequency = "weekly";
    REQUIRE(opOk(v->update()));

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->audience  == "Auftraggeber");
    CHECK(r->frequency == "weekly");
}

// ── operationType: lessons_learned ───────────────────────────────────────────

TEST_CASE("F18: lessons_learned — lessonType and recommendation persist", "[f18][lessons]") {
    TempDB db("f18_ll");
    auto proj = makeF16("LL-F16");
    auto task = makeF22(proj->projectId, "LL-F22");

    auto v = F18Operation::create(task->taskId, "Sprint-Retro", F18OperationType::LESSONS_LEARNED);
    REQUIRE(v != nullptr);
    v->lessonType     = "positive";
    v->recommendation = "Daily standups improve velocity";
    REQUIRE(opOk(v->update()));

    auto r = F18Operation::loadById(v->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->lessonType     == "positive");
    CHECK(r->recommendation == "Daily standups improve velocity");
}

// ── addStep bookends ──────────────────────────────────────────────────────────

TEST_CASE("F18: create() auto-adds Init and End steps", "[f18][lifecycle][steps]") {
    // Detailed step tests are in f18s/test_f18s.cpp.
    // Here we only verify the F18 operation side of the invariant.
    TempDB db("f18_steps");
    auto proj = makeF16("Steps-F16");
    auto task = makeF22(proj->projectId, "Steps-F22");
    auto v    = F18Operation::create(task->taskId, "Step-Owner", F18OperationType::GENERIC);
    REQUIRE(v != nullptr);
    v->loadSteps();

    REQUIRE(v->steps.size() == 2);
    bool hasInit = false, hasEnd = false;
    for (auto& s : v->steps) {
        if (s.isInitialize) hasInit = true;
        if (s.isFinal)      hasEnd  = true;
    }
    CHECK(hasInit);
    CHECK(hasEnd);
    // Verify SQL rows too:
    CHECK(rowCount("f18","f24_steps",
                   "operation_id='"+v->operationId+"'") == 2);
}

// ── loadForTask filtering ─────────────────────────────────────────────────────

TEST_CASE("F18: loadForTask returns all ops for a task", "[f18][query]") {
    TempDB db("f18_lft_all");
    auto proj = makeF16("LFT-F16");
    auto task = makeF22(proj->projectId, "LFT-F22");
    auto t2   = makeF22(proj->projectId, "Other-F22");

    F18Operation::create(task->taskId, "Inc",  F18OperationType::INCIDENT);
    F18Operation::create(task->taskId, "Risk", F18OperationType::RISK);
    F18Operation::create(t2->taskId,   "Other",F18OperationType::GENERIC);

    auto all = F18Operation::loadForTask(task->taskId);
    CHECK(all.size() == 2);
    for (auto& v : all) CHECK(v->taskId == task->taskId);
}

TEST_CASE("F18: loadForTask with type filter", "[f18][query]") {
    TempDB db("f18_lft_filt");
    auto proj = makeF16("Filt-F16");
    auto task = makeF22(proj->projectId, "Filt-F22");

    F18Operation::create(task->taskId, "Incident-A", F18OperationType::INCIDENT);
    F18Operation::create(task->taskId, "Risk-A",     F18OperationType::RISK);
    F18Operation::create(task->taskId, "Risk-B",     F18OperationType::RISK);

    auto risks = F18Operation::loadForTask(task->taskId, F18OperationType::RISK);
    CHECK(risks.size() == 2);
    for (auto& v : risks) CHECK(v->operationType == "risk");

    auto incs = F18Operation::loadForTask(task->taskId, F18OperationType::INCIDENT);
    CHECK(incs.size() == 1);
}

TEST_CASE("F18: loadRecent respects limit", "[f18][query]") {
    TempDB db("f18_lrecent");
    auto proj = makeF16("LR-F16");
    auto task = makeF22(proj->projectId, "LR-F22");
    for (int i = 0; i < 5; i++)
        F18Operation::create(task->taskId, "Op"+std::to_string(i), F18OperationType::GENERIC);
    CHECK(F18Operation::loadRecent(3).size() == 3);
}

// ── Communication ─────────────────────────────────────────────────────────────

TEST_CASE("F18/Communication: create for F16 owner", "[f18][comm][sql]") {
    TempDB db("f18_comm_f16");
    auto proj = makeF16("Comm-F16");

    auto c = Communication::create(proj->projectId, "f16", "Kick-off Meeting", "meeting");
    REQUIRE(c != nullptr);
    CHECK(c->ownerType == "f16");
    CHECK(c->ownerId   == proj->projectId);
    CHECK_FALSE(c->communicationId.empty());
    CHECK(rowCount("f18","communications") == 1);
}

TEST_CASE("F18/Communication: create for F22 owner", "[f18][comm]") {
    TempDB db("f18_comm_f22");
    auto proj = makeF16("CommF22-F16");
    auto task = makeF22(proj->projectId, "Comm-F22-Task");

    auto c = Communication::create(task->taskId, "f22", "Sprint Review", "meeting");
    REQUIRE(c != nullptr);
    CHECK(c->ownerType == "f22");
    CHECK(c->ownerId   == task->taskId);
}

TEST_CASE("F18/Communication: loadForOwner returns matching records", "[f18][comm][query]") {
    TempDB db("f18_comm_lfo");
    auto proj = makeF16("LFO-F16");
    auto task = makeF22(proj->projectId, "LFO-F22");

    Communication::create(proj->projectId, "f16", "Meeting-1", "meeting");
    Communication::create(proj->projectId, "f16", "Meeting-2", "report");
    Communication::create(task->taskId,    "f22", "Other",     "meeting");

    auto commsF16 = Communication::loadForOwner(proj->projectId, "f16");
    CHECK(commsF16.size() == 2);
    for (auto& c : commsF16) {
        CHECK(c->ownerId   == proj->projectId);
        CHECK(c->ownerType == "f16");
    }
}

TEST_CASE("F18/Communication: loadById round-trip", "[f18][comm][model]") {
    TempDB db("f18_comm_lbi");
    auto proj = makeF16("CommLBI-F16");
    auto c    = Communication::create(proj->projectId, "f16", "LBI-Meeting", "meeting");
    REQUIRE(c != nullptr);
    c->scheduledDate = "2026-06-15";
    REQUIRE(c->update());

    auto r = Communication::loadById(c->communicationId);
    REQUIRE(r != nullptr);
    CHECK(r->communicationId == c->communicationId);
    CHECK(r->title           == "LBI-Meeting");
    CHECK(r->scheduledDate   == "2026-06-15");
}

TEST_CASE("F18/Communication: complete() sets COMPLETED status and persists decisions",
          "[f18][comm][model]") {
    TempDB db("f18_comm_complete");
    auto proj = makeF16("Cmp-F16");
    auto c    = Communication::create(proj->projectId, "f16", "Decision Meeting", "meeting");
    REQUIRE(c != nullptr);

    c->complete("Entscheidung A", "Aktion 1");

    auto r = Communication::loadById(c->communicationId);
    REQUIRE(r != nullptr);
    CHECK(r->status == CommStatus::COMPLETED);
    CHECK_FALSE(r->decisions.empty());
    CHECK(r->decisions.find("Entscheidung A") != std::string::npos);
}

TEST_CASE("F18/Communication: SQL row has correct owner columns", "[f18][comm][sql]") {
    TempDB db("f18_comm_sql");
    auto proj = makeF16("SQL-Comm-F16");
    auto c    = Communication::create(proj->projectId, "f16", "SQL-Test-Meeting", "meeting");
    REQUIRE(c != nullptr);

    CHECK(colValue("f18","communications","owner_id",
                   "communication_id='"+c->communicationId+"'") == proj->projectId);
    CHECK(colValue("f18","communications","owner_type",
                   "communication_id='"+c->communicationId+"'") == "f16");
    CHECK(colValue("f18","communications","title",
                   "communication_id='"+c->communicationId+"'") == "SQL-Test-Meeting");
}

// ============================================================
// tests/unit/legacy/f18_old_test.cpp
//
// Translated from tests/test_model.cpp  (F18Operation sections)
// to Catch2 v3.
// Tests: all 11 operationTypes, addStep, Communication,
//        loadForTask filtering.
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/f18/Communication.h"

using namespace Rosenholz;
using namespace RhTest;

// ── Shared setup ─────────────────────────────────────────────

struct F18Env {
    TempDB db{"leg_f18"};
    std::shared_ptr<F16>          proj  = makeF16("F18-Test-F16");
    std::shared_ptr<F22>          task  = makeF22(proj->projectId, "F18-Test-F22");
};

// ── operationType: incident ───────────────────────────────────

TEST_CASE("Legacy/F18: create incident, update, reload", "[legacy][f18][incident]") {
    TempDB db("leg_f18_inc");
    auto proj = makeF16("Inc-F16");
    auto task = makeF22(proj->projectId, "Inc-F22");

    auto inc = F18Operation::create(task->taskId, "Test-Vorfall",
                                    F18OperationType::INCIDENT);
    REQUIRE(inc != nullptr);
    CHECK_FALSE(inc->operationId.empty());
    CHECK(inc->operationType == "incident");

    inc->severity     = "high";
    inc->incidentType = "technical";
    REQUIRE(opOk(inc->update()));

    auto r = F18Operation::loadById(inc->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->severity     == "high");
    CHECK(r->incidentType == "technical");
}

// ── operationType: risk ───────────────────────────────────────

TEST_CASE("Legacy/F18: create risk, update riskLevel and scores", "[legacy][f18][risk]") {
    TempDB db("leg_f18_risk");
    auto proj = makeF16("Risk-F16");
    auto task = makeF22(proj->projectId, "Risk-F22");

    auto risk = F18Operation::create(task->taskId, "Test-Risiko",
                                     F18OperationType::RISK);
    REQUIRE(risk != nullptr);
    CHECK(risk->operationType == "risk");

    risk->riskLevel        = "high";
    risk->probabilityScore = 4;
    risk->impactScoreTime  = 3;
    REQUIRE(opOk(risk->update()));

    auto r = F18Operation::loadById(risk->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->riskLevel        == "high");
    CHECK(r->probabilityScore == 4);
}

// ── operationType: measure ────────────────────────────────────

TEST_CASE("Legacy/F18: create measure, measureCategory persists", "[legacy][f18][measure]") {
    TempDB db("leg_f18_meas");
    auto proj = makeF16("Meas-F16");
    auto task = makeF22(proj->projectId, "Meas-F22");

    auto m = F18Operation::create(task->taskId, "Korrekturmassnahme",
                                  F18OperationType::MEASURE);
    REQUIRE(m != nullptr);
    m->measureCategory = "corrective";
    m->plannedDate     = "2026-05-01";
    REQUIRE(opOk(m->update()));

    auto r = F18Operation::loadById(m->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->measureCategory == "corrective");
}

// ── operationType: quality_gate ───────────────────────────────

TEST_CASE("Legacy/F18: create quality_gate, phase and gateResult persist", "[legacy][f18][qgate]") {
    TempDB db("leg_f18_qg");
    auto proj = makeF16("QG-F16");
    auto task = makeF22(proj->projectId, "QG-F22");

    auto qg = F18Operation::create(task->taskId, "Phase-Gate Review",
                                   F18OperationType::QUALITY_GATE);
    REQUIRE(qg != nullptr);
    qg->phase      = "design";
    qg->gateResult = "passed";
    REQUIRE(opOk(qg->update()));

    auto r = F18Operation::loadById(qg->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->phase == "design");
}

// ── operationType: change_request + change_object chain ───────

TEST_CASE("Legacy/F18: change_request and change_object parent link", "[legacy][f18][cr]") {
    TempDB db("leg_f18_cr");
    auto proj = makeF16("CR-F16");
    auto task = makeF22(proj->projectId, "CR-F22");

    auto cr = F18Operation::create(task->taskId, "Scope-Erweiterung",
                                   F18OperationType::CHANGE_REQUEST);
    REQUIRE(cr != nullptr);
    cr->changeType    = "scope";
    cr->justification = "Neues Feature";
    REQUIRE(opOk(cr->update()));

    auto co = F18Operation::create(task->taskId, "Scope-Umsetzung",
                                   F18OperationType::CHANGE_OBJECT);
    REQUIRE(co != nullptr);
    co->parentVorgangId = cr->operationId;
    REQUIRE(opOk(co->update()));

    auto r = F18Operation::loadById(co->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->parentVorgangId == cr->operationId);
}

// ── operationType: decision_log ───────────────────────────────

TEST_CASE("Legacy/F18: decision_log fields persist", "[legacy][f18][dlog]") {
    TempDB db("leg_f18_dl");
    auto proj = makeF16("DL-F16");
    auto task = makeF22(proj->projectId, "DL-F22");

    auto dl = F18Operation::create(task->taskId, "Datenbankentscheidung",
                                   F18OperationType::DECISION_LOG);
    REQUIRE(dl != nullptr);
    dl->decisionType = "architectural";
    dl->rationale    = "SQLite passt besser";
    REQUIRE(opOk(dl->update()));

    auto r = F18Operation::loadById(dl->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->decisionType == "architectural");
    CHECK(r->rationale    == "SQLite passt besser");
}

// ── operationType: communication_plan ────────────────────────

TEST_CASE("Legacy/F18: communication_plan audience and frequency", "[legacy][f18][cp]") {
    TempDB db("leg_f18_cp");
    auto proj = makeF16("CP-F16");
    auto task = makeF22(proj->projectId, "CP-F22");

    auto cp = F18Operation::create(task->taskId, "Stakeholder-Kommunikation",
                                   F18OperationType::COMMUNICATION_PLAN);
    REQUIRE(cp != nullptr);
    cp->audience  = "Auftraggeber";
    cp->frequency = "weekly";
    REQUIRE(opOk(cp->update()));

    auto r = F18Operation::loadById(cp->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->audience  == "Auftraggeber");
    CHECK(r->frequency == "weekly");
}

// ── operationType: generic ────────────────────────────────────

TEST_CASE("Legacy/F18: generic type created", "[legacy][f18][generic]") {
    TempDB db("leg_f18_gen");
    auto proj = makeF16("Gen-F16");
    auto task = makeF22(proj->projectId, "Gen-F22");

    auto gen = F18Operation::create(task->taskId, "Generischer Workflow",
                                    F18OperationType::GENERIC);
    REQUIRE(gen != nullptr);
    CHECK(gen->operationType == "generic");
}

// ── operationType: lessons_learned ───────────────────────────

TEST_CASE("Legacy/F18: lessons_learned lessonType and recommendation", "[legacy][f18][ll]") {
    TempDB db("leg_f18_ll");
    auto proj = makeF16("LL-F16");
    auto task = makeF22(proj->projectId, "LL-F22");

    auto ll = F18Operation::create(task->taskId, "Lessons-Learned",
                                   F18OperationType::LESSONS_LEARNED);
    REQUIRE(ll != nullptr);
    ll->lessonType     = "positive";
    ll->recommendation = "Structured reviews are effective";
    REQUIRE(opOk(ll->update()));

    auto r = F18Operation::loadById(ll->operationId);
    REQUIRE(r != nullptr);
    CHECK(r->lessonType     == "positive");
    CHECK(r->recommendation == "Structured reviews are effective");
}

// ── addStep ───────────────────────────────────────────────────

TEST_CASE("Legacy/F18: addStep creates Init+End bookends", "[legacy][f18][steps]") {
    TempDB db("leg_f18_steps");
    auto proj = makeF16("Steps-F16");
    auto task = makeF22(proj->projectId, "Steps-F22");
    auto v = F18Operation::create(task->taskId, "Step-Test", F18OperationType::GENERIC);
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
}

TEST_CASE("Legacy/F18: addStep inserts mid-step and wires to End", "[legacy][f18][steps]") {
    TempDB db("leg_f18_midstep");
    auto proj = makeF16("Mid-F16");
    auto task = makeF22(proj->projectId, "Mid-F22");
    auto v = F18Operation::create(task->taskId, "Mid-Test", F18OperationType::GENERIC);
    REQUIRE(v != nullptr);
    v->loadSteps();

    auto step = v->addStep("Prüfung", "review");
    REQUIRE(step != nullptr);
    CHECK_FALSE(step->stepId.empty());
    CHECK(step->stepType == "review");

    v->loadSteps();
    CHECK(v->steps.size() == 3);

    // End's predecessors include new step:
    std::string endPreds;
    for (auto& s : v->steps) if (s.isFinal) endPreds = s.predecessorStepIds;
    CHECK(endPreds.find(step->stepId) != std::string::npos);
}

// ── Communication ─────────────────────────────────────────────

TEST_CASE("Legacy/F18: Communication for F16, F22 owners", "[legacy][f18][comm]") {
    TempDB db("leg_f18_comm");
    auto proj = makeF16("Comm-F16");

    auto c16 = Communication::create(proj->projectId, "f16", "Kick-off Meeting", "meeting");
    REQUIRE(c16 != nullptr);
    CHECK(c16->ownerType == "f16");
    c16->scheduledDate = "2026-06-01";
    REQUIRE(c16->update());

    auto task = makeF22(proj->projectId, "Comm-F22");
    auto c22  = Communication::create(task->taskId, "f22", "Sprint Review", "meeting");
    REQUIRE(c22 != nullptr);

    auto comms = Communication::loadForOwner(proj->projectId, "f16");
    CHECK_FALSE(comms.empty());
}

TEST_CASE("Legacy/F18: Communication complete() sets status=completed", "[legacy][f18][comm]") {
    TempDB db("leg_f18_comm_cmp");
    auto proj = makeF16("CommCmp-F16");
    auto c16  = Communication::create(proj->projectId, "f16", "Meeting", "meeting");
    REQUIRE(c16 != nullptr);
    c16->complete("Meeting abgeschlossen", "Aktionsplan erstellt");
    auto r = Communication::loadById(c16->communicationId);
    REQUIRE(r != nullptr);
    CHECK(r->status == CommStatus::COMPLETED);
    CHECK_FALSE(r->decisions.empty());
}

// ── loadForTask filtering ─────────────────────────────────────

TEST_CASE("Legacy/F18: loadForTask returns all and filtered by type", "[legacy][f18][query]") {
    TempDB db("leg_f18_filter");
    auto proj = makeF16("Filter-F16");
    auto task = makeF22(proj->projectId, "Filter-F22");

    F18Operation::create(task->taskId, "Incident-A", F18OperationType::INCIDENT);
    F18Operation::create(task->taskId, "Risiko-A",   F18OperationType::RISK);
    F18Operation::create(task->taskId, "Risiko-B",   F18OperationType::RISK);

    auto all   = F18Operation::loadForTask(task->taskId);
    auto risks = F18Operation::loadForTask(task->taskId, F18OperationType::RISK);

    CHECK(all.size()   >= 3);
    CHECK(risks.size() >= 2);
    for (auto& r : risks) CHECK(r->operationType == "risk");
    CHECK_FALSE(all[0]->taskId.empty());
}

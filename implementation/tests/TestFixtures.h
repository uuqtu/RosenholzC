#pragma once
// ============================================================
// TestFixtures.h  —  Test Fixture Pattern for Rosenholz PM
//
// Each fixture creates self-contained test data and can
// optionally clean up after itself. Tests are fully independent.
// ============================================================
#include "../src/model/person/Person.h"
#include "../src/model/team/Team.h"
#include "../src/model/f16/ProjectF16.h"
#include "../src/model/f22/TaskF22.h"
#include "../src/model/dok/Document.h"
#include "../src/model/f18/F18Operation.h"
#include "../src/model/f18/F18OperationStep.h"
#include "../src/workflow/F77Workflow.h"
#include <memory>
#include <vector>
#include <string>

namespace Rosenholz { namespace Test {

// ── Base fixture ─────────────────────────────────────────────
struct Fixture {
    std::vector<std::string> createdIds;  // tracked for cleanup

    virtual ~Fixture() = default;

    void trackId(const std::string& id) {
        if (!id.empty()) createdIds.push_back(id);
    }
};

// ── Person fixture ────────────────────────────────────────────
struct PersonFixture : Fixture {
    std::shared_ptr<Person> person;

    explicit PersonFixture(const std::string& first    = "Test",
                           const std::string& last     = "Person",
                           const std::string& email    = "test@fixture.de",
                           const std::string& type     = "internal")
    {
        person = Person::create(first, last, email, type);
        person->roleTitle   = "Test Role";
        person->department  = "Test Dept";
        person->dayRate     = 800.0;
        person->save();
        trackId(person->personId);
    }
};

// ── Team fixture ──────────────────────────────────────────────
struct TeamFixture : Fixture {
    std::shared_ptr<Team>   team;
    std::shared_ptr<Person> lead;

    explicit TeamFixture(const std::string& name = "Test Team",
                         const std::string& type = "engineering")
    {
        lead = Person::create("Team","Lead","lead@fixture.de","internal");
        lead->save();
        trackId(lead->personId);

        team = Team::create(name, type);
        team->leadId = lead->personId;
        team->save();
        trackId(team->teamId);
    }
};

// ── Project fixture ───────────────────────────────────────────
struct ProjectFixture : Fixture {
    std::shared_ptr<ProjectF16> project;
    std::shared_ptr<Person>     lead;
    std::shared_ptr<Team>       team;

    explicit ProjectFixture(const std::string& title   = "Fixture-Vorgang",
                            const std::string& type    = "OV",
                            const std::string& size    = "medium")
    {
        lead = Person::create("Projekt","Leiter","pl@fixture.de","internal");
        lead->save();
        trackId(lead->personId);

        team = Team::create("Fixture Team","engineering");
        team->save();
        trackId(team->teamId);

        project = ProjectF16::create(title, type, size);
        project->leadId       = lead->personId;
        project->ownerTeamId  = team->teamId;
        project->budgetPlanned= 100000.0;
        project->startDatePlanned = "2025-01-01";
        project->endDatePlanned   = "2025-12-31";
        project->save();
        trackId(project->projectId);
    }
};

// ── Task fixture (requires project) ──────────────────────────
struct TaskFixture : Fixture {
    std::shared_ptr<TaskF22>    task;
    std::shared_ptr<Person>     assignee;
    ProjectFixture              projFix;

    explicit TaskFixture(const std::string& title = "Fixture-Aufgabe")
        : projFix()
    {
        assignee = Person::create("Task","Bearbeiter","tb@fixture.de","internal");
        assignee->save();
        trackId(assignee->personId);

        task = TaskF22::create(projFix.project->projectId, title,
                               assignee->personId, "");
        task->wbsCode          = "1.1";
        task->priority         = "high";
        task->startDatePlanned = "2025-01-15";
        task->dueDatePlanned   = "2025-03-31";
        task->effortPlannedHrs = 40.0;
        task->save();
        trackId(task->taskId);
    }
};

// ── Full project fixture (project + multiple tasks + team) ────
struct FullProjectFixture : Fixture {
    std::shared_ptr<ProjectF16>  project;
    std::shared_ptr<Person>      lead;
    std::shared_ptr<Person>      member1;
    std::shared_ptr<Person>      member2;
    std::shared_ptr<Team>        team;
    std::shared_ptr<TaskF22>     task1;
    std::shared_ptr<TaskF22>     task2;
    std::shared_ptr<TaskF22>     childTask;
    std::shared_ptr<Rosenholz::F18Operation> incident;  // vorgangType=incident
    std::shared_ptr<Rosenholz::F18Operation> risk;      // vorgangType=risk

    FullProjectFixture() {
        lead    = Person::create("Full","Leiter","fl@fixture.de","internal");
        member1 = Person::create("Full","Member1","fm1@fixture.de","internal");
        member2 = Person::create("Full","Member2","fm2@fixture.de","external");
        for (auto& p : {lead, member1, member2}) { p->save(); trackId(p->personId); }

        team = Team::create("Full Fixture Team","cross-functional");
        team->leadId = lead->personId;
        team->save();
        trackId(team->teamId);
        team->addMember(member1->personId, "developer");
        team->addMember(member2->personId, "qa");

        project = ProjectF16::create("Full Fixture Vorgang","OV","large");
        project->leadId       = lead->personId;
        project->ownerTeamId  = team->teamId;
        project->budgetPlanned= 500000.0;
        project->earnedValue  = 250000.0;
        project->plannedValue = 300000.0;
        project->actualCost   = 260000.0;
        project->recalcEarnedValue();
        project->save();
        trackId(project->projectId);

        task1 = TaskF22::create(project->projectId,"Analyse",member1->personId,"");
        task1->wbsCode="1.1"; task1->effortPlannedHrs=80.0; task1->save();
        trackId(task1->taskId);

        task2 = TaskF22::create(project->projectId,"Implementierung",member1->personId,"");
        task2->wbsCode="1.2"; task2->effortPlannedHrs=120.0; task2->save();
        trackId(task2->taskId);

        childTask = TaskF22::create(project->projectId,"Teilaufgabe",member2->personId,
                                    task2->taskId);
        childTask->wbsCode="1.2.1"; childTask->save();
        trackId(childTask->taskId);

        incident = Rosenholz::F18Operation::create(
            project->projectId, "Fixture-Vorfall",
            Rosenholz::F18OperationType::INCIDENT);
        if (incident) { incident->severity="high"; incident->ownerId=member2->personId;
                         incident->update(); trackId(incident->vorgangId); }

        risk = Rosenholz::F18Operation::create(
            project->projectId, "Fixture-Risiko",
            Rosenholz::F18OperationType::RISK);
        if (risk) { risk->probabilityScore=3; risk->impactScoreTime=4;
                     risk->impactScoreCost=4; risk->recalcRiskScore();
                     trackId(risk->vorgangId); }
    }
};

// ── Workflow fixture ──────────────────────────────────────────
struct WorkflowFixture : Fixture {
    ProjectFixture projFix;
    std::shared_ptr<F77_WorkflowTemplate> templ;
    std::shared_ptr<F77_Workflow>         instance;

    WorkflowFixture() : projFix("WF-Fixture-Project") {
        templ = F77_WorkflowTemplate::create("Fixture-Workflow","released","f16");
        templ->save();
        auto init = templ->addTemplateStep("Init","sequential",true,false); init.save();
        auto step = templ->addTemplateStep("Pruefung","sequential",false,false);
        step.predecessorTplStepIds = init.tplStepId; step.save();
        auto fin  = templ->addTemplateStep("End","sequential",false,true);
        fin.predecessorTplStepIds = step.tplStepId; fin.autoApprove = true; fin.save();
        instance = F77_Engine::startFromTemplate(
            templ->templateId, "f16", projFix.project->projectId, "fixture");
    }
};

}} // namespace

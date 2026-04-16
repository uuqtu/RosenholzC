#pragma once
// ============================================================
// cli_common.h  —  Shared CLI utilities and forward declarations
// ============================================================
#include "../model/ProjectF16.h"
#include "../model/TaskF22.h"
#include "../model/Document.h"
#include "../model/Person.h"
#include "../model/Team.h"
#include "../model/Milestone.h"
#include "../model/f18/F18Workflow.h"
#include "../model/f18/F18WorkflowStep.h"
#include "../model/f18/Communication.h"
#include "../workflow/WorkflowEngine.h"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <memory>

namespace CLI {

// ── Input helpers (defined in Utilities.cpp) ──────────────
std::string readLine(const std::string& prompt);
std::string readOpt(const std::string& prompt);
int  readInt(const std::string& prompt, int lo, int hi);
bool yesno(const std::string& prompt);
std::string fval(const std::string& v);
std::string fdate(const std::string& d);
void hdr(const std::string& title);
void hr();

// ── Forward menu declarations ──────────────────────────────
void projectMenu(std::shared_ptr<Rosenholz::ProjectF16>);
void taskMenu(std::shared_ptr<Rosenholz::TaskF22>);
void documentMenu(std::shared_ptr<Rosenholz::Document>);
void documentBrowserMenu(const std::string& projectId = "",
                         const std::string& taskId    = "");
void workflowMenu();
void instanceMenu(const std::string& instanceId);
void milestoneMenu(const std::string& projectId);
void communicationMenu(const std::string& ownerId,
                       const std::string& ownerType);
void meetingMenu(const std::string& taskId, const std::string& projectId = "");
void measureMenu(const std::string& projectId);

std::shared_ptr<Rosenholz::Document> createDocumentWizard(
    const std::string& projectId = "",
    const std::string& taskId    = "");
std::string startWfInstanceWizard(const std::string& entityType = "",
                                   const std::string& entityId   = "");
void listWfInstances(const std::string& entityType = "",
                     const std::string& entityId   = "");
void run();

// Print/list helpers (defined in Utilities.cpp and PersonMenu.cpp)
void printDocument(const Rosenholz::Document&);
void listDocuments(const std::vector<std::shared_ptr<Rosenholz::Document>>& docs, const std::string& title = "DOCUMENTS");
void printPerson(const Rosenholz::Person&);
void listPersons();
void listProjects();
void printProject(const Rosenholz::ProjectF16&);
std::shared_ptr<Rosenholz::Person>      createPersonWizard();
std::shared_ptr<Rosenholz::ProjectF16>  createProjectWizard();
std::shared_ptr<Rosenholz::TaskF22>     createTaskWizard(const std::string& projectId);
std::shared_ptr<Rosenholz::F18Workflow> createF18Wizard(
    const std::string& projectId,
    const std::string& taskId = "",
    const std::string& type   = "");

// Additional shared display/list functions (defined in Utilities.cpp)
void listTasks(const std::string& projectId);
void printTask(const Rosenholz::TaskF22&);

// Global search across all entity categories
void globalSearch(const std::string& query);

// Show last 20 items across all entity types (for link/attach dialogs)
void showRecentItems(const std::string& filter = "");

// F18 Workflow display
void printF18Workflow(const Rosenholz::F18Workflow&);

// F18 Workflow menu
void f18Menu(std::shared_ptr<Rosenholz::F18Workflow> v);
void f18BrowserMenu(const std::string& projectId = "",
                    const std::string& taskId = "");

// Universal document attachment dialog
std::shared_ptr<Rosenholz::Document> attachDocumentDialog(
    const std::string& projectId = "", const std::string& taskId = "");

// Incident detail

// Team menu
void teamMenu();

// New menus
void changeRequestMenu(const std::string& projectId);
void riskMenu(const std::string& projectId);
void qualityGateMenu(const std::string& projectId);

} // namespace CLI

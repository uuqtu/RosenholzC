// ============================================================
// cli_tasks.cpp — "Meine Aufgaben" — F77Task browser
//
// Surfaces workflow-spawned tasks that require manual decisions.
// Each task carries full navigation context; the user can act
// directly without leaving this menu.
// ============================================================
#include "cli_common.h"
#include "../workflow/F77Task.h"
#include "../model/f22/F22.h"
#include "../model/akt/Folder.h"
#include "../model/akt/FolderObject.h"
#include "../model/akt/FolderRevision.h"
#include "../core/FileOps.h"
#include <iomanip>

namespace CLI {

using namespace Rosenholz;

// ── Display helpers ───────────────────────────────────────────────────────
static void printTaskList(const std::vector<std::shared_ptr<F77Task>>& tasks,
                           const std::string& heading)
{
    std::cout << "\n  " << heading << " (" << tasks.size() << "):\n";
    if (tasks.empty()) { std::cout << "  (keine)\n\n"; return; }
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(8)  << "STATUS"
              << std::setw(28) << "ENTITÄT (TYP/ID)"
              << std::setw(28) << "ENTITÄT-TITEL"
              << "AUFGABE\n"
              << "  " << std::string(90, '-') << "\n";
    int n = 1;
    for (auto& t : tasks) {
        // Resolve the entity title for context
        std::string entityTitle;
        if (t->targetEntityType == "f22") {
            auto e = Rosenholz::F22::loadById(t->targetEntityId);
            if (e) entityTitle = e->title.substr(0, 26);
        } else if (t->targetEntityType == "akt") {
            auto e = Rosenholz::Folder::loadById(t->targetEntityId);
            if (e) entityTitle = e->title.substr(0, 26);
        } else if (t->targetEntityType == "f18") {
            auto e = Rosenholz::F18Operation::loadById(t->targetEntityId);
            if (e) entityTitle = e->title.substr(0, 26);
        }

        std::string ent = t->targetEntityType + ":" + t->targetEntityId.substr(0, 20);
        std::string statusStr = t->status == "open" ? "offen" :
                                t->status == "completed" ? "erledigt" :
                                t->status == "skipped" ? "überspr." : t->status;

        std::cout << "  " << std::setw(4) << n++
                  << std::left
                  << std::setw(8)  << statusStr
                  << std::setw(28) << ent
                  << std::setw(28) << (entityTitle.empty() ? "—" : entityTitle)
                  << t->title.substr(0, 38) << "\n";
        // Show what action is expected
        if (!t->targetAction.empty())
            std::cout << "       Aktion:  " << t->targetAction << "\n";
        if (!t->fileName.empty()) {
            std::cout << "       Datei:   " << t->fileName << "\n";
            if (!t->filePath.empty())
                std::cout << "       Pfad:    " << t->filePath << "\n";
        }
        // Show workflow source
        if (!t->workflowId.empty())
            std::cout << "       Workflow: " << t->workflowId.substr(0, 26) << "\n";
        std::cout << "\n";
    }
}

// ── Handle a single F77Task interactively ────────────────────────────────
// Business logic: navigate to the target entity, perform the required action,
// then mark the task complete/skipped.
static void executeTask(std::shared_ptr<F77Task> task) {
    hdr("F77-AUFGABE — " + task->taskId);
    std::cout << "  Titel   : " << task->title << "\n"
              << "  Status  : " << task->status << "\n"
              << "  Typ     : " << task->targetEntityType
              << " / " << task->targetEntityId << "\n"
              << "  Aktion  : " << (task->targetAction.empty() ? "—" : task->targetAction) << "\n";
    if (!task->fileName.empty())
        std::cout << "  Datei   : " << task->fileName << "\n"
                  << "  Pfad    : " << task->filePath << "\n";

    if (!task->isOpen()) {
        std::cout << "  >> Aufgabe bereits abgeschlossen (" << task->status << ").\n";
        return;
    }

    // ── Nacherfassen: unregistered file → assign to existing/new Akte ────
    if (task->targetAction == "nacherfassen" && !task->filePath.empty()) {
        std::cout << "\n  Optionen fuer '" << task->fileName << "':\n"
                  << "  1. Neue Akte anlegen und Datei hinzufuegen\n"
                  << "  2. Vorhandener Akte hinzufuegen\n"
                  << "  3. Datei ignorieren (Task ueberspringen)\n"
                  << "  0. Spaeter entscheiden (Task offen lassen)\n";
        int ch = readInt("Wahl", 0, 3);
        if (ch == 0) return;
        if (ch == 3) { task->skip("Datei ignoriert"); std::cout << "  >> Uebersprungen.\n"; return; }

        // Route by entity type:
        std::shared_ptr<Folder> targetDoc;

        if (task->targetEntityType == "akt") {
            // File found in an AKT revision folder — add directly to that AKT
            targetDoc = Folder::loadById(task->targetEntityId);
            if (!targetDoc) {
                std::cout << "  >> Akte nicht gefunden: " << task->targetEntityId << "\n";
                return;
            }
            std::cout << "  Akte: " << targetDoc->folderId << " — " << targetDoc->title << "\n";
        } else {
            // F22 (or F16/F18): load/create an AKT under that entity
            auto f22 = F22::loadById(task->targetEntityId);
            if (!f22) {
                std::cout << "  >> Entität nicht gefunden: " << task->targetEntityId << "\n";
                return;
            }
            if (ch == 1) {
                std::string stem = task->fileName;
                auto dot = stem.rfind('.'); if (dot != std::string::npos) stem = stem.substr(0,dot);
                std::string title = readOpt("  Titel (leer=" + stem + "): ");
                if (title.empty()) title = stem;
                targetDoc = Folder::create(title, "other", task->targetEntityId);
                if (!opOk(targetDoc->save())) { std::cout << "  >> Fehler.\n"; return; }
                std::cout << "  >> Akte angelegt: " << targetDoc->folderId << "\n";
            } else {
                auto docs = Folder::loadForEntity("f22", task->targetEntityId);
                if (docs.empty()) { std::cout << "  (keine Akten vorhanden)\n"; return; }
                for (size_t i=0; i<docs.size(); ++i)
                    std::cout << "  " << std::setw(3) << (i+1) << ". "
                              << docs[i]->folderId << "  " << docs[i]->title << "\n";
                int pick = readInt("Akte #", 1, (int)docs.size());
                targetDoc = docs[pick-1];
            }
        }

        // Ensure inWork revision:
        auto cur = FolderRevision::currentRevision(targetDoc->folderId);
        if (!cur) {
            cur = FolderRevision::createRevision(targetDoc->folderId, 1);
            if (!cur) { std::cout << "  >> Revision konnte nicht angelegt werden.\n"; return; }
        }
        if (cur->revState != RevState::IN_WORK) {
            std::cout << "  >> Akte nicht in_work.\n"; return;
        }

        // Import:
        std::string label = readOpt("  Bezeichnung (leer=Dateiname): ");
        std::string desc  = readOpt("  Beschreibung: ");
        OperationResult res = OperationResult::OPERATION_ACK;
        auto obj = FolderObject::importFile(
            targetDoc->folderId, cur->rev, task->filePath, res, label, desc);
        if (opOk(res) && obj) {
            std::cout << "  >> Importiert: " << obj->displayName() << "\n";
            task->complete("Datei einer Akte zugewiesen: " + targetDoc->folderId);
            std::cout << "  >> Aufgabe abgeschlossen.\n";
        } else {
            std::cout << "  >> " << opResultMessage(res) << "\n";
        }
        return;
    }

    // ── Generic task: review/approve ─────────────────────────────────────
    std::cout << "\n  1. Aufgabe abschliessen\n"
              << "  2. Ueberspringen\n"
              << "  0. Spaeter\n";
    int ch = readInt("Wahl", 0, 2);
    if (ch == 0) return;
    std::string note = readOpt("  Notiz (optional): ");
    if (ch == 1)      task->complete(note);
    else              task->skip(note);
    std::cout << "  >> " << task->status << ".\n";
}

// ── cmdTasks ─────────────────────────────────────────────────────────────
void cmdTasks(const std::vector<std::string>& args) {
    // No args: show open tasks
    auto open = F77Task::loadOpen();

    if (args.empty() || args[0] == "-o" || args[0] == "--open") {
        printTaskList(open, "Offene F77-Aufgaben");
        if (open.empty()) return;

        std::cout << "  Aufgabe oeffnen? (Nummer oder 0): ";
        int pick = readInt("Wahl", 0, (int)open.size());
        if (pick > 0) executeTask(open[pick-1]);
        return;
    }

    // -a / --all: show all tasks
    if (args[0] == "-a" || args[0] == "--all") {
        auto all = F77Task::loadAll(200);
        printTaskList(all, "Alle F77-Aufgaben");
        return;
    }

    // Direct ID:
    auto task = F77Task::loadById(args[0]);
    if (task) { executeTask(task); return; }

    printErr("Unbekanntes Argument: " + args[0]);
}

} // namespace CLI

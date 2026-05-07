// ============================================================
// cli_nav.cpp  —  Linux-style navigation (cf / ls / lo)
//
// cd <id>   → navigate into entity (push nav stack)
// ..        → one level up (pop nav stack)  [handled in main_cli.cpp]
// ls [opts] → list children of current entity
// lo        → context-sensitive option list (-h also routes here)
//
// Context-aware command wrappers:
//   f22 -n  / f18 -n / akt -n   → use current context as parent
//   f77 -s  → start workflow on current entity
//   f77 -d  → show workflow on current entity
//   rev     → revise current AKT
// ============================================================
#include "cli_common.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/f18/F18Operation.h"
#include "../model/f18/F18OperationStep.h"
#include "../model/akt/Folder.h"
#include "../model/akt/FolderRevision.h"
#include "../model/akt/FolderObject.h"
#include "../model/person/Person.h"
#include "../workflow/F77Workflow.h"
#include "../workflow/F77Task.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include <iomanip>
#include <sstream>

namespace CLI {

using namespace Rosenholz;

// ── Forward declarations (defined in other CLI modules) ─────────────────────
void projectMenu(std::shared_ptr<F16> p);
void taskMenu(std::shared_ptr<F22> t);
void f18Menu(std::shared_ptr<F18Operation> v);
void documentMenu(std::shared_ptr<Folder> d);
void instanceMenu(const std::string& wfId);
std::string startWfInstanceWizard(const std::string& entityType,
                                   const std::string& entityId);
std::shared_ptr<F22>    createTaskWizard(const std::string& projectId);
std::shared_ptr<Folder> createDocumentWizard(const std::string& projectId,
                                              const std::string& taskId);
std::shared_ptr<F18Operation> createF18Wizard(const std::string& projectId,
                                               const std::string& taskId,
                                               const std::string& type);
void communicationMenu(const std::string& ownerId, const std::string& ownerType);

// ── Helpers ─────────────────────────────────────────────────────────────────

static void showF16Children(const std::string& projectId) {
    auto tasks = F22::loadForProject(projectId);
    auto akten = Folder::loadForEntity("f16", projectId);

    if (!tasks.empty()) {
        std::cout << "\n  F22-Vorgaenge (" << tasks.size() << "):\n"
                  << "  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(34) << "TITEL"
                  << "STATUS\n"
                  << "  " << std::string(72,'-') << "\n";
        int n = 1;
        for (auto& t : tasks)
            std::cout << "  " << std::setw(4) << n++
                      << std::setw(26) << t->regNumber.toString()
                      << std::setw(34) << t->title.substr(0,32)
                      << Color::statusColor(entityStatusToString(t->status)) << "\n";
    } else {
        std::cout << "  (keine F22-Vorgaenge)\n";
    }

    if (!akten.empty()) {
        std::cout << "\n  Akten (" << akten.size() << "):\n"
                  << "  " << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(34) << "TITEL"
                  << "TYP\n"
                  << "  " << std::string(68,'-') << "\n";
        int n = 1;
        for (auto& d : akten)
            std::cout << "  " << std::setw(4) << n++
                      << std::setw(26) << d->folderId
                      << std::setw(34) << d->title.substr(0,32)
                      << d->docType << "\n";
    }
    std::cout << "\n";
}

static void showF22Children(const std::string& taskId) {
    auto f18s  = F18Operation::loadForTask(taskId);
    auto akten = Folder::loadForEntity("f22", taskId);

    if (!f18s.empty()) {
        std::cout << "\n  F18-Vorgaenge (" << f18s.size() << "):\n"
                  << "  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(12) << "TYP"
                  << std::setw(30) << "TITEL"
                  << "STATUS\n"
                  << "  " << std::string(76,'-') << "\n";
        int n = 1;
        for (auto& v : f18s)
            std::cout << "  " << std::setw(4) << n++
                      << std::setw(26) << v->operationId
                      << std::setw(12) << v->operationType.substr(0,10)
                      << std::setw(30) << v->title.substr(0,28)
                      << Color::statusColor(entityStatusToString(v->status)) << "\n";
    } else {
        std::cout << "\n  (keine F18-Vorgaenge)\n";
    }

    if (!akten.empty()) {
        std::cout << "\n  Akten (" << akten.size() << "):\n"
                  << "  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(34) << "TITEL"
                  << "TYP\n"
                  << "  " << std::string(68,'-') << "\n";
        int n = 1;
        for (auto& d : akten)
            std::cout << "  " << std::setw(4) << n++
                      << std::setw(26) << d->folderId
                      << std::setw(34) << d->title.substr(0,32)
                      << d->docType << "\n";
    } else {
        std::cout << "\n  (keine Akten)\n";
    }
    std::cout << "\n";
}

// ── ls — list folder (children of current location) ─────────────────────────

void cmdLs(const std::vector<std::string>& args) {
    auto& nav = NavigationStack::instance();
    auto cur = nav.current();

    bool showRevisions = false;
    for (auto& a : args) if (a == "-rev") showRevisions = true;

    if (!cur.valid()) {
        // Top level: all F16
        auto all = F16::loadAll();
        if (all.empty()) { std::cout << "  (keine F16-Karten)\n  f16 -n  anlegen\n"; return; }
        std::cout << "\n  F16-Karten (" << all.size() << "):\n"
                  << "  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(36) << "TITEL"
                  << "STATUS\n"
                  << "  " << std::string(72,'-') << "\n";
        int n = 1;
        for (auto& p : all)
            std::cout << "  " << std::setw(4) << n++
                      << std::setw(26) << p->regNumber.toString()
                      << std::setw(36) << p->title.substr(0,34)
                      << (p->archived ? Color::dim("archiviert") : Color::green("aktiv")) << "\n";
        std::cout << "\n  cd <ID>  navigieren  |  f16 -n  neue Kartei\n\n";
        return;
    }

    switch (cur.type) {
    case EntityType::F16: {
        auto p = F16::loadById(cur.id);
        if (!p) { printErr("F16 nicht gefunden: " + cur.id); return; }
        std::cout << "\n  " << Color::bold("F16") << " " << Color::cyan(cur.id)
                  << " — " << p->title << "\n";
        showF16Children(p->projectId);
        break;
    }
    case EntityType::F22: {
        auto t = F22::loadById(cur.id);
        if (!t) { printErr("F22 nicht gefunden: " + cur.id); return; }
        std::cout << "\n  " << Color::bold("F22") << " " << Color::blue(cur.id)
                  << " — " << t->title << "\n"
                  << "  Status: " << Color::statusColor(entityStatusToString(t->status))
                  << "  Prio: " << t->priority << "\n";
        showF22Children(t->taskId);
        break;
    }
    case EntityType::F18: {
        auto v = F18Operation::loadById(cur.id);
        if (!v) { printErr("F18 nicht gefunden: " + cur.id); return; }
        v->loadSteps();  // always load steps before listing
        std::cout << "\n  " << Color::bold("F18") << " " << Color::magenta(cur.id)
                  << " — " << v->title << "\n"
                  << "  Typ: " << v->operationType
                  << "  Status: " << Color::statusColor(entityStatusToString(v->status)) << "\n\n";

        // Steps
        if (!v->steps.empty()) {
            std::cout << "  Schritte (" << v->steps.size() << "):\n"
                      << "  " << std::left << std::setw(4) << "#"
                      << std::setw(26) << "ID"
                      << std::setw(24) << "TITEL"
                      << std::setw(12) << "START-PLAN"
                      << std::setw(12) << "ENDE-PLAN"
                      << "STATUS\n"
                      << "  " << std::string(78,'-') << "\n";
            int n = 1;
            for (auto& s : v->steps) {
                std::string sym = f18StepSymbolStr(s.displaySymbol());
                std::cout << "  " << std::setw(4) << n++
                          << std::setw(26) << s.stepId
                          << std::setw(24) << s.title.substr(0,22)
                      << std::setw(12) << (s.startDatePlanned.empty() ? "-" : s.startDatePlanned.substr(0,10))
                      << std::setw(12) << (s.endDatePlanned.empty() ? "-" : s.endDatePlanned.substr(0,10));
                if (s.status == F18StepStatus::DONE)
                    std::cout << Color::green(sym);
                else if (s.status == F18StepStatus::IN_PROGRESS)
                    std::cout << Color::yellow(sym);
                else
                    std::cout << Color::dim(sym);
                std::cout << "\n";
            }
        }
        // Akten under this F18
        auto akten = Folder::loadForEntity("f18", v->operationId);
        if (!akten.empty()) {
            std::cout << "\n  Akten (" << akten.size() << "):\n";
            int n = 1;
            for (auto& d : akten)
                std::cout << "  " << std::setw(4) << n++
                          << std::setw(26) << d->folderId
                          << "  " << d->title.substr(0,32) << "\n";
        }
        std::cout << "\n";
        break;
    }
    case EntityType::AKT: {
        auto d = Folder::loadById(cur.id);
        if (!d) { printErr("Akte nicht gefunden: " + cur.id); return; }
        auto rev = FolderRevision::currentRevision(d->folderId);
        std::cout << "\n  " << Color::bold("AKT") << " " << Color::yellow(cur.id)
                  << " — " << d->title << "\n";
        if (rev)
            std::cout << "  Rev:" << rev->rev
                      << "  Status:" << Color::statusColor(revStateToString(rev->revState))
                      << (rev->revState == RevState::IN_WORK ? "  (bearbeitbar)" : "  (unveraenderlich)")
                      << "\n";

        if (showRevisions) {
            auto allRevs = FolderRevision::loadAllRevisions(d->folderId);
            std::cout << "\n  Alle Revisionen (" << allRevs.size() << "):\n"
                      << "  " << std::setw(6) << "REV"
                      << std::setw(18) << "STATUS"
                      << "ERSTELLT\n"
                      << "  " << std::string(50,'-') << "\n";
            for (auto& r : allRevs)
                std::cout << "  " << std::setw(6) << r->rev
                          << std::setw(18) << Color::statusColor(revStateToString(r->revState))
                          << r->createdAt.substr(0,16) << "\n";
        }

        if (rev) {
            auto objs = FolderObject::loadForRevision(d->folderId, rev->rev);
            std::cout << "\n  Objekte Rev " << rev->rev << " (" << objs.size() << "):\n";
            if (!objs.empty()) {
                std::cout << "  " << std::left << std::setw(4) << "#"
                          << std::setw(20) << "OBJ-ID"
                          << std::setw(34) << "NAME"
                          << std::setw(8) << "FORMAT"
                          << "STATUS\n"
                          << "  " << std::string(72,'-') << "\n";
                int n = 1;
                for (auto& o : objs)
                    std::cout << "  " << std::setw(4) << n++
                              << std::setw(20) << o->objectId.substr(0,18)
                              << std::setw(34) << o->displayName().substr(0,32)
                              << std::setw(8) << o->format.substr(0,6)
                              << (o->committed ? Color::green("committed") : Color::yellow("in_work"))
                              << "\n";
            } else {
                std::cout << "  (keine Objekte in aktueller Revision)\n";
            }
        }
        std::cout << "\n  Tipp: ls -rev  zeigt alle Revisionen\n\n";
        break;
    }
    default:
        std::cout << "  (keine Unterobjekte fuer diesen Typ)\n";
    }
}

// ── lo — list options (context-sensitive help) ────────────────────────────────

void cmdLo(const std::vector<std::string>& args) {
    (void)args;
    auto& nav = NavigationStack::instance();
    auto cur = nav.current();

    std::cout << "\n";

    if (!cur.valid()) {
        std::cout
            << Color::bold("  Rosenholz PM — Befehle (oberste Ebene)") << "\n"
            << "  " << std::string(54,'-') << "\n"
            << "  Navigation:\n"
            << "    cd <F16-ID>    In F16-Kartei navigieren\n"
            << "    ls             Alle F16-Karten listen\n"
            << "    ..             (hier: nichts)\n"
            << "  Erstellen:\n"
            << "    f16 -n         Neue F16-Kartei\n"
            << "    f22 -n         Neue F22-Aufgabe (fragt nach F16)\n"
            << "    f18 -n         Neuen F18-Vorgang (fragt nach F22)\n"
            << "    akt -n         Neue Akte (fragt nach Entitaet)\n"
            << "    per -n         Neue Person\n"
            << "  Suchen/Listen:\n"
            << "    f16 -s <q>     F16 suchen\n"
            << "    f22 -s <q>     F22 suchen\n"
            << "    f18 -s <q>     F18 suchen\n"
            << "    akt -s <q>     Akte suchen\n"
            << "    tsk            Offene F77-Aufgaben\n"
            << "    srch <q>       Globale Suche\n"
            << "    tree           Hierarchiebaum\n"
            << "    cal            Kalenderansicht\n"
            << "    his            Verlauf\n"
            << "  System:\n"
            << "    sts            Datenbankzaehler\n"
            << "    bak            Backup starten\n"
            << "    wch [N]        Watch-Polling (N=Sekunden)\n"
            << "    exit           Beenden\n\n";
        return;
    }

    switch (cur.type) {

    case EntityType::F16: {
        auto p = F16::loadById(cur.id);
        std::string t = p ? p->title : cur.id;
        std::cout << Color::bold("  F16 " + cur.id + " — " + t) << "\n"
                  << "  " << std::string(54,'-') << "\n"
                  << "  Navigation:\n"
                  << "    cd <F22-ID>    In F22-Vorgang navigieren\n"
                  << "    cd <AKT-ID>    In Akte navigieren\n"
                  << "    ls             Alle F22 und Akten listen\n"
                  << "    ..             Zurueck (oberste Ebene)\n"
                  << "  Erstellen:\n"
                  << "    f22 -n         Neue F22-Aufgabe in diesem Projekt\n"
                  << "    akt -n         Neue Akte zu diesem Projekt\n"
                  << "    kom -n         Neue Kommunikation\n"
                  << "  Bearbeiten:\n"
                  << "    f16 -e         F16-Felder bearbeiten\n"
                  << "    f16 -v         F16-Details anzeigen\n"
                  << "    f16 -arc       Projekt archivieren\n"
                  << "  Suchen:\n"
                  << "    f22 -s <q>     F22-Aufgaben suchen\n"
                  << "    akt -s <q>     Akten suchen\n"
                  << "    note <Text>    Schnellnotiz zu diesem Projekt\n"
                  << "    tree           Hierarchiebaum ab hier\n\n";
        break;
    }

    case EntityType::F22: {
        auto t = F22::loadById(cur.id);
        std::string title = t ? t->title : cur.id;
        std::string wfi   = t ? t->releaseWorkflowId : "";
        std::cout << Color::bold("  F22 " + cur.id + " — " + title) << "\n"
                  << "  " << std::string(54,'-') << "\n"
                  << "  Navigation:\n"
                  << "    cd <F18-ID>    In F18-Vorgang navigieren\n"
                  << "    cd <AKT-ID>    In Akte navigieren\n"
                  << "    ls             F18-Vorgaenge und Akten listen\n"
                  << "    ..             Zurueck zum F16\n"
                  << "  Erstellen:\n"
                  << "    f18 -n         Neuen F18-Vorgang in dieser Aufgabe\n"
                  << "    akt -n         Neue Akte zu dieser Aufgabe\n"
                  << "    kom -n         Neue Kommunikation\n"
                  << "  Bearbeiten:\n"
                  << "    f22 -e         F22-Felder bearbeiten\n"
                  << "    f22 -v         F22-Details anzeigen\n"
                  << "    f22 -ind       Nacherfassung unregistrierter Dateien\n"
                  << "  Workflow:\n";
        if (wfi.empty())
            std::cout << "    f77 -s         F77-Workflow starten\n";
        else
            std::cout << "    f77 -d         F77-Workflow anzeigen (" << wfi.substr(0,20) << ")\n"
                      << "    f77 -s         Neuen F77-Workflow starten\n";
        std::cout << "    note <Text>    Schnellnotiz\n\n";
        break;
    }

    case EntityType::F18: {
        auto v = F18Operation::loadById(cur.id);
        std::string title = v ? v->title : cur.id;
        std::cout << Color::bold("  F18 " + cur.id + " — " + title) << "\n"
                  << "  " << std::string(54,'-') << "\n"
                  << "  Navigation:\n"
                  << "    cd <AKT-ID>    In Akte navigieren\n"
                  << "    ls             Schritte und Akten listen\n"
                  << "    ..             Zurueck zur F22\n"
                  << "  Erstellen:\n"
                  << "    akt -n         Neue Akte zu diesem Vorgang\n"
                  << "    kom -n         Neue Kommunikation\n"
                  << "    f18 -stp       Neuen Schritt hinzufuegen\n"
                  << "  Bearbeiten:\n"
                  << "    f18 -e         F18-Felder bearbeiten\n"
                  << "    f18 -v         F18-Details anzeigen\n"
                  << "  Workflow:\n"
                  << "    f77 -s         F77-Workflow starten\n"
                  << "    f77 -d         F77-Workflow anzeigen\n"
                  << "    note <Text>    Schnellnotiz\n\n";
        break;
    }

    case EntityType::AKT: {
        auto d = Folder::loadById(cur.id);
        std::string title = d ? d->title : cur.id;
        auto rev = d ? FolderRevision::currentRevision(d->folderId) : nullptr;
        bool inWork = rev && (rev->revState == RevState::IN_WORK);
        std::cout << Color::bold("  AKT " + cur.id + " — " + title) << "\n"
                  << "  " << std::string(54,'-') << "\n"
                  << "  Navigation:\n"
                  << "    ls             Objekte in aktueller Revision\n"
                  << "    ls -rev        Alle Revisionen listen\n"
                  << "    ..             Zurueck\n"
                  << "  Ansicht:\n"
                  << "    akt -v         Akte-Details anzeigen\n"
                  << "  Inhalt:\n";
        if (inWork) {
            std::cout << "    akt -obj       Objekt hinzufuegen (Datei/URL/Stub)\n"
                      << "    akt -url       URL-Objekte aktualisieren\n"
                      << "    akt -co <#>    Objekt auschecken (bearbeiten)\n"
                      << "    akt -ci        Objekt einchecken\n";
        } else {
            std::cout << "    akt -co <#>    Objekt oeffnen (lesen)\n";
        }
        std::cout << "  Versionen:\n"
                  << "    rev            Neue Revision erstellen\n"
                  << "    akt -rv <n>    Revision wechseln zu Rev N\n"
                  << "  Workflow:\n"
                  << "    f77 -s         F77-Workflow starten\n"
                  << "    f77 -d         F77-Workflow anzeigen\n"
                  << "    note <Text>    Schnellnotiz\n\n";
        break;
    }

    default:
        std::cout << "  Keine kontextspezifischen Optionen.\n  lo  fuer globale Hilfe.\n\n";
    }
}

// ── cd — change folder ────────────────────────────────────────────────────────


// ── Auto-MFS: called after every mutation ─────────────────────────────────────
// Writes the MFS index card for the current entity silently.
// Only prints "MFS aktualisiert" on success.
void autoMFS() {
    auto& stack = Rosenholz::NavigationStack::instance();
    auto  cur   = stack.current();
    if (!cur.valid()) return;
    auto& cfg   = Rosenholz::Config::instance();
    std::string root = cfg.basePath() + "/mfs";
    bool ok = false;
    switch (cur.type) {
        case Rosenholz::EntityType::F16: {
            auto p = Rosenholz::F16::loadById(cur.id);
            if (p) ok = Rosenholz::MFSWriter::writeProject(*p, root);
            break;
        }
        case Rosenholz::EntityType::F22: {
            auto t = Rosenholz::F22::loadById(cur.id);
            if (t) ok = t->writeMFSFile(root);
            break;
        }
        case Rosenholz::EntityType::F18: {
            auto v = Rosenholz::F18Operation::loadById(cur.id);
            if (v) ok = Rosenholz::MFSWriter::writeF18(*v, root);
            break;
        }
        case Rosenholz::EntityType::AKT: {
            auto d = Rosenholz::Folder::loadById(cur.id);
            if (d) ok = Rosenholz::MFSWriter::writeDocument(*d, root);
            break;
        }
        default: break;
    }
    if (ok) std::cout << "  >> MFS aktualisiert.\n";
}

void cmdCd(const std::vector<std::string>& args) {
    auto& nav = NavigationStack::instance();

    if (args.empty()) { cmdLs({}); return; }

    std::string target = args[0];
    if (target == "..") { nav.pop(); cmdLs({}); return; }

    // Try each entity type:
    {
        auto p = F16::loadById(target);
        if (p) {
            nav.push({EntityType::F16, p->projectId, p->title, p->regNumber.toString()});
            std::cout << "  >> " << Color::cyan("F16:" + p->regNumber.toString())
                      << "  " << Color::bold(p->title) << "\n";
            cmdLs({}); return;
        }
    }
    {
        auto t = F22::loadById(target);
        if (t) {
            nav.push({EntityType::F22, t->taskId, t->title, t->regNumber.toString()});
            std::cout << "  >> " << Color::blue("F22:" + t->regNumber.toString())
                      << "  " << Color::bold(t->title) << "\n";
            cmdLs({}); return;
        }
    }
    {
        auto v = F18Operation::loadById(target);
        if (v) {
            nav.push({EntityType::F18, v->operationId, v->title, v->operationId});
            std::cout << "  >> " << Color::magenta("F18:" + v->operationId)
                      << "  " << Color::bold(v->title) << "\n";
            cmdLs({}); return;
        }
    }
    {
        auto d = Folder::loadById(target);
        if (d) {
            nav.push({EntityType::AKT, d->folderId, d->title, d->folderId});
            std::cout << "  >> " << Color::yellow("AKT:" + d->folderId)
                      << "  " << Color::bold(d->title) << "\n";
            cmdLs({}); return;
        }
    }
    printErr("Entitaet nicht gefunden: " + target);
    std::cout << "  Tipp: ls  zeigt verfuegbare Eintraege  |  lo  zeigt Optionen\n";
}

// ── Context-aware command dispatch ───────────────────────────────────────────
//
// These short-form commands use the current navigation context as the
// implicit parent entity, so you don't have to type the ID every time.
//
// Command mapping (max 3 chars):
//   f16 [-n/-e/-v/-s/-o/-arc]   F16 actions
//   f22 [-n/-e/-v/-s/-ind]      F22 actions
//   f18 [-n/-e/-v/-s/-stp]      F18 actions
//   akt [-n/-v/-s/-obj/-url/-co/-ci/-rv] AKT actions
//   f77 [-s/-d/-tpl]             F77 actions
//   rev                         Revise current AKT
//   kom [-n/-l]                  Communications
//   note [text]                  Quick note
//   tsk                         Tasks (F77-Aufgaben)
//   srch <q>                    Global search
//   sts                         Status
//   bak                         Backup
//   wch [N]                     Watch
//   tree                        Tree view
//   cal                         Calendar
//   his                         History

static std::string contextId(EntityType expected = EntityType::NONE) {
    auto& nav = NavigationStack::instance();
    auto cur = nav.current();
    if (!cur.valid()) return "";
    if (expected == EntityType::NONE || cur.type == expected) return cur.id;
    return "";
}

void cmdContextual(const std::string& cmd, const std::vector<std::string>& args) {
    auto& nav = NavigationStack::instance();
    auto cur = nav.current();

    // ── f16 ──────────────────────────────────────────────────────────────────
    if (cmd == "f16") {
        // -f77 -n / -f77 -d: start or show workflow for THIS entity
        if (!args.empty() && args[0] == "-f77") {
            std::string sub = (args.size() > 1) ? args[1] : "";
            if (sub == "-n") {
                if (!cur.valid()) { printErr("Kein Kontext."); return; }
                auto wf = F77Engine::startDefault(cmd, cur.id);
                if (wf) { printOk("Workflow gestartet: " + wf->workflowId); autoMFS(); }
                else    printErr("Workflow konnte nicht gestartet werden (läuft bereits?)");
                return;
            }
            if (sub == "-d") {
                auto wfs = F77W::loadForEntity(cmd, cur.id);
                if (wfs.empty()) { std::cout << "  (keine Workflows fuer " << cur.id << ")\n"; return; }
                std::cout << "\n  " << std::left << std::setw(28) << "WORKFLOW-ID"
                          << std::setw(20) << "VORLAGE"
                          << "STATUS\n  " << std::string(60,'-') << "\n";
                for (auto& w : wfs)
                    std::cout << "  " << std::setw(28) << w->workflowId
                              << std::setw(20) << w->templateName.substr(0,18)
                              << std::string(toString(w->status)) << "\n";
                std::cout << "\n";
                return;
            }
        }
        std::vector<std::string> fullArgs;
        // No args and in F16 context → show F16 details/menu
        if (args.empty() && cur.type == EntityType::F16) {
            auto p = F16::loadById(cur.id);
            if (p) { projectMenu(p); return; }
        }
        // -n: create new F16 (no context needed)
        if (!args.empty() && args[0] == "-n") { cmdF16({"-n"}); return; }
        // -e: edit current F16
        if (!args.empty() && args[0] == "-e" && cur.type == EntityType::F16) {
            cmdF16({cur.id}); return;
        }
        // -v: view current F16
        if (!args.empty() && args[0] == "-v" && cur.type == EntityType::F16) {
            auto p = F16::loadById(cur.id);
            if (p) printProject(*p); return;
        }
        // -arc: archive current F16
        if (!args.empty() && args[0] == "-arc" && cur.type == EntityType::F16) {
            auto p = F16::loadById(cur.id);
            if (p) { if (opOk(p->archive())) std::cout << "  >> Archiviert.\n";
                    else std::cout << "  >> Fehler beim Archivieren.\n"; }
            return;
        }
        // -o: selection list
        if (!args.empty() && args[0] == "-o") { cmdF16({"-o"}); return; }
        // -s: search
        if (!args.empty() && args[0] == "-s") {
            std::vector<std::string> sa(args.begin(), args.end());
            cmdF16(sa); return;
        }
        // -note: quick note on current F16 (no ID needed)
        if (!args.empty() && args[0] == "-note" && cur.type == EntityType::F16) {
            std::string text;
            for (std::size_t i=1; i<args.size(); i++) { if(!text.empty()) text+=" "; text+=args[i]; }
            if (text.empty()) text = readLine("  Notiz: ");
            if (text.empty()) return;
            std::string etype = entityTypeLabel(cur.type);
            std::transform(etype.begin(), etype.end(), etype.begin(), ::tolower);
            auto n = Note::create(etype, cur.id, text);
            if (n) std::cout << "  >> Notiz gespeichert: " << n->noteId << "\n";
            else   printErr("Fehler beim Speichern");
            return;
        }
        // Context guard: these args require F16 context
        if (!args.empty()) {
            static const std::vector<std::string> ctxRequired = {"-e","-v","-arc","-note"};
            for (auto& a : ctxRequired) {
                if (args[0] == a) {
                    printErr("'" + cmd + " " + args[0] + "' erfordert F16-Kontext.\n"
                             "  cd <F16-ID>  dann: . " + args[0]);
                    return;
                }
            }
        }
        // -n, -o, -s, -so: global — route through
        cmdF16(args); return;
    }

    // ── f22 ──────────────────────────────────────────────────────────────────
    if (cmd == "f22") {
        // -f77 -n / -f77 -d: start or show workflow for THIS entity
        if (!args.empty() && args[0] == "-f77") {
            std::string sub = (args.size() > 1) ? args[1] : "";
            if (sub == "-n") {
                if (!cur.valid()) { printErr("Kein Kontext."); return; }
                auto wf = F77Engine::startDefault(cmd, cur.id);
                if (wf) { printOk("Workflow gestartet: " + wf->workflowId); autoMFS(); }
                else    printErr("Workflow konnte nicht gestartet werden (läuft bereits?)");
                return;
            }
            if (sub == "-d") {
                auto wfs = F77W::loadForEntity(cmd, cur.id);
                if (wfs.empty()) { std::cout << "  (keine Workflows fuer " << cur.id << ")\n"; return; }
                std::cout << "\n  " << std::left << std::setw(28) << "WORKFLOW-ID"
                          << std::setw(20) << "VORLAGE"
                          << "STATUS\n  " << std::string(60,'-') << "\n";
                for (auto& w : wfs)
                    std::cout << "  " << std::setw(28) << w->workflowId
                              << std::setw(20) << w->templateName.substr(0,18)
                              << std::string(toString(w->status)) << "\n";
                std::cout << "\n";
                return;
            }
        }
        // -n: create under current F16 (if in F16 context)
        if (!args.empty() && args[0] == "-n") {
            if (cur.type == EntityType::F16) {
                auto t = createTaskWizard(cur.id);
                if (t) {
                    printOk("F22 angelegt: " + t->regNumber.toString() + "  " + t->title);
                    if (yesno("  Jetzt navigieren?"))
                        cmdCd({t->taskId});
                }
            } else {
                cmdF22({"-n"});
            }
            return;
        }
        // -e: edit current F22
        if (!args.empty() && args[0] == "-e" && cur.type == EntityType::F22) {
            auto t = F22::loadById(cur.id);
            if (t) taskMenu(t);
            return;
        }
        // -v: view current F22
        if (!args.empty() && args[0] == "-v" && cur.type == EntityType::F22) {
            auto t = F22::loadById(cur.id);
            if (t) printTask(*t);
            return;
        }
        // -ind: Nacherfassung on current F22
        if (!args.empty() && args[0] == "-ind" && cur.type == EntityType::F22) {
            auto t = F22::loadById(cur.id);
            if (t) taskMenu(t);  // taskMenu has Nacherfassen option
            return;
        }
        // -note: quick note on current entity
        if (!args.empty() && args[0] == "-note") {
            std::string text;
            for (std::size_t i=1; i<args.size(); i++) { if(!text.empty()) text+=" "; text+=args[i]; }
            if (text.empty()) text = readLine("  Notiz: ");
            if (text.empty()) return;
            std::string etype = entityTypeLabel(cur.type);
            std::transform(etype.begin(), etype.end(), etype.begin(), ::tolower);
            auto n = Note::create(etype, cur.id, text);
            if (n) std::cout << "  >> Notiz gespeichert: " << n->noteId << "\n";
            else   printErr("Fehler beim Speichern");
            return;
        }
        // No flag: open full menu if in F22 context
        if (args.empty() && cur.type == EntityType::F22) {
            auto t = F22::loadById(cur.id);
            if (t) taskMenu(t);
            return;
        }
        cmdF22(args); return;
    }

    // ── f18 ──────────────────────────────────────────────────────────────────
    if (cmd == "f18") {
        // -f77 -n / -f77 -d: start or show workflow for THIS entity
        if (!args.empty() && args[0] == "-f77") {
            std::string sub = (args.size() > 1) ? args[1] : "";
            if (sub == "-n") {
                if (!cur.valid()) { printErr("Kein Kontext."); return; }
                auto wf = F77Engine::startDefault(cmd, cur.id);
                if (wf) { printOk("Workflow gestartet: " + wf->workflowId); autoMFS(); }
                else    printErr("Workflow konnte nicht gestartet werden (läuft bereits?)");
                return;
            }
            if (sub == "-d") {
                auto wfs = F77W::loadForEntity(cmd, cur.id);
                if (wfs.empty()) { std::cout << "  (keine Workflows fuer " << cur.id << ")\n"; return; }
                std::cout << "\n  " << std::left << std::setw(28) << "WORKFLOW-ID"
                          << std::setw(20) << "VORLAGE"
                          << "STATUS\n  " << std::string(60,'-') << "\n";
                for (auto& w : wfs)
                    std::cout << "  " << std::setw(28) << w->workflowId
                              << std::setw(20) << w->templateName.substr(0,18)
                              << std::string(toString(w->status)) << "\n";
                std::cout << "\n";
                return;
            }
        }
        // -n: create under current F22
        if (!args.empty() && args[0] == "-n") {
            std::string taskId;
            if (cur.type == EntityType::F22) taskId = cur.id;
            else if (cur.type == EntityType::F18) {
                // Get parent F22 of current F18:
                auto v = F18Operation::loadById(cur.id);
                if (v) taskId = v->taskId;
            }
            if (!taskId.empty()) {
                auto v = createF18Wizard("", taskId, "");
                if (v) {
                    printOk("F18 angelegt: " + v->operationId + "  " + v->title);
                    if (yesno("  Jetzt navigieren?"))
                        cmdCd({v->operationId});
                }
            } else {
                cmdF18({"-n"});
            }
            return;
        }
        // -e: edit / open menu
        if (!args.empty() && args[0] == "-e" && cur.type == EntityType::F18) {
            auto v = F18Operation::loadById(cur.id);
            if (v) f18Menu(v);
            return;
        }
        // -v: view
        if (!args.empty() && args[0] == "-v" && cur.type == EntityType::F18) {
            auto v = F18Operation::loadById(cur.id);
            if (v) printF18Operation(*v);
            return;
        }
        // -stp: removed (use f18s instead)
        // -note: quick note on current entity
        if (!args.empty() && args[0] == "-note") {
            std::string text;
            for (std::size_t i=1; i<args.size(); i++) { if(!text.empty()) text+=" "; text+=args[i]; }
            if (text.empty()) text = readLine("  Notiz: ");
            if (text.empty()) return;
            std::string etype = entityTypeLabel(cur.type);
            std::transform(etype.begin(), etype.end(), etype.begin(), ::tolower);
            auto n = Note::create(etype, cur.id, text);
            if (n) std::cout << "  >> Notiz gespeichert: " << n->noteId << "\n";
            else   printErr("Fehler beim Speichern");
            return;
        }
        // -note: quick note on current entity
        if (!args.empty() && args[0] == "-note") {
            std::string text;
            for (std::size_t i=1; i<args.size(); i++) { if(!text.empty()) text+=" "; text+=args[i]; }
            if (text.empty()) text = readLine("  Notiz: ");
            if (text.empty()) return;
            std::string etype = entityTypeLabel(cur.type);
            std::transform(etype.begin(), etype.end(), etype.begin(), ::tolower);
            auto n = Note::create(etype, cur.id, text);
            if (n) std::cout << "  >> Notiz gespeichert: " << n->noteId << "\n";
            else   printErr("Fehler beim Speichern");
            return;
        }
        // No flag: open menu if in F18 context
        if (args.empty() && cur.type == EntityType::F18) {
            auto v = F18Operation::loadById(cur.id);
            if (v) f18Menu(v);
            return;
        }
        // Context guard: these args require F18 context
        if (!args.empty()) {
            static const std::vector<std::string> ctxRequired = {"-e","-v","-note"};
            for (auto& a : ctxRequired) {
                if (args[0] == a) {
                    printErr("'" + cmd + " " + args[0] + "' erfordert F18-Kontext.\n"
                             "  cd <F18-ID>  dann: . " + args[0]);
                    return;
                }
            }
        }
        // -o, -so, -s: global route through
        cmdF18(args); return;
    }

    // ── akt ──────────────────────────────────────────────────────────────────
    if (cmd == "akt") {
        // -f77 -n / -f77 -d: start or show workflow for THIS entity
        if (!args.empty() && args[0] == "-f77") {
            std::string sub = (args.size() > 1) ? args[1] : "";
            if (sub == "-n") {
                if (!cur.valid()) { printErr("Kein Kontext."); return; }
                auto wf = F77Engine::startDefault(cmd, cur.id);
                if (wf) { printOk("Workflow gestartet: " + wf->workflowId); autoMFS(); }
                else    printErr("Workflow konnte nicht gestartet werden (läuft bereits?)");
                return;
            }
            if (sub == "-d") {
                auto wfs = F77W::loadForEntity(cmd, cur.id);
                if (wfs.empty()) { std::cout << "  (keine Workflows fuer " << cur.id << ")\n"; return; }
                std::cout << "\n  " << std::left << std::setw(28) << "WORKFLOW-ID"
                          << std::setw(20) << "VORLAGE"
                          << "STATUS\n  " << std::string(60,'-') << "\n";
                for (auto& w : wfs)
                    std::cout << "  " << std::setw(28) << w->workflowId
                              << std::setw(20) << w->templateName.substr(0,18)
                              << std::string(toString(w->status)) << "\n";
                std::cout << "\n";
                return;
            }
        }
        // -n: create new AKT under current entity
        if (!args.empty() && args[0] == "-n") {
            std::string projId, taskId;
            if (cur.type == EntityType::F16) {
                // Akten gehören unter F22 oder F18, nicht direkt unter F16.
                // Eine AKT im F16-Kontext hätte keine gültige Eltern-Aufgabe.
                printErr("akt -n ist hier nicht möglich.\n"
                         "  Navigiere zuerst in eine F22: cd <F22-ID>\n"
                         "  Dann: akt -n");
                return;
            } else if (cur.type == EntityType::F22) {
                auto t = F22::loadById(cur.id);
                if (t) { projId = t->projectId; taskId = t->taskId; }
            } else if (cur.type == EntityType::F18) {
                auto v = F18Operation::loadById(cur.id);
                if (v) {
                    auto t = F22::loadById(v->taskId);
                    if (t) projId = t->projectId;
                    taskId = v->taskId;
                }
            }
            if (!projId.empty() || !taskId.empty()) {
                // createDocumentWizard takes (taskId, f18OpId):
                // taskId is set from context; projId is not needed by the wizard
                auto doc = createDocumentWizard(taskId, "");
                if (doc) {
                    printOk("AKT angelegt: " + doc->folderId + "  " + doc->title);
                    autoMFS();
                    if (yesno("  Jetzt navigieren?"))
                        cmdCd({doc->folderId});
                }
            } else {
                cmdAkt({"-n"});
            }
            return;
        }
        // -v: view/open current AKT menu
        if (!args.empty() && (args[0] == "-v" || args[0] == "-e") && cur.type == EntityType::AKT) {
            auto d = Folder::loadById(cur.id);
            if (d) documentMenu(d);
            return;
        }
        // -obj: add object to current AKT
        if (!args.empty() && args[0] == "-obj" && cur.type == EntityType::AKT) {
            auto d = Folder::loadById(cur.id);
            if (d) documentMenu(d);  // documentMenu handles object addition
            return;
        }
        // -co <n>: checkout object #N
        if (!args.empty() && args[0] == "-co" && cur.type == EntityType::AKT) {
            auto d = Folder::loadById(cur.id);
            if (d) documentMenu(d);
            return;
        }
        // -ci: checkin
        if (!args.empty() && args[0] == "-ci" && cur.type == EntityType::AKT) {
            auto d = Folder::loadById(cur.id);
            if (d) documentMenu(d);
            return;
        }
        // -url: update all URLs in current AKT
        if (!args.empty() && args[0] == "-url" && cur.type == EntityType::AKT) {
            auto d = Folder::loadById(cur.id);
            if (d) documentMenu(d);
            return;
        }
        // -rv <n>: switch revision
        if (!args.empty() && args[0] == "-rv" && cur.type == EntityType::AKT) {
            auto d = Folder::loadById(cur.id);
            if (d) documentMenu(d);
            return;
        }
        // -oo / -soo: list FolderObjects in current context
        // Context-aware: only shows objects in AKT attached to current entity
        if (!args.empty() && (args[0] == "-oo" || args[0] == "-soo")) {
            bool doOpen = (args[0] == "-soo");
            std::string q = (doOpen && args.size() > 1) ? args[1] : "";
            // Determine which folders to scan based on context:
            std::vector<std::shared_ptr<Folder>> contextFolders;
            std::string entType, entId;
            if (cur.type == EntityType::F16)  { entType="f16"; entId=cur.id; }
            else if (cur.type == EntityType::F22) { entType="f22"; entId=cur.id; }
            else if (cur.type == EntityType::F18) { entType="f18"; entId=cur.id; }
            else if (cur.type == EntityType::AKT) {
                // In AKT context: show objects of THIS folder
                contextFolders.push_back(Folder::loadById(cur.id));
            }
            if (contextFolders.empty() && !entType.empty())
                contextFolders = Folder::loadForEntity(entType, entId);

            struct ObjRow { std::shared_ptr<FolderObject> obj; std::string folderTitle, folderId; };
            std::vector<ObjRow> hits;
            for (auto& d : contextFolders) {
                if (!d) continue;
                auto rev = FolderRevision::currentRevision(d->folderId);
                if (!rev) continue;
                for (auto& o : FolderObject::loadForRevision(d->folderId, rev->rev)) {
                    if (!q.empty() && !matchesPattern(o->originalName, q)
                                   && !matchesPattern(o->displayName(), q)) continue;
                    hits.push_back({o, d->title, d->folderId});
                }
            }
            if (hits.empty()) { std::cout << "  (keine Objekte im Kontext)\n"; return; }
            std::cout << "\n  " << std::left << std::setw(4) << "#"
                      << std::setw(30) << "DATEI"
                      << std::setw(10) << "FORMAT"
                      << "AKTE\n"
                      << "  " << std::string(60,'-') << "\n";
            for (int i=0; i<(int)hits.size(); i++)
                std::cout << "  " << std::setw(4) << (i+1)
                          << std::setw(30) << hits[i].obj->displayName().substr(0,28)
                          << std::setw(10) << hits[i].obj->format.substr(0,8)
                          << hits[i].folderTitle.substr(0,28) << "\n";
            if (!doOpen) return;
            int pick = readInt("  Auswahl [0=Abbrechen]", 0, (int)hits.size());
            if (pick < 1) return;
            cmdCd({hits[pick-1].folderId});
            return;
        }

        // -note: quick note on current AKT (no ID needed)
        if (!args.empty() && args[0] == "-note" && cur.type == EntityType::AKT) {
            std::string text;
            for (std::size_t i=1; i<args.size(); i++) { if(!text.empty()) text+=" "; text+=args[i]; }
            if (text.empty()) text = readLine("  Notiz: ");
            if (text.empty()) return;
            std::string etype = entityTypeLabel(cur.type);
            std::transform(etype.begin(), etype.end(), etype.begin(), ::tolower);
            auto n = Note::create(etype, cur.id, text);
            if (n) std::cout << "  >> Notiz gespeichert: " << n->noteId << "\n";
            else   printErr("Fehler beim Speichern");
            return;
        }
        // No flag: open menu if in AKT context
        if (args.empty() && cur.type == EntityType::AKT) {
            auto d = Folder::loadById(cur.id);
            if (d) documentMenu(d);
            return;
        }
        cmdAkt(args); return;
    }

    // ── f77 ──────────────────────────────────────────────────────────────────
    if (cmd == "f77") {
        // -s: start workflow on current entity
        if (!args.empty() && args[0] == "-s") {
            if (!cur.valid()) { printErr("Kein Kontext — cd <ID>  um zu navigieren"); return; }
            std::string etype;
            switch (cur.type) {
                case EntityType::F22: etype = "f22"; break;
                case EntityType::F18: etype = "f18"; break;
                case EntityType::AKT: etype = "akt"; break;
                default: printErr("F77 nicht unterstuetzt fuer diesen Typ"); return;
            }
            startWfInstanceWizard(etype, cur.id);
            return;
        }
        // -d: show active workflow on current entity
        if (!args.empty() && args[0] == "-d") {
            if (!cur.valid()) { printErr("Kein Kontext"); return; }
            std::string etype;
            switch (cur.type) {
                case EntityType::F22: etype = "f22"; break;
                case EntityType::F18: etype = "f18"; break;
                case EntityType::AKT: etype = "akt"; break;
                default: printErr("Kein F77 fuer diesen Typ"); return;
            }
            auto wfs = F77W::loadForEntity(etype, cur.id);
            if (wfs.empty()) { std::cout << "  (kein F77 fuer dieses Objekt)\n"; return; }
            // Show most recent active one:
            for (auto& wf : wfs) {
                if (wf->status == WorkflowStatus::ACTIVE) {
                    instanceMenu(wf->workflowId);
                    return;
                }
            }
            // No active: show most recent
            instanceMenu(wfs.front()->workflowId);
            return;
        }
        // -tpl: show templates
        if (!args.empty() && args[0] == "-tpl") { cmdF77({"-tpl"}); return; }
        // No args: list active workflows
        cmdF77(args); return;
    }

    // ── rev — new revision of current AKT ────────────────────────────────────
    if (cmd == "rev") {
        if (cur.type != EntityType::AKT) {
            printErr("rev nur fuer AKT verfuegbar — cd <AKT-ID>  um zu navigieren");
            return;
        }
        auto d = Folder::loadById(cur.id);
        if (d) documentMenu(d);  // documentMenu option 2 = revise
        return;
    }

    // ── kom — communications ──────────────────────────────────────────────────
    if (cmd == "kom") {
        std::string ownerId, ownerType;
        switch (cur.type) {
            case EntityType::F16: ownerId = cur.id; ownerType = "f16"; break;
            case EntityType::F22: ownerId = cur.id; ownerType = "f22"; break;
            case EntityType::F18: ownerId = cur.id; ownerType = "f18"; break;
            default: break;
        }
        if (ownerId.empty()) { printErr("Kein Kontext fuer Kommunikation"); return; }
        // -l: list, anything else: open menu
        communicationMenu(ownerId, ownerType);
        return;
    }

    // ── f99 — F99-Notiz (im Kontext, Kurzbefehle) ────────────────────────────
    if (cmd == "f99" || cmd == "note") {
        // -s <q>: search in context (or global if no context)
        if (!args.empty() && args[0] == "-s") {
            std::string q;
            for (std::size_t i=1; i<args.size(); i++) { if(!q.empty()) q+=" "; q+=args[i]; }
            if (q.empty()) q = readLine("  Suche: ");
            std::string etype_filter;
            if (cur.valid()) {
                etype_filter = entityTypeLabel(cur.type);
                std::transform(etype_filter.begin(), etype_filter.end(), etype_filter.begin(), ::tolower);
            }
            auto hits = Note::search(q, etype_filter);
            if (hits.empty()) { std::cout << "  (keine F99-Notizen gefunden)\n"; return; }
            std::cout << "\n  " << std::left << std::setw(22) << "ID"
                      << std::setw(18) << "DATUM"
                      << std::setw(32) << "PFAD"
                      << "INHALT\n"
                      << "  " << std::string(88,'-') << "\n";
            for (auto& r : hits)
                std::cout << "  " << std::setw(22) << r.note->noteId
                          << std::setw(18) << r.note->createdAt.substr(0,16)
                          << std::setw(32) << (r.entityPath + " " + r.entityTitle).substr(0,30)
                          << r.note->body.substr(0,36) << "\n";
            std::cout << "\n";
            return;
        }
        // -so [q]: F99 Manager — suchen, auswählen, bearbeiten/löschen
        if (!args.empty() && args[0] == "-so") {
            cmdF99({"-so", args.size()>1 ? args[1] : ""});
            return;
        }
        // -o <id>: open specific note
        if (!args.empty() && args[0] == "-o") {
            std::string nid = args.size()>1 ? args[1] : readLine("  F99-ID: ");
            cmdF99({"-o", nid});
            return;
        }
        // Default: create note in context
        if (!cur.valid()) { printErr("Kein Kontext — cd <ID>  um zu navigieren"); return; }
        std::string text;
        for (auto& a : args) { if (!text.empty()) text += " "; text += a; }
        if (text.empty()) text = readLine("  Notiz: ");
        if (text.empty()) return;
        std::string etype = entityTypeLabel(cur.type);
        std::transform(etype.begin(), etype.end(), etype.begin(), ::tolower);
        auto n = Note::create(etype, cur.id, text);
        if (n) { std::cout << "  >> F99 gespeichert: " << n->noteId << "\n"; autoMFS(); }
        else   printErr("Fehler beim Speichern");
        return;
    }

    // ── mfs ──────────────────────────────────────────────────────────────────
    // Plain "mfs" in context: refresh only the current entity (fast).
    // Dash "-mfs" always runs a full rebuild (via globalHandler in registry).
    if (cmd == "mfs") {
        if (cur.valid() && args.empty()) {
            autoMFS();
        } else {
            cmdMfs(args);  // -sync or no context: full rebuild
        }
        return;
    }

    // ── tsk — F77 tasks ───────────────────────────────────────────────────────
    if (cmd == "tsk") { cmdTasks(args); return; }

    // ── srch — global search ──────────────────────────────────────────────────
    if (cmd == "srch") {
        std::string q;
        for (auto& a : args) { if (!q.empty()) q += " "; q += a; }
        if (q.empty()) q = readLine("  Suche: ");
        cmdSearch(q);
        return;
    }

} // end cmdContextual


std::vector<std::pair<std::string,std::string>> getContextChildren() {
    std::vector<std::pair<std::string,std::string>> result;
    auto cur = Rosenholz::NavigationStack::instance().current();
    if (!cur.valid()) return result;

    switch (cur.type) {
    case EntityType::F16: {
        for (auto& t : Rosenholz::F22::loadForProject(cur.id))
            result.push_back({t->regNumber.toString(), t->title});
        for (auto& d : Rosenholz::Folder::loadForEntity("f16", cur.id))
            result.push_back({d->folderId, d->title});
        break;
    }
    case EntityType::F22: {
        for (auto& v : Rosenholz::F18Operation::loadForTask(cur.id))
            result.push_back({v->operationId, v->title});
        for (auto& d : Rosenholz::Folder::loadForEntity("f22", cur.id))
            result.push_back({d->folderId, d->title});
        break;
    }
    case EntityType::F18: {
        for (auto& d : Rosenholz::Folder::loadForEntity("f18", cur.id))
            result.push_back({d->folderId, d->title});
        break;
    }
    default: break;
    }
    return result;
}

} // namespace CLI

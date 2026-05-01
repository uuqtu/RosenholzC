// ============================================================
// cli_nav.cpp  —  Linux-style navigation commands
//
// cf <id>    — "change folder": navigate into an entity
// ..         — navigate back (already in main_cli.cpp)
// lf         — "list folder": list children of current entity
// lo         — "list options": show context-aware help
// -h at prompt — shows lo (context help), not global help
//
// These commands integrate fully into the existing shell,
// replacing the number-menu approach with linux-style navigation
// while keeping all existing commands available.
// ============================================================
#include "cli_common.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/f18/F18Operation.h"
#include "../model/f18/F18OperationStep.h"
#include "../model/akt/Folder.h"
#include "../model/akt/FolderRevision.h"
#include "../model/akt/FolderObject.h"
#include "../workflow/F77Workflow.h"
#include "../workflow/F77Task.h"
#include <iomanip>
#include <sstream>

namespace CLI {

using namespace Rosenholz;

// ── Forward declarations (defined in other CLI modules) ───────────────────
void projectMenu(std::shared_ptr<F16> p);
void taskMenu(std::shared_ptr<F22> t);
void f18Menu(std::shared_ptr<F18Operation> v);
void documentMenu(std::shared_ptr<Folder> d);
void instanceMenu(const std::string& wfId);

// ── lf — list children of current entity ─────────────────────────────────
//
// Shows what's inside the current location in a structured format.
// Format: "  #  ID                  TITLE              TYPE"
//
void cmdLf(const std::vector<std::string>& args) {
    auto& nav = NavigationStack::instance();
    auto cur = nav.current();

    bool showRevisions = false;
    for (auto& a : args) if (a == "-rev") showRevisions = true;

    if (!cur.valid()) {
        // Top level: list all F16 projects
        auto all = F16::loadAll();
        if (all.empty()) { std::cout << "  (keine F16-Karten)\n"; return; }
        std::cout << "\n  F16-Karten (" << all.size() << "):\n"
                  << "  " << std::left << std::setw(4) << "#"
                  << std::setw(24) << "ID"
                  << std::setw(34) << "TITEL"
                  << "STATUS\n"
                  << "  " << std::string(70,'-') << "\n";
        int n = 1;
        for (auto& p : all)
            std::cout << "  " << std::setw(4) << n++
                      << std::setw(24) << p->regNumber.toString()
                      << std::setw(34) << p->title.substr(0,32)
                      << (p->archived ? "archiviert" : "aktiv") << "\n";
        std::cout << "\n  cf <ID>  um hineinzugehen\n\n";
        return;
    }

    switch (cur.type) {

    case EntityType::F16: {
        auto p = F16::loadById(cur.id);
        if (!p) { printErr("F16 nicht gefunden: " + cur.id); return; }

        auto tasks = F22::loadForProject(p->projectId);
        auto akten = Folder::loadForEntity("f16", p->projectId);

        std::cout << "\n  F16:" << cur.id << " — " << p->title << "\n\n";

        if (!tasks.empty()) {
            std::cout << "  F22-Vorgaenge (" << tasks.size() << "):\n"
                      << "  " << std::left << std::setw(4) << "#"
                      << std::setw(24) << "ID"
                      << std::setw(34) << "TITEL"
                      << "STATUS\n"
                      << "  " << std::string(70,'-') << "\n";
            int n = 1;
            for (auto& t : tasks)
                std::cout << "  " << std::setw(4) << n++
                          << std::setw(24) << t->regNumber.toString()
                          << std::setw(34) << t->title.substr(0,32)
                          << entityStatusToString(t->status) << "\n";
        } else {
            std::cout << "  (keine F22-Vorgaenge)\n";
        }

        if (!akten.empty()) {
            std::cout << "\n  Akten (" << akten.size() << "):\n";
            int n = 1;
            for (auto& d : akten)
                std::cout << "  " << std::setw(4) << n++
                          << std::setw(24) << d->folderId
                          << "  " << d->title.substr(0,32) << "\n";
        }
        std::cout << "\n";
        break;
    }

    case EntityType::F22: {
        auto t = F22::loadById(cur.id);
        if (!t) { printErr("F22 nicht gefunden: " + cur.id); return; }

        auto f18s  = F18Operation::loadForTask(t->taskId);
        auto akten = Folder::loadForEntity("f22", t->taskId);

        std::cout << "\n  F22:" << cur.id << " — " << t->title << "\n"
                  << "  Status: " << Color::statusColor(entityStatusToString(t->status))
                  << "  Prioritaet: " << t->priority << "\n\n";

        if (!f18s.empty()) {
            std::cout << "  F18-Vorgaenge (" << f18s.size() << "):\n"
                      << "  " << std::left << std::setw(4) << "#"
                      << std::setw(24) << "ID"
                      << std::setw(10) << "TYP"
                      << std::setw(30) << "TITEL"
                      << "STATUS\n"
                      << "  " << std::string(75,'-') << "\n";
            int n = 1;
            for (auto& v : f18s)
                std::cout << "  " << std::setw(4) << n++
                          << std::setw(24) << v->operationId
                          << std::setw(10) << v->operationType.substr(0,9)
                          << std::setw(30) << v->title.substr(0,28)
                          << entityStatusToString(v->status) << "\n";
        } else {
            std::cout << "  (keine F18-Vorgaenge)\n";
        }

        if (!akten.empty()) {
            std::cout << "\n  Akten (" << akten.size() << "):\n"
                      << "  " << std::left << std::setw(4) << "#"
                      << std::setw(24) << "ID"
                      << std::setw(30) << "TITEL"
                      << "TYP\n"
                      << "  " << std::string(66,'-') << "\n";
            int n = 1;
            for (auto& d : akten)
                std::cout << "  " << std::setw(4) << n++
                          << std::setw(24) << d->folderId
                          << std::setw(30) << d->title.substr(0,28)
                          << d->docType << "\n";
        } else {
            std::cout << "\n  (keine Akten)\n";
        }
        std::cout << "\n";
        break;
    }

    case EntityType::F18: {
        auto v = F18Operation::loadById(cur.id);
        if (!v) { printErr("F18 nicht gefunden: " + cur.id); return; }

        std::cout << "\n  F18:" << cur.id << " — " << v->title << "\n"
                  << "  Typ: " << v->operationType
                  << "  Status: " << Color::statusColor(entityStatusToString(v->status)) << "\n\n";

        // Steps:
        std::cout << "  Schritte (" << v->steps.size() << "):\n"
                  << "  " << std::left << std::setw(4) << "#"
                  << std::setw(26) << "ID"
                  << std::setw(28) << "TITEL"
                  << "STATUS\n"
                  << "  " << std::string(66,'-') << "\n";
        int n = 1;
        for (auto& s : v->steps) {
            auto sym = f18StepSymbolStr(s.displaySymbol());
            std::cout << "  " << std::setw(4) << n++
                      << std::setw(26) << s.stepId
                      << std::setw(28) << s.title.substr(0,26);
            if (s.status == F18StepStatus::DONE) std::cout << Color::green(sym);
            else if (s.status == F18StepStatus::IN_PROGRESS) std::cout << Color::yellow(sym);
            else std::cout << sym;
            std::cout << "\n";
        }

        // Akten:
        auto akten = Folder::loadForEntity("f18", v->operationId);
        if (!akten.empty()) {
            std::cout << "\n  Akten (" << akten.size() << "):\n";
            n = 1;
            for (auto& d : akten)
                std::cout << "  " << std::setw(4) << n++
                          << std::setw(24) << d->folderId
                          << "  " << d->title.substr(0,32) << "\n";
        }
        std::cout << "\n";
        break;
    }

    case EntityType::AKT: {
        auto d = Folder::loadById(cur.id);
        if (!d) { printErr("Akte nicht gefunden: " + cur.id); return; }

        auto rev = FolderRevision::currentRevision(d->folderId);
        std::cout << "\n  AKT:" << cur.id << " — " << d->title << "\n";
        if (rev) {
            std::cout << "  Rev:" << rev->rev << "  Status:" << rev->revState << "\n";
        }
        std::cout << "\n";

        if (showRevisions) {
            // List all revisions:
            auto allRevs = FolderRevision::loadAllRevisions(d->folderId);
            std::cout << "  Revisionen (" << allRevs.size() << "):\n"
                      << "  " << std::setw(6) << "REV"
                      << std::setw(16) << "STATUS"
                      << "ERSTELLT\n"
                      << "  " << std::string(50,'-') << "\n";
            for (auto& r : allRevs)
                std::cout << "  " << std::setw(6) << r->rev
                          << std::setw(16) << revStateToString(r->revState)
                          << r->createdAt.substr(0,16) << "\n";
            std::cout << "\n";
        }

        if (rev) {
            auto objs = FolderObject::loadForRevision(d->folderId, rev->rev);
            if (!objs.empty()) {
                std::cout << "  Objekte in Rev " << rev->rev
                          << " (" << objs.size() << "):\n"
                          << "  " << std::left << std::setw(4) << "#"
                          << std::setw(20) << "ID"
                          << std::setw(30) << "NAME"
                          << std::setw(8) << "FMT"
                          << "STATUS\n"
                          << "  " << std::string(68,'-') << "\n";
                int n = 1;
                for (auto& o : objs)
                    std::cout << "  " << std::setw(4) << n++
                              << std::setw(20) << o->objectId.substr(0,18)
                              << std::setw(30) << o->displayName().substr(0,28)
                              << std::setw(8) << o->format.substr(0,6)
                              << (o->committed ? "committed" : "in_work") << "\n";
            } else {
                std::cout << "  (keine Objekte in aktueller Revision)\n";
            }
        }
        std::cout << "\n  lo  fuer Optionen  |  -rev  fuer alle Revisionen\n\n";
        break;
    }

    default:
        std::cout << "  (keine Unterobjekte fuer diesen Typ)\n";
        break;
    }
}

// ── lo — list options at current location ────────────────────────────────────
//
// Shows context-specific commands available at the current navigation level.
// This is the "local -h" — what you can do from here.
//
void cmdLo(const std::vector<std::string>& args) {
    auto& nav = NavigationStack::instance();
    auto cur = nav.current();

    std::cout << "\n";

    if (!cur.valid()) {
        // Top level
        std::cout << "  Rosenholz PM — Befehle (oberste Ebene)\n"
                  << "  " << std::string(52,'-') << "\n"
                  << "  cf <F16-ID>      In F16-Kartei navigieren\n"
                  << "  lf               Alle F16-Karten listen\n"
                  << "  -f16 -n          Neue F16-Kartei anlegen\n"
                  << "  -f16 -o          F16 auswaehlen und navigieren\n"
                  << "  -f16 -s <q>      F16 suchen\n"
                  << "  -f22 -n          Neue F22-Aufgabe anlegen\n"
                  << "  -tasks           Offene Workflow-Aufgaben\n"
                  << "  -search <q>      Globale Suche\n"
                  << "  -tree            Hierarchiebaum\n"
                  << "  -status          Datenbankzaehler\n"
                  << "  -watch [N]       MFS+F77 Polling (N=Sekunden)\n"
                  << "  -hist            Verlauf\n"
                  << "  -h               Diese Uebersicht\n"
                  << "  exit             Beenden\n\n";
        return;
    }

    switch (cur.type) {

    case EntityType::F16: {
        auto p = F16::loadById(cur.id);
        std::string title = p ? p->title : cur.id;
        std::cout << "  F16: " << cur.id << " — " << title << "\n"
                  << "  " << std::string(52,'-') << "\n"
                  << "  Navigation:\n"
                  << "    cf <F22-ID>      In F22-Vorgang navigieren\n"
                  << "    cf <AKT-ID>      In Akte navigieren\n"
                  << "    lf               F22-Vorgaenge und Akten listen\n"
                  << "    ..               Zurueck (oberste Ebene)\n"
                  << "  Aktionen:\n"
                  << "    -f22 -n          Neue F22-Aufgabe in diesem Projekt\n"
                  << "    -f22 <F22-ID>    F22-Menue oeffnen (Bearbeiten etc.)\n"
                  << "    -akt -n          Neue Akte anlegen\n"
                  << "    -f16 " << cur.id << "   F16-Menue oeffnen\n"
                  << "    -note " << cur.id << " <Text>   Schnellnotiz\n"
                  << "    -tree " << cur.id << "  Hierarchiebaum\n\n";
        break;
    }

    case EntityType::F22: {
        auto t = F22::loadById(cur.id);
        std::string title = t ? t->title : cur.id;
        std::cout << "  F22: " << cur.id << " — " << title << "\n"
                  << "  " << std::string(52,'-') << "\n"
                  << "  Navigation:\n"
                  << "    cf <F18-ID>      In F18-Vorgang navigieren\n"
                  << "    cf <AKT-ID>      In Akte navigieren\n"
                  << "    lf               F18-Vorgaenge und Akten listen\n"
                  << "    ..               Zurueck zum F16\n"
                  << "  Aktionen:\n"
                  << "    -f18 -n          Neuen F18-Vorgang anlegen\n"
                  << "    -akt -n          Neue Akte anlegen\n"
                  << "    -f22 " << cur.id << "   F22-Menue (Bearbeiten, F77, etc.)\n"
                  << "    -note " << cur.id << " <Text>   Schnellnotiz\n";
        if (t && !t->releaseWorkflowId.empty())
            std::cout << "    -f77 " << t->releaseWorkflowId << "  F77-Workflow\n";
        std::cout << "\n";
        break;
    }

    case EntityType::F18: {
        auto v = F18Operation::loadById(cur.id);
        std::string title = v ? v->title : cur.id;
        std::cout << "  F18: " << cur.id << " — " << title << "\n"
                  << "  " << std::string(52,'-') << "\n"
                  << "  Navigation:\n"
                  << "    cf <AKT-ID>      In Akte navigieren\n"
                  << "    lf               Schritte und Akten listen\n"
                  << "    ..               Zurueck zur F22\n"
                  << "  Aktionen:\n"
                  << "    -f18 " << cur.id << "   F18-Menue (Schritte, Bearbeiten)\n"
                  << "    -akt -n          Neue Akte zu diesem Vorgang\n"
                  << "    -note " << cur.id << " <Text>   Schnellnotiz\n\n";
        break;
    }

    case EntityType::AKT: {
        auto d = Folder::loadById(cur.id);
        std::string title = d ? d->title : cur.id;
        std::cout << "  AKT: " << cur.id << " — " << title << "\n"
                  << "  " << std::string(52,'-') << "\n"
                  << "  Navigation:\n"
                  << "    lf               Objekte in aktueller Revision listen\n"
                  << "    lf -rev          Alle Revisionen listen\n"
                  << "    ..               Zurueck\n"
                  << "  Aktionen:\n"
                  << "    -akt " << cur.id << "   Akten-Menue (Objekte hinzufuegen, checkout)\n"
                  << "    -note " << cur.id << " <Text>   Schnellnotiz\n\n";
        break;
    }

    default:
        std::cout << "  Keine kontextspezifischen Optionen.\n"
                  << "  -h fuer globale Hilfe.\n\n";
        break;
    }
}

// ── cf — change folder (navigate into entity) ─────────────────────────────────
//
// cf <id>   → resolve ID, push to nav stack, show lf
// cf ..     → pop nav stack
// cf        → show current location (same as lf)
//
void cmdCf(const std::vector<std::string>& args) {
    auto& nav = NavigationStack::instance();

    if (args.empty()) {
        // No arg: show current location
        cmdLf({});
        return;
    }

    std::string target = args[0];

    if (target == "..") {
        nav.pop();
        cmdLf({});
        return;
    }

    // Resolve: try to load as F16, F22, F18, AKT, PER
    // Check each type by prefix or direct load:
    auto tryLoad = [&]() -> bool {
        // Try F16:
        auto p = F16::loadById(target);
        if (p) {
            nav.push({EntityType::F16, p->projectId, p->title, p->regNumber.toString()});
            std::cout << "  -> " << Color::cyan("F16:" + p->regNumber.toString())
                      << "  " << p->title << "\n";
            cmdLf({});
            return true;
        }
        // Try F22:
        auto t = F22::loadById(target);
        if (t) {
            nav.push({EntityType::F22, t->taskId, t->title, t->regNumber.toString()});
            std::cout << "  -> " << Color::blue("F22:" + t->regNumber.toString())
                      << "  " << t->title << "\n";
            cmdLf({});
            return true;
        }
        // Try F18:
        auto v = F18Operation::loadById(target);
        if (v) {
            nav.push({EntityType::F18, v->operationId, v->title, v->operationId});
            std::cout << "  -> " << Color::magenta("F18:" + v->operationId)
                      << "  " << v->title << "\n";
            cmdLf({});
            return true;
        }
        // Try AKT:
        auto d = Folder::loadById(target);
        if (d) {
            nav.push({EntityType::AKT, d->folderId, d->title, d->folderId});
            std::cout << "  -> " << Color::yellow("AKT:" + d->folderId)
                      << "  " << d->title << "\n";
            cmdLf({});
            return true;
        }
        return false;
    };

    if (!tryLoad()) {
        printErr("Entitaet nicht gefunden: " + target);
        std::cout << "  Tipp: lf  zeigt verfuegbare Eintraege.\n";
    }
}

// ── getContextChildren: returns IDs + labels for Tab completion ──────────────
std::vector<std::pair<std::string,std::string>> getContextChildren() {
    auto& nav = NavigationStack::instance();
    auto cur = nav.current();
    std::vector<std::pair<std::string,std::string>> result;

    if (!cur.valid()) {
        // Top level: all F16
        for (auto& p : F16::loadAll())
            result.push_back({p->regNumber.toString(), p->title});
        return result;
    }

    switch (cur.type) {
    case EntityType::F16: {
        auto tasks = F22::loadForProject(cur.id);
        for (auto& t : tasks) result.push_back({t->regNumber.toString(), t->title});
        auto akten = Folder::loadForEntity("f16", cur.id);
        for (auto& d : akten) result.push_back({d->folderId, d->title});
        break;
    }
    case EntityType::F22: {
        auto f18s = F18Operation::loadForTask(cur.id);
        for (auto& v : f18s) result.push_back({v->operationId, v->title});
        auto akten = Folder::loadForEntity("f22", cur.id);
        for (auto& d : akten) result.push_back({d->folderId, d->title});
        break;
    }
    case EntityType::F18: {
        auto akten = Folder::loadForEntity("f18", cur.id);
        for (auto& d : akten) result.push_back({d->folderId, d->title});
        break;
    }
    default: break;
    }
    return result;
}

} // namespace CLI

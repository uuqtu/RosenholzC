// ============================================================
// cli_dok.cpp  —  DOK Dokument: Befehlshandler, Wizard, Menü
//
// Public functions:
//   cmdDok(args)                  — dispatch for 'rh -dok ...'
//   printDocument(d)              — see cli_utils.cpp
//   listDocuments(docs, title)    — see cli_utils.cpp
//   documentMenu(doc)             — interactive detail menu
//   documentBrowserMenu(proj, task) — filtered browser
//   createDocumentWizard(proj, task) — create under known parent
//   createDocumentWizardGuided()  — guided: pick parent entity first
//   attachDocumentDialog(proj, task) — attach-or-create dialog
// ============================================================
#include "cli_common.h"
#include "../core/OperationResult.h"
#include "../model/akt/FolderObject.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include "../mfs/MFSWriter.h"
#include "../model/akt/FolderRevision.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace CLI {

using namespace Rosenholz;

// ── createDocumentWizardGuided ────────────────────────────────
//
// Guided variant for 'rh -dok -n'.
// Shows a menu to pick the parent entity type, then lists the
// available entities of that type for the user to pick from,
// and finally calls createDocumentWizard with the chosen IDs.

std::shared_ptr<Folder> createDocumentWizardGuided() {
    hdr("AKT ANLEGEN — ENTITÄT WÄHLEN");
    std::cout << "  Wo soll das Dokument abgelegt werden?\n"
              << "  1.  F22 (Vorgangskartei)\n"
              << "  2.  F18 (Vorgang)\n"
              << "  0.  Abbrechen\n";
    int choice = readInt("Typ", 0, 2);
    if (choice == 0) return nullptr;

    if (choice == 99 /* F16 parent no longer supported */) {
        // Pick from all projects
        auto projects = F16::loadAll();
        if (projects.empty()) {
            std::cout << "  (keine F16-Karten vorhanden)\n";
            return nullptr;
        }
        hdr("F16 WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int pick = readInt("F16-Nr", 1, (int)projects.size());
        return createDocumentWizard("", "");
    }

    if (choice == 2) {
        // First pick the project, then the task
        auto projects = F16::loadAll();
        if (projects.empty()) {
            std::cout << "  (keine F16-Karten vorhanden)\n";
            return nullptr;
        }
        hdr("F16 WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int ppick = readInt("F16-Nr", 1, (int)projects.size());
        auto& proj = projects[ppick - 1];

        auto tasks = F22::loadForProject(proj->projectId);
        if (tasks.empty()) {
            std::cout << "  (keine F22-Vorgänge in diesem Projekt — lege unter Projekt ab)\n";
            return createDocumentWizard("", "");
        }
        hdr("F22 WÄHLEN");
        for (int i = 0; i < (int)tasks.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << tasks[i]->regNumber.toString()
                      << tasks[i]->title.substr(0, 36) << "\n";
        int tpick = readInt("F22-Nr", 1, (int)tasks.size());
        return createDocumentWizard(proj->projectId, tasks[tpick - 1]->taskId);
    }

    if (choice == 3) {
        // Pick from recent F18 operations
        auto ops = F18Operation::loadRecent(40);
        if (ops.empty()) {
            std::cout << "  (keine F18-Operationen vorhanden)\n";
            return nullptr;
        }
        hdr("F18 WÄHLEN");
        for (int i = 0; i < (int)ops.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << ops[i]->operationId.substr(0, 24)
                      << "  [" << std::setw(14) << ops[i]->operationType << "]  "
                      << ops[i]->title.substr(0, 30) << "\n";
        int opick = readInt("F18-Nr", 1, (int)ops.size());
        auto& op = ops[opick - 1];
        auto doc = createDocumentWizard(op->taskId, "");
        if (doc) {
            doc->f18OperationId = op->operationId;
            { auto r = doc->update(); (void)r; }
        }
        return doc;
    }

    return nullptr;
}

// ── cmdDok ────────────────────────────────────────────────────
//
// Dispatch table for 'rh -dok [args]':
//
//   rh -dok                          → list 20 most recent docs
//   rh -dok -n                       → guided: pick parent entity, create doc
//   rh -dok -f16                     → guided with F16 as parent type
//   rh -dok -f22                     → guided with F22 as parent type
//   rh -dok -f18                     → guided with F18 as parent type
//   rh -dok <doc-id>                 → open documentMenu
//   rh -dok <project-id>             → open documentBrowserMenu
//   rh -dok <project-id> (anything)  → create doc under project

void cmdAkt(const std::vector<std::string>& args) {

    // No arguments: list 20 most recent documents
    if (args.empty()) {
        auto all = Folder::loadRecent(20);
        if (all.empty()) { std::cout << "  (keine Akten)\n"; return; }
        std::cout << "  " << std::left
                  << std::setw(26) << "ID (für rh -akt <id>)"
                  << std::setw(14) << "STATUS"
                  << std::setw(12) << "VERSION"
                  << "TITEL\n"
                  << "  " << std::string(70, '-') << "\n";
        for (auto& d : all)
            std::cout << "  " << std::setw(26) << d->folderId.substr(0, 24)
                      << std::setw(14) << (std::string("[") + revStateToString(d->currentRevisionState()) + "]")
                      << std::setw(12) << ("v" + d->version)
                      << d->title.substr(0, 32) << "\n";
        std::cout << "  " << all.size() << " Dokument(e)\n";
        return;
    }

    // -n  —  fully guided creation (pick entity type, then entity)
    if (args[0] == "-s") {
        std::string q;
        for (size_t i=1; i<args.size(); ++i) { if(!q.empty()) q+=" "; q+=args[i]; }
        if (q.empty()) { printErr("-s benoetigt einen Suchbegriff"); return; }
        auto all = Folder::loadRecent(9999);
        bool found=false;
        for (auto& d : all) {
            if (matchesPattern(d->title, q) || matchesPattern(d->folderId, q)) {
                std::cout << "  AKT  " << std::left << std::setw(28) << d->folderId
                          << " " << d->title << "  [" << d->docType << "]\n";
                found=true;
            }
        }
        if(!found) std::cout << "  (keine Akte gefunden fuer: " << q << ")\n";
        return;
    }

    if (args[0] == "-n" || args[0] == "--neu") {
        auto doc = createDocumentWizardGuided();
        if (doc) printOk("  >> Dokument angelegt: " + doc->folderId + "  " + doc->title);
        return;
    }

    // -f16  —  guided creation under F16 (skips entity-type question)
    if (args[0] == "-f16") {
        auto projects = F16::loadAll();
        if (projects.empty()) { std::cout << "  (keine F16-Karten)\n"; return; }
        hdr("AKT ANLEGEN — F16 WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int pick = readInt("F16-Nr", 1, (int)projects.size());
        auto doc = createDocumentWizard("", "");
        if (doc) printOk("  >> Dokument angelegt: " + doc->folderId + "  " + doc->title);
        return;
    }

    // -f22  —  guided creation under F22
    if (args[0] == "-f22") {
        auto projects = F16::loadAll();
        if (projects.empty()) { std::cout << "  (keine F16-Karten)\n"; return; }
        hdr("AKT ANLEGEN — F16 WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int ppick = readInt("F16-Nr", 1, (int)projects.size());
        auto& proj = projects[ppick - 1];
        auto tasks = F22::loadForProject(proj->projectId);
        if (tasks.empty()) {
            std::cout << "  (keine F22-Vorgänge — lege unter Projekt ab)\n";
            auto doc = createDocumentWizard("", "");
            if (doc) printOk("  >> Dokument angelegt: " + doc->folderId + "  " + doc->title);
            return;
        }
        hdr("F22 WÄHLEN");
        for (int i = 0; i < (int)tasks.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << tasks[i]->regNumber.toString()
                      << tasks[i]->title.substr(0, 36) << "\n";
        int tpick = readInt("F22-Nr", 1, (int)tasks.size());
        auto doc = createDocumentWizard(proj->projectId, tasks[tpick - 1]->taskId);
        if (doc) printOk("  >> Dokument angelegt: " + doc->folderId + "  " + doc->title);
        return;
    }

    // -f18  —  guided creation linked to F18
    if (args[0] == "-f18") {
        auto ops = F18Operation::loadRecent(40);
        if (ops.empty()) { std::cout << "  (keine F18-Operationen)\n"; return; }
        hdr("AKT ANLEGEN — F18 WÄHLEN");
        for (int i = 0; i < (int)ops.size(); ++i)
            std::cout << "  " << std::setw(4)  << (i + 1)
                      << "  " << std::setw(26) << ops[i]->operationId.substr(0, 24)
                      << "  [" << std::setw(14) << ops[i]->operationType << "]  "
                      << ops[i]->title.substr(0, 28) << "\n";
        int pick = readInt("F18-Nr", 1, (int)ops.size());
        auto& op = ops[pick - 1];
        auto doc = createDocumentWizard(op->taskId, "");
        if (doc) {
            doc->f18OperationId = op->operationId;
            { auto r = doc->update(); (void)r; }
            printOk("  >> Dokument angelegt: " + doc->folderId + "  " + doc->title);
        }
        return;
    }

    // All remaining paths require a valid ID
    if (!isId(args[0])) { printErr("Ungültiges Argument: " + args[0]
                            + "  (erwartet ID, -n, -f16, -f22, oder -f18)"); return; }

    // Try as document ID → open documentMenu
    auto doc = Folder::loadById(args[0]);
    if (doc) {
        documentMenu(doc);
        return;
    }

    // Try as project ID
    auto proj = F16::loadById(args[0]);
    if (!proj) { printErr("ID nicht gefunden: " + args[0]); return; }
    // v7: list Akten for this project — use cd <id> to navigate
    auto docs = Folder::loadForEntity("f16", proj->projectId);
    if (docs.empty()) { std::cout << "  (keine Akten fuer " << args[0] << ")\n"; return; }
    std::cout << "  Akten fuer " << proj->regNumber.toString() << ":\n";
    for (auto& d : docs)
        std::cout << "  " << std::left << std::setw(26) << d->folderId
                  << "  " << d->title.substr(0,36) << "\n";
    std::cout << "  cd <AKT-ID>  zum Navigieren\n";
}

// ── createDocumentWizard ──────────────────────────────────────
//
// Step-by-step wizard for registering a new document.
// Parent project ID (and optionally task ID) must be provided.
// Source: local file, URL download, or new empty file.

std::shared_ptr<Rosenholz::Folder> createDocumentWizard(
    const std::string& taskId, const std::string& f18OpId)
{
    using namespace Rosenholz;

    hdr("AKTE ANLEGEN / REGISTRIEREN");

    // ── Titel + Typ ─────────────────────────────────────────────
    std::string title = readLine("Dokumententitel: ");
    if (title.empty()) return nullptr;

    std::cout << "  Typ:\n"
              << "    1.Bericht        2.Spezifikation  3.Vertrag\n"
              << "    4.Schriftverkehr 5.Nachweis       6.Plan\n"
              << "    7.Protokoll      8.Archiv         9.Sonstiges\n"
              << "    10.Analyse\n";
    int dt = readInt("Typ", 1, 10);
    static const char* dtypes[] = {
        "report","specification","contract","correspondence",
        "evidence","plan","minutes","archive","other","analysis"};
    std::string docType = dtypes[dt-1];

    // ── Dokument-Datensatz speichern (keine Datei — Objekte später hinzufügen) ──
    // URLs gehören zu Objekten, nicht zum Dokument-Container.
    auto doc = Folder::create(title, docType, taskId, f18OpId);
    doc->format      = "pdf";  // default; overridden when a file is attached
    doc->dateCreated = nowIso();

    if (!opOk(doc->save())) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }
    doc->revise("Revision 1 — Erstanlage");

    // ── Gemeinsame optionale Felder ───────────────────────────────
    doc->summary        = readOpt("Kurzbeschreibung (optional): ");
    doc->authorId       = readOpt("Autor Person-ID (optional): ");
    doc->classification = readOpt("Einstufung (intern/vertraulich/öffentlich, leer=intern): ");
    if (doc->classification.empty()) doc->classification = "intern";
    { auto r = doc->update(); (void)r; }

    std::cout << "  >> Dokument registriert: " << doc->folderId << "\n"
              << "  >> Titel  : " << doc->title << "\n"
              << "  >> Dateien können über Option 5 (Objekt hinzufügen) im Dokumentmenü hinzugefügt werden.\n";

    // Write MFS index file (the .txt metadata card)
    MFSWriter::writeDocument(*doc, Config::instance().mfsPath());

    std::cout << "  >> Revision 1 angelegt (in_work)\n";
    return doc;
}



// ── revisionMenu (helper for documentMenu) ────────────────────
//
// Shows the current FolderRevision lifecycle state and lets
// the user perform a state transition.
// Called from documentMenu option 2 (Revisions-Lifecycle).

static void revisionMenu(std::shared_ptr<Folder> doc) {
    using namespace Rosenholz;
    while (true) {
        auto rev = FolderRevision::currentRevision(doc->folderId);
        if (!rev) {
            std::cout << "  (keine aktive Revision)\n";
            return;
        }
        hdr("REVISION " + std::to_string(rev->rev) + "  — " + doc->folderId.substr(0, 22));
        std::cout << "  Zustand    : " << rev->revState << "\n";
        std::cout << "  Inhalt-SHA : " << (rev->contentHash.empty() ? "—" : rev->contentHash.substr(0, 20)) << "\n";
        std::cout << "  Groesse    : " << rev->contentSize << " Bytes\n";
        std::cout << "  Erstellt   : " << fdate(rev->createdAt) << "\n";
        if (!rev->changeNote.empty())
            std::cout << "  Notiz      : " << rev->changeNote.substr(0, 50) << "\n";
        std::cout << "\n"
                     "  Erlaubte Übergänge:\n"
                     "    1. in_work        4. locked\n"
                     "    2. pre_released   5. closed\n"
                     "    2. released       0. Zurück\n";
        static const char* states[] = {"in_work","pre_released","released","locked","closed"};
        int ch = readInt("Zielzustand", 0, 5);
        if (ch == 0) return;
        std::string target = states[ch - 1];
        if (!FolderRevision::isTransitionAllowed(rev->revState,
                revStateFromString(target),
                Config::instance().admin().enabled)) {
            std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_TRANSITION_NOT_ALLOWED)
                      << "\n";
            std::cout << "  >> Aktuell: " << rev->revState
                      << " → Ziel: " << target << "\n";
            continue;
        }
        if (rev->transitionState(target)) {
            // status is now computed from revision — no manual assignment needed
            { auto r = doc->update(); (void)r; }
            std::cout << "  >> Revision in Zustand: " << target << "\n";
        } else {
            std::cout << "  >> Fehler beim Zustandswechsel.\n";
        }
        return;
    }
}

// ── documentMenu and documentBrowserMenu ─────────────────────
//
// Interactive menus from DocumentMenu.cpp.


// ── DocumentMenu context ──────────────────────────────────────────────────────
// Passed to every handler so each function has exactly the state it needs.
// No handler looks up anything globally — all reads go through this struct.
struct DocMenuCtx {
    std::shared_ptr<Folder>                    doc;
    std::shared_ptr<FolderRevision>            cur;
    bool                                          inWork;
    bool                                          immutable;
    uint32_t                                      curRevNum;
    std::vector<std::shared_ptr<FolderObject>> curObjs;
};

// ch==1: dokHandleEditFields
static bool dokHandleEditFields(DocMenuCtx& ctx) {

    // Felder bearbeiten
    if (!ctx.doc->canEdit()) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
        return true;  // was: continue (handler)
    }
    hdr("BEARBEITEN — " + ctx.doc->folderId.substr(0, 24));
    auto s = [](const std::string& p) { return readOpt(p + " (leer=behalten): "); };
    auto apply = [](std::string& f, const std::string& v) { if (!v.empty()) f = v; };
    apply(ctx.doc->title,          s("Titel"));
    apply(ctx.doc->docCategory,    s("Kategorie"));
    apply(ctx.doc->version,        s("Version"));
    apply(ctx.doc->classification, s("Einstufung (intern/vertraulich/oeffentlich)"));
    apply(ctx.doc->summary,        s("Kurzbeschreibung"));
    apply(ctx.doc->authorId,       s("Autor-ID"));
    apply(ctx.doc->approvedBy,     s("Genehmiger-ID"));
    { auto r = ctx.doc->update(); (void)r; }
    std::cout << "  >> Gespeichert.\n";
    return true;
}

// ch==2: dokHandleNewRevision
static bool dokHandleNewRevision(DocMenuCtx& ctx) {

    // Neue Revision anlegen
    if (!ctx.doc->canRevise()) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_IS_IN_WORK) << "\n";
        std::cout << "  >> Starten Sie: rh -f77 -start " << ctx.doc->folderId << "\n";
        return true;  // was: continue (handler)
    }
    std::string note = readLine("Revisionsnotiz (Pflicht): ");
    if (note.empty()) return true;
    std::string by = readOpt("Erstellt von (Person-ID, leer=Autor): ");
    auto newRev = ctx.doc->revise(note, by);
    if (newRev) {
        std::cout << "  >> Revision " << newRev->rev << " angelegt (in_work).\n";
        // Navigate directly to the new in_work revision
        ctx.curRevNum = newRev->rev;
        ctx.curObjs   = FolderObject::loadForRevision(ctx.doc->folderId, ctx.curRevNum);
        std::cout << "  >> Aktuelle Ansicht: Rev " << ctx.curRevNum << " [in_work]\n";
    } else {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_SAVE_FAILED) << "\n";
    }
    return true;
}

// ch==3: dokHandleWorkflow
static bool dokHandleWorkflow(DocMenuCtx& ctx) {

    // Workflow starten / anzeigen
    hdr("F77 / FREIGABE — " + ctx.doc->folderId.substr(0, 26));
    if (ctx.doc->workflowId.empty()) {
        std::cout << "  Kein aktiver F77.\n";
        if (readInt("  1.F77 starten  0.Zurueck", 0, 1) == 1) {
            std::string wid = startWfInstanceWizard("akt", ctx.doc->folderId);
            if (!wid.empty()) instanceMenu(wid);
        }
    } else {
        auto wf = F77W::loadById(ctx.doc->workflowId);
        std::cout << "  F77-ID : " << ctx.doc->workflowId.substr(0, 36) << "\n"
                  << "  Status      : " << (wf ? toString(wf->status) : "unbekannt") << "\n";
        if (wf && wf->status == WorkflowStatus::ACTIVE) {
            if (readInt("  1.Oeffnen  0.Zurueck", 0, 1) == 1)
                instanceMenu(ctx.doc->workflowId);
        } else {
            std::cout << "  (F77 abgeschlossen/abgebrochen)\n";
            if (readInt("  1.Neuen F77 starten  0.Zurueck", 0, 1) == 1) {
                std::string wid = startWfInstanceWizard("akt", ctx.doc->folderId);
                if (!wid.empty()) instanceMenu(wid);
            }
        }
    }
    return true;
}

// ch==4: dokHandleListObjects
static bool dokHandleListObjects(DocMenuCtx& ctx) {

    // Objekte auflisten
    hdr("OBJEKTE — Rev " + std::to_string(ctx.curRevNum)
        + " | " + ctx.doc->folderId.substr(0, 22));
    if (ctx.curObjs.empty()) {
        std::cout << "  (keine Objekte in dieser Revision)\n";
        std::cout << "  Tipp: Option 5 um Dateien zu importieren (nur in in_work).\n";
    } else {
        std::cout << "  " << std::left
                  << std::setw(4) << "Nr."
                  << std::setw(42) << "Dateiname"
                  << std::setw(8)  << "Status"
                  << "Größe\n"
                  << "  " << std::string(72, '-') << "\n";
        for (size_t i = 0; i < ctx.curObjs.size(); ++i) {
            auto& o = ctx.curObjs[i];
            std::cout << "  " << std::setw(4) << (i+1)
                      << std::setw(42) << o->displayName().substr(0, 40)
                      << std::setw(8)  << (o->committed ? "[LMDB]" : "[MFS] ")
                      << (o->contentSize / 1024) << " KB\n";
            if (!o->filePath.empty())
                std::cout << "       " << o->filePath << "\n";
        }
    }
    return true;
}

// ch==5: dokHandleAddObject
// Three options:
//   1. Lokal kopieren  — kopiert eine lokale Datei ins MFS
//   2. URL-Download    — lädt URL herunter; Office-Dateien → PDF-Konvertierung
//   3. Leere Datei     — Stub anlegen und sofort öffnen; Templates aus tpl/
static bool dokHandleAddObject(DocMenuCtx& ctx) {

    if (!ctx.inWork) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
        return true;
    }

    hdr("DATEI HINZUFÜGEN");
    std::cout << "  1. Lokal kopieren   (Datei von Pfad ins MFS kopieren)\n"
              << "  2. URL herunterladen (Download; Office → PDF Konvertierung)\n"
              << "  3. Leere Datei anlegen (Stub + Template, dann öffnen)\n"
              << "  0. Abbrechen\n";
    int choice = readInt("Wahl", 0, 3);
    if (choice == 0) return true;

    const std::string& base = Config::instance().basePath();

    // ── Option 1: Lokale Datei ─────────────────────────────────────────────
    if (choice == 1) {
        std::string srcPath = readLine("Lokaler Dateipfad: ");
        if (srcPath.empty() || cliIsInterrupted()) return true;
        std::string label1 = readOpt("  Bezeichnung (sprechend, leer=Dateiname): ");
        std::string desc1  = readOpt("  Beschreibung (optional): ");
        OperationResult res = OperationResult::OPERATION_ACK;
        auto obj = FolderObject::importFile(
            ctx.doc->folderId, ctx.curRevNum, srcPath, res, label1, desc1);
        if (opOk(res) && obj)
            std::cout << "  >> Importiert: " << obj->displayName() << "\n"
                      << "  >> MFS-Datei:  " << obj->storedFileName << "\n";
        else
            std::cout << "  >> " << opResultMessage(res) << "\n";
        return true;
    }

    // ── Option 2: URL-Download ─────────────────────────────────────────────
    if (choice == 2) {
        std::string url = readLine("URL: ");
        if (url.empty() || cliIsInterrupted()) return true;

        std::cout << "  Herunterladen...\n";
        std::string tmpDir = FileOps::joinPath(base, "documents", "tmp");
        FileOps::makeDirs(tmpDir);
        std::string downloaded = FileOps::downloadUrl(url, tmpDir);
        if (downloaded.empty()) {
            std::cout << "  >> Download fehlgeschlagen — bitte URL prüfen.\n";
            return true;
        }

        // Detect file type — Office files get PDF conversion
        std::string ext = FileOps::extension(downloaded);
        if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
        static const std::vector<std::string> officeExts =
            {"xls","xlsx","doc","docx","ppt","pptx","odt","ods","odp"};
        bool isOffice = false;
        for (auto& e : officeExts) if (ext == e) { isOffice = true; break; }

        std::string toImport = downloaded;
#ifndef _WIN32
        if (isOffice) {
            std::cout << "  Office-Datei erkannt (" << ext << ") — konvertiere zu PDF...\n";
            std::string pdfPath = downloaded.substr(0, downloaded.rfind('.')) + ".pdf";
            std::string cmd = "libreoffice --headless --convert-to pdf"
                              " --outdir \"" + tmpDir + "\""
                              " \"" + downloaded + "\" 2>/dev/null";
            if (std::system(cmd.c_str()) == 0 && FileOps::fileExists(pdfPath)) {
                FileOps::deleteFile(downloaded);
                toImport = pdfPath;
                ext = "pdf";
                std::cout << "  >> PDF erstellt.\n";
            } else {
                std::cout << "  >> Konvertierung fehlgeschlagen — Original wird importiert.\n";
            }
        }
#endif

        std::string label2 = readOpt("  Bezeichnung (sprechend, leer=Dateiname): ");
        std::string desc2  = readOpt("  Beschreibung (optional): ");
        OperationResult res = OperationResult::OPERATION_ACK;
        auto obj = FolderObject::importFile(
            ctx.doc->folderId, ctx.curRevNum, toImport, res, label2, desc2);
        if (opOk(res) && obj) {
            obj->sourceUrl = url;
            obj->update();
            std::cout << "  >> Importiert: " << obj->displayName() << "\n"
                      << "  >> MFS-Datei:  " << obj->storedFileName << "\n";
        } else {
            std::cout << "  >> " << opResultMessage(res) << "\n";
        }
        FileOps::deleteFile(toImport);
        return true;
    }

    // ── Option 3: Leere Datei / Template ──────────────────────────────────
    if (choice == 3) {
        std::cout << "  Dateiformat wählen:\n"
                  << "    1. txt           2. txt — Template\n"
                  << "    3. docx          4. docx — Template\n"
                  << "    5. pptx          6. pptx — Template\n"
                  << "    7. xlsx\n"
                  << "    8. Anderes Format eingeben\n";
        int fmt = readInt("Format", 1, 8);

        std::string ext, tplFile;
        bool useTemplate = false;
        switch (fmt) {
            case 1: ext = "txt";  break;
            case 2: ext = "txt";  tplFile = "template.txt";  useTemplate = true; break;
            case 3: ext = "docx"; break;
            case 4: ext = "docx"; tplFile = "template.docx"; useTemplate = true; break;
            case 5: ext = "pptx"; break;
            case 6: ext = "pptx"; tplFile = "template.pptx"; useTemplate = true; break;
            case 7: ext = "xlsx"; break;
            default:
                ext = readOpt("Format/Erweiterung (z.B. pdf, md, csv): ");
                if (ext.empty()) ext = "txt";
        }

        // Build target path in MFS
        const std::string& mfs = Config::instance().mfsPath();
        std::string sane     = sanitiseRegNr(ctx.doc->folderId);
        std::string safeName = FileOps::sanitizeFilename(ctx.doc->title);
        if (safeName.size() > 40) safeName = safeName.substr(0, 40);
        std::string parentSane = sanitiseRegNr(
            ctx.doc->taskId.empty() ? ctx.doc->f18OperationId : ctx.doc->taskId);
        std::string dir = FileOps::joinPath(mfs, "AKT", parentSane);
        FileOps::makeDirs(dir);
        std::string fname = sane + "_" + safeName + "_v"
                          + std::to_string(ctx.curRevNum) + "." + ext;
        std::string fpath = FileOps::joinPath(dir, fname);

        // Populate from template or create stub
        bool written = false;
        if (useTemplate) {
            std::string tplDir = FileOps::joinPath(base, "tpl");
            std::string tplPath = FileOps::joinPath(tplDir, tplFile);
            if (FileOps::fileExists(tplPath)) {
                written = FileOps::copyFile(tplPath, fpath);
                if (written) std::cout << "  >> Template kopiert: " << tplFile << "\n";
                else         std::cout << "  >> Template-Kopie fehlgeschlagen — Stub wird erstellt.\n";
            } else {
                std::cout << "  >> Template nicht gefunden: " << tplPath
                          << "\n  >> Leere Datei wird angelegt.\n";
            }
        }
        if (!written) {
            // Plain stub (only for txt; others stay empty binary)
            std::string stub = (ext == "txt")
                ? "ROSENHOLZ PM — " + ctx.doc->title + "\n"
                  "ID      : " + ctx.doc->folderId + "\n"
                  "Erstellt: " + Rosenholz::nowIso() + "\n--- Inhalt ---\n"
                : "";
            if (ext == "txt")
                FileOps::writeTextFile(fpath, stub);
            else {
                // Create an empty binary file for Office formats
                std::ofstream f(fpath, std::ios::binary);
                (void)f; // just touch the file
            }
        }

        // Name + Beschreibung
        std::string label3 = readOpt("  Bezeichnung (sprechend, leer=Dateiname): ");
        std::string desc3  = readOpt("  Beschreibung (optional): ");
        // Import into FolderObject
        OperationResult res = OperationResult::OPERATION_ACK;
        auto obj = FolderObject::importFile(
            ctx.doc->folderId, ctx.curRevNum, fpath, res, label3, desc3);
        if (opOk(res) && obj) {
            std::cout << "  >> Angelegt: " << obj->displayName() << "\n"
                      << "  >> MFS-Datei: " << obj->storedFileName << "\n";
            ctx.doc->openFile("edit");
        } else {
            std::cout << "  >> " << opResultMessage(res) << "\n";
        }
        return true;
    }

    return true;
}


// ch==6: dokHandleRemoveObject
static bool dokHandleRemoveObject(DocMenuCtx& ctx) {

    // Objekt entfernen — nur in in_work
    if (!ctx.inWork || ctx.curObjs.empty()) {
        auto r = ctx.inWork ? OperationResult::INVALID_ARGUMENT
                        : OperationResult::DOC_REV_NOT_IN_WORK;
        std::cout << "  >> " << opResultMessage(r) << "\n";
        return true;  // was: continue (handler)
    }
    for (size_t i = 0; i < ctx.curObjs.size(); ++i)
        std::cout << "  " << (i+1) << ". " << ctx.curObjs[i]->displayName() << "\n";
    int sel = readInt("Objekt entfernen (Nummer)", 1, (int)ctx.curObjs.size());
    if (yesno("Objekt '" + ctx.curObjs[sel-1]->displayName() + "' entfernen?")) {
        if (!ctx.curObjs[sel-1]->filePath.empty())
            FileOps::deleteFile(ctx.curObjs[sel-1]->filePath);
        ctx.curObjs[sel-1]->remove();
        std::cout << "  >> Objekt entfernt.\n";
    }
    return true;
}

// ch==7: dokHandleOpenObject
static bool dokHandleOpenObject(DocMenuCtx& ctx) {

    // Objekt öffnen
    if (ctx.curObjs.empty()) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_FILE_NOT_FOUND) << "\n";
        return true;  // was: continue (handler)
    }
    // Select object
    for (size_t i = 0; i < ctx.curObjs.size(); ++i)
        std::cout << "  " << (i+1) << ". "
                  << std::left << std::setw(40) << ctx.curObjs[i]->displayName()
                  << "  " << (ctx.curObjs[i]->checkedOut ? "[AUSGECHECKT]" : "")
                  << "\n";
    int sel = readInt("Objekt oeffnen", 1, (int)ctx.curObjs.size());
    auto& obj = ctx.curObjs[sel-1];

    if (!ctx.inWork) {
        // Not in_work: always open from tmp (read-only, changes not tracked)
        std::cout << "  (Nur Lesen — Revision ist " << ctx.cur->revState << ")\n";
        bool wco = false;
        std::string path = obj->openObject(false, wco);
        if (path.empty())
            std::cout << "  >> " << opResultMessage(OperationResult::DOC_FILE_NOT_FOUND) << "\n";
        else
            std::cout << "  >> Geöffnet (tmp, Änderungen werden nicht gespeichert): " << path << "\n";
    } else {
        // In_work: offer checkout or tmp view
        if (obj->checkedOut) {
            // Already checked out — open directly
            std::cout << "  Objekt ist ausgecheckt unter: " << obj->checkoutPath << "\n";
            bool wco = true;
            obj->openObject(true, wco);
            // After user returns: offer checkin
            std::cout << "  Dokument geschlossen?\n";
            if (yesno("  Einchecken?")) {
                auto ci = obj->checkinObject();
                if (opOk(ci))
                    std::cout << "  >> Eingecheckt.\n";
                else
                    std::cout << "  >> " << opResultMessage(ci) << "\n";
            }
        } else {
            std::cout << "  Objekt ist in_work.\n"
                      << "    1. Auschecken (Änderungen werden gespeichert)\n"
                      << "    2. Nur lesen   (tmp-Kopie, Änderungen nicht gespeichert)\n";
            int mode = readInt("Modus", 1, 2);
            if (mode == 1) {
                // Checkout
                std::string path = obj->checkoutObject();
                if (path.empty()) {
                    std::cout << "  >> " << opResultMessage(OperationResult::IO_ERROR) << "\n";
                } else {
                    std::cout << "  >> Ausgecheckt: " << path << "\n";
                    bool wco = true;
                    obj->openObject(true, wco);
                    // After user returns: detect if document was closed
                    std::cout << "  Dokument geschlossen? ";
                    if (yesno("Einchecken?")) {
                        auto ci = obj->checkinObject();
                        if (opOk(ci))
                            std::cout << "  >> Eingecheckt.\n";
                        else
                            std::cout << "  >> " << opResultMessage(ci) << "\n";
                    }
                }
            } else {
                // Tmp copy
                bool wco = false;
                std::string path = obj->openObject(true, wco);
                if (!path.empty())
                    std::cout << "  >> Geöffnet (tmp, Änderungen nicht gespeichert): " << path << "\n";
                else
                    std::cout << "  >> " << opResultMessage(OperationResult::DOC_FILE_NOT_FOUND) << "\n";
            }
        }
    }
    return true;
}

// ch==8: dokHandleCheckout
static bool dokHandleCheckout(DocMenuCtx& ctx) {

    // Checkout eines Objekts
    if (!ctx.inWork) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
        return true;  // was: continue (handler)
    }
    if (ctx.curObjs.empty()) {
        std::cout << "  >> Keine Objekte vorhanden. Zuerst Dateien hinzufügen (Option 5).\n";
        return true;  // was: continue (handler)
    }
    for (size_t i = 0; i < ctx.curObjs.size(); ++i)
        std::cout << "  " << (i+1) << ". "
                  << ctx.curObjs[i]->displayName()
                  << (ctx.curObjs[i]->checkedOut ? "  [AUSGECHECKT]" : "") << "\n";
    int sel = readInt("Objekt auschecken", 1, (int)ctx.curObjs.size());
    auto& obj = ctx.curObjs[sel-1];
    if (obj->checkedOut) {
        std::cout << "  >> Bereits ausgecheckt unter: " << obj->checkoutPath << "\n";
        return true;  // was: continue (handler)
    }
    std::string dest = readOpt("Zielverzeichnis (leer = MFS-Rev-Ordner): ");
    std::string path = obj->checkoutObject(dest);
    if (!path.empty())
        std::cout << "  >> Ausgecheckt: " << path << "\n";
    else
        std::cout << "  >> " << opResultMessage(OperationResult::IO_ERROR) << "\n";
    return true;
}

// ch==9: dokHandleCheckin
static bool dokHandleCheckin(DocMenuCtx& ctx) {

    // Checkin eines ausgecheckten Objekts
    if (!ctx.inWork) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
        return true;  // was: continue (handler)
    }
    // List checked-out objects
    std::vector<std::shared_ptr<FolderObject>> coObjs;
    for (auto& o : ctx.curObjs)
        if (o->checkedOut) coObjs.push_back(o);
    if (coObjs.empty()) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_NO_CHECKOUT) << "\n";
        return true;  // was: continue (handler)
    }
    for (size_t i = 0; i < coObjs.size(); ++i)
        std::cout << "  " << (i+1) << ". " << coObjs[i]->displayName()
                  << "  →  " << coObjs[i]->checkoutPath << "\n";
    int sel = readInt("Objekt einchecken", 1, (int)coObjs.size());
    std::string src = readOpt("Dateipfad (leer = ausgecheckte Datei): ");
    auto ci = coObjs[sel-1]->checkinObject(src);
    if (opOk(ci))
        std::cout << "  >> Eingecheckt. Revisions-Inhalt aktualisiert.\n";
    else
        std::cout << "  >> " << opResultMessage(ci) << "\n";
    return true;
}

// ch==10: dokHandleRevert
static bool dokHandleRevert(DocMenuCtx& ctx) {

    // Revert eines Objekts auf Vorgänger-Revision
    if (!ctx.inWork) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
        return true;  // was: continue (handler)
    }
    if (!ctx.cur || ctx.cur->parentRev == 0) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_NO_PARENT_REV)
                  << "\n  >> Revert ist erst ab der zweiten Revision möglich.\n";
        return true;  // was: continue (handler)
    }
    if (ctx.curObjs.empty()) {
        std::cout << "  >> Keine Objekte in dieser Revision.\n";
        return true;  // was: continue (handler)
    }
    std::cout << "  Welches Objekt auf Rev " << ctx.cur->parentRev << " zurücksetzen?\n";
    for (size_t i = 0; i < ctx.curObjs.size(); ++i)
        std::cout << "  " << (i+1) << ". " << ctx.curObjs[i]->displayName() << "\n";
    std::cout << "  " << (ctx.curObjs.size()+1) << ". Alle Objekte zurücksetzen\n";
    int sel = readInt("Auswahl", 1, (int)ctx.curObjs.size()+1);

    if (yesno("Objekt(e) auf Zustand von Rev " + std::to_string(ctx.cur->parentRev) + " zurücksetzen?")) {
        int ok_count = 0, fail_count = 0;
        if (sel <= (int)ctx.curObjs.size()) {
            auto rv = ctx.curObjs[sel-1]->revertObject(ctx.cur->parentRev);
            if (opOk(rv)) { ok_count++; std::cout << "  >> Zurückgesetzt.\n"; }
            else { fail_count++; std::cout << "  >> " << opResultMessage(rv) << "\n"; }
        } else {
            for (auto& o : ctx.curObjs) {
                auto rv = o->revertObject(ctx.cur->parentRev);
                if (opOk(rv)) ok_count++;
                else { fail_count++;
                       std::cout << "  >> " << o->displayName() << ": " << opResultMessage(rv) << "\n"; }
            }
            std::cout << "  >> " << ok_count << " Objekt(e) zurückgesetzt, "
                      << fail_count << " Fehler.\n";
        }
    }
    return true;
}


// ch==12: dokHandleVersionHistory
static bool dokHandleVersionHistory(DocMenuCtx& ctx) {

    // Versionsverlauf
    auto vl = ctx.doc->loadVersions();
    hdr("REVISIONSVERLAUF (" + std::to_string(vl.size()) + ")");
    for (auto& v : vl)
        std::cout << "  Rev " << std::setw(3) << v.versionNumber
                  << "  " << v.createdAt.substr(0, 16)
                  << "  " << v.changeNote.substr(0, 40) << "\n";
    return true;
}

// ch==13: dokHandleMfsWrite
static bool dokHandleMfsWrite(DocMenuCtx& ctx) {

    MFSWriter::writeDocument(*ctx.doc, Config::instance().mfsPath());
    std::cout << "  >> MFS-Datei geschrieben.\n";
    return true;
}

// ch==14: dokHandleDelete
static bool dokHandleDelete(DocMenuCtx& ctx) {

    std::cout << "  Dokument " << ctx.doc->folderId << " loeschen? (ja): ";
    std::string c; std::getline(std::cin, c);
    if (c == "ja") { ctx.doc->remove(); std::cout << "  >> Geloescht.\n"; return false; }
    return true;
}

// ch==15: dokHandlePrint
static bool dokHandlePrint(DocMenuCtx& ctx) {

    // ── Drucken: Objekt als PDF in mfs/TEMP/ speichern ──────────────
    if (ctx.curObjs.empty()) {
        std::cout << "  >> Keine Objekte in dieser Revision.\n";
        return true;  // was: continue (handler)
    }
    for (size_t i = 0; i < ctx.curObjs.size(); ++i)
        std::cout << "  " << (i+1) << ". " << ctx.curObjs[i]->displayName() << "\n";
    int sel = readInt("Objekt drucken", 1, (int)ctx.curObjs.size());
    auto& obj = ctx.curObjs[sel-1];

    // Resolve source path
    std::string srcPath = obj->filePath;
    if (srcPath.empty() || !FileOps::fileExists(srcPath)) {
        // Extract from LMDB via tmp copy (reuse open mechanism)
        bool wco = false;
        srcPath = obj->openObject(false, wco);
    }
    if (srcPath.empty()) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_FILE_NOT_FOUND) << "\n";
        return true;  // was: continue (handler)
    }

    // Output: mfs/TEMP/{objectId}_{originalName}.pdf
    std::string tmpDir = FileOps::joinPath(
        Config::instance().mfsPath(), "TEMP");
    FileOps::makeDirs(tmpDir);
    std::string pdfName = obj->objectId.substr(obj->objectId.rfind(':')+1)
                        + "_" + obj->displayName() + ".pdf";
    std::string pdfPath = FileOps::joinPath(tmpDir, pdfName);

    // Convert to PDF via LibreOffice (headless) if available, else copy as-is
#ifdef _WIN32
    // On Windows, PDF conversion requires a platform-specific tool.
    // For now, use a simple file copy as fallback.
    int rc = 1;  // force fallback
#else
    std::string cmd = std::string("libreoffice --headless --convert-to pdf")
                    + " --outdir \"" + tmpDir + "\""
                    + " \"" + srcPath + "\"" + " 2>/dev/null";
    int rc = std::system(cmd.c_str());
#endif
    if (rc == 0) {
        std::cout << "  >> PDF erstellt: " << pdfPath << "\n";
    } else {
        // Fallback: plain copy with .pdf extension
        FileOps::copyFile(srcPath, pdfPath, true);
        std::cout << "  >> Datei kopiert (kein LibreOffice): " << pdfPath << "\n";
    }
    return true;
}

// ch==16: dokHandleRevisionSwitch
static bool dokHandleRevisionSwitch(DocMenuCtx& ctx) {

    // ── Revision wechseln — zwischen Rev 1…N navigieren ─────────────
    auto allRevs = FolderRevision::loadAllRevisions(ctx.doc->folderId);
    if (allRevs.size() <= 1) {
        std::cout << "  >> Nur eine Revision vorhanden.\n";
        return true;  // was: continue (handler)
    }
    std::cout << "  Alle Revisionen:\n";
    for (auto& r : allRevs)
        std::cout << "    Rev " << r->rev
                  << "  [" << r->revState << "]"
                  << (r->superseded ? "" : "  ← aktiv")
                  << "  " << r->createdAt.substr(0,10) << "\n";
    int sel = readInt("Zur Revision wechseln (Rev-Nr.)", 1,
                      (int)allRevs.back()->rev);
    // Load the selected revision and set it as the view context for this
    // menu session — does NOT change superseded flag
    auto selRev = FolderRevision::loadByRev(ctx.doc->folderId, (uint32_t)sel);
    if (!selRev) {
        std::cout << "  >> Revision " << sel << " nicht gefunden.\n";
        return true;  // was: continue (handler)
    }
    // Reload ctx.curObjs for the selected revision (changes local state for
    // this iteration; outer loop reloads from currentRevision)
    ctx.curRevNum = selRev->rev;
    ctx.curObjs   = FolderObject::loadForRevision(ctx.doc->folderId, ctx.curRevNum);
    std::cout << "  >> Zeige Rev " << sel << "  [" << selRev->revState << "]"
              << "  " << ctx.curObjs.size() << " Objekt(e)\n";
    std::cout << "  Hinweis: Zustandsübergänge nur via F77 möglich.\n";
    return true;
}

void documentMenu(std::shared_ptr<Folder> doc) {
    // Push AKT context onto navigation stack.
    // This makes ". -e", ". -r" etc. work for the document.
    Rosenholz::NavigationStack::instance().push({
        Rosenholz::EntityType::AKT, doc->folderId, doc->title, doc->folderId});

    auto printHeader = [&]() {
        if (auto fresh = Rosenholz::Folder::loadById(doc->folderId)) *doc = *fresh;

        auto cur       = Rosenholz::FolderRevision::currentRevision(doc->folderId);
        bool inWork    = doc->isEditable();
        uint32_t rev   = cur ? cur->rev : 0;
        auto curObjs   = rev > 0
            ? Rosenholz::FolderObject::loadForRevision(doc->folderId, rev)
            : std::vector<std::shared_ptr<Rosenholz::FolderObject>>{};

        printDocument(*doc);
        if (cur)
            std::cout << "  Rev " << rev << "  [" << cur->revState << "]"
                      << (inWork ? "  ✏ bearbeitbar" : "  ⚠ unveraenderlich")
                      << "  |  " << curObjs.size() << " Objekt(e)\n";

        std::cout << "\n"
                  << "  . -e       Akte bearbeiten\n"
                  << "  . -r       Neue Revision anlegen\n"
                  << "  . -hist    Revisionsverlauf\n"
                  << "  . -rv      Revision wechseln\n";
        if (inWork) {
            std::cout << "  . -obj     Neues Objekt hinzufügen\n"
                      << "  . -url     URL-Objekte aktualisieren\n";
        }
        std::cout << "  . -f77 -n  Workflow starten\n"
                  << "  . -f77 -d  Workflows anzeigen\n"
                  << "  ls         Objekte auflisten\n"
                  << "  ls -rev    Alle Revisionen\n"
                  << "  f99 <Text> Notiz\n"
                  << "  ..         Zurück\n\n";
    };

    while (true) {
        printHeader();
        std::string line = readLine("> ");
        if (line.empty() || line == ".." || line == "0") break;

        std::istringstream iss(line);
        std::string cmd; iss >> cmd;
        std::vector<std::string> args;
        std::string tok;
        while (iss >> tok) args.push_back(tok);

        // "." expands to "akt" in AKT context:
        if (cmd == ".") cmd = "akt";

        // Reload fresh state for each command:
        if (auto fresh = Rosenholz::Folder::loadById(doc->folderId)) *doc = *fresh;
        auto cur      = Rosenholz::FolderRevision::currentRevision(doc->folderId);
        bool inWork   = doc->isEditable();
        uint32_t rev  = cur ? cur->rev : 0;
        auto curObjs  = rev > 0
            ? Rosenholz::FolderObject::loadForRevision(doc->folderId, rev)
            : std::vector<std::shared_ptr<Rosenholz::FolderObject>>{};
        DocMenuCtx ctx{doc, cur, inWork, !inWork, rev, curObjs};

        // ── akt self-commands ────────────────────────────────────────────────
        if (cmd == "akt") {
            std::string sub = args.empty() ? "" : args[0];

            if (sub == "-e") {
                dokHandleEditFields(ctx); autoMFS();

            } else if (sub == "-r" || sub == "-rev") {
                dokHandleNewRevision(ctx); autoMFS();

            } else if (sub == "-hist") {
                dokHandleVersionHistory(ctx);

            } else if (sub == "-rv") {
                dokHandleRevisionSwitch(ctx);

            } else if (sub == "-obj" && inWork) {
                dokHandleAddObject(ctx); autoMFS();

            } else if (sub == "-url" && inWork) {
                // Update all URL-based objects:
                int updated = 0;
                for (auto& o : curObjs)
                    if (!o->sourceUrl.empty()) {
                        std::cout << "  Aktualisiere: " << o->displayName() << "\n";
                        o->updateFromUrl("");
                        ++updated;
                    }
                std::cout << "  >> " << updated << " Objekt(e) aktualisiert.\n";
                autoMFS();

            } else if (sub == "-f77") {
                std::string sub2 = (args.size() > 1) ? args[1] : "";
                if (sub2 == "-n") {
                    auto wf = Rosenholz::F77Engine::startDefault("akt", doc->folderId);
                    if (wf) { printOk("Workflow gestartet: " + wf->workflowId); autoMFS(); }
                    else    printErr("Workflow konnte nicht gestartet werden.");
                } else if (sub2 == "-d") {
                    auto wfs = Rosenholz::F77W::loadForEntity("akt", doc->folderId);
                    if (wfs.empty()) { std::cout << "  (keine Workflows)\n"; continue; }
                    for (auto& w : wfs)
                        std::cout << "  " << std::left << std::setw(28) << w->workflowId
                                  << std::setw(20) << w->templateName.substr(0,18)
                                  << std::string(Rosenholz::toString(w->status)) << "\n";
                }

            } else if (sub == "-del" && Rosenholz::Config::instance().admin().enabled) {
                dokHandleDelete(ctx);
                if (doc->folderId.empty()) break;  // was deleted

            } else if (sub.empty()) {
                continue;  // just refresh header

            } else {
                printErr("Unbekannter Befehl: akt " + sub + "  (lo = Hilfe)");
            }

        // ── ls — list objects ────────────────────────────────────────────────
        } else if (cmd == "ls") {
            bool showRev = !args.empty() && args[0] == "-rev";
            if (showRev) {
                dokHandleVersionHistory(ctx);
            } else {
                // List current revision objects:
                if (curObjs.empty()) { std::cout << "  (keine Objekte in Rev " << rev << ")\n"; continue; }
                for (size_t i = 0; i < curObjs.size(); ++i) {
                    auto& o = curObjs[i];
                    std::string flags;
                    if (!o->checkoutPath.empty()) flags += " [CO]";
                    if (!o->sourceUrl.empty())    flags += " [URL]";
                    if (o->committed)             flags += " [LMDB]";
                    std::cout << "  " << std::setw(3) << (i+1) << ". "
                              << std::left << std::setw(38) << o->displayName().substr(0,36)
                              << "  " << (o->contentSize/1024) << "KB" << flags << "\n";
                }
                if (!curObjs.empty()) {
                    int pick = readInt("  Objekt öffnen [0=Abbrechen]", 0, (int)curObjs.size());
                    if (pick >= 1) {
                        auto& sel = curObjs[pick-1];
                        // Per-object sub-commands:
                        while (true) {
                            std::cout << "  -- " << sel->displayName().substr(0,40) << " --\n"
                                      << "  . -open   öffnen (lesen)\n";
                            if (inWork) {
                                std::cout << "  . -co     checkout\n"
                                          << "  . -ci     checkin\n"
                                          << "  . -rv     revert\n"
                                          << "  . -url    URL aktualisieren\n"
                                          << "  . -del    entfernen\n";
                            }
                            std::cout << "  ..        zurück\n";
                            std::string oLine = readLine("  > ");
                            if (oLine.empty() || oLine == "..") break;

                            std::istringstream ois(oLine);
                            std::string ocmd; ois >> ocmd;
                            if (ocmd == ".") ois >> ocmd; else /* already have cmd */ {};
                            if (oLine == "." || oLine.substr(0,2) == ". ") {
                                // strip leading ". "
                                if (oLine.size() > 2) ocmd = oLine.substr(2);
                            }

                            if (ocmd == "-open" || ocmd == "open") {
                                doc->openFile("read", sel->filePath);
                            } else if (ocmd == "-co" && inWork) {
                                std::string p = sel->checkoutObject();
                                if (!p.empty()) std::cout << "  >> Ausgecheckt: " << p << "\n";
                                else            std::cout << "  >> Checkout fehlgeschlagen.\n";
                            } else if (ocmd == "-ci" && inWork) {
                                std::cout << "  >> " << opResultMessage(sel->checkinObject()) << "\n"; break;
                            } else if (ocmd == "-rv" && inWork) {
                                std::cout << "  >> " << opResultMessage(
                                    sel->revertObject(rev > 1 ? rev-1 : 1)) << "\n"; break;
                            } else if (ocmd == "-url" && inWork && !sel->sourceUrl.empty()) {
                                std::string nu = readOpt("  Neue URL (leer=behalten): ");
                                std::cout << "  >> " << opResultMessage(sel->updateFromUrl(nu)) << "\n"; break;
                            } else if (ocmd == "-del" && inWork) {
                                if (yesno("  Wirklich entfernen?")) { sel->remove(); std::cout << "  >> Entfernt.\n"; break; }
                            }
                        }
                    }
                }
            }

        // ── f99 — note ───────────────────────────────────────────────────────
        } else if (cmd == "f99") {
            std::string text;
            for (auto& a : args) { if (!text.empty()) text += " "; text += a; }
            if (text.empty()) text = readLine("  Notiz: ");
            if (!text.empty()) {
                auto n = Rosenholz::Note::create("akt", doc->folderId, text);
                if (n) { std::cout << "  >> F99: " << n->noteId << "\n"; autoMFS(); }
            }

        // ── lo / help ────────────────────────────────────────────────────────
        } else if (cmd == "lo" || cmd == "-h") {
            std::cout << "\n  AKT " << doc->folderId << " — Befehle:\n"
                      << "    . -e         Felder bearbeiten\n"
                      << "    . -r         Neue Revision\n"
                      << "    . -hist      Revisionsverlauf\n"
                      << "    . -rv        Revision wechseln\n"
                      << "    . -f77 -n/-d Workflow starten/anzeigen\n"
                      << (inWork ? "    . -obj       Neues Objekt\n"
                                   "    . -url       URL-Objekte aktualisieren\n" : "")
                      << "    ls           Objekte der aktuellen Revision\n"
                      << "    ls -rev      Alle Revisionen\n"
                      << "    f99 <Text>   Notiz\n"
                      << "    ..           Zurück\n\n";

        } else {
            printErr("Unbekannter Befehl: " + cmd + "  (lo = Hilfe)");
        }
    }

    // Pop AKT context when leaving documentMenu:
    Rosenholz::NavigationStack::instance().pop();
}




} // namespace CLI

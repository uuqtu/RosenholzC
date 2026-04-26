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
#include "../model/dok/DocumentObject.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include "../mfs/MFSWriter.h"
#include "../repository/DocumentRevision.h"
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

std::shared_ptr<Document> createDocumentWizardGuided() {
    hdr("DOKUMENT ANLEGEN — ÜBERGEORDNETE ENTITÄT WÄHLEN");
    std::cout << "  Wo soll das Dokument abgelegt werden?\n"
              << "  1.  Projekt (F16)\n"
              << "  2.  Aufgabe (F22)\n"
              << "  3.  F18-Operation (F18)\n"
              << "  0.  Abbrechen\n";
    int choice = readInt("Typ", 0, 3);
    if (choice == 0) return nullptr;

    if (choice == 1) {
        // Pick from all projects
        auto projects = ProjectF16::loadAll();
        if (projects.empty()) {
            std::cout << "  (keine Projekte vorhanden)\n";
            return nullptr;
        }
        hdr("PROJEKT WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int pick = readInt("Projektnummer", 1, (int)projects.size());
        return createDocumentWizard(projects[pick - 1]->projectId, "");
    }

    if (choice == 2) {
        // First pick the project, then the task
        auto projects = ProjectF16::loadAll();
        if (projects.empty()) {
            std::cout << "  (keine Projekte vorhanden)\n";
            return nullptr;
        }
        hdr("PROJEKT WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int ppick = readInt("Projektnummer", 1, (int)projects.size());
        auto& proj = projects[ppick - 1];

        auto tasks = TaskF22::loadForProject(proj->projectId);
        if (tasks.empty()) {
            std::cout << "  (keine Aufgaben in diesem Projekt — lege unter Projekt ab)\n";
            return createDocumentWizard(proj->projectId, "");
        }
        hdr("AUFGABE WÄHLEN");
        for (int i = 0; i < (int)tasks.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << tasks[i]->regNumber.toString()
                      << tasks[i]->title.substr(0, 36) << "\n";
        int tpick = readInt("Aufgabennummer", 1, (int)tasks.size());
        return createDocumentWizard(proj->projectId, tasks[tpick - 1]->taskId);
    }

    if (choice == 3) {
        // Pick from recent F18 operations
        auto ops = F18Operation::loadRecent(40);
        if (ops.empty()) {
            std::cout << "  (keine F18-Operationen vorhanden)\n";
            return nullptr;
        }
        hdr("F18-OPERATION WÄHLEN");
        for (int i = 0; i < (int)ops.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << ops[i]->vorgangId.substr(0, 24)
                      << "  [" << std::setw(14) << ops[i]->vorgangType << "]  "
                      << ops[i]->title.substr(0, 30) << "\n";
        int opick = readInt("Operations-Nummer", 1, (int)ops.size());
        auto& op = ops[opick - 1];
        auto doc = createDocumentWizard(op->taskId, "");
        if (doc) {
            doc->f18OperationId = op->vorgangId;
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

void cmdDok(const std::vector<std::string>& args) {

    // No arguments: list 20 most recent documents
    if (args.empty()) {
        auto all = Document::loadRecent(20);
        if (all.empty()) { std::cout << "  (keine Dokumente)\n"; return; }
        std::cout << "  " << std::left
                  << std::setw(26) << "ID (für rh -dok <id>)"
                  << std::setw(14) << "STATUS"
                  << std::setw(12) << "VERSION"
                  << "TITEL\n"
                  << "  " << std::string(70, '-') << "\n";
        for (auto& d : all)
            std::cout << "  " << std::setw(26) << d->documentId.substr(0, 24)
                      << std::setw(14) << (std::string("[") + revStateToString(d->currentRevisionState()) + "]")
                      << std::setw(12) << ("v" + d->version)
                      << d->title.substr(0, 32) << "\n";
        std::cout << "  " << all.size() << " Dokument(e)\n";
        return;
    }

    // -n  —  fully guided creation (pick entity type, then entity)
    if (args[0] == "-n" || args[0] == "--neu") {
        auto doc = createDocumentWizardGuided();
        if (doc) printOk("  >> Dokument angelegt: " + doc->documentId + "  " + doc->title);
        return;
    }

    // -f16  —  guided creation under F16 (skips entity-type question)
    if (args[0] == "-f16") {
        auto projects = ProjectF16::loadAll();
        if (projects.empty()) { std::cout << "  (keine Projekte)\n"; return; }
        hdr("DOKUMENT ANLEGEN — PROJEKT WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int pick = readInt("Projektnummer", 1, (int)projects.size());
        auto doc = createDocumentWizard(projects[pick - 1]->projectId, "");
        if (doc) printOk("  >> Dokument angelegt: " + doc->documentId + "  " + doc->title);
        return;
    }

    // -f22  —  guided creation under F22
    if (args[0] == "-f22") {
        auto projects = ProjectF16::loadAll();
        if (projects.empty()) { std::cout << "  (keine Projekte)\n"; return; }
        hdr("DOKUMENT ANLEGEN — PROJEKT WÄHLEN");
        for (int i = 0; i < (int)projects.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << projects[i]->regNumber.toString()
                      << projects[i]->title.substr(0, 36) << "\n";
        int ppick = readInt("Projektnummer", 1, (int)projects.size());
        auto& proj = projects[ppick - 1];
        auto tasks = TaskF22::loadForProject(proj->projectId);
        if (tasks.empty()) {
            std::cout << "  (keine Aufgaben — lege unter Projekt ab)\n";
            auto doc = createDocumentWizard(proj->projectId, "");
            if (doc) printOk("  >> Dokument angelegt: " + doc->documentId + "  " + doc->title);
            return;
        }
        hdr("AUFGABE WÄHLEN");
        for (int i = 0; i < (int)tasks.size(); ++i)
            std::cout << "  " << std::setw(4) << (i + 1)
                      << "  " << std::setw(26) << tasks[i]->regNumber.toString()
                      << tasks[i]->title.substr(0, 36) << "\n";
        int tpick = readInt("Aufgabennummer", 1, (int)tasks.size());
        auto doc = createDocumentWizard(proj->projectId, tasks[tpick - 1]->taskId);
        if (doc) printOk("  >> Dokument angelegt: " + doc->documentId + "  " + doc->title);
        return;
    }

    // -f18  —  guided creation linked to F18
    if (args[0] == "-f18") {
        auto ops = F18Operation::loadRecent(40);
        if (ops.empty()) { std::cout << "  (keine F18-Operationen)\n"; return; }
        hdr("DOKUMENT ANLEGEN — F18 WÄHLEN");
        for (int i = 0; i < (int)ops.size(); ++i)
            std::cout << "  " << std::setw(4)  << (i + 1)
                      << "  " << std::setw(26) << ops[i]->vorgangId.substr(0, 24)
                      << "  [" << std::setw(14) << ops[i]->vorgangType << "]  "
                      << ops[i]->title.substr(0, 28) << "\n";
        int pick = readInt("Operations-Nummer", 1, (int)ops.size());
        auto& op = ops[pick - 1];
        auto doc = createDocumentWizard(op->taskId, "");
        if (doc) {
            doc->f18OperationId = op->vorgangId;
            { auto r = doc->update(); (void)r; }
            printOk("  >> Dokument angelegt: " + doc->documentId + "  " + doc->title);
        }
        return;
    }

    // All remaining paths require a valid ID
    if (!isId(args[0])) { printErr("Ungültiges Argument: " + args[0]
                            + "  (erwartet ID, -n, -f16, -f22, oder -f18)"); return; }

    // Try as document ID → open documentMenu
    auto doc = Document::loadById(args[0]);
    if (doc) {
        documentMenu(doc);
        return;
    }

    // Try as project ID
    auto proj = ProjectF16::loadById(args[0]);
    if (!proj) { printErr("ID nicht gefunden: " + args[0]); return; }

    if (args.size() > 1) {
        // Additional argument → create document under this project
        auto d = createDocumentWizard(proj->projectId, "");
        if (d) printOk("  >> Dokument angelegt: " + d->documentId + "  " + d->title);
    } else {
        // Just a project ID → browse documents for this project
        documentBrowserMenu(proj->projectId);
    }
}

// ── createDocumentWizard ──────────────────────────────────────
//
// Step-by-step wizard for registering a new document.
// Parent project ID (and optionally task ID) must be provided.
// Source: local file, URL download, or new empty file.

std::shared_ptr<Rosenholz::Document> createDocumentWizard(
    const std::string& projectId, const std::string& taskId)
{
    using namespace Rosenholz;

    hdr("DOKUMENT ANLEGEN / REGISTRIEREN");

    // ── Titel + Typ ─────────────────────────────────────────────
    std::string title = readLine("Dokumententitel: ");
    if (title.empty()) return nullptr;

    std::cout << "  Typ:\n"
              << "    1.Bericht        2.Spezifikation  3.Vertrag\n"
              << "    4.Schriftverkehr 5.Nachweis       6.Plan\n"
              << "    7.Protokoll      8.Archiv         9.Sonstiges\n";
    int dt = readInt("Typ", 1, 9);
    static const char* dtypes[] = {
        "report","specification","contract","correspondence",
        "evidence","plan","minutes","archive","other"};
    std::string docType = dtypes[dt-1];

    // ── Optionale Felder ─────────────────────────────────────────
    std::string fmt = readOpt("Dateiformat (pdf/docx/xlsx/txt/png/..., leer=pdf): ");

    // ── Dokument-Datensatz speichern (keine Datei — Objekte später hinzufügen) ──
    auto doc = Document::create(title, docType, projectId);
    doc->taskId      = taskId;
    doc->format      = fmt.empty() ? "pdf" : fmt;
    doc->dateCreated = nowIso();

    if (!opOk(doc->save())) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }
    doc->revise("Revision 1 — Erstanlage");

    // ── Gemeinsame optionale Felder ───────────────────────────────
    doc->summary        = readOpt("Kurzbeschreibung (optional): ");
    doc->authorId       = readOpt("Autor Person-ID (optional): ");
    doc->classification = readOpt("Einstufung (intern/vertraulich/öffentlich, leer=intern): ");
    if (doc->classification.empty()) doc->classification = "intern";
    { auto r = doc->update(); (void)r; }

    std::cout << "  >> Dokument registriert: " << doc->documentId << "\n"
              << "  >> Titel  : " << doc->title << "\n"
              << "  >> Dateien können über Option 5 (Objekt hinzufügen) im Dokumentmenü hinzugefügt werden.\n";

    // Write MFS index file (the .txt metadata card)
    MFSWriter::writeDocument(*doc, Config::instance().mfsPath());

    std::cout << "  >> Revision 1 angelegt (in_work)\n";
    return doc;
}



// ── revisionMenu (helper for documentMenu) ────────────────────
//
// Shows the current DocumentRevision lifecycle state and lets
// the user perform a state transition.
// Called from documentMenu option 2 (Revisions-Lifecycle).

static void revisionMenu(std::shared_ptr<Document> doc) {
    using namespace Rosenholz;
    while (true) {
        auto rev = DocumentRevision::currentRevision(doc->documentId);
        if (!rev) {
            std::cout << "  (keine aktive Revision)\n";
            return;
        }
        hdr("REVISION " + std::to_string(rev->rev) + "  — " + doc->documentId.substr(0, 22));
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
        if (!DocumentRevision::isTransitionAllowed(rev->revState,
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
    std::shared_ptr<Document>                    doc;
    std::shared_ptr<DocumentRevision>            cur;
    bool                                          inWork;
    bool                                          immutable;
    uint32_t                                      curRevNum;
    std::vector<std::shared_ptr<DocumentObject>> curObjs;
};

// ch==1: dokHandleEditFields
static bool dokHandleEditFields(DocMenuCtx& ctx) {

    // Felder bearbeiten
    if (!ctx.doc->canEdit()) {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
        return true;  // was: continue (handler)
    }
    hdr("BEARBEITEN — " + ctx.doc->documentId.substr(0, 24));
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
        std::cout << "  >> Starten Sie: rh -f77 -start " << ctx.doc->documentId << "\n";
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
        ctx.curObjs   = DocumentObject::loadForRevision(ctx.doc->documentId, ctx.curRevNum);
        std::cout << "  >> Aktuelle Ansicht: Rev " << ctx.curRevNum << " [in_work]\n";
    } else {
        std::cout << "  >> " << opResultMessage(OperationResult::DOC_SAVE_FAILED) << "\n";
    }
    return true;
}

// ch==3: dokHandleWorkflow
static bool dokHandleWorkflow(DocMenuCtx& ctx) {

    // Workflow starten / anzeigen
    hdr("WORKFLOW / FREIGABE — " + ctx.doc->documentId.substr(0, 26));
    if (ctx.doc->releaseWorkflowId.empty()) {
        std::cout << "  Kein aktiver Workflow.\n";
        if (readInt("  1.Workflow starten  0.Zurueck", 0, 1) == 1) {
            std::string wid = startWfInstanceWizard("dok", ctx.doc->documentId);
            if (!wid.empty()) instanceMenu(wid);
        }
    } else {
        auto wf = F77_Workflow::loadById(ctx.doc->releaseWorkflowId);
        std::cout << "  Workflow-ID : " << ctx.doc->releaseWorkflowId.substr(0, 36) << "\n"
                  << "  Status      : " << (wf ? wf->status : "unbekannt") << "\n";
        if (wf && wf->status == "active") {
            if (readInt("  1.Oeffnen  0.Zurueck", 0, 1) == 1)
                instanceMenu(ctx.doc->releaseWorkflowId);
        } else {
            std::cout << "  (Workflow abgeschlossen/abgebrochen)\n";
            if (readInt("  1.Neuen Workflow starten  0.Zurueck", 0, 1) == 1) {
                std::string wid = startWfInstanceWizard("dok", ctx.doc->documentId);
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
        + " | " + ctx.doc->documentId.substr(0, 22));
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
            if (!o->mfsPath.empty())
                std::cout << "       " << o->mfsPath << "\n";
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
        OperationResult res = OperationResult::OPERATION_ACK;
        auto obj = DocumentObject::importFile(ctx.doc->documentId, ctx.curRevNum, srcPath, res);
        if (opOk(res) && obj)
            std::cout << "  >> Objekt importiert: " << obj->mfsPath << "\n"
                      << "  >> Name in MFS: " << obj->displayName() << "\n";
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

        OperationResult res = OperationResult::OPERATION_ACK;
        auto obj = DocumentObject::importFile(ctx.doc->documentId, ctx.curRevNum, toImport, res);
        FileOps::deleteFile(toImport);
        if (opOk(res) && obj)
            std::cout << "  >> Heruntergeladen und importiert: " << obj->mfsPath << "\n";
        else
            std::cout << "  >> " << opResultMessage(res) << "\n";
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
        std::string sane     = sanitiseRegNr(ctx.doc->documentId);
        std::string safeName = FileOps::sanitizeFilename(ctx.doc->title);
        if (safeName.size() > 40) safeName = safeName.substr(0, 40);
        std::string parentSane = sanitiseRegNr(
            ctx.doc->projectId.empty() ? ctx.doc->taskId : ctx.doc->projectId);
        std::string dir = FileOps::joinPath(mfs, "DOK", parentSane);
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
                  "ID      : " + ctx.doc->documentId + "\n"
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

        // Import into DocumentObject
        OperationResult res = OperationResult::OPERATION_ACK;
        auto obj = DocumentObject::importFile(ctx.doc->documentId, ctx.curRevNum, fpath, res);
        if (opOk(res) && obj) {
            std::cout << "  >> Datei angelegt: " << fpath << "\n";
            // Open immediately with the system default application
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
        if (!ctx.curObjs[sel-1]->mfsPath.empty())
            FileOps::deleteFile(ctx.curObjs[sel-1]->mfsPath);
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
    std::vector<std::shared_ptr<DocumentObject>> coObjs;
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

// ch==11: dokHandleUrlDownload
static bool dokHandleUrlDownload(DocMenuCtx& ctx) {

    // URL neu herunterladen
    if (ctx.doc->fileUrl.empty()) { ctx.doc->fileUrl = readLine("URL: "); { auto r = ctx.doc->update(); (void)r; } }
    if (!ctx.doc->fileUrl.empty()) {
        auto rfRes = ctx.doc->refreshFromUrl();
        if (opOk(rfRes))
            std::cout << "  >> Aktualisiert: " << ctx.doc->filePath << "\n";
        else
            std::cout << "  >> " << opResultMessage(rfRes) << "\n";
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

    std::cout << "  Dokument " << ctx.doc->documentId << " loeschen? (ja): ";
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
    std::string srcPath = obj->mfsPath;
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
    auto allRevs = DocumentRevision::loadAllRevisions(ctx.doc->documentId);
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
    auto selRev = DocumentRevision::loadByRev(ctx.doc->documentId, (uint32_t)sel);
    if (!selRev) {
        std::cout << "  >> Revision " << sel << " nicht gefunden.\n";
        return true;  // was: continue (handler)
    }
    // Reload ctx.curObjs for the selected revision (changes local state for
    // this iteration; outer loop reloads from currentRevision)
    ctx.curRevNum = selRev->rev;
    ctx.curObjs   = DocumentObject::loadForRevision(ctx.doc->documentId, ctx.curRevNum);
    std::cout << "  >> Zeige Rev " << sel << "  [" << selRev->revState << "]"
              << "  " << ctx.curObjs.size() << " Objekt(e)\n";
    std::cout << "  Hinweis: Zustandsübergänge nur via F77-Workflow möglich.\n";
    return true;
}

void documentMenu(std::shared_ptr<Document> doc) {
    while (true) {
        // Reload to pick up changes from sub-menus or workflows
        if (auto fresh = Document::loadById(doc->documentId)) *doc = *fresh;

        auto cur         = DocumentRevision::currentRevision(doc->documentId);
        bool inWork      = doc->isInWork();
        bool immutable   = doc->isFrozen();
        bool hasCheckout = !doc->checkedOutPath.empty();
        uint32_t curRevNum = cur ? cur->rev : 0;
        auto curObjs     = curRevNum > 0
            ? DocumentObject::loadForRevision(doc->documentId, curRevNum)
            : std::vector<std::shared_ptr<DocumentObject>>{};

        printDocument(*doc);
        if (cur)
            std::cout << "  Rev " << curRevNum << "  [" << cur->revState << "]"
                      << (immutable ? "  ⚠ unveraenderlich" : "  ✏ bearbeitbar")
                      << "  |  " << curObjs.size() << " Objekt(e)\n";

        // ── Menü ────────────────────────────────────────────────
        std::cout << "  1.Felder  2.Revision  3.Workflow  4.Objekte\n";
        if (inWork) {
            std::cout << "    5. Objekt hinzufügen (Datei importieren)\n";
            if (!curObjs.empty())
                std::cout << "    6. Objekt entfernen\n";
        }
        std::cout << "  7.Öffnen\n";
        if (inWork) {
            std::cout << "    8. Checkout — Objekt zum Bearbeiten holen\n"
                      << "    9. Checkin — Objekt zurückgeben\n"
                      << "   10. Änderungen verwerfen (Revert)\n";
        }
        std::cout << "  11.URL  12.Verlauf  13.MFS  14.Löschen  15.Drucken  16.RevWechsel  0.Zurück\n";
        hr();

        int ch = readInt("Wahl", 0, 16);
        if (ch == 0) break;

        DocMenuCtx ctx{ doc, cur, inWork, immutable, curRevNum, curObjs };

        // Dispatch table — one line per option, each handler is a named function
        using Handler = bool(*)(DocMenuCtx&);
        static const Handler dispatch[17] = {
            nullptr,                // 0 = break (handled above)
            dokHandleEditFields,    // 1
            dokHandleNewRevision,   // 2
            dokHandleWorkflow,      // 3
            dokHandleListObjects,   // 4
            dokHandleAddObject,     // 5
            dokHandleRemoveObject,  // 6
            dokHandleOpenObject,    // 7
            dokHandleCheckout,      // 8
            dokHandleCheckin,       // 9
            dokHandleRevert,        // 10
            dokHandleUrlDownload,   // 11
            dokHandleVersionHistory,// 12
            dokHandleMfsWrite,      // 13
            dokHandleDelete,        // 14
            dokHandlePrint,         // 15
            dokHandleRevisionSwitch,// 16
        };
        if (ch >= 1 && ch <= 16) {
            bool keepGoing = dispatch[ch](ctx);
            doc      = ctx.doc;
            curRevNum = ctx.curRevNum;
            curObjs  = ctx.curObjs;
            if (!keepGoing) break;
        }
    }
}


// LoadRuleContext: rule + optional date for DATE_RELEASED
struct LoadRuleCtx {
    Rosenholz::DocLoadRule rule;
    std::string targetDate;
};

static LoadRuleCtx askLoadRule() {
    std::cout << "\n  Laderegelung:\n"
              << "    1. Neueste freigegebene Revision  (Standard)\n"
              << "    2. Neueste Arbeitsrevision         (in_work/pre_released/released/locked)\n"
              << "    3. Datumsbasiert (freigegebene Revision zum Stichtag)\n";
    int r = readInt("Laderegelung", 1, 3);
    if (r == 2) return { Rosenholz::DocLoadRule::LATEST_WORKING, "" };
    if (r == 3) {
        std::string date = readLine("Stichtag (JJJJ-MM-TT): ");
        return { Rosenholz::DocLoadRule::DATE_RELEASED, date };
    }
    return { Rosenholz::DocLoadRule::LATEST_RELEASED, "" };
}


void documentBrowserMenu(const std::string& projectId, const std::string& taskId) {
    // Use the global default load rule from settings, allow override
    Rosenholz::DocLoadRule globalRule;
    std::string globalDate;
    switch (Rosenholz::Config::instance().ui().defaultLoadRule) {
        case 2:  globalRule = Rosenholz::DocLoadRule::LATEST_WORKING; break;
        case 3:  globalRule = Rosenholz::DocLoadRule::DATE_RELEASED;
                 globalDate = Rosenholz::Config::instance().ui().defaultLoadDate; break;
        default: globalRule = Rosenholz::DocLoadRule::LATEST_RELEASED;
    }
    auto lrc = LoadRuleCtx{globalRule, globalDate};
    // Allow one-time session override
    if (readInt("  Laderegelung überschreiben? (0=Nein, 1=Ja)", 0, 1) == 1) {
        lrc = askLoadRule();
        // Optionally save as new default
        if (yesno("  Als Standard speichern?")) {
            auto& ui = Rosenholz::Config::instance().uiMut();
            ui.defaultLoadRule = (lrc.rule == Rosenholz::DocLoadRule::LATEST_WORKING) ? 2
                               : (lrc.rule == Rosenholz::DocLoadRule::DATE_RELEASED)  ? 3 : 1;
            ui.defaultLoadDate = lrc.targetDate;
            Rosenholz::Config::instance().save();
        }
    }
    while (true) {
        std::vector<std::shared_ptr<Document>> docs;
        if (!taskId.empty())
            docs = Document::loadForEntity("f22", taskId);
        else if (!projectId.empty())
            docs = Document::loadForProject(projectId, lrc.rule, lrc.targetDate);
        else
            docs = Document::loadRecent(30, lrc.rule, lrc.targetDate);
        
        listDocuments(docs, "DOKUMENTE (" + std::to_string(docs.size()) + ")");
        // Check parent released status
        bool parentReleased = false;
        if (!taskId.empty()) {
            auto t = TaskF22::loadById(taskId);  // warns internally if not found
            if (t) parentReleased = t->isReleased();
            // If task not found, silently continue — docs may still be linked
        } else if (!projectId.empty()) {
            auto p = ProjectF16::loadById(projectId);
            parentReleased = p && p->isReleased();
        }
        if (parentReleased)
            std::cout << "  1.Öffnen  0.Zurück  (Released — kein Neu anlegen)\n";
        else
            std::cout << "  1.Öffnen  2.Neu anlegen  0.Zurück\n";
        int ch = readInt("Wahl",0,2); if (ch==0) break;
        if (ch==1 && !docs.empty()) {
            int pick = readInt("Nummer",1,(int)docs.size());
            documentMenu(docs[pick-1]);
        } else if (ch==2) {
            if (parentReleased) { std::cout << "  >> Released — kein Neu anlegen.\n"; continue; }
            std::string pid = projectId, tid = taskId;
            if (pid.empty() && !tid.empty()) {
                auto t = TaskF22::loadById(tid); if (t) pid = t->projectId;
            }
            auto d = createDocumentWizard(pid, tid);
            if (d) documentMenu(d);
        }
    }
}

} // namespace CLI

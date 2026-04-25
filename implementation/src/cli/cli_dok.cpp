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
        auto doc = createDocumentWizard(op->projectId, "");
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
        auto doc = createDocumentWizard(op->projectId, "");
        if (doc) {
            doc->f18OperationId = op->vorgangId;
            { auto r = doc->update(); (void)r; }
            printOk("  >> Dokument angelegt: " + doc->documentId + "  " + doc->title);
        }
        return;
    }

    // All remaining paths require a valid ID
    if (!isId(args[0])) die("Ungültiges Argument: " + args[0]
                            + "  (erwartet ID, -n, -f16, -f22, oder -f18)");

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

    // ── Quelle wählen ────────────────────────────────────────────
    std::cout << "\n  Quelle:\n"
              << "    1. Lokale Datei (Pfad angeben → in MFS kopieren)\n"
              << "    2. Link / URL   (herunterladen → in MFS ablegen)\n"
              << "    2. Neu anlegen  (Leer-Datei in MFS erstellen)\n";
    int src = readInt("Quelle", 1, 3);

    // ── Dokument-Objekt erzeugen ─────────────────────────────────
    auto doc = Document::create(title, docType, projectId);
    doc->taskId = taskId;

    // Format/Erweiterung
    std::string fmt = readOpt("Dateiformat (pdf/docx/xlsx/txt/png/..., leer=pdf): ");
    doc->format = fmt.empty() ? "pdf" : fmt;

    // ── Quelle 1: Lokale Datei ────────────────────────────────────
    if (src == 1) {
        std::string srcPath = readLine("Lokaler Dateipfad: ");
        if (srcPath.empty() || !FileOps::fileExists(srcPath)) {
            std::cout << "  >> Datei nicht gefunden.\n";
            return nullptr;
        }
        // Detect format from file extension
        std::string ext = FileOps::extension(srcPath);
        if (!ext.empty()) doc->format = ext.substr(1);
        doc->dateCreated = nowIso();
        if (!opOk(doc->save())) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }
        doc->revise("Revision 1 — Erstanlage");
        // Copy to MFS
        if (!opOk(doc->importLocalFile(srcPath))) {
            std::cout << "  >> Warnung: MFS-Import fehlgeschlagen.\n";
        } else {
            std::cout << "  >> Datei in MFS abgelegt: " << doc->filePath << "\n";
        }
    }

    // ── Quelle 2: URL / Link ──────────────────────────────────────
    else if (src == 2) {
        // Step 1: URL
        std::string url = readLine("URL: ");
        if (url.empty() || cliIsInterrupted()) return nullptr;

        // Step 2: Download immediately — abort if it fails
        std::cout << "  Herunterladen...\n";
        const std::string& base = Config::instance().basePath();
        std::string tmpDir = FileOps::joinPath(base, "documents", "tmp");
        FileOps::makeDirs(tmpDir);
        std::string downloaded = FileOps::downloadUrl(url, tmpDir);
        if (downloaded.empty()) {
            std::cout << "  >> Download fehlgeschlagen — bitte URL prüfen.\n";
            return nullptr;   // abort, nothing saved yet
        }

        // Step 3: Detect format from downloaded filename — no question if clear
        std::string detectedExt;
        {
            std::string ext = FileOps::extension(downloaded);
            if (!ext.empty()) detectedExt = ext.substr(1); // strip leading dot
        }
        if (!detectedExt.empty()) {
            // Format is unambiguous — use it, only offer override
            std::cout << "  Format erkannt: " << detectedExt << "\n";
            std::string override = readOpt("Anderes Format verwenden? (leer='" + detectedExt + "'): ");
            doc->format = override.empty() ? detectedExt : override;
        } else {
            // No extension detectable — must ask
            std::string fmtInput = readOpt("Format (pdf/docx/xlsx/txt/png/..., leer=bin): ");
            doc->format = fmtInput.empty() ? "bin" : fmtInput;
        }

        doc->fileUrl     = url;
        doc->dateCreated = nowIso();
        if (!opOk(doc->save())) {
            FileOps::deleteFile(downloaded);
            std::cout << "  >> DB-Fehler.\n";
            return nullptr;
        }
        doc->revise("Revision 1 — Erstanlage");

        // Step 4: Import into MFS
        if (!opOk(doc->importLocalFile(downloaded))) {
            std::cout << "  >> MFS-Import fehlgeschlagen.\n";
        } else {
            std::cout << "  >> Heruntergeladen und in MFS abgelegt: " << doc->filePath << "\n";
        }
        FileOps::deleteFile(downloaded);
    }

    // ── Quelle 3: Neu anlegen ─────────────────────────────────────
    else {
        std::cout << "  Neue Datei anlegen:\n"
                  << "    1.Textdatei (.txt)   2.Word-Dokument (.docx)\n"
                  << "    3.Excel-Tabelle (.xlsx)  4.Präsentation (.pptx)\n"
                  << "    5.PDF-Vorlage (.pdf)  6.Weiteres Format\n";
        int nf = readInt("Art", 1, 6);
        doc->dateCreated = nowIso();
        if (!opOk(doc->save())) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }
        doc->revise("Revision 1 — Erstanlage");

        // Create empty file in MFS
        const std::string& mfs = Config::instance().mfsPath();
        std::string sane = sanitiseRegNr(doc->documentId);
        std::string safeName = FileOps::sanitizeFilename(doc->title);
        if (safeName.size() > 40) safeName = safeName.substr(0, 40);
        static const char* exts[] = {"txt","docx","xlsx","pptx","pdf",""};
        std::string ext = (nf <= 5) ? exts[nf-1] : doc->format;
        if (nf == 6) ext = readOpt("Format/Erweiterung: ");
        doc->format = ext.empty() ? "txt" : ext;

        std::string parentId = projectId.empty() ? taskId : projectId;
        std::string parentSane = sanitiseRegNr(parentId);
        std::string dir = FileOps::joinPath(mfs, "DOK", parentSane);
        FileOps::makeDirs(dir);

        std::string fname = sane + "_" + safeName + "_v" + doc->version + "." + ext;
        std::string fpath = FileOps::joinPath(dir, fname);

        // Write a header stub
        std::string stub = "ROSENHOLZ PM — " + doc->title + "\n"
                         + "ID      : " + doc->documentId + "\n"
                         + "Version : " + doc->version + "\n"
                         + "Erstellt: " + doc->dateCreated + "\n"
                         + "\n--- Inhalt ---\n";
        FileOps::writeTextFile(fpath, stub);

        doc->filePath = fpath;
        doc->fileSize = FileOps::fileSize(fpath);
        doc->fileHash = ""; // no hash for stub
        { auto r = doc->update(); (void)r; }
        std::cout << "  >> Leer-Datei angelegt: " << fpath << "\n";
    }

    // ── Gemeinsame optionale Felder ───────────────────────────────
    doc->summary    = readOpt("Kurzbeschreibung (optional): ");
    doc->authorId   = readOpt("Autor Person-ID (optional): ");
    doc->classification = readOpt("Einstufung (intern/vertraulich/öffentlich, leer=intern): ");
    if (doc->classification.empty()) doc->classification = "intern";
    { auto r = doc->update(); (void)r; }

    std::cout << "  >> Dokument registriert: " << doc->documentId << "\n";
    std::cout << "  >> Titel   : " << doc->title << "\n";
    std::cout << "  >> MFS-Pfad: " << (doc->filePath.empty() ? "(kein)" : doc->filePath) << "\n";

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
        std::cout << "  Zustand    : " << rev->revStateStr() << "\n";
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
            std::cout << "  >> Aktuell: " << rev->revStateStr()
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
            std::cout << "  Rev " << curRevNum << "  [" << cur->revStateStr() << "]"
                      << (immutable ? "  ⚠ unveraenderlich" : "  ✏ bearbeitbar")
                      << "  |  " << curObjs.size() << " Objekt(e)\n\n";

        // ── Menü ────────────────────────────────────────────────
        std::cout << "  [DOKUMENT]\n"
                  << "    1. Felder bearbeiten\n"
                  << "    2. Neue Revision anlegen"
                  << (inWork ? "  [GESPERRT — erst Workflow starten]" : "") << "\n"
                  << "\n  [WORKFLOW / FREIGABE]\n"
                  << "    3. Workflow starten / anzeigen\n"
                  << "\n  [OBJEKTE (" << curObjs.size() << " Datei(en) in Rev " << curRevNum << ")]\n"
                  << "    4. Objekte auflisten\n";
        if (inWork) {
            std::cout << "    5. Objekt hinzufügen (Datei importieren)\n";
            if (!curObjs.empty())
                std::cout << "    6. Objekt entfernen\n";
        }
        std::cout << "\n  [DATEI-OPERATIONEN]\n"
                  << "    7. Objekt öffnen (Lesen)\n";
        if (inWork) {
            std::cout << "    8. Checkout — Objekt zum Bearbeiten holen\n";
            if (hasCheckout)
                std::cout << "    9. Checkin — Objekt zurückgeben\n"
                          << "   10. Änderungen verwerfen (Revert)\n";
        }
        std::cout << "   11. URL neu herunterladen\n"
                  << "   12. Versionsverlauf\n"
                  << "\n  [SYSTEM]\n"
                  << "   13. MFS-Datei schreiben\n"
                  << "   14. Dokument löschen\n"
                  << "\n    0. Zurück\n";
        hr();

        int ch = readInt("Wahl", 0, 14);
        if (ch == 0) break;

        if (ch == 1) {
            // Felder bearbeiten
            if (!doc->canEdit()) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
                continue;
            }
            hdr("BEARBEITEN — " + doc->documentId.substr(0, 24));
            auto s = [](const std::string& p) { return readOpt(p + " (leer=behalten): "); };
            auto apply = [](std::string& f, const std::string& v) { if (!v.empty()) f = v; };
            apply(doc->title,          s("Titel"));
            apply(doc->docCategory,    s("Kategorie"));
            apply(doc->version,        s("Version"));
            apply(doc->classification, s("Einstufung (intern/vertraulich/oeffentlich)"));
            apply(doc->summary,        s("Kurzbeschreibung"));
            apply(doc->authorId,       s("Autor-ID"));
            apply(doc->approvedBy,     s("Genehmiger-ID"));
            { auto r = doc->update(); (void)r; }
            std::cout << "  >> Gespeichert.\n";

        } else if (ch == 2) {
            // Neue Revision anlegen
            if (!doc->canRevise()) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_IS_IN_WORK) << "\n";
                std::cout << "  >> Starten Sie: rh -f77 -start " << doc->documentId << "\n";
                continue;
            }
            std::string note = readLine("Revisionsnotiz (Pflicht): ");
            if (note.empty()) continue;
            std::string by = readOpt("Erstellt von (Person-ID, leer=Autor): ");
            auto newRev = doc->revise(note, by);
            if (newRev)
                std::cout << "  >> Revision " << newRev->rev << " angelegt (in_work).\n";
            else
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_SAVE_FAILED) << "\n";

        } else if (ch == 3) {
            // Workflow starten / anzeigen
            hdr("WORKFLOW / FREIGABE — " + doc->documentId.substr(0, 26));
            if (doc->releaseWorkflowId.empty()) {
                std::cout << "  Kein aktiver Workflow.\n";
                if (readInt("  1.Workflow starten  0.Zurueck", 0, 1) == 1) {
                    std::string wid = startWfInstanceWizard("dok", doc->documentId);
                    if (!wid.empty()) instanceMenu(wid);
                }
            } else {
                auto wf = F77_Workflow::loadById(doc->releaseWorkflowId);
                std::cout << "  Workflow-ID : " << doc->releaseWorkflowId.substr(0, 36) << "\n"
                          << "  Status      : " << (wf ? wf->status : "unbekannt") << "\n";
                if (wf && wf->status == "active") {
                    if (readInt("  1.Oeffnen  0.Zurueck", 0, 1) == 1)
                        instanceMenu(doc->releaseWorkflowId);
                } else {
                    std::cout << "  (Workflow abgeschlossen/abgebrochen)\n";
                    if (readInt("  1.Neuen Workflow starten  0.Zurueck", 0, 1) == 1) {
                        std::string wid = startWfInstanceWizard("dok", doc->documentId);
                        if (!wid.empty()) instanceMenu(wid);
                    }
                }
            }

        } else if (ch == 4) {
            // Objekte auflisten
            hdr("OBJEKTE — Rev " + std::to_string(curRevNum)
                + " | " + doc->documentId.substr(0, 22));
            if (curObjs.empty()) {
                std::cout << "  (keine Objekte in dieser Revision)\n";
                std::cout << "  Tipp: Option 5 um Dateien zu importieren (nur in in_work).\n";
            } else {
                std::cout << "  " << std::left
                          << std::setw(4) << "Nr."
                          << std::setw(42) << "Dateiname"
                          << std::setw(8)  << "Status"
                          << "Größe\n"
                          << "  " << std::string(72, '-') << "\n";
                for (size_t i = 0; i < curObjs.size(); ++i) {
                    auto& o = curObjs[i];
                    std::cout << "  " << std::setw(4) << (i+1)
                              << std::setw(42) << o->displayName().substr(0, 40)
                              << std::setw(8)  << (o->committed ? "[LMDB]" : "[MFS] ")
                              << (o->contentSize / 1024) << " KB\n";
                    if (!o->mfsPath.empty())
                        std::cout << "       " << o->mfsPath << "\n";
                }
            }

        } else if (ch == 5) {
            // Objekt hinzufügen — nur in in_work
            if (!inWork) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
                continue;
            }
            std::string srcPath = readLine("Dateipfad: ");
            if (srcPath.empty() || cliIsInterrupted()) continue;
            OperationResult res = OperationResult::OPERATION_ACK;
            auto obj = DocumentObject::importFile(doc->documentId, curRevNum, srcPath, res);
            if (opOk(res) && obj)
                std::cout << "  >> Objekt importiert: " << obj->mfsPath << "\n"
                          << "  >> Name in MFS: " << obj->displayName() << "\n";
            else
                std::cout << "  >> " << opResultMessage(res) << "\n";

        } else if (ch == 6) {
            // Objekt entfernen — nur in in_work
            if (!inWork || curObjs.empty()) {
                auto r = inWork ? OperationResult::INVALID_ARGUMENT
                                : OperationResult::DOC_REV_NOT_IN_WORK;
                std::cout << "  >> " << opResultMessage(r) << "\n";
                continue;
            }
            for (size_t i = 0; i < curObjs.size(); ++i)
                std::cout << "  " << (i+1) << ". " << curObjs[i]->displayName() << "\n";
            int sel = readInt("Objekt entfernen (Nummer)", 1, (int)curObjs.size());
            if (yesno("Objekt '" + curObjs[sel-1]->displayName() + "' entfernen?")) {
                if (!curObjs[sel-1]->mfsPath.empty())
                    FileOps::deleteFile(curObjs[sel-1]->mfsPath);
                curObjs[sel-1]->remove();
                std::cout << "  >> Objekt entfernt.\n";
            }

        } else if (ch == 7) {
            // Objekt öffnen
            if (curObjs.empty()) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_FILE_NOT_FOUND) << "\n";
                continue;
            }
            // Select object
            for (size_t i = 0; i < curObjs.size(); ++i)
                std::cout << "  " << (i+1) << ". "
                          << std::left << std::setw(40) << curObjs[i]->displayName()
                          << "  " << (curObjs[i]->checkedOut ? "[AUSGECHECKT]" : "")
                          << "\n";
            int sel = readInt("Objekt oeffnen", 1, (int)curObjs.size());
            auto& obj = curObjs[sel-1];

            if (!inWork) {
                // Not in_work: always open from tmp (read-only, changes not tracked)
                std::cout << "  (Nur Lesen — Revision ist " << cur->revStateStr() << ")\n";
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

        } else if (ch == 8) {
            // Checkout eines Objekts
            if (!inWork) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
                continue;
            }
            if (curObjs.empty()) {
                std::cout << "  >> Keine Objekte vorhanden. Zuerst Dateien hinzufügen (Option 5).\n";
                continue;
            }
            for (size_t i = 0; i < curObjs.size(); ++i)
                std::cout << "  " << (i+1) << ". "
                          << curObjs[i]->displayName()
                          << (curObjs[i]->checkedOut ? "  [AUSGECHECKT]" : "") << "\n";
            int sel = readInt("Objekt auschecken", 1, (int)curObjs.size());
            auto& obj = curObjs[sel-1];
            if (obj->checkedOut) {
                std::cout << "  >> Bereits ausgecheckt unter: " << obj->checkoutPath << "\n";
                continue;
            }
            std::string dest = readOpt("Zielverzeichnis (leer = MFS-Rev-Ordner): ");
            std::string path = obj->checkoutObject(dest);
            if (!path.empty())
                std::cout << "  >> Ausgecheckt: " << path << "\n";
            else
                std::cout << "  >> " << opResultMessage(OperationResult::IO_ERROR) << "\n";

        } else if (ch == 9) {
            // Checkin eines ausgecheckten Objekts
            if (!inWork) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
                continue;
            }
            // List checked-out objects
            std::vector<std::shared_ptr<DocumentObject>> coObjs;
            for (auto& o : curObjs)
                if (o->checkedOut) coObjs.push_back(o);
            if (coObjs.empty()) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_NO_CHECKOUT) << "\n";
                continue;
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

        } else if (ch == 10) {
            // Revert eines Objekts auf Vorgänger-Revision
            if (!inWork) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_REV_NOT_IN_WORK) << "\n";
                continue;
            }
            if (!cur || cur->parentRev == 0) {
                std::cout << "  >> " << opResultMessage(OperationResult::DOC_NO_PARENT_REV)
                          << "\n  >> Revert ist erst ab der zweiten Revision möglich.\n";
                continue;
            }
            if (curObjs.empty()) {
                std::cout << "  >> Keine Objekte in dieser Revision.\n";
                continue;
            }
            std::cout << "  Welches Objekt auf Rev " << cur->parentRev << " zurücksetzen?\n";
            for (size_t i = 0; i < curObjs.size(); ++i)
                std::cout << "  " << (i+1) << ". " << curObjs[i]->displayName() << "\n";
            std::cout << "  " << (curObjs.size()+1) << ". Alle Objekte zurücksetzen\n";
            int sel = readInt("Auswahl", 1, (int)curObjs.size()+1);

            if (yesno("Objekt(e) auf Zustand von Rev " + std::to_string(cur->parentRev) + " zurücksetzen?")) {
                int ok_count = 0, fail_count = 0;
                if (sel <= (int)curObjs.size()) {
                    auto rv = curObjs[sel-1]->revertObject(cur->parentRev);
                    if (opOk(rv)) { ok_count++; std::cout << "  >> Zurückgesetzt.\n"; }
                    else { fail_count++; std::cout << "  >> " << opResultMessage(rv) << "\n"; }
                } else {
                    for (auto& o : curObjs) {
                        auto rv = o->revertObject(cur->parentRev);
                        if (opOk(rv)) ok_count++;
                        else { fail_count++;
                               std::cout << "  >> " << o->displayName() << ": " << opResultMessage(rv) << "\n"; }
                    }
                    std::cout << "  >> " << ok_count << " Objekt(e) zurückgesetzt, "
                              << fail_count << " Fehler.\n";
                }
            }

        } else if (ch == 11) {
            // URL neu herunterladen
            if (doc->fileUrl.empty()) { doc->fileUrl = readLine("URL: "); { auto r = doc->update(); (void)r; } }
            if (!doc->fileUrl.empty()) {
                auto rfRes = doc->refreshFromUrl();
                if (opOk(rfRes))
                    std::cout << "  >> Aktualisiert: " << doc->filePath << "\n";
                else
                    std::cout << "  >> " << opResultMessage(rfRes) << "\n";
            }

        } else if (ch == 12) {
            // Versionsverlauf
            auto vl = doc->loadVersions();
            hdr("REVISIONSVERLAUF (" + std::to_string(vl.size()) + ")");
            for (auto& v : vl)
                std::cout << "  Rev " << std::setw(3) << v.versionNumber
                          << "  " << v.createdAt.substr(0, 16)
                          << "  " << v.changeNote.substr(0, 40) << "\n";

        } else if (ch == 13) {
            MFSWriter::writeDocument(*doc, Config::instance().mfsPath());
            std::cout << "  >> MFS-Datei geschrieben.\n";

        } else if (ch == 14) {
            std::cout << "  Dokument " << doc->documentId << " loeschen? (ja): ";
            std::string c; std::getline(std::cin, c);
            if (c == "ja") { doc->remove(); std::cout << "  >> Geloescht.\n"; break; }
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
    auto lrc = askLoadRule();
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
            auto t = TaskF22::loadById(taskId);
            parentReleased = t && t->isReleased();
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

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
            doc->update();
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
                      << std::setw(14) << ("[" + d->status + "]")
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
            doc->update();
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
    if (!proj) die("ID nicht gefunden: " + args[0]);

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
              << "    3. Neu anlegen  (Leer-Datei in MFS erstellen)\n";
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
        doc->version    = readOpt("Version (leer=1.0): ");
        if (doc->version.empty()) doc->version = "1.0";
        doc->dateCreated = nowIso();
        if (!doc->save()) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }
        doc->revise("Revision 1 — Erstanlage");
        // Copy to MFS
        if (!doc->importLocalFile(srcPath)) {
            std::cout << "  >> Warnung: MFS-Import fehlgeschlagen.\n";
        } else {
            std::cout << "  >> Datei in MFS abgelegt: " << doc->filePath << "\n";
        }
    }

    // ── Quelle 2: URL / Link ──────────────────────────────────────
    else if (src == 2) {
        std::string url = readLine("URL: ");
        if (url.empty()) return nullptr;
        doc->fileUrl    = url;
        doc->version    = readOpt("Version (leer=1.0): ");
        if (doc->version.empty()) doc->version = "1.0";
        doc->dateCreated = nowIso();
        std::cout << "  Herunterladen...\n";
        if (!doc->save()) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }
        doc->revise("Revision 1 — Erstanlage");
        // Download and import
        const std::string& base = Config::instance().basePath();
        std::string tmpDir = FileOps::joinPath(base, "documents", "tmp");
        FileOps::makeDirs(tmpDir);
        std::string downloaded = FileOps::downloadUrl(url, tmpDir);
        if (downloaded.empty()) {
            std::cout << "  >> Download fehlgeschlagen — URL gespeichert, Datei fehlt.\n";
        } else {
            // Detect format
            std::string ext = FileOps::extension(downloaded);
            if (!ext.empty()) doc->format = ext.substr(1);
            if (!doc->importLocalFile(downloaded)) {
                std::cout << "  >> MFS-Import fehlgeschlagen.\n";
            } else {
                std::cout << "  >> Heruntergeladen und in MFS abgelegt: " << doc->filePath << "\n";
            }
            FileOps::deleteFile(downloaded); // clean up tmp
        }
    }

    // ── Quelle 3: Neu anlegen ─────────────────────────────────────
    else {
        std::cout << "  Neue Datei anlegen:\n"
                  << "    1.Textdatei (.txt)   2.Word-Dokument (.docx)\n"
                  << "    3.Excel-Tabelle (.xlsx)  4.Präsentation (.pptx)\n"
                  << "    5.PDF-Vorlage (.pdf)  6.Weiteres Format\n";
        int nf = readInt("Art", 1, 6);
        doc->version     = readOpt("Version (leer=1.0): ");
        if (doc->version.empty()) doc->version = "1.0";
        doc->dateCreated = nowIso();
        if (!doc->save()) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }
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
        doc->update();
        std::cout << "  >> Leer-Datei angelegt: " << fpath << "\n";
    }

    // ── Gemeinsame optionale Felder ───────────────────────────────
    doc->summary    = readOpt("Kurzbeschreibung (optional): ");
    doc->authorId   = readOpt("Autor Person-ID (optional): ");
    doc->classification = readOpt("Einstufung (intern/vertraulich/öffentlich, leer=intern): ");
    if (doc->classification.empty()) doc->classification = "intern";
    doc->update();

    std::cout << "  >> Dokument registriert: " << doc->documentId << "\n";
    std::cout << "  >> Titel   : " << doc->title << "\n";
    std::cout << "  >> MFS-Pfad: " << (doc->filePath.empty() ? "(kein)" : doc->filePath) << "\n";

    // Lifecycle: ensure Rev 1 and Main WFI on first creation
    doc->revise("Revision 1 — Erstanlage");

    // Write MFS index file (the .txt metadata card)
    MFSWriter::writeDocument(*doc, Config::instance().mfsPath());

    std::cout << "  >> Revision 1 angelegt (in_work)\n";
    std::cout << "  >> Main Workflow gestartet\n";
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
                     "    3. released       0. Zurück\n";
        static const char* states[] = {"in_work","pre_released","released","locked","closed"};
        int ch = readInt("Zielzustand", 0, 5);
        if (ch == 0) return;
        std::string target = states[ch - 1];
        if (!DocumentRevision::isTransitionAllowed(rev->revState, target)) {
            std::cout << "  >> Übergang von '" << rev->revState
                      << "' nach '" << target << "' nicht erlaubt.\n";
            continue;
        }
        if (rev->transitionState(target)) {
            doc->status = target;
            doc->update();
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

        printDocument(*doc);

        if (immutable)
            std::cout << "  ⚠  Rev " << cur->rev << " ist " << cur->revState
                      << " — unveraenderlich\n\n";

        std::cout
            << "  [DOKUMENT]\n"
            << "    1. Bearbeiten (Felder)\n"
            << "    2. Revision: Zustand wechseln\n"
            << "    3. Neue Revision anlegen"
            << (inWork ? "  [GESPERRT — Revision noch in_work]" : "") << "\n"
            << "\n  [WORKFLOW]\n"
            << "    4. F77-Workflow anzeigen / starten\n"
            << "\n  [DATEI]\n"
            << "    5. Datei oeffnen (Lesen)\n";

        if (inWork) {
            std::cout
                << "    6. Checkout — Datei zum Bearbeiten holen\n";
            if (hasCheckout) {
                std::cout
                    << "    7. Checkin — Datei zurueckgeben (aktualisiert Revisions-Inhalt)\n"
                    << "    8. Aenderungen verwerfen (Revert auf Vorgaenger-Revision)\n";
            }
        }
        std::cout
            << "    9. URL neu herunterladen\n"
            << "   10. Versionsverlauf\n"
            << "\n  [SYSTEM]\n"
            << "   11. MFS-Datei schreiben\n"
            << "   12. Dokument loeschen\n"
            << "\n    0. Zurueck\n";
        hr();

        int ch = readInt("Wahl", 0, 12);
        if (ch == 0) break;

        if (ch == 1) {
            // Field editing — blocked when immutable
            if (!doc->canEdit()) {
                std::cout << "  >> Eingefroren — kein Bearbeiten moeglich.\n";
                continue;
            }
            hdr("BEARBEITEN — " + doc->documentId.substr(0, 24));
            std::string t = readOpt("Titel (leer=behalten): ");
            if (!t.empty()) doc->title = t;
            std::string cat = readOpt("Kategorie (leer=behalten): ");
            if (!cat.empty()) doc->docCategory = cat;
            std::string ver = readOpt("Version (leer=behalten): ");
            if (!ver.empty()) doc->version = ver;
            std::string cls = readOpt("Einstufung (intern/vertraulich/oeffentlich): ");
            if (!cls.empty()) doc->classification = cls;
            std::string sum = readOpt("Kurzbeschreibung: ");
            if (!sum.empty()) doc->summary = sum;
            std::string auth = readOpt("Autor-ID: ");
            if (!auth.empty()) doc->authorId = auth;
            std::string approv = readOpt("Genehmiger-ID: ");
            if (!approv.empty()) doc->approvedBy = approv;
            doc->update();
            std::cout << "  >> Gespeichert.\n";

        } else if (ch == 2) {
            // Revision lifecycle: 5-state transition
            revisionMenu(doc);

        } else if (ch == 3) {
            // New revision — refused if current revision is still in_work
            if (!doc->canRevise()) {
                std::cout
                    << "  >> Neue Revision nicht moeglich: aktive Revision ist noch in_work.\n"
                    << "  >> Erst F77-Workflow starten (rh -f77 -start " << doc->documentId
                    << ") und Revision freigeben.\n";
                continue;
            }
            std::string note = readLine("Revisionsnotiz (Pflicht): ");
            if (note.empty()) continue;
            std::string by = readOpt("Erstellt von (Person-ID, leer=Autor): ");
            auto newRev = doc->revise(note, by);
            if (newRev)
                std::cout << "  >> Revision " << newRev->rev << " angelegt (in_work).\n";
            else
                std::cout << "  >> Fehler: Revision konnte nicht angelegt werden.\n";

        } else if (ch == 4) {
            // Workflow section
            hdr("F77-WORKFLOW — " + doc->documentId.substr(0, 26));
            if (doc->releaseWorkflowId.empty()) {
                std::cout << "  Kein Workflow aktiv.\n";
                std::cout << "  Workflow jetzt starten? (ja/nein): ";
                std::string yn; std::getline(std::cin, yn);
                if (yn == "ja" || yn == "j") {
                    std::string wid = startWfInstanceWizard("dok", doc->documentId);
                    if (!wid.empty()) instanceMenu(wid);
                }
            } else {
                auto wf = F77_Workflow::loadById(doc->releaseWorkflowId);
                std::cout << "  Workflow-ID : " << doc->releaseWorkflowId.substr(0, 36) << "\n";
                std::cout << "  Status      : " << (wf ? wf->status : "unbekannt") << "\n";
                if (wf && (wf->status == "active")) {
                    if (readInt("  1.Oeffnen  0.Zurueck", 0, 1) == 1)
                        instanceMenu(doc->releaseWorkflowId);
                } else {
                    // Cancelled or completed — allow starting a new one
                    std::cout << "  Neuen Workflow starten? (ja/nein): ";
                    std::string yn; std::getline(std::cin, yn);
                    if (yn == "ja" || yn == "j") {
                        std::string wid = startWfInstanceWizard("dok", doc->documentId);
                        if (!wid.empty()) instanceMenu(wid);
                    }
                }
            }

        } else if (ch == 5) {
            // Read-only file open
            if (!doc->filePath.empty() && FileOps::fileExists(doc->filePath))
                doc->openFile("read");
            else
                std::cout << "  >> Keine Datei vorhanden.\n";

        } else if (ch == 6) {
            // Checkout — only when in_work
            if (!doc->canCheckout()) {
                std::cout << "  >> Checkout nur im Zustand in_work ohne offenen Checkout moeglich.\n";
                continue;
            }
            std::string dest = readOpt("Zielverzeichnis (leer = Standardpfad): ");
            std::string path = doc->checkout(dest);
            if (!path.empty())
                std::cout << "  >> Ausgecheckt nach: " << path << "\n";
            else
                std::cout << "  >> Checkout fehlgeschlagen.\n";

        } else if (ch == 7) {
            // Checkin — only when in_work with open checkout
            if (!doc->canCheckin()) {
                std::cout << "  >> Checkin nur bei offenem Checkout im Zustand in_work moeglich.\n";
                continue;
            }
            std::string src = readOpt("Dateipfad (leer = ausgecheckte Datei): ");
            if (doc->checkin(src))
                std::cout << "  >> Eingecheckt. Inhalt der Revision aktualisiert.\n";
            else
                std::cout << "  >> Checkin fehlgeschlagen.\n";

        } else if (ch == 8) {
            // Revert — only when in_work with open checkout
            if (!doc->canRevert()) {
                std::cout << "  >> Revert nur bei offenem Checkout mit Vorgaenger-Revision moeglich.\n";
                continue;
            }
            if (yesno("Aenderungen verwerfen und auf Vorgaenger-Revision zuruecksetzen?")) {
                if (doc->revertChanges())
                    std::cout << "  >> Revert erfolgreich.\n";
                else
                    std::cout << "  >> Revert: kein Vorgaenger oder Fehler.\n";
            }

        } else if (ch == 9) {
            // URL re-download
            if (doc->fileUrl.empty()) { doc->fileUrl = readLine("URL: "); doc->update(); }
            if (!doc->fileUrl.empty()) {
                if (doc->refreshFromUrl())
                    std::cout << "  >> Aktualisiert: " << doc->filePath << "\n";
                else
                    std::cout << "  >> Fehler beim Download.\n";
            }

        } else if (ch == 10) {
            // Version history
            auto vl = doc->loadVersions();
            hdr("VERSIONSVERLAUF (" + std::to_string(vl.size()) + ")");
            for (auto& v : vl)
                std::cout << "  Rev " << std::setw(3) << v.versionNumber
                          << "  " << v.createdAt.substr(0, 16)
                          << "  " << v.changeNote.substr(0, 40) << "\n";

        } else if (ch == 11) {
            MFSWriter::writeDocument(*doc, Config::instance().mfsPath());
            std::cout << "  >> MFS-Datei geschrieben.\n";

        } else if (ch == 12) {
            std::cout << "  Dokument " << doc->documentId << " loeschen? (ja): ";
            std::string c; std::getline(std::cin, c);
            if (c == "ja") { doc->remove(); std::cout << "  >> Geloescht.\n"; break; }
        }
    }
}


void documentBrowserMenu(const std::string& projectId, const std::string& taskId) {
    while (true) {
        std::vector<std::shared_ptr<Document>> docs;
        if (!projectId.empty())
            docs = Document::loadForProject(projectId);
        else if (!taskId.empty())
            docs = Document::loadForEntity("f22", taskId);
        else
            docs = Document::loadRecent(30);
        
        listDocuments(docs, "DOKUMENTE (" + std::to_string(docs.size()) + ")");
        std::cout << "  1.Öffnen  0.Zurück\n";
        int ch = readInt("Wahl",0,1); if (ch==0) break;
        if (ch==1 && !docs.empty()) {
            int pick = readInt("Nummer",1,(int)docs.size());
            documentMenu(docs[pick-1]);
        }
    }
}

} // namespace CLI

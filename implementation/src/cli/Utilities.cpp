// ============================================================
// Utilities.cpp  —  Shared CLI helpers, wizards, and print functions
//
// readLine/readOpt/readInt/yesno : user input primitives
// printProject/printTask/…       : formatted entity display
// createDocumentWizard()         : 3-source document registration
// attachDocumentDialog()         : universal attachment picker
// createProjectWizard()          : new project wizard
// createPersonWizard()           : new person wizard
// ============================================================
#include "cli_common.h"
#include "../workflow/F77Workflow.h"
#include "../workflow/F77Workflow.h"
#include "../model/dok/Document.h"
#include "../model/f22/TaskF22.h"
#include "../model/f16/ProjectF16.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include <sstream>
#include <algorithm>

namespace CLI {
void hr()  { std::cout << "  " << std::string(52,'-') << "\n"; }
void hdr(const std::string& t) {
    std::cout << "\n  +" << std::string(52,'-') << "+\n";
    std::cout << "  |  " << t << std::string(52 - (int)t.size() - 1,' ') << "|\n";
    std::cout << "  +" << std::string(52,'-') << "+\n";
}

// Read a non-empty line, trim trailing whitespace
// ------------------------------
// Prompt the user for a required line of text.
//
// Parameters:
//   prompt : displayed before the cursor
//
// Behavior:
//   - Prints prompt to stdout
//   - Reads one line via std::getline
//   - Returns the raw input (no trimming)
// ------------------------------
std::string readLine(const std::string& prompt) {
    if (!prompt.empty()) std::cout << "  " << prompt;
    std::string s;
    while (std::getline(std::cin, s)) {
        // trim right
        while (!s.empty() && (s.back()=='\r'||s.back()=='\n'||s.back()==' ')) s.pop_back();
        if (!s.empty()) return s;
        if (!prompt.empty()) std::cout << "  " << prompt;
    }
    return "";  // EOF
}

// Read integer in [lo,hi]
// ------------------------------
// Prompt the user for an integer within [lo, hi].
//
// Parameters:
//   prompt : displayed before the cursor
//   lo     : inclusive lower bound
//   hi     : inclusive upper bound
//
// Behavior:
//   - Re-prompts until a valid integer in range is entered
//   - Invalid input (non-numeric, out of range) is rejected silently
// ------------------------------
int readInt(const std::string& prompt, int lo, int hi) {
    while (true) {
        std::cout << "  " << prompt << " [" << lo << "-" << hi << "]: ";
        std::string s;
        if (!std::getline(std::cin, s)) return lo;
        try {
            int v = std::stoi(s);
            if (v >= lo && v <= hi) return v;
        } catch (...) {}
        std::cout << "  >> Please enter a number between " << lo << " and " << hi << "\n";
    }
}

// Read optional line (may be empty)
// ------------------------------
// Prompt the user for an optional line of text.
//
// Parameters:
//   prompt : displayed before the cursor
//
// Returns:
//   The input string, or "" if the user just pressed Enter.
//   Unlike readLine, empty input is explicitly valid here.
// ------------------------------
std::string readOpt(const std::string& prompt) {
    std::cout << "  " << prompt;
    std::string s;
    std::getline(std::cin, s);
    while (!s.empty() && (s.back()=='\r'||s.back()=='\n'||s.back()==' ')) s.pop_back();
    return s;
}

// ── Format a date string or "—" ───────────────────────────────
std::string fdate(const std::string& d) { return d.empty() ? "—" : d; }
std::string fval (const std::string& v) { return v.empty() ? "—" : v; }


// ─────────────────────────────────────────────────────────────
// DOCUMENT HELPERS
// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
// DISPLAY HELPERS
// ─────────────────────────────────────────────────────────────
void printPerson(const Rosenholz::Person& p) {
    hdr("PERSON  " + p.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",           p.personId);
    row("Name",         p.fullName());
    row("Email",        fval(p.email));
    row("Phone",        fval(p.phone));
    row("Role",         fval(p.roleTitle));
    row("Dept",         fval(p.department));
    row("Type",         fval(p.personType));
    row("Status",       fval(p.status));
    row("Day-rate",     std::to_string((int)p.dayRate) + " EUR");
    row("Avail.%",      std::to_string((int)p.availabilityPct));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}


void listProjects() {
    auto all = Rosenholz::ProjectF16::loadAll();
    if (all.empty()) { std::cout << "  (keine Projekte)\n"; return; }
    std::cout << "  " << std::left
              << std::setw(24) << "REG-NR"
              << std::setw(30) << "TITEL"
              << std::setw(14) << "STATUS"
              << std::setw(12) << "PHASE"
              << std::setw(8)  << "PRIO"
              << "CPI\n"
              << "  " << std::string(80,'-') << "\n";
    for (auto& p : all) {
        std::string title = p->title.size()>28 ? p->title.substr(0,27)+"~" : p->title;
        std::string phase = p->phase.empty() ? "-" : p->phase;
        std::string prio  = p->priority.empty() ? "-" : p->priority;
        char cpibuf[8]; snprintf(cpibuf, sizeof(cpibuf), "%.2f", p->cpi);
        std::cout << "  " << std::left
                  << std::setw(24) << p->regNumber.toString()
                  << std::setw(30) << title
                  << std::setw(14) << p->status
                  << std::setw(12) << phase
                  << std::setw(8)  << prio
                  << cpibuf << "\n";
    }
    std::cout << "  " << all.size() << " Projekt(e)\n";
}

void listTasks(const std::string& projectId) {
    auto tasks = Rosenholz::TaskF22::loadForProject(projectId);
    if (tasks.empty()) { std::cout << "  (keine Aufgaben)\n"; return; }
    std::cout << "  " << std::left
              << std::setw(24) << "REG-NR"
              << std::setw(28) << "TITEL"
              << std::setw(14) << "STATUS"
              << std::setw(6)  << "  %"
              << std::setw(10) << "PRIO"
              << "ASSIGNEE\n"
              << "  " << std::string(80,'-') << "\n";
    for (auto& t : tasks) {
        std::string title = t->title.size()>26 ? t->title.substr(0,25)+"~" : t->title;
        std::string ass   = t->assigneeId.empty() ? "-" : t->assigneeId.substr(0,12);
        std::cout << "  " << std::left
                  << std::setw(24) << t->regNumber.toString()
                  << std::setw(28) << title
                  << std::setw(14) << t->status
                  << std::setw(6)  << t->percentComplete
                  << std::setw(10) << t->priority
                  << ass << "\n";
    }
    std::cout << "  " << tasks.size() << " Aufgabe(n)\n";
}



void listPersons() {
    auto all = Rosenholz::Person::loadAll();
    if (all.empty()) { std::cout << "  (keine Personen)\n"; return; }
    std::cout << "  " << std::left
              << std::setw(20) << "REG-NR"
              << std::setw(24) << "NAME"
              << std::setw(16) << "ROLLE"
              << std::setw(14) << "TYP"
              << "STATUS\n"
              << "  " << std::string(76,'-') << "\n";
    for (auto& p : all) {
        std::string name = p->fullName().size()>22 ? p->fullName().substr(0,21)+"~" : p->fullName();
        std::string role = p->roleTitle.empty() ? "-" :
                           (p->roleTitle.size()>14 ? p->roleTitle.substr(0,13)+"~" : p->roleTitle);
        std::cout << "  " << std::left
                  << std::setw(20) << p->regNumber.toString()
                  << std::setw(24) << name
                  << std::setw(16) << role
                  << std::setw(14) << (p->employmentType.empty() ? "-" : p->employmentType)
                  << p->status << "\n";
    }
    std::cout << "  " << all.size() << " Person(en)\n";
}

std::shared_ptr<Rosenholz::ProjectF16> createProjectWizard() {
    hdr("CREATE NEW PROJECT (F16)");
    std::string title = readLine("Title: ");
    std::cout << "  Project type:\n"
              << "    1. OV  (Operativer Vorgang — active investigation)\n"
              << "    2. IM  (IM-Vorgang — contributor engagement)\n"
              << "    3. OPK (Operative Personenkontrolle — due diligence)\n"
              << "    4. GMS (GMS-Akte — advisory relationship)\n"
              << "    5. AU  (Untersuchungsvorgang — formal inquiry)\n"
              << "    6. SVG (Sicherungsvorgang — monitoring)\n";
    int tc = readInt("Choose type", 1, 6);
    static const char* types[] = {"OV","IM","OPK","GMS","AU","SVG"};
    std::string ptype = types[tc-1];

    std::cout << "  Size class:\n"
              << "    1. large   2. medium   3. small\n";
    int sc = readInt("Choose size", 1, 3);
    static const char* sizes[] = {"large","medium","small"};
    std::string size = sizes[sc-1];

    std::string codename   = readOpt("Codename (optional): ");
    std::string priority   = readOpt("Priority (high/medium/low, optional): ");
    std::string complexity = readOpt("Complexity (complex/moderate/simple, optional): ");
    std::string method     = readOpt("Methodology (agile/waterfall/kanban, optional): ");
    std::string scope      = readOpt("Scope statement (optional): ");
    std::string startPlan  = readOpt("Planned start date (YYYY-MM-DD, optional): ");
    std::string endPlan    = readOpt("Planned end date  (YYYY-MM-DD, optional): ");

    std::string budgetStr  = readOpt("Budget planned (EUR, optional): ");
    double budget = 0.0;
    if (!budgetStr.empty()) try { budget = std::stod(budgetStr); } catch(...) {}

    auto p = Rosenholz::ProjectF16::create(title, ptype, size);
    p->codename        = codename;
    p->priority        = priority;
    p->complexity      = complexity;
    p->methodology     = method;
    p->scopeStatement  = scope;
    p->startDatePlanned= startPlan;
    p->endDatePlanned  = endPlan;
    p->budgetPlanned   = budget;

    if (p->save()) {
        p->ensureReleaseWorkflow();
        std::cout << "\n  >> Project created: " << p->regNumber.toString()
                  << " (" << p->projectId << ")\n\n";
        // write MFS file
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) p->writeMFSFile(cfg.mfsPath());
        return p;
    } else {
        std::cout << "\n  >> ERROR: Project could not be saved.\n\n";
        return nullptr;
    }
}

std::shared_ptr<Rosenholz::TaskF22> createTaskWizard(const std::string& projectId) {
    hdr("CREATE TASK (F22)");
    std::string title    = readLine("Title: ");
    std::string desc     = readOpt("Description (optional): ");
    std::string assignee = readOpt("Assignee-ID (optional): ");
    std::string parent   = readOpt("Parent task-ID (optional): ");
    std::string priority = readOpt("Priority (high/medium/low): ");
    std::string wbs      = readOpt("WBS code (e.g. 1.2.3, optional): ");
    std::string start    = readOpt("Planned start (YYYY-MM-DD, optional): ");
    std::string due      = readOpt("Due date      (YYYY-MM-DD, optional): ");
    std::string effortStr= readOpt("Planned effort hours (optional): ");
    double effort = 0.0;
    if (!effortStr.empty()) try { effort = std::stod(effortStr); } catch(...) {}

    auto t = Rosenholz::TaskF22::create(projectId, title, assignee, parent);
    t->description      = desc;
    t->priority         = priority;
    t->wbsCode          = wbs;
    t->startDatePlanned = start;
    t->dueDatePlanned   = due;
    t->effortPlannedHrs = effort;

    if (t->save()) {
        t->ensureReleaseWorkflow();
        std::cout << "\n  >> Task created: " << t->regNumber.toString()
                  << " (" << t->taskId << ")\n\n";
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) t->writeMFSFile(cfg.mfsPath());
        return t;
    } else {
        std::cout << "\n  >> ERROR: Task could not be saved.\n\n";
        return nullptr;
    }
}



std::shared_ptr<Rosenholz::Person> createPersonWizard() {
    hdr("CREATE PERSON");
    std::string first  = readLine("First name: ");
    std::string last   = readLine("Last name: ");
    std::string email  = readOpt("Email (optional): ");
    std::string role   = readOpt("Role title (optional): ");
    std::string dept   = readOpt("Department (optional): ");
    std::cout << "  Person type:\n"
              << "    1. internal  2. external  3. contractor  4. advisor\n";
    int tc = readInt("Choose type", 1, 4);
    static const char* ptypes[] = {"internal","external","contractor","advisor"};

    std::string rateStr = readOpt("Day rate EUR (optional): ");
    double rate = 0.0;
    if (!rateStr.empty()) try { rate = std::stod(rateStr); } catch(...) {}

    auto p = Rosenholz::Person::create(first, last, email, ptypes[tc-1]);
    p->roleTitle  = role;
    p->department = dept;
    p->dayRate    = rate;

    if (p->save()) {
        std::cout << "\n  >> Person created: " << p->regNumber.toString()
                  << " (" << p->personId << ")\n\n";
        return p;
    } else {
        std::cout << "\n  >> ERROR: Person could not be saved.\n\n";
        return nullptr;
    }
}


// ------------------------------
// Interactive wizard: register a new Document with MFS filing.
//
// Parameters:
//   projectId : owning project ID (may be empty if taskId is set)
//   taskId    : owning task ID (may be empty if projectId is set)
//
// Source selection (always shown):
//   1. Lokale Datei  — user supplies a path; file is copied into MFS
//   2. Link / URL    — file is downloaded and imported into MFS
//   3. Neu anlegen   — a stub file is created directly in MFS
//
// Behavior (all sources):
//   - Title, type, format, version, classification requested
//   - Document record saved to DB first (gets ID)
//   - Physical file placed under mfs/DOK/{parent-sane}/
//     using naming convention: {DOK-ID}_{Title}_v{Version}.{ext}
//   - MFSWriter::writeDocument() called to write index card
//   - fileSize and SHA-256 fileHash computed and stored
//
// Returns:
//   Shared pointer to saved Document, or nullptr on cancel/error
// ------------------------------
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
        doc->ensureRevision1();
        doc->ensureReleaseWorkflow();
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
        doc->ensureRevision1();
        doc->ensureReleaseWorkflow();
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
        doc->ensureRevision1();
        doc->ensureReleaseWorkflow();

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
    doc->ensureRevision1();
    doc->ensureReleaseWorkflow();

    // Write MFS index file (the .txt metadata card)
    MFSWriter::writeDocument(*doc, Config::instance().mfsPath());

    std::cout << "  >> Revision 1 angelegt (in_work)\n";
    std::cout << "  >> Main Workflow gestartet\n";
    return doc;
}


// ─────────────────────────────────────────────────────────────
// Universal document attachment dialog
// Called from any entity that can attach documents.
// Shows: Lokale Datei / Link / Neu anlegen / Bestehend per ID
// Copies to MFS automatically and returns the Document.
// ─────────────────────────────────────────────────────────────
// ------------------------------
// Universal document attachment dialog.
//
// Presented whenever any entity needs to attach a document.
// Wraps createDocumentWizard() and Document::loadById().
//
// Parameters:
//   projectId : context project ID (passed to wizard if creating)
//   taskId    : context task ID (passed to wizard if creating)
//
// Options shown:
//   1. Lokale Datei  — new doc, source = local file
//   2. Link / URL    — new doc, source = URL download
//   3. Neu anlegen   — new doc, stub file created in MFS
//   4. Bestehend per ID — load existing doc by ID (no copy)
//   0. Abbrechen
//
// Returns:
//   Shared pointer to the Document (new or existing), or nullptr
// ------------------------------
std::shared_ptr<Rosenholz::Document> attachDocumentDialog(
    const std::string& projectId, const std::string& taskId)
{
    using namespace Rosenholz;

    std::cout << "\n  Dokument anhängen:\n"
              << "    1. Lokale Datei (Pfad → in MFS kopieren)\n"
              << "    2. Link / URL   (herunterladen → in MFS)\n"
              << "    3. Neu anlegen  (leere Datei in MFS)\n"
              << "    4. Bestehendes Dokument per ID\n"
              << "    0. Abbrechen\n";
    int ch = readInt("Wahl", 0, 4);
    if (ch == 0) return nullptr;

    if (ch == 1 || ch == 2 || ch == 3) {
        // Delegate to the full wizard (handles all 3 source types)
        // Pass the source type hint via a prefix prompt
        return createDocumentWizard(projectId, taskId);
    }

    // ch == 4: Existing document by ID
    std::string docId = readLine("Dokument-ID (XV/DOK/...): ");
    if (docId.empty()) return nullptr;
    auto doc = Document::loadById(docId);
    if (!doc) {
        std::cout << "  >> Dokument nicht gefunden.\n";
        return nullptr;
    }
    std::cout << "  Gefunden: " << doc->title << " [" << doc->status << "]\n";
    return doc;
}

// ------------------------------
// showRecentItems
//
// Displays the 20 most recently created items across ALL entity types.
// Used before any ID-entry prompt so the user can pick from a list.
//
// Parameters:
//   types : bitmask or comma-sep list of which types to include
//           (empty = all types)
//
// Format:
//   [F16] XV/F16/0001/2026   Projektname         2026-04-01
//   [F22] XV/F22/0003/2026   Aufgabentitel        2026-04-02
//   ...
// ------------------------------
void showRecentItems(const std::string& filter) {
    using namespace Rosenholz;
    hdr("ZULETZT ANGELEGT (letzte 20 je Typ)");

    bool all = filter.empty();

    if (all || filter.find("F16") != std::string::npos) {
        auto items = ProjectF16::loadRecent(20);
        if (!items.empty()) {
            std::cout << "  [F16] Projekte:\n";
            for (auto& p : items)
                std::cout << "    [F16] " << std::left << std::setw(28)
                          << p->projectId.substr(0,26)
                          << "  " << std::setw(28) << p->title.substr(0,26)
                          << "  " << p->status << "\n";
        }
    }
    if (all || filter.find("F22") != std::string::npos) {
        auto items = TaskF22::loadRecent(20);
        if (!items.empty()) {
            std::cout << "  [F22] Aufgaben:\n";
            for (auto& t : items)
                std::cout << "    [F22] " << std::left << std::setw(28)
                          << t->taskId.substr(0,26)
                          << "  " << std::setw(28) << t->title.substr(0,26)
                          << "  " << t->status << "\n";
        }
    }
    if (all || filter.find("F18") != std::string::npos) {
        auto items = Rosenholz::F18Operation::loadRecent(20);
        if (!items.empty()) {
            std::cout << "  [F18] Workflows:\n";
            for (auto& v : items)
                std::cout << "    [F18] " << std::left << std::setw(28)
                          << v->vorgangId.substr(0,26)
                          << "  " << std::setw(20) << v->title.substr(0,18)
                          << "  [" << v->vorgangType.substr(0,12) << "]\n";
        }
    }

    if (all || filter.find("DOK") != std::string::npos) {
        auto items = Document::loadRecent(20);
        if (!items.empty()) {
            std::cout << "  [DOK] Dokumente:\n";
            for (auto& d : items)
                std::cout << "    [DOK] " << std::left << std::setw(28)
                          << d->documentId.substr(0,26)
                          << "  " << std::setw(28) << d->title.substr(0,26)
                          << "  v" << d->version << "\n";
        }
    }
    std::cout << "\n";
}

// ------------------------------
// globalSearch
//
// Search across ALL entity categories by keyword.
// Shows results grouped by type. Offers to jump to the entity.
//
// Parameters:
//   query : substring matched case-insensitively against title/name/ID
//
// Categories searched:
//   F16 Projects, F22 Tasks, F18 Incidents, RSK Risks,
//   DOK Documents, PER Persons, WFI Workflow Instances
// ------------------------------
void globalSearch(const std::string& query) {
    using namespace Rosenholz;
    if (query.empty()) return;

    std::string lq = query;
    for (char& c : lq) c = (char)std::tolower(c);

    auto match = [&](const std::string& s) {
        std::string ls = s;
        for (char& c : ls) c = (char)std::tolower(c);
        return ls.find(lq) != std::string::npos;
    };

    hdr("GLOBALE SUCHE: \"" + query + "\"");

    struct Hit { std::string typeCode; std::string id; std::string label; std::string sub; };
    std::vector<Hit> hits;

    // F16 Projects
    auto projs = ProjectF16::loadRecent(200);
    for (auto& p : projs)
        if (match(p->title) || match(p->projectId) || match(p->codename))
            hits.push_back({"F16", p->projectId, p->title, p->status});

    // F22 Tasks
    auto tasks = TaskF22::loadRecent(200);
    for (auto& t : tasks)
        if (match(t->title) || match(t->taskId))
            hits.push_back({"F22", t->taskId, t->title, t->status});

    // F18 Workflows (all types)
    auto vorgaenge = Rosenholz::F18Operation::loadRecent(200);
    for (auto& v : vorgaenge)
        if (match(v->title) || match(v->vorgangId))
            hits.push_back({"F18", v->vorgangId, v->title,
                            v->vorgangType + "|" + v->status});

    // DOK Documents
    auto docs = Document::loadRecent(200);
    for (auto& d : docs)
        if (match(d->title) || match(d->documentId) || match(d->tags))
            hits.push_back({"DOK", d->documentId, d->title, "v"+d->version});

    // F77 Workflows (active)
    auto wfis = F77_Workflow::loadActive();
    for (auto& w : wfis)
        if (match(w->templateName) || match(w->workflowId))
            hits.push_back({"F77", w->workflowId, w->templateName, w->status});

    if (hits.empty()) {
        std::cout << "  (keine Treffer für \"" << query << "\")\n\n";
        return;
    }

    // Display
    int n = 1;
    for (auto& h : hits) {
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << "[" << std::left << std::setw(4) << h.typeCode << "] "
                  << std::setw(26) << h.id.substr(0,24)
                  << "  " << std::setw(28) << h.label.substr(0,26)
                  << "  " << h.sub.substr(0,14) << "\n";
    }
    std::cout << "\n  " << hits.size() << " Treffer gefunden.\n";

    // Jump to entity
    std::cout << "  Nummer zum Öffnen (leer=zurück): ";
    std::string pick; std::getline(std::cin, pick);
    if (pick.empty()) return;

    int idx = 0;
    try { idx = std::stoi(pick)-1; } catch(...) { return; }
    if (idx < 0 || idx >= (int)hits.size()) return;

    auto& h = hits[idx];
    if (h.typeCode == "F16") {
        auto p = ProjectF16::loadById(h.id);
        if (p) { p->loadQTCSLinks(); projectMenu(p); }
    } else if (h.typeCode == "F22") {
        auto t = TaskF22::loadById(h.id);
        if (t) taskMenu(t);
    } else if (h.typeCode == "F18") {
        auto v = Rosenholz::F18Operation::loadById(h.id);
        if (v) f18Menu(v);
    } else if (h.typeCode == "DOK") {
        auto d = Document::loadById(h.id);
        if (d) documentMenu(d);
    } else if (h.typeCode == "WFI") {
        instanceMenu(h.id);
    }
}


// ------------------------------
// printF18Operation — Display an F18Operation entity in a structured box.
// Shows common fields plus type-specific fields.
// ------------------------------
void printF18Operation(const Rosenholz::F18Operation& v) {
    using namespace Rosenholz;
    hdr("F18 WORKFLOW  " + v.vorgangId.substr(0, 22));
    auto row = [](const std::string& k, const std::string& val) {
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(30) << val.substr(0,29) << "|\n";
    };
    auto hr = []() { std::cout << "  +" << std::string(52,'-') << "+\n"; };
    row("ID",          v.vorgangId);
    row("Typ",         v.vorgangType);
    row("Titel",       v.title);
    row("Status",      v.status);
    row("Priorität",   v.priority);
    row("Owner-ID",    fval(v.ownerId));
    if (!v.projectId.empty()) row("Projekt-ID", v.projectId);
    if (!v.taskId.empty())    row("Aufgabe-ID", v.taskId);
    hr();
    // Type-specific fields
    if (v.vorgangType == "incident") {
        row("Vorfall-Typ",   fval(v.incidentType));
        row("Schwere",       fval(v.severity));
        row("Aufgetreten",   fdate(v.occurredDate));
        row("Gelöst",        fdate(v.resolvedDate));
        row("Ursache",       fval(v.rootCause));
    } else if (v.vorgangType == "risk") {
        row("Risiko-Level",  v.riskLevel);
        row("W-Score",       std::to_string(v.probabilityScore));
        row("Ges.Score",     std::to_string(v.overallRiskScore));
        row("Strategie",     fval(v.responseStrategy));
    } else if (v.vorgangType == "measure") {
        row("Kategorie",     fval(v.measureCategory));
        row("Geplant",       fdate(v.plannedDate));
        row("Ist",           fdate(v.actualDate));
        row("Wirksamkeit",   fval(v.effectiveness));
    } else if (v.vorgangType == "qualityGate") {
        row("Phase",         fval(v.phase));
        row("Ergebnis",      fval(v.gateResult));
        row("Entscheidung",  fval(v.gateDecision));
    } else if (v.vorgangType == "changeRequest") {
        row("CR-Typ",        fval(v.changeType));
        row("Begründung",    fval(v.justification));
        row("Entscheidung",  fval(v.crDecisionDate));
    } else if (v.vorgangType == "changeObject") {
        row("Basis-CR",      fval(v.parentVorgangId));
        row("Ausgeführt von",fval(v.executedBy));
        row("Ausf.-Datum",   fdate(v.executionDate));
    } else if (v.vorgangType == "lessonsLearned") {
        row("Typ",           fval(v.lessonType));
        row("Empfehlung",    fval(v.recommendation));
    } else if (v.vorgangType == "decisionLog") {
        row("Entsch.-Typ",   fval(v.decisionType));
        row("Rationale",     fval(v.rationale));
        row("Datum",         fdate(v.decisionDate));
    } else if (v.vorgangType == "assumptionConstraint") {
        row("AC-Typ",        fval(v.acType));
        row("Auswirkung",    fval(v.impact));
    } else if (v.vorgangType == "communicationPlan") {
        row("Zielgruppe",    fval(v.audience));
        row("Häufigkeit",    fval(v.frequency));
        row("Kanal",         fval(v.channel));
    }
    hr();
}


// ------------------------------
// createF18Wizard — Interactive wizard to create a new F18Operation.
//
// Parameters:
//   projectId : owning F16 project ID
//   taskId    : owning F22 task ID (optional)
//   type      : pre-selected vorgangType (optional, prompted if empty)
// ------------------------------
std::shared_ptr<Rosenholz::F18Operation> createF18Wizard(
    const std::string& taskId,
    const std::string& type)
{
    using namespace Rosenholz;
    hdr("NEUEN F18 WORKFLOW ANLEGEN");

    // Step 1: Title
    std::string title = readLine("Titel des Vorgangs: ");
    if (title.empty()) return nullptr;

    // Step 2: Type (ask after title, not upfront)
    std::string chosenType = type;
    if (chosenType.empty()) {
        hdr("TYP WÄHLEN");
        std::cout
            << "    1.  Incident         (Vorfall)\n"
            << "    2.  Risk             (Risiko)\n"
            << "    3.  Measure          (Maßnahme)\n"
            << "    4.  QualityGate      (Qualitätsstor)\n"
            << "    5.  AssumptionConstraint\n"
            << "    6.  CommunicationPlan\n"
            << "    7.  LessonsLearned\n"
            << "    8.  DecisionLog\n"
            << "    9.  ChangeRequest\n"
            << "   10.  ChangeObject\n"
            << "   11.  Generic\n";
        static const char* types[] = {
            "incident","risk","measure","qualityGate",
            "assumptionConstraint","communicationPlan","lessonsLearned",
            "decisionLog","changeRequest","changeObject","generic"};
        int t = readInt("Typ",1,11);
        chosenType = types[t-1];
    }
    // Wizard: common fields (title was already asked above)
    std::string owner  = readOpt("Verantwortlich (Person-ID, leer=offen): ");
    std::string prio   = readOpt("Priorität (low/medium/high/critical, leer=medium): ");

    auto v = Rosenholz::F18Operation::create(taskId, title, chosenType);
    if (!v) {
        std::cout << "  >> FEHLER: F18 Workflow konnte nicht angelegt werden.\n";
        return nullptr;
    }
    if (!owner.empty()) v->ownerId   = owner;
    if (!prio.empty())  v->priority  = prio;
    else                v->priority  = "medium";
    v->update();

    std::cout << "  >> F18 Vorgang angelegt: " << v->vorgangId << "\n";
    std::cout << "  >> Typ: " << v->vorgangType << "  Status: " << v->status << "\n";
    return v;
}

} // namespace CLI

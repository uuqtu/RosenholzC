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
void printDocument(const Rosenholz::Document& d) {
    hdr("DOCUMENT  " + d.documentId.substr(0, 16) + "...");
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(22) << k
                  << std::setw(30) << v << "|\n";
    };
    row("ID",           d.documentId);
    row("Title",        d.title);
    row("Type",         fval(d.docType));
    row("Category",     fval(d.docCategory));
    row("Version",      fval(d.version));
    row("Status",       fval(d.status));
    row("Format",       fval(d.format));
    row("Language",     fval(d.language));
    row("Classification",fval(d.classification));
    hr();
    row("Author-ID",    fval(d.authorId));
    row("Approved-by",  fval(d.approvedBy));
    row("Project-ID",   fval(d.projectId));
    row("Task-ID",      fval(d.taskId));
    hr();
    row("Created",      fdate(d.dateCreated));
    row("Modified",     fdate(d.dateModified));
    row("Approved",     fdate(d.dateApproved));
    row("Expires",      fdate(d.dateExpires));
    hr();
    row("Storage",      fval(d.storageSystem));
    row("Pages",        d.pageCount > 0 ? std::to_string(d.pageCount) : "—");
    if (!d.fileUrl.empty())
        row("Source URL", d.fileUrl.size() > 29
            ? d.fileUrl.substr(0,28) + "~" : d.fileUrl);
    if (!d.filePath.empty()) {
        row("MFS-Pfad", d.filePath.size() > 29
            ? "..."+d.filePath.substr(d.filePath.size()-26) : d.filePath);
        if (d.fileSize > 0)
            row("Dateigröße", std::to_string(d.fileSize/1024+1) + " KB");
        if (!d.fileHash.empty())
            row("SHA-256", d.fileHash.size() > 20 ?
                d.fileHash.substr(0,18)+"..." : d.fileHash);
    }
    if (!d.summary.empty())
        std::cout << "  | Summary: " << d.summary.substr(0,43) << "|\n";
    if (!d.tags.empty())
        row("Tags",      d.tags);
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

void listDocuments(const std::vector<std::shared_ptr<Rosenholz::Document>>& docs,
                          const std::string& heading) {
    if (docs.empty()) { std::cout << "\n  (no documents)\n\n"; return; }
    hdr(heading);
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(12) << "Type"
              << std::setw(26) << "Title"
              << std::setw(8)  << "Status"
              << std::setw(7)  << "Format"
              << "\n";
    hr();
    int n = 1;
    for (auto& d : docs) {
        std::string title = d->title.size() > 24 ? d->title.substr(0,23)+"~" : d->title;
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(12) << fval(d->docType)
                  << std::setw(26) << title
                  << std::setw(8)  << fval(d->status)
                  << std::setw(7)  << fval(d->format)
                  << "\n";
    }
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────
// DISPLAY HELPERS
// ─────────────────────────────────────────────────────────────
void printProject(const Rosenholz::ProjectF16& p) {
    hdr("PROJECT (F16)  " + p.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",           p.projectId);
    row("Reg-Nr",       p.regNumber.toString());
    row("Title",        p.title);
    row("Codename",     fval(p.codename));
    row("Type",         fval(p.projectType));
    row("Size",         fval(p.sizeClass));
    row("Status",       fval(p.status));
    row("Phase",        fval(p.phase));
    row("Priority",     fval(p.priority));
    row("Complexity",   fval(p.complexity));
    row("Methodology",  fval(p.methodology));
    hr();
    row("Lead-ID",      fval(p.leadId));
    row("Team-ID",      fval(p.ownerTeamId));
    row("Sponsor-ID",   fval(p.sponsorId));
    hr();
    row("Start planned",fdate(p.startDatePlanned));
    row("Start actual", fdate(p.startDateActual));
    row("End planned",  fdate(p.endDatePlanned));
    row("End actual",   fdate(p.endDateActual));
    row("Sched.var.(d)",std::to_string(p.scheduleVarianceDays));
    hr();
    row("Budget plan",  std::to_string((int)p.budgetPlanned) + " " + p.currency);
    row("Budget actual",std::to_string((int)p.budgetActual)  + " " + p.currency);
    row("Cost var.",    std::to_string((int)p.costVariance)  + " " + p.currency);
    row("CPI",          std::to_string(p.cpi).substr(0,6));
    row("SPI",          std::to_string(p.spi).substr(0,6));
    row("EV",           std::to_string((int)p.earnedValue));
    row("EAC",          std::to_string((int)p.eac));
    hr();
    row("Scope ver.",   fval(p.scopeVersion));
    row("Scope chgs",   std::to_string(p.scopeChangeCount));
    if (!p.scopeStatement.empty())
        std::cout << "  | Scope: " << p.scopeStatement.substr(0,46) << "|\n";
    hr();
    // QTCS dimension counts
    std::cout << "  | Quality dims: " << std::left << std::setw(4) << p.qualityIds.size()
              << "  Cost dims: " << std::setw(4) << p.costIds.size()
              << "  Time dims: " << std::setw(4) << p.timeIds.size()
              << "  Scope dims: " << std::setw(4) << p.scopeIds.size() << "|\n";
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

void printIncident(const Rosenholz::IncidentF18& i) {
    hdr("INCIDENT (F18)  " + i.regNumber.toString());
    auto row = [](const std::string& k, const std::string& v){
        std::cout << "  | " << std::left << std::setw(24) << k
                  << std::setw(28) << v << "|\n";
    };
    row("ID",             i.incidentId);
    row("Reg-Nr",         i.regNumber.toString());
    row("Title",          i.title);
    row("Project-ID",     fval(i.projectId));
    row("Status",         fval(i.status));
    row("Severity",       fval(i.severity));
    row("Type",           fval(i.incidentType));
    row("Category",       fval(i.category));
    row("Owner-ID",       fval(i.ownerId));
    row("Reported-by",    fval(i.reportedBy));
    hr();
    row("Occurred",       fdate(i.occurredDate));
    row("Reported",       fdate(i.reportedDate));
    row("Resolved",       fdate(i.resolvedDate));
    hr();
    row("Cost impact",    std::to_string((int)i.costImpact));
    row("Sched.impact(d)",std::to_string(i.scheduleImpactDays));
    row("Scope impact",   fval(i.scopeImpact));
    row("Quality impact", fval(i.qualityImpact));
    hr();
    row("Root cause",     i.rootCause.empty() ? "—" : i.rootCause.substr(0,27));
    row("Immed.action",   i.immediateAction.empty() ? "—" : i.immediateAction.substr(0,27));
    row("Resolution",     i.resolution.empty() ? "—" : i.resolution.substr(0,27));
    row("Escalated",      i.escalated ? "YES -> " + i.escalatedTo : "no");
    row("Linked Risk",    fval(i.riskId));
    std::cout << "  +" << std::string(52,'-') << "+\n\n";
}

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
    if (all.empty()) {
        std::cout << "\n  (no projects yet)\n\n";
        return;
    }
    hdr("ALL PROJECTS");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(14) << "Reg-Nr"
              << std::setw(28) << "Title"
              << std::setw(8)  << "Status"
              << "\n";
    hr();
    int n = 1;
    for (auto& p : all) {
        std::string title = p->title.size() > 26 ? p->title.substr(0,25)+"~" : p->title;
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(14) << p->regNumber.toString()
                  << std::setw(28) << title
                  << std::setw(8)  << p->status
                  << "\n";
    }
    std::cout << "\n";
}

void listTasks(const std::string& projectId) {
    auto tasks = Rosenholz::TaskF22::loadForProject(projectId);
    if (tasks.empty()) {
        std::cout << "\n  (no tasks for this project)\n\n";
        return;
    }
    hdr("TASKS FOR PROJECT");
    std::cout << "  " << std::left
              << std::setw(4)  << "#"
              << std::setw(14) << "Reg-Nr"
              << std::setw(24) << "Title"
              << std::setw(6)  << "WBS"
              << std::setw(8)  << "Status"
              << std::setw(5)  << "%"
              << "\n";
    hr();
    int n = 1;
    for (auto& t : tasks) {
        std::string title = t->title.size()>22 ? t->title.substr(0,21)+"~" : t->title;
        std::cout << "  " << std::left
                  << std::setw(4)  << n++
                  << std::setw(14) << t->regNumber.toString()
                  << std::setw(24) << title
                  << std::setw(6)  << fval(t->wbsCode)
                  << std::setw(8)  << t->status
                  << std::setw(5)  << t->percentComplete
                  << "\n";
    }
    std::cout << "\n";
}

void listIncidents(const std::string& projectId) {
    auto incs = Rosenholz::IncidentF18::loadForProject(projectId);
    if (incs.empty()) { std::cout << "\n  (no incidents)\n\n"; return; }
    hdr("INCIDENTS FOR PROJECT");
    int n = 1;
    for (auto& i : incs) {
        std::cout << "  " << std::setw(3) << n++ << ". ["
                  << i->regNumber.toString() << "]  "
                  << std::left << std::setw(26) << i->title
                  << "  sev=" << i->severity
                  << "  status=" << i->status << "\n";
    }
    std::cout << "\n";
}

void listPersons() {
    auto all = Rosenholz::Person::loadAll();
    if (all.empty()) { std::cout << "\n  (no persons yet)\n\n"; return; }
    hdr("PERSONS");
    int n = 1;
    for (auto& p : all) {
        std::cout << "  " << std::setw(3) << n++ << ". ["
                  << p->regNumber.toString() << "]  "
                  << std::left << std::setw(22) << p->fullName()
                  << "  " << std::setw(20) << p->email
                  << "  " << p->roleTitle << "\n";
    }
    std::cout << "\n";
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

std::shared_ptr<Rosenholz::IncidentF18> createIncidentWizard(const std::string& projectId) {
    hdr("CREATE INCIDENT (F18)");
    std::string title    = readLine("Title: ");
    std::string desc     = readOpt("Description (optional): ");
    std::cout << "  Severity:\n"
              << "    1. critical  2. high  3. medium  4. low\n";
    int sc = readInt("Choose severity", 1, 4);
    static const char* sevs[] = {"critical","high","medium","low"};
    std::string sev = sevs[sc-1];

    std::string type     = readOpt("Incident type (financial/technical/schedule/quality, optional): ");
    std::string occurred = readOpt("Occurred date (YYYY-MM-DD, optional): ");
    std::string reporter = readOpt("Reported-by person-ID (optional): ");
    std::string cause    = readOpt("Root cause (optional): ");
    std::string action   = readOpt("Immediate action taken (optional): ");
    std::string costStr  = readOpt("Cost impact EUR (optional): ");
    double cost = 0.0;
    if (!costStr.empty()) try { cost = std::stod(costStr); } catch(...) {}

    auto i = Rosenholz::IncidentF18::create(projectId, title, sev, reporter);
    i->description    = desc;
    i->incidentType   = type;
    i->occurredDate   = occurred;
    i->rootCause      = cause;
    i->immediateAction= action;
    i->costImpact     = cost;

    if (i->save()) {
        std::cout << "\n  >> Incident created: " << i->regNumber.toString()
                  << " (" << i->incidentId << ")\n\n";
        auto& cfg = Rosenholz::Config::instance();
        if (cfg.mfs().enabled) i->writeMFSFile(cfg.mfsPath());
        return i;
    } else {
        std::cout << "\n  >> ERROR: Incident could not be saved.\n\n";
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
                  << "    1.Textdatei (.txt)  2.PDF-Vorlage  "
                     "3.Weitere (Format oben angegeben)\n";
        int nf = readInt("Art", 1, 3);
        doc->version     = readOpt("Version (leer=1.0): ");
        if (doc->version.empty()) doc->version = "1.0";
        doc->dateCreated = nowIso();
        if (!doc->save()) { std::cout << "  >> DB-Fehler.\n"; return nullptr; }

        // Create empty file in MFS
        const std::string& mfs = Config::instance().mfsPath();
        std::string sane = sanitiseRegNr(doc->documentId);
        std::string safeName = FileOps::sanitizeFilename(doc->title);
        if (safeName.size() > 40) safeName = safeName.substr(0, 40);
        std::string ext = (nf == 1) ? "txt" : (nf == 2 ? "pdf" : doc->format);
        doc->format = ext;

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

    // Write MFS index file (the .txt metadata card)
    MFSWriter::writeDocument(*doc, Config::instance().mfsPath());

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

} // namespace CLI

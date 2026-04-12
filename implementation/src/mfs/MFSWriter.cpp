// ============================================================
// MFSWriter.cpp  —  DDR MfS-style physical filing system
//
// Folder structure mirrors actual MfS Ablage:
//
//   mfs/
//   ├── F16/                    (Vorgangsakten)
//   │   └── XV/2026/
//   │       ├── F16_XV_F16_0001_2026.txt         (Karteikarte)
//   │       └── XV_F16_0001_2026/                (Hängeregister)
//   │           ├── F22/                         (Aufgaben-Unterhefter)
//   │           │   └── F22_XV_F22_0003_2026.txt
//   │           ├── F18/                         (Vorfall-Unterhefter)
//   │           ├── DOK/                         (Dokumente)
//   │           │   └── DOK_XV_DOK_0001_2026_Projektcharter.txt
//   │           ├── MASS/                        (Massnahmen)
//   │           ├── QG/                          (Qualitaetstore)
//   │           ├── KPI/
//   │           ├── LL/                          (Lernerkenntnisse)
//   │           ├── DL/                          (Entscheidungen)
//   │           ├── CR/                          (Aenderungsantraege)
//   │           ├── AC/                          (Annahmen/Beschraenkungen)
//   │           └── MS/                          (Meilensteine)
//   ├── F22/                    (Aufgabenakten - standalone index)
//   ├── F18/                    (Vorfallakten  - standalone index)
//   ├── PERSONEN/
//   ├── DIENSTEINHEITEN/
//   ├── RISIKEN/
//   └── owner_key.txt           (Klarnamendatei - owner-only 600)
// ============================================================

#include "MFSWriter.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include "../core/Config.h"
#include "../model/ProjectF16.h"
#include "../model/TaskF22.h"
#include "../model/IncidentF18.h"
#include "../model/Risk.h"
#include "../model/Document.h"
#include "../model/Person.h"
#include "../model/Team.h"
#include "../model/Utils.h"

#ifndef _WIN32
  #include <sys/stat.h>
#endif
#include <sstream>
#include <map>

namespace Rosenholz {

// ── Helper: owner-only file write ────────────────────────────
bool MFSWriter::ownerOnlyWrite(const std::string& path, const std::string& content) {
    FileOps::makeDirs(FileOps::dirName(path));
    bool ok = FileOps::writeTextFile(path, content, false);
#ifndef _WIN32
    if (ok) chmod(path.c_str(), S_IRUSR | S_IWUSR);  // 600 — owner only
#endif
    return ok;
}

// ── Get Diensteinheit code from config ───────────────────────
static std::string deCode() {
    const std::string& de = Config::instance().registratur().diensteinheitKuerzel;
    return de.empty() ? "XX" : de;
}

// ── Helper: Hängeregister root for a project ─────────────────
// Returns mfs/F16/<DE>/<YEAR>/<sanitised_regNr>/
static std::string projectHeft(const std::string& mfsRoot, const std::string& regNr) {
    // regNr = "F16/108/2026" — the *sequential* reg number (type/seq/year)
    // Folder path: mfs/F16/<DE-from-config>/<year>/<sanitised_regNr>/
    // DE comes from config.registratur.diensteinheitKuerzel, NOT from regNr
    std::string year = "0000";
    auto sl = regNr.rfind('/');
    if (sl != std::string::npos) year = regNr.substr(sl + 1);
    std::string sane = sanitiseRegNr(regNr);
    return FileOps::joinPath(
        FileOps::joinPath(
            FileOps::joinPath(mfsRoot, "F16"),
            deCode()),
        FileOps::joinPath(year, sane));
}

// ── Helper: Unterhefter path inside a project Hängeregister ──
static std::string subHeft(const std::string& projHeft, const std::string& sub) {
    return FileOps::joinPath(projHeft, sub);
}

// ── Ensure standard subfolder structure for a project ────────
static void ensureProjectSubfolders(const std::string& heft) {
    for (auto& sub : {"F22","F18","DOK","RSK","MSN","QT","KPI","LE","ENT","AEA","ABE","MEI","BSP"})
        FileOps::makeDirs(FileOps::joinPath(heft, sub));
}

// ─────────────────────────────────────────────────────────────
// PROJECT (F16)
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeProject(const ProjectF16& p, const std::string& mfsRoot) {
    std::string sane = sanitiseRegNr(p.regNumber.toString());

    // 1. Karteikarte in F16 index folder
    std::string de   = deCode();
    std::string year = std::to_string(p.regNumber.year);
    std::string idxDir  = FileOps::joinPath(FileOps::joinPath(FileOps::joinPath(mfsRoot, "F16"), de), year);
    std::string idxFile = FileOps::joinPath(idxDir, sane + ".txt");

    std::ostringstream idx;
    idx << "REGISTRIERNUMMER : " << p.regNumber.toString() << "\n"
        << "VORGANGSART      : " << p.projectType            << "\n"
        << "GROESSENKLASSE   : " << p.sizeClass              << "\n"
        << "STATUS           : " << p.status                 << "\n"
        << "PHASE            : " << p.phase                  << "\n"
        << "ANGELEGT         : " << p.createdAt              << "\n"
        << "GEAENDERT        : " << p.updatedAt              << "\n"
        << "---\n"
        << "HAENGEREGISTER   : F16/" << de << "/" << year << "/" << sane << "/\n"
        << "F22-VORGAENGE    : " << sane << "/F22/\n"
        << "F18-VORFAELLE    : " << sane << "/F18/\n"
        << "DOKUMENTE        : " << sane << "/DOK/\n";
    ownerOnlyWrite(idxFile, idx.str());

    // 2. Hängeregister with all subfolders
    std::string heft = projectHeft(mfsRoot, p.regNumber.toString());
    ensureProjectSubfolders(heft);

    // 3. Heft cover sheet
    std::string coverFile = FileOps::joinPath(heft, "00_DECKBLATT.txt");
    std::ostringstream cover;
    cover << "DECKBLATT\n"
          << "=========================================\n"
          << "VORGANGSNUMMER : " << p.regNumber.toString() << "\n"
          << "VORGANGSART    : " << p.projectType           << "\n"
          << "REGISTRIERT AM : " << p.createdAt             << "\n"
          << "AKTENZEICHEN   : " << Config::instance().registratur().geschaeftszeichen
          << "-" << sane << "\n"
          << "=========================================\n"
          << "INHALT:\n"
          << "  F22/  Aufgabenkartei (Vorgangskartei)\n"
          << "  F18/  Vorfall-/Zwischenfallkartei\n"
          << "  DOK/  Dokumente und Schriftgut\n"
          << "  MASS/ Massnahmen\n"
          << "  QG/   Qualitaetstore\n"
          << "  KPI/  Kennzahlen\n"
          << "  LL/   Lernerkenntnisse\n"
          << "  DL/   Entscheidungslog\n"
          << "  CR/   Aenderungsantraege\n"
          << "  AC/   Annahmen und Beschraenkungen\n"
          << "  MS/   Meilensteine\n"
          << "  MTG/  Besprechungen\n";
    ownerOnlyWrite(coverFile, cover.str());

    LOG_DEBUG("MFS F16 written: " + idxFile);

    // Owner key entry
    std::map<std::string,std::string> conn;
    conn["LEITER"]    = p.leadId;
    conn["REFERAT"]   = p.ownerTeamId;
    conn["AUFTRAGG"]  = p.sponsorId;
    appendOwnerKey(p.regNumber.toString(), p.title, conn, mfsRoot);
    return true;
}

// ─────────────────────────────────────────────────────────────
// TASK (F22) — filed under parent project Hängeregister
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeTask(const TaskF22& t, const std::string& mfsRoot) {
    std::string taskSane = sanitiseRegNr(t.regNumber.toString());

    // Also write standalone index card in F22/ root
    std::string de   = deCode();
    std::string year = std::to_string(t.regNumber.year);
    std::string idxDir  = FileOps::joinPath(FileOps::joinPath(FileOps::joinPath(mfsRoot, "F22"), de), year);
    std::string idxFile = FileOps::joinPath(idxDir, taskSane + ".txt");

    std::ostringstream idx;
    idx << "REGISTRIERNUMMER : " << t.regNumber.toString()  << "\n"
        << "VORGANG-REF      : " << t.projectId             << "\n"
        << "STATUS           : " << t.status                << "\n"
        << "FORTSCHRITT      : " << t.percentComplete << "%" << "\n"
        << "WBS              : " << t.wbsCode               << "\n"
        << "ANGELEGT         : " << t.createdAt             << "\n";
    if (!t.parentTaskId.empty())
        idx << "UEBERGEORDNET    : " << t.parentTaskId << "\n";
    ownerOnlyWrite(idxFile, idx.str());

    // If we know the project's reg number, also file under project Hängeregister
    // Load project to get its regNumber for the folder path
    auto proj = ProjectF16::loadById(t.projectId);
    if (proj) {
        std::string heft   = projectHeft(mfsRoot, proj->regNumber.toString());
        std::string f22dir = subHeft(heft, "F22");
        std::string filed  = FileOps::joinPath(f22dir, taskSane + ".txt");
        ownerOnlyWrite(filed, idx.str());
        LOG_DEBUG("MFS F22 filed under project: " + filed);
    }

    std::map<std::string,std::string> conn;
    conn["VORGANG"]   = t.projectId;
    conn["BEARBEITER"]= t.assigneeId;
    appendOwnerKey(t.regNumber.toString(), t.title, conn, mfsRoot);
    return true;
}

// ─────────────────────────────────────────────────────────────
// INCIDENT (F18) — filed under parent project Hängeregister
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeIncident(const IncidentF18& i, const std::string& mfsRoot) {
    if (i.regNumber.toString().empty() || i.regNumber.sequence == 0) {
        LOG_WARN("writeIncident: incident has no valid regNumber, skipping MFS filing");
        return false;
    }
    std::string sane = sanitiseRegNr(i.regNumber.toString());
    std::string de   = deCode();
    std::string year = std::to_string(i.regNumber.year);

    // Standalone index
    std::string idxFile = FileOps::joinPath(FileOps::joinPath(FileOps::joinPath(FileOps::joinPath(mfsRoot, "F18"), de), year), sane + ".txt");
    std::ostringstream idx;
    idx << "REGISTRIERNUMMER : " << i.regNumber.toString() << "\n"
        << "VORGANG-REF      : " << i.projectId            << "\n"
        << "SCHWERE          : " << i.severity             << "\n"
        << "STATUS           : " << i.status               << "\n"
        << "GEMELDET         : " << i.reportedDate         << "\n"
        << "GELOEST          : " << i.resolvedDate         << "\n";
    if (!i.riskId.empty()) idx << "RISIKO-REF       : " << i.riskId << "\n";
    ownerOnlyWrite(idxFile, idx.str());

    // Filed under project
    auto proj = ProjectF16::loadById(i.projectId);
    if (proj) {
        std::string filed = FileOps::joinPath(
            projectHeft(mfsRoot, proj->regNumber.toString()), "F18",
            sane + ".txt");
        ownerOnlyWrite(filed, idx.str());
    }

    std::map<std::string,std::string> conn;
    conn["VORGANG"]  = i.projectId;
    conn["EIGENTUM"] = i.ownerId;
    appendOwnerKey(i.regNumber.toString(), i.title, conn, mfsRoot);
    return true;
}

// ─────────────────────────────────────────────────────────────
// DOCUMENT — ALWAYS filed under parent entity's folder
// Never loose — must have a project or task reference
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeDocument(const Document& d, const std::string& mfsRoot) {
    if (d.projectId.empty() && d.taskId.empty()) {
        LOG_WARN("MFSWriter::writeDocument — document '" + d.title +
                 "' has no project/task reference. Skipping MFS filing.");
        return false;
    }

    // Filename: <ID>_<title>.txt  — ID always leads for uniqueness
    std::string sane     = sanitiseRegNr(d.documentId);
    std::string safeName = FileOps::sanitizeFilename(d.title);
    if (safeName.size() > 40) safeName = safeName.substr(0, 40);
    std::string fname    = sane + "_" + safeName + ".txt";
    // Lambda: remove any prior version of this document (same ID prefix)
    auto removeExisting = [&](const std::string& dir) {
        for (auto& e : FileOps::listDir(dir))
            if (e.size() >= sane.size() && e.substr(0, sane.size()) == sane && e != fname)
                FileOps::deleteFile(FileOps::joinPath(dir, e));
    };

    std::ostringstream content;
    content << "DOKUMENT-ID      : " << d.documentId          << "\n"
            << "TITEL            : " << d.title               << "\n"
            << "TYP              : " << d.docType             << "\n"
            << "FORMAT           : " << d.format              << "\n"
            << "VERSION          : " << d.version             << "\n"
            << "STATUS           : " << d.status              << "\n"
            << "ERSTELLT         : " << d.dateCreated         << "\n"
            << "GENEHMIGT        : " << d.dateApproved        << "\n"
            << "ABLAGE-DATEI     : " << fname                 << "\n";

    bool ok = false;

    // File under project Hängeregister/DOK/
    if (!d.projectId.empty()) {
        auto proj = ProjectF16::loadById(d.projectId);
        if (proj) {
            std::string dokDir = FileOps::joinPath(
                projectHeft(mfsRoot, proj->regNumber.toString()), "DOK");
            FileOps::makeDirs(dokDir);
            removeExisting(dokDir);
            std::string path = FileOps::joinPath(dokDir, fname);
            content << "F16/" << proj->regNumber.toString() << "/DOK/" << fname << "\n";
            if (!d.fileUrl.empty())  content << "QUELLE-URL       : " << d.fileUrl   << "\n";
            if (!d.filePath.empty()) content << "LOKALER-PFAD     : " << d.filePath  << "\n";
            ok = ownerOnlyWrite(path, content.str());
            LOG_DEBUG("MFS DOK filed under project: " + path);
        }
    }

    // Also file under task Hängeregister if taskId set
    if (!d.taskId.empty()) {
        auto task = TaskF22::loadById(d.taskId);
        if (task) {
            auto proj = ProjectF16::loadById(task->projectId);
            if (proj) {
                std::string taskDocDir = FileOps::joinPath(
                    FileOps::joinPath(
                        FileOps::joinPath(projectHeft(mfsRoot, proj->regNumber.toString()), "F22"),
                        sanitiseRegNr(task->regNumber.toString())),
                    "DOK");
                FileOps::makeDirs(taskDocDir);
                ownerOnlyWrite(FileOps::joinPath(taskDocDir, fname), content.str());
            }
        }
    }

    std::map<std::string,std::string> conn;
    conn["VORGANG"]  = d.projectId;
    conn["AUFGABE"]  = d.taskId;
    conn["VERFASSER"]= d.authorId;
    appendOwnerKey(d.documentId, d.title, conn, mfsRoot);
    return ok;
}

// ─────────────────────────────────────────────────────────────
// RISK — filed under project RISIKEN subfolder (if we had it)
// but stored in reporting.db; write to mfs/RISIKEN standalone
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeRisk(const Risk& r, const std::string& mfsRoot) {
    std::string sane = sanitiseRegNr(r.riskId);
    std::string riskDir = FileOps::joinPath(mfsRoot, "RISIKEN");
    FileOps::makeDirs(riskDir);
    std::string path = FileOps::joinPath(riskDir, "RISK_" + sane + ".txt");

    std::ostringstream oss;
    oss << "RISIKO-ID        : " << r.riskId           << "\n"
        << "VORGANG-REF      : " << r.projectId        << "\n"
        << "RISIKONIVEAU     : " << r.riskLevel        << "\n"
        << "GESAMTSCORE      : " << r.overallRiskScore << "\n"
        << "STRATEGIE        : " << r.responseStrategy << "\n"
        << "STATUS           : " << r.status           << "\n";

    bool ok = ownerOnlyWrite(path, oss.str());

    // Also file under project
    if (!r.projectId.empty()) {
        auto proj = ProjectF16::loadById(r.projectId);
        if (proj) {
            // Create RISIKEN subfolder inside Hängeregister
            std::string rDir = FileOps::joinPath(
                projectHeft(mfsRoot, proj->regNumber.toString()), "RISIKEN");
            FileOps::makeDirs(rDir);
            ownerOnlyWrite(FileOps::joinPath(rDir, "RISK_" + sane + ".txt"), oss.str());
        }
    }

    std::map<std::string,std::string> conn;
    conn["VORGANG"]  = r.projectId;
    conn["EIGENTUM"] = r.ownerId;
    appendOwnerKey(r.riskId, r.title, conn, mfsRoot);
    return ok;
}

// ─────────────────────────────────────────────────────────────
// PERSON
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writePerson(const Person& p, const std::string& mfsRoot) {
    std::string sane = sanitiseRegNr(p.regNumber.toString());
    std::string de   = deCode();
    std::string year = std::to_string(p.regNumber.year);
    std::string dir  = FileOps::joinPath(FileOps::joinPath(FileOps::joinPath(mfsRoot, "PERSONEN"), de), year);
    std::string path = FileOps::joinPath(dir, "PER_" + sane + ".txt");

    std::ostringstream oss;
    oss << "REGISTRIERNUMMER : " << p.regNumber.toString() << "\n"
        << "TYP              : " << p.personType           << "\n"
        << "STATUS           : " << p.status               << "\n"
        << "EINHEIT          : " << p.orgUnit              << "\n";
    // No real name in the file — only in owner_key.txt
    return ownerOnlyWrite(path, oss.str());
}

// ─────────────────────────────────────────────────────────────
// TEAM
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeTeam(const Team& t, const std::string& mfsRoot) {
    std::string sane = sanitiseRegNr(t.teamId);
    std::string dir  = FileOps::joinPath(mfsRoot, "DIENSTEINHEITEN");
    FileOps::makeDirs(dir);
    std::string path = FileOps::joinPath(dir, "DE_" + sane + ".txt");

    std::ostringstream oss;
    oss << "ID               : " << t.teamId   << "\n"
        << "TYP              : " << t.type     << "\n"
        << "STATUS           : " << t.status   << "\n";
    if (!t.parentTeamId.empty())
        oss << "UEBERGEORDNET    : " << t.parentTeamId << "\n";
    return ownerOnlyWrite(path, oss.str());
}

// ─────────────────────────────────────────────────────────────
// OWNER KEY — maps all reg numbers to real names (600 perms)
// ─────────────────────────────────────────────────────────────
bool MFSWriter::appendOwnerKey(
    const std::string& regNr,
    const std::string& realTitle,
    const std::map<std::string,std::string>& connections,
    const std::string& mfsRoot)
{
    std::string path = FileOps::joinPath(mfsRoot, "owner_key.txt");
    std::ostringstream oss;
    oss << "[" << regNr << "]\n"
        << "KLARNAME : " << realTitle << "\n";
    for (auto& [k,v] : connections)
        if (!v.empty()) oss << k << " : " << v << "\n";
    oss << "\n";

    bool ok = FileOps::writeTextFile(path, oss.str(), true);
#ifndef _WIN32
    if (ok) chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
    return ok;
}

// ─────────────────────────────────────────────────────────────
// REBUILD ALL
// ─────────────────────────────────────────────────────────────
bool MFSWriter::rebuildAll(const std::string& mfsRoot) {
    LOG_INFO("Rebuilding MFS tree at: " + mfsRoot);
    FileOps::ensureMFSTree(mfsRoot);

    // Remove and recreate owner key
    FileOps::deleteFile(FileOps::joinPath(mfsRoot, "owner_key.txt"));

    bool ok = true;
    auto projects = ProjectF16::loadAll();
    LOG_INFO("Writing " + std::to_string(projects.size()) + " F16 entries");
    for (auto& p : projects) {
        ok &= writeProject(*p, mfsRoot);
        // Write all tasks under each project
        auto tasks = TaskF22::loadForProject(p->projectId);
        for (auto& t : tasks) ok &= writeTask(*t, mfsRoot);
        // Incidents — skip any with uninitialized regNumbers gracefully
        auto incidents = IncidentF18::loadForProject(p->projectId);
        for (auto& i : incidents) {
            if (i->regNumber.sequence == 0) continue;  // not yet registered
            ok &= writeIncident(*i, mfsRoot);
        }
    }

    LOG_INFO("MFS rebuild complete. OK=" + std::string(ok?"yes":"NO"));
    return ok;
}

} // namespace Rosenholz

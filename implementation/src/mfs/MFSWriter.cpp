// ============================================================
// MFSWriter.cpp  —  DDR MfS-style physical filing system
//
// Design principle (true Rosenholz):
//   Every file references every other file.
//   No folder makes sense in isolation.
//   A single extracted folder is meaningless without the others.
//
// Physical structure:
//
//   mfs/
//   ├── F16/<DE>/<YEAR>/<REG-NR>/          ← Hängeregister (one per project)
//   │   ├── 00_DECKBLATT.txt               ← cover: refs all F22, DOK, F18, persons
//   │   ├── F22/<F22-REG>.txt              ← task card (refs F16, persons, DOK)
//   │   ├── F18/<F18-ID>.txt               ← vorgang card (refs F16, F22, DOK)
//   │   └── DOK/<DOK-ID>_<title>.txt       ← doc card (refs F16, F22, F18, person)
//   └── owner_key.txt                      ← Klarnamendatei (600 owner-only)
//                                             maps every reg-nr → real name + connections
//
// Rules:
//   - No standalone PERSONEN/ folder  — person real name only in owner_key.txt
//   - No standalone DIENSTEINHEITEN/  — team referenced by ID string in F16 cover
//   - No standalone RISIKEN/          — risks are F18 vorgänge filed under F16
//   - No standalone F22/ or F18/ root — every card lives ONLY under its F16
//   - Documents filed ONLY under F16 (and optionally also under F16/F22/)
// ============================================================

#include "MFSWriter.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include "../core/Config.h"
#include "../model/ProjectF16.h"
#include "../model/TaskF22.h"
#include "../model/Document.h"
#include "../model/f18/F18Workflow.h"
#include "../model/Person.h"
#include "../model/Team.h"
#include "../model/Utils.h"

#ifndef _WIN32
  #include <sys/stat.h>
#endif
#include <sstream>
#include <map>

namespace Rosenholz {

// ── Helper: owner-only file write (600) ──────────────────────
bool MFSWriter::ownerOnlyWrite(const std::string& path, const std::string& content) {
    FileOps::makeDirs(FileOps::dirName(path));
    bool ok = FileOps::writeTextFile(path, content, false);
#ifndef _WIN32
    if (ok) chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
    return ok;
}

// ── Config helpers ────────────────────────────────────────────
static std::string deCode() {
    const std::string& de = Config::instance().registratur().diensteinheitKuerzel;
    return de.empty() ? "XX" : de;
}

// ── Hängeregister root for a project ─────────────────────────
// Returns: mfs/F16/<DE>/<YEAR>/<sanitised_regNr>/
static std::string projectHeft(const std::string& mfsRoot, const std::string& regNr) {
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

// ── Ensure only the folders we actually use ───────────────────
static void ensureProjectSubfolders(const std::string& heft) {
    // F22 tasks, F18 vorgänge, DOK documents — nothing else
    for (auto& sub : {"F22", "F18", "DOK"})
        FileOps::makeDirs(FileOps::joinPath(heft, sub));
}

// ─────────────────────────────────────────────────────────────
// PROJECT (F16) — Hängeregister + cover sheet
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeProject(const ProjectF16& p, const std::string& mfsRoot) {
    std::string sane = sanitiseRegNr(p.regNumber.toString());
    std::string de   = deCode();
    std::string year = std::to_string(p.regNumber.year);
    std::string heft = projectHeft(mfsRoot, p.regNumber.toString());
    ensureProjectSubfolders(heft);

    // ── Cover sheet — references everything ──────────────────
    std::string coverFile = FileOps::joinPath(heft, "00_DECKBLATT.txt");
    std::ostringstream cover;
    cover << "VORGANGSAKTE\n"
          << "=======================================================\n"
          << "REGISTRIERNUMMER : " << p.regNumber.toString()   << "\n"
          << "VORGANGSART      : " << p.projectType             << "\n"
          << "GROESSENKLASSE   : " << p.sizeClass               << "\n"
          << "STATUS           : " << p.status                  << "\n"
          << "PHASE            : " << p.phase                   << "\n"
          << "MAIN-WORKFLOW    : " << p.mainWorkflowId          << "\n"
          << "AKTENZEICHEN     : "
          << Config::instance().registratur().geschaeftszeichen
          << "-" << sane << "\n"
          << "ANGELEGT         : " << p.createdAt               << "\n"
          << "GEAENDERT        : " << p.updatedAt               << "\n"
          << "=======================================================\n"
          << "\n";

    // Cross-references: persons linked to this project
    if (!p.leadId.empty())      cover << "LEITER-REF       : " << p.leadId      << "\n";
    if (!p.ownerTeamId.empty()) cover << "EINHEIT-REF      : " << p.ownerTeamId << "\n";
    if (!p.sponsorId.empty())   cover << "AUFTRAGGEBER-REF : " << p.sponsorId   << "\n";

    cover << "\n"
          << "VERWEISE (alle Unterakten dieser Vorgangsakte):\n"
          << "  F22/   → Aufgabenkartei    [" << heft << "/F22/]\n"
          << "  F18/   → Vorgangskartei    [" << heft << "/F18/]\n"
          << "  DOK/   → Schriftgut        [" << heft << "/DOK/]\n"
          << "\n"
          << "KLARNAMENZUORDNUNG → owner_key.txt\n"
          << "  Alle Registriernummern sind dort aufgeloest.\n"
          << "  Diese Akte ist ohne owner_key.txt nicht vollstaendig.\n";

    ownerOnlyWrite(coverFile, cover.str());

    // ── Owner key entry ───────────────────────────────────────
    std::map<std::string,std::string> conn;
    conn["LEITER"]    = p.leadId;
    conn["EINHEIT"]   = p.ownerTeamId;
    conn["AUFTRAGG"]  = p.sponsorId;
    appendOwnerKey(p.regNumber.toString(), p.title, conn, mfsRoot);

    LOG_DEBUG("MFS F16 written: " + coverFile);
    return true;
}

// ─────────────────────────────────────────────────────────────
// TASK (F22) — ONLY filed under parent F16 Hängeregister/F22/
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeTask(const TaskF22& t, const std::string& mfsRoot) {
    auto proj = ProjectF16::loadById(t.projectId);
    if (!proj) {
        LOG_WARN("MFSWriter::writeTask — project not found for task " + t.taskId);
        return false;
    }

    std::string taskSane = sanitiseRegNr(t.regNumber.toString());
    std::string projSane = sanitiseRegNr(proj->regNumber.toString());
    std::string heft     = projectHeft(mfsRoot, proj->regNumber.toString());
    std::string f22dir   = FileOps::joinPath(heft, "F22");
    FileOps::makeDirs(f22dir);
    std::string path     = FileOps::joinPath(f22dir, taskSane + ".txt");

    std::ostringstream oss;
    oss << "AUFGABENKARTE (F22)\n"
        << "=======================================================\n"
        << "REGISTRIERNUMMER : " << t.regNumber.toString()   << "\n"
        << "STATUS           : " << t.status                  << "\n"
        << "FORTSCHRITT      : " << t.percentComplete << "%"  << "\n"
        << "MAIN-WORKFLOW    : " << t.mainWorkflowId          << "\n"
        << "ANGELEGT         : " << t.createdAt               << "\n"
        << "GEAENDERT        : " << t.updatedAt               << "\n"
        << "=======================================================\n"
        << "\n"
        << "VERWEISE:\n"
        << "  UEBERGEORDNETER VORGANG : " << proj->regNumber.toString()
        << "  [" << heft << "/00_DECKBLATT.txt]\n";
    if (!t.parentTaskId.empty())
        oss << "  UEBERGEORDNETE AUFGABE  : " << t.parentTaskId << "\n";
    if (!t.assigneeId.empty())
        oss << "  BEARBEITER-REF          : " << t.assigneeId << "\n";
    oss << "\n"
        << "  DOKUMENTE dieser Aufgabe: " << heft << "/DOK/ (mit Aufgaben-Prefix)\n"
        << "  F18-VORGAENGE           : " << heft << "/F18/ (mit Aufgaben-Ref)\n"
        << "\n"
        << "KLARNAMENZUORDNUNG → owner_key.txt\n";

    ownerOnlyWrite(path, oss.str());

    std::map<std::string,std::string> conn;
    conn["F16"]        = proj->regNumber.toString();
    conn["BEARBEITER"] = t.assigneeId;
    appendOwnerKey(t.regNumber.toString(), t.title, conn, mfsRoot);

    LOG_DEBUG("MFS F22 filed under F16: " + path);
    return true;
}

// ─────────────────────────────────────────────────────────────
// F18 VORGANG — filed under parent F16 Hängeregister/F18/
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeF18(const F18Workflow& v, const std::string& mfsRoot) {
    if (v.projectId.empty()) {
        LOG_WARN("MFSWriter::writeF18 — no project reference for " + v.vorgangId);
        return false;
    }
    auto proj = ProjectF16::loadById(v.projectId);
    if (!proj) return false;

    std::string heft   = projectHeft(mfsRoot, proj->regNumber.toString());
    std::string f18dir = FileOps::joinPath(heft, "F18");
    FileOps::makeDirs(f18dir);
    std::string sane   = sanitiseRegNr(v.vorgangId);
    std::string path   = FileOps::joinPath(f18dir, sane + ".txt");

    std::ostringstream oss;
    oss << "VORGANGSKARTE F18 (" << v.vorgangType << ")\n"
        << "=======================================================\n"
        << "VORGANG-ID       : " << v.vorgangId                   << "\n"
        << "VORGANGSART      : " << v.vorgangType                  << "\n"
        << "STATUS           : " << v.status                       << "\n"
        << "PRIORITAET       : " << v.priority                     << "\n"
        << "MAIN-WORKFLOW    : " << v.mainWorkflowId               << "\n"
        << "ANGELEGT         : " << v.createdAt                    << "\n"
        << "=======================================================\n"
        << "\n"
        << "VERWEISE:\n"
        << "  UEBERGEORDNETER VORGANG : " << proj->regNumber.toString()
        << "  [" << heft << "/00_DECKBLATT.txt]\n";
    if (!v.taskId.empty())
        oss << "  ZUGEHOERIGE AUFGABE     : " << v.taskId << "\n";
    if (!v.parentVorgangId.empty())
        oss << "  UEBERGEORDNETER F18     : " << v.parentVorgangId << "\n";
    oss << "\n"
        << "KLARNAMENZUORDNUNG → owner_key.txt\n";

    ownerOnlyWrite(path, oss.str());

    std::map<std::string,std::string> conn;
    conn["F16"]     = proj->regNumber.toString();
    conn["F22"]     = v.taskId;
    conn["OWNER"]   = v.ownerId;
    appendOwnerKey(v.vorgangId, v.title, conn, mfsRoot);

    LOG_DEBUG("MFS F18 filed under F16: " + path);
    return true;
}

// ─────────────────────────────────────────────────────────────
// DOCUMENT — filed under F16 Hängeregister/DOK/
// Must have a project reference — orphan documents refused.
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeDocument(const Document& d, const std::string& mfsRoot) {
    if (d.projectId.empty() && d.taskId.empty()) {
        LOG_WARN("MFSWriter::writeDocument — '" + d.title +
                 "' has no project/task reference. Skipping.");
        return false;
    }

    std::string sane     = sanitiseRegNr(d.documentId);
    std::string safeName = FileOps::sanitizeFilename(d.title);
    if (safeName.size() > 40) safeName = safeName.substr(0, 40);
    std::string fname    = sane + "_" + safeName + ".txt";

    std::ostringstream content;
    content << "DOKUMENT-KARTE\n"
            << "=======================================================\n"
            << "DOKUMENT-ID      : " << d.documentId          << "\n"
            << "TITEL            : " << d.title               << "\n"
            << "TYP              : " << d.docType             << "\n"
            << "FORMAT           : " << d.format              << "\n"
            << "VERSION          : " << d.version             << "\n"
            << "STATUS           : " << d.status              << "\n"
            << "ERSTELLT         : " << d.dateCreated         << "\n"
            << "GENEHMIGT        : " << d.dateApproved        << "\n"
            << "=======================================================\n"
            << "\n"
            << "VERWEISE:\n";

    bool ok = false;

    if (!d.projectId.empty()) {
        auto proj = ProjectF16::loadById(d.projectId);
        if (proj) {
            std::string heft   = projectHeft(mfsRoot, proj->regNumber.toString());
            std::string dokDir = FileOps::joinPath(heft, "DOK");
            FileOps::makeDirs(dokDir);

            content << "  VORGANG : " << proj->regNumber.toString()
                    << "  [" << heft << "/00_DECKBLATT.txt]\n";
            if (!d.taskId.empty())
                content << "  AUFGABE : " << d.taskId << "\n";
            if (!d.authorId.empty())
                content << "  VERFASSER-REF : " << d.authorId << "\n";
            if (!d.fileUrl.empty())
                content << "  QUELLE-URL    : " << d.fileUrl  << "\n";
            if (!d.filePath.empty())
                content << "  DATEI-PFAD    : " << d.filePath << "\n";
            content << "\n"
                    << "KLARNAMENZUORDNUNG → owner_key.txt\n";

            // Remove any prior version of this doc (same ID prefix)
            for (auto& e : FileOps::listDir(dokDir))
                if (e.size() >= sane.size() && e.substr(0, sane.size()) == sane && e != fname)
                    FileOps::deleteFile(FileOps::joinPath(dokDir, e));

            ok = ownerOnlyWrite(FileOps::joinPath(dokDir, fname), content.str());

            // Copy physical file alongside the index card
            if (!d.filePath.empty() && FileOps::fileExists(d.filePath)) {
                std::string physDest = FileOps::joinPath(
                    dokDir, FileOps::baseName(d.filePath));
                if (physDest != d.filePath)
                    FileOps::copyFile(d.filePath, physDest, true);
            }
            LOG_DEBUG("MFS DOK filed under F16: " + FileOps::joinPath(dokDir, fname));
        }
    }

    std::map<std::string,std::string> conn;
    conn["F16"]       = d.projectId;
    conn["F22"]       = d.taskId;
    conn["VERFASSER"] = d.authorId;
    appendOwnerKey(d.documentId, d.title, conn, mfsRoot);
    return ok;
}

// ─────────────────────────────────────────────────────────────
// PERSON — only in owner_key.txt, no standalone folder
// The person card is a reference record, not a named file
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writePerson(const Person& p, const std::string& mfsRoot) {
    // Person real name goes ONLY into owner_key.txt (600 perms).
    // No standalone PERSONEN/ folder — a person file alone is meaningless.
    // Persons are referenced BY ID from the F16/F22/DOK files that use them.
    std::map<std::string,std::string> conn;
    conn["EINHEIT"]  = p.orgUnit;
    conn["TYP"]      = p.personType;
    conn["STATUS"]   = p.status;
    // Real name stored in owner_key only:
    std::string realName = p.lastName + ", " + p.firstName;
    appendOwnerKey(p.regNumber.toString(), realName, conn, mfsRoot);
    LOG_DEBUG("MFS Person written to owner_key: " + p.regNumber.toString());
    return true;
}

// ─────────────────────────────────────────────────────────────
// TEAM — no standalone folder; referenced by ID from F16
// Teams are organisational units, not separate filing entities.
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeTeam(const Team& t, const std::string& mfsRoot) {
    // Teams are referenced by teamId from F16 cover sheets.
    // No DIENSTEINHEITEN/ folder — a team folder alone is meaningless.
    // Team name goes into owner_key.txt for lookup.
    std::map<std::string,std::string> conn;
    conn["TYP"]          = t.type;
    conn["UEBERGEORDNET"] = t.parentTeamId;
    appendOwnerKey(t.teamId, t.name, conn, mfsRoot);
    return true;
}

// ─────────────────────────────────────────────────────────────
// OWNER KEY — the only place real names appear (600 perms)
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
    // Only F16/ folder and owner_key.txt — everything else lives under F16
    FileOps::makeDirs(FileOps::joinPath(mfsRoot, "F16"));

    // Reset owner key
    FileOps::deleteFile(FileOps::joinPath(mfsRoot, "owner_key.txt"));

    bool ok = true;
    auto projects = ProjectF16::loadAll();
    LOG_INFO("Writing " + std::to_string(projects.size()) + " F16 entries");
    for (auto& p : projects) {
        ok &= writeProject(*p, mfsRoot);
        auto tasks = TaskF22::loadForProject(p->projectId);
        for (auto& t : tasks) ok &= writeTask(*t, mfsRoot);
        // F18 Vorgänge
        auto f18s = F18Workflow::loadForProject(p->projectId);
        for (auto& v : f18s) ok &= writeF18(*v, mfsRoot);
    }
    LOG_INFO("MFS rebuild complete. OK=" + std::string(ok?"yes":"NO"));
    return ok;
}

} // namespace Rosenholz

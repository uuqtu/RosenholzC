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
#include "../model/f16/ProjectF16.h"
#include "../model/f22/TaskF22.h"
#include "../model/dok/Document.h"
#include "../model/f18/F18Operation.h"
#include "../model/Person.h"
#include "../model/Team.h"
#include "../workflow/F77Workflow.h"
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


// ── Entity folder helpers ─────────────────────────────────────
static std::string f16Dir(const std::string& mfsRoot, const std::string& regNr) {
    return FileOps::joinPath(FileOps::joinPath(mfsRoot,"F16"), sanitiseRegNr(regNr));
}
static std::string f22Dir(const std::string& mfsRoot, const std::string& regNr) {
    return FileOps::joinPath(FileOps::joinPath(mfsRoot,"F22"), sanitiseRegNr(regNr));
}
static std::string f18Dir(const std::string& mfsRoot, const std::string& vorgangId) {
    return FileOps::joinPath(FileOps::joinPath(mfsRoot,"F18"), sanitiseRegNr(vorgangId));
}
static std::string f77Dir(const std::string& mfsRoot, const std::string& wfiId) {
    return FileOps::joinPath(FileOps::joinPath(mfsRoot,"F77"), sanitiseRegNr(wfiId));
}
static std::string docEntityDir(const std::string& entityDir, const std::string& docId) {
    return FileOps::joinPath(FileOps::joinPath(entityDir,"DOK"), sanitiseRegNr(docId));
}



// ─────────────────────────────────────────────────────────────
// PROJECT (F16) — Hängeregister + cover sheet
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeProject(const ProjectF16& p, const std::string& mfsRoot) {
    std::string sane = sanitiseRegNr(p.regNumber.toString());
    std::string dir  = f16Dir(mfsRoot, p.regNumber.toString());
    FileOps::makeDirs(dir);

    std::string coverFile = FileOps::joinPath(dir, "00_DECKBLATT.txt");
    std::ostringstream cover;
    cover << "VORGANGSAKTE\n"
          << "=======================================================\n"
          << "REGISTRIERNUMMER : " << p.regNumber.toString()   << "\n"
          << "VORGANGSART      : " << p.projectType             << "\n"
          << "GROESSENKLASSE   : " << p.sizeClass               << "\n"
          << "STATUS           : " << p.status                  << "\n"
          << "PHASE            : " << p.phase                   << "\n"
          << "MAIN-WORKFLOW    : " << p.releaseWorkflowId          << "\n"
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
          << "  F22/   → Aufgabenkartei    [mfs/F22/<reg>/]\n"
          << "  F18/   → Vorgangskartei    [mfs/F18/<id>/]\n"
          << "  DOK/   → Schriftgut        [mfs/F16/<reg>/DOK/<id>/]\n"
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
// ─────────────────────────────────────────────────────────────
// TASK (F22) — mfs/F22/<reg>/ own subfolder
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeTask(const TaskF22& t, const std::string& mfsRoot) {
    std::string dir = f22Dir(mfsRoot, t.regNumber.toString());
    FileOps::makeDirs(dir);

    auto proj = ProjectF16::loadById(t.projectId);
    std::string projRef = proj ? proj->regNumber.toString() : t.projectId;

    std::string cardPath = FileOps::joinPath(dir, "00_KARTE.txt");
    std::ostringstream oss;
    oss << "AUFGABENKARTE (F22)\n"
        << "=======================================================\n"
        << "REGISTRIERNUMMER : " << t.regNumber.toString()   << "\n"
        << "STATUS           : " << t.status                  << "\n"
        << "FORTSCHRITT      : " << t.percentComplete << "%"  << "\n"
        << "F77-FREIGABE-WFI : " << t.releaseWorkflowId       << "\n"
        << "ANGELEGT         : " << t.createdAt               << "\n"
        << "GEAENDERT        : " << t.updatedAt               << "\n"
        << "=======================================================\n\n"
        << "VERWEISE:\n"
        << "  F16 (VORGANG)   : " << projRef
        << "  [mfs/F16/" << sanitiseRegNr(projRef) << "/00_DECKBLATT.txt]\n";
    if (!t.parentTaskId.empty())
        oss << "  ELTERN-AUFGABE  : " << t.parentTaskId
            << "  [mfs/F22/" << sanitiseRegNr(t.parentTaskId) << "/]\n";
    if (!t.assigneeId.empty())
        oss << "  BEARBEITER-REF  : " << t.assigneeId
            << "  → owner_key.txt\n";
    oss << "\nKLARNAMENZUORDNUNG → owner_key.txt\n";

    ownerOnlyWrite(cardPath, oss.str());

    std::map<std::string,std::string> conn;
    conn["F16"]        = projRef;
    conn["BEARBEITER"] = t.assigneeId;
    appendOwnerKey(t.regNumber.toString(), t.title, conn, mfsRoot);

    LOG_DEBUG("MFS F22 written: " + cardPath);
    return true;
}

// ─────────────────────────────────────────────────────────────
// F18 OPERATION — mfs/F18/<vorgangId>/ own subfolder
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeF18(const F18Operation& v, const std::string& mfsRoot) {
    if (v.projectId.empty()) {
        LOG_WARN("MFSWriter::writeF18 — no project reference for " + v.vorgangId);
        return false;
    }
    auto proj = ProjectF16::loadById(v.projectId);
    if (!proj) return false;

    std::string dir = f18Dir(mfsRoot, v.vorgangId);
    FileOps::makeDirs(dir);

    std::string cardPath = FileOps::joinPath(dir, "00_KARTE.txt");
    std::ostringstream oss;
    oss << "VORGANGSKARTE F18 (" << v.vorgangType << ")\n"
        << "=======================================================\n"
        << "VORGANG-ID       : " << v.vorgangId              << "\n"
        << "VORGANGSART      : " << v.vorgangType             << "\n"
        << "STATUS           : " << v.status                  << "\n"
        << "PRIORITAET       : " << v.priority                << "\n"
        << "F77-FREIGABE-WFI : " << v.releaseWorkflowId       << "\n"
        << "ANGELEGT         : " << v.createdAt               << "\n"
        << "=======================================================\n\n"
        << "VERWEISE:\n"
        << "  F16 (VORGANG)   : " << proj->regNumber.toString()
        << "  [mfs/F16/" << sanitiseRegNr(proj->regNumber.toString()) << "/00_DECKBLATT.txt]\n";
    if (!v.taskId.empty())
        oss << "  F22 (AUFGABE)   : " << v.taskId
            << "  [mfs/F22/" << sanitiseRegNr(v.taskId) << "/00_KARTE.txt]\n";
    if (!v.parentVorgangId.empty())
        oss << "  ELTERN-F18      : " << v.parentVorgangId
            << "  [mfs/F18/" << sanitiseRegNr(v.parentVorgangId) << "/]\n";
    oss << "\nKLARNAMENZUORDNUNG → owner_key.txt\n";

    ownerOnlyWrite(cardPath, oss.str());

    std::map<std::string,std::string> conn;
    conn["F16"]   = proj->regNumber.toString();
    conn["F22"]   = v.taskId;
    conn["OWNER"] = v.ownerId;
    appendOwnerKey(v.vorgangId, v.title, conn, mfsRoot);

    LOG_DEBUG("MFS F18 written: " + cardPath);
    return true;
}

// ─────────────────────────────────────────────────────────────
// DOCUMENT — filed inside parent entity subfolder/DOK/<docId>/
// Parent can be F16, F22, F18, or F77 — each doc in own subfolder
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeDocument(const Document& d, const std::string& mfsRoot) {
    // Determine parent entity directory
    std::string parentDir;
    std::string parentRef;

    if (!d.projectId.empty()) {
        auto proj = ProjectF16::loadById(d.projectId);
        if (proj) {
            parentDir = f16Dir(mfsRoot, proj->regNumber.toString());
            parentRef = "F16/" + proj->regNumber.toString();
        }
    } else if (!d.taskId.empty()) {
        auto task = TaskF22::loadById(d.taskId);
        if (task) {
            parentDir = f22Dir(mfsRoot, task->regNumber.toString());
            parentRef = "F22/" + task->regNumber.toString();
        }
    } else if (!d.f18OperationId.empty()) {
        parentDir = f18Dir(mfsRoot, d.f18OperationId);
        parentRef = "F18/" + d.f18OperationId;
    }
    // f18StepId: file under its parent F18 Operation
    else if (!d.f18StepId.empty()) {
        parentDir = f18Dir(mfsRoot, d.f18StepId);
        parentRef = "F18-STEP/" + d.f18StepId;
    }

    if (parentDir.empty()) {
        LOG_WARN("MFSWriter::writeDocument — no valid parent for: " + d.title);
        return false;
    }

    // Each document gets its own subfolder: DOK/<docId>/
    std::string docDir = docEntityDir(parentDir, d.documentId);
    FileOps::makeDirs(docDir);

    std::string sane     = sanitiseRegNr(d.documentId);
    std::string safeName = FileOps::sanitizeFilename(d.title);
    if (safeName.size() > 40) safeName = safeName.substr(0, 40);
    std::string cardName = sane + ".txt";
    std::string cardPath = FileOps::joinPath(docDir, cardName);

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
            << "=======================================================\n\n"
            << "VERWEISE:\n"
            << "  UEBERGEORDNET   : " << parentRef
            << "  [mfs/" << parentRef << "/]\n";
    if (!d.authorId.empty())
        content << "  VERFASSER-REF   : " << d.authorId << "  → owner_key.txt\n";
    if (!d.fileUrl.empty())
        content << "  QUELLE-URL      : " << d.fileUrl  << "\n";
    if (!d.filePath.empty())
        content << "  DATEI-PFAD      : " << d.filePath << "\n";
    content << "\nKLARNAMENZUORDNUNG → owner_key.txt\n";

    bool ok = ownerOnlyWrite(cardPath, content.str());

    // Copy physical file into the document subfolder
    if (!d.filePath.empty() && FileOps::fileExists(d.filePath)) {
        std::string physDest = FileOps::joinPath(docDir, FileOps::baseName(d.filePath));
        if (physDest != d.filePath)
            FileOps::copyFile(d.filePath, physDest, true);
    }

    std::map<std::string,std::string> conn;
    conn["PARENT"]    = parentRef;
    conn["VERFASSER"] = d.authorId;
    appendOwnerKey(d.documentId, d.title, conn, mfsRoot);

    LOG_DEBUG("MFS DOK written: " + cardPath);
    return ok;
}

// ─────────────────────────────────────────────────────────────
// PERSON — only in owner_key.txt
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writePerson(const Person& p, const std::string& mfsRoot) {
    std::map<std::string,std::string> conn;
    conn["EINHEIT"] = p.orgUnit;
    conn["TYP"]     = p.personType;
    conn["STATUS"]  = p.status;
    std::string realName = p.lastName + ", " + p.firstName;
    appendOwnerKey(p.regNumber.toString(), realName, conn, mfsRoot);
    return true;
}

// ─────────────────────────────────────────────────────────────
// TEAM — only in owner_key.txt
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeTeam(const Team& t, const std::string& mfsRoot) {
    std::map<std::string,std::string> conn;
    conn["TYP"]           = t.type;
    conn["UEBERGEORDNET"] = t.parentTeamId;
    appendOwnerKey(t.teamId, t.name, conn, mfsRoot);
    return true;
}


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
// ─────────────────────────────────────────────────────────────
// F77 FREIGABE-WORKFLOW — mfs/F77/<wfiId>/
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeF77(const std::string& wfiId,
                          const std::string& entityType,
                          const std::string& entityTitle,
                          const std::string& mfsRoot) {
    std::string dir = f77Dir(mfsRoot, wfiId);
    FileOps::makeDirs(dir);
    std::string cardPath = FileOps::joinPath(dir, "00_KARTE.txt");
    std::ostringstream oss;
    oss << "F77 FREIGABE-WORKFLOW\n"
        << "=======================================================\n"
        << "WFI-ID           : " << wfiId       << "\n"
        << "ENTITAETSTYP     : " << entityType   << "\n"
        << "=======================================================\n\n"
        << "VERWEISE:\n"
        << "  GESTEUERTE ENTITAET: " << entityType << "/" << entityTitle
        << "  → owner_key.txt\n"
        << "\nKLARNAMENZUORDNUNG → owner_key.txt\n";
    std::map<std::string,std::string> conn;
    conn["ENTITAET"] = entityType + "/" + entityTitle;
    appendOwnerKey(wfiId, "F77-Freigabe: " + entityTitle, conn, mfsRoot);
    return ownerOnlyWrite(cardPath, oss.str());
}


bool MFSWriter::rebuildAll(const std::string& mfsRoot) {
    LOG_INFO("Rebuilding MFS tree at: " + mfsRoot);
    for (auto& sub : {"F16","F22","F18","F77","DOK"})
        FileOps::makeDirs(FileOps::joinPath(mfsRoot, sub));

    // Reset owner key
    FileOps::deleteFile(FileOps::joinPath(mfsRoot, "owner_key.txt"));

    bool ok = true;
    int nProj=0, nTask=0, nF18=0, nDok=0, nF77=0;

    auto projects = ProjectF16::loadAll();
    for (auto& p : projects) {
        ok &= writeProject(*p, mfsRoot); ++nProj;

        // Tasks (F22) — filed under F22/<reg>/
        auto tasks = TaskF22::loadForProject(p->projectId);
        for (auto& t : tasks) { ok &= writeTask(*t, mfsRoot); ++nTask; }

        // F18 operations — filed under F18/<id>/
        auto f18s = F18Operation::loadForProject(p->projectId);
        for (auto& v : f18s) { ok &= writeF18(*v, mfsRoot); ++nF18; }

        // Documents — filed under their parent entity folder
        auto docs = Document::loadForProject(p->projectId);
        for (auto& d : docs) { ok &= writeDocument(*d, mfsRoot); ++nDok; }
    }

    // F77 active workflows — filed under F77/<id>/
    auto wfs = F77_Workflow::loadActive();
    for (auto& wf : wfs) {
        ok &= writeF77(wf->workflowId, wf->entityType, wf->templateName, mfsRoot);
        ++nF77;
    }

    // Persons and teams — only owner_key.txt entries
    auto persons = Person::loadAll();
    for (auto& p : persons) ok &= writePerson(*p, mfsRoot);
    auto teams = Team::loadAll();
    for (auto& t : teams) ok &= writeTeam(*t, mfsRoot);

    LOG_INFO("MFS rebuild: F16=" + std::to_string(nProj)
             + " F22=" + std::to_string(nTask)
             + " F18=" + std::to_string(nF18)
             + " DOK=" + std::to_string(nDok)
             + " F77=" + std::to_string(nF77)
             + " OK=" + std::string(ok?"yes":"NO"));
    return ok;
}


} // namespace Rosenholz

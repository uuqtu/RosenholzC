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
#ifndef _WIN32
#  include <sys/stat.h>
#endif
#include "../model/akt/FolderRevision.h"
#include "../core/FileOps.h"

static std::string fval(const std::string& v) {
    return v.empty() ? "-" : v;
}

#include "../core/Logger.h"
#include "../core/Config.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/akt/Folder.h"
#include "../model/akt/FolderObject.h"
#include "../model/f18/F18Operation.h"
#include "../model/f18/F18OperationStep.h"
#include "../model/person/Person.h"
#include "../model/team/Team.h"
#include "../workflow/F77Workflow.h"
#include "../model/Utils.h"

#ifndef _WIN32
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
static std::string f22Dir(const std::string& mfsRoot, const std::string& regNr) {
    return FileOps::joinPath(FileOps::joinPath(mfsRoot,"F22"), sanitiseRegNr(regNr));
}
static std::string f18Dir(const std::string& mfsRoot, const std::string& operationId) {
    return FileOps::joinPath(FileOps::joinPath(mfsRoot,"F18"), sanitiseRegNr(operationId));
}
static std::string f77Dir(const std::string& mfsRoot, const std::string& wfiId) {
    return FileOps::joinPath(FileOps::joinPath(mfsRoot,"F77"), sanitiseRegNr(wfiId));
}
static std::string docEntityDir(const std::string& entityDir, const std::string& docId) {
    return FileOps::joinPath(FileOps::joinPath(entityDir,"AKT"), sanitiseRegNr(docId));
}



// ─────────────────────────────────────────────────────────────
// PROJECT (F16) — Hängeregister + cover sheet
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeProject(const F16& p, const std::string& mfsRoot) {
    // F16 writes a single flat file in mfs/F16/ named by registration number.
    // No subdirectory is created — the F16 folder contains one file per project.
    // The file is named: XV_F16_0001_26.txt (sanitised registration number)

    std::string f16FolderPath = FileOps::joinPath(mfsRoot, "F16");
    FileOps::makeDirs(f16FolderPath);

    std::string sanitisedId = sanitiseRegNr(p.regNumber.toString());
    std::string schluesselPath = FileOps::joinPath(f16FolderPath, sanitisedId + ".txt");

    std::ostringstream schluessel;
    schluessel
        << "PROJEKTKARTEI (F16)\n"
        << std::string(60, '=') << "\n"
        << "REGISTRIERNUMMER : " << p.regNumber.toString() << "\n"
        << "AKTENZEICHEN     : "
        << Config::instance().registratur().geschaeftszeichen
        << "-" << sanitisedId << "\n"
        << "TITEL            : " << p.title << "\n"
        << "VORGANGSART      : " << p.projectType << "\n"
        << "PHASE            : " << fval(p.phase) << "\n"
        << "CODENAME         : " << fval(p.codename) << "\n"
        << "ARCHIVIERT       : " << (p.archived ? "ja" : "nein") << "\n"
        << "\n"
        << "ORGANISATION\n"
        << std::string(40, '-') << "\n";
    if (!p.leadId.empty())       schluessel << "LEITER     : " << p.leadId << "\n";
    if (!p.ownerTeamId.empty())  schluessel << "EINHEIT    : " << p.ownerTeamId << "\n";
    if (!p.sponsorId.empty())    schluessel << "AUFTRAGG   : " << p.sponsorId << "\n";

    schluessel << "\n"
        << "ZEITPLANUNG\n"
        << std::string(40, '-') << "\n"
        << "GEPLANTER START  : " << fval(p.startDatePlanned) << "\n"
        << "GEPLANTES ENDE   : " << fval(p.endDatePlanned) << "\n"
        << "TATS. START      : " << fval(p.startDateActual) << "\n"
        << "TATS. ENDE       : " << fval(p.endDateActual) << "\n"
        << "\n"
        << "KOSTEN / EARNED VALUE\n"
        << std::string(40, '-') << "\n"
        << "BUDGET GEPLANT   : " << p.budgetPlanned << " " << p.currency << "\n"
        << "BUDGET AKTUELL   : " << p.budgetActual << " " << p.currency << "\n"
        << "CPI              : " << p.costPerformanceIndex << "\n"
        << "SPI              : " << p.schedulePerformanceIndex << "\n";

    if (!p.scopeStatement.empty())
        schluessel << "\nSCOPE\n"
                   << std::string(40, '-') << "\n"
                   << p.scopeStatement << "\n";

    schluessel << "\n"
        << "VERBUNDENE AUFGABEN (F22)\n"
        << std::string(40, '-') << "\n";
    auto tasks = F22::loadForProject(p.projectId);
    if (tasks.empty()) {
        schluessel << "  (keine)\n";
    } else {
        for (auto& task : tasks)
            schluessel << "  " << std::left << std::setw(28) << task->regNumber.toString()
                       << "  " << task->title.substr(0, 34) << "\n";
    }

    schluessel << "\nANGELEGT         : " << p.createdAt << "\n"
               << "ZULETZT GEAENDERT: " << p.updatedAt << "\n";

    ownerOnlyWrite(schluesselPath, schluessel.str());
    LOG_DEBUG("MFS F16 geschrieben: " + schluesselPath);
    return true;
}

// ─────────────────────────────────────────────────────────────
// TASK (F22) — ONLY filed under parent F16 Hängeregister/F22/
// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
// TASK (F22) — mfs/F22/<reg>/ own subfolder
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeTask(const F22& t, const std::string& mfsRoot) {
    std::string dir = f22Dir(mfsRoot, t.regNumber.toString());
    FileOps::makeDirs(dir);

    auto proj = F16::loadById(t.projectId);
    std::string projRef = proj ? proj->regNumber.toString() : t.projectId;

    std::string cardPath = FileOps::joinPath(dir, "00_KARTE.txt");
    std::ostringstream oss;
    oss << "AUFGABENKARTE (F22)\n"
        << "=======================================================\n"
        << "REGISTRIERNUMMER : " << t.regNumber.toString()   << "\n"
        << "TITEL            : " << t.title                  << "\n"
        << "STATUS           : " << entityStatusToString(t.status) << "\n"
        << "FORTSCHRITT      : " << t.percentComplete << "%"  << "\n"
        << "BEARBEITER-REF   : " << t.assigneeId              << "\n"
        << "F77-FREIGABE-WFI : " << t.releaseWorkflowId       << "\n"
        << "ANGELEGT         : " << t.createdAt               << "\n"
        << "GEAENDERT        : " << t.updatedAt               << "\n"
        << "=======================================================\n\n";
    if (!t.description.empty())
        oss << "BESCHREIBUNG:\n" << t.description << "\n\n";
    if (!t.acceptanceCriteria.empty())
        oss << "ABNAHME-KRITERIEN:\n" << t.acceptanceCriteria << "\n\n";
    oss << "VERWEISE:\n"
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
// F18 OPERATION — mfs/F18/<operationId>/ own subfolder
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeF18(const F18Operation& v, const std::string& mfsRoot) {
    if (v.taskId.empty()) {
        LOG_WARN("MFSWriter::writeF18 — no task reference for " + v.operationId);
        return false;
    }
    // F18 is a SINGLE TEXT FILE under mfs/F18/<sane-id>.txt (no subfolder).
    // F18S steps each get their own folder: mfs/F18S/<step-id>/<step-id>.txt
    auto task = F22::loadById(v.taskId);
    if (!task) return false;

    // ── F18 index card: single file ─────────────────────────────────────────
    std::string f18Root = FileOps::joinPath(mfsRoot, "F18");
    FileOps::makeDirs(f18Root);
    std::string cardPath = FileOps::joinPath(f18Root,
                               sanitiseRegNr(v.operationId) + ".txt");

    std::ostringstream oss;
    oss << "VORGANGSKARTE F18 (" << v.operationType << ")\n"
        << "=======================================================\n"
        << "VORGANG-ID       : " << v.operationId              << "\n"
        << "TITEL            : " << v.title                    << "\n"
        << "VORGANGSART      : " << v.operationType             << "\n"
        << "STATUS           : " << entityStatusToString(v.status) << "\n"
        << "F22 (AUFGABE)    : " << v.taskId                   << "\n"
        << "F77-FREIGABE-WFI : " << v.releaseWorkflowId        << "\n"
        << "ANGELEGT         : " << v.createdAt                << "\n"
        << "=======================================================\n\n";

    if (!v.description.empty())
        oss << "BESCHREIBUNG:\n" << v.description << "\n\n";
    if (!v.rootCause.empty())
        oss << "URSACHE/ROOT-CAUSE:\n" << v.rootCause << "\n\n";

    // List all steps inline:
    auto steps = F18OperationStep::loadForVorgang(v.operationId);
    if (!steps.empty()) {
        oss << "F18S-SCHRITTE (" << steps.size() << "):\n"
            << std::string(40, '-') << "\n";
        for (auto& s : steps) {
            oss << "  " << s.stepId
                << "  [" << f18StepStatusToString(s.status) << "]"
                << "  " << s.title << "\n";
            if (!s.startDatePlanned.empty())
                oss << "    Start: " << s.startDatePlanned << "\n";
            if (!s.endDatePlanned.empty())
                oss << "    Ende : " << s.endDatePlanned   << "\n";
            oss << "    mfs/F18S/" << sanitiseRegNr(s.stepId) << "/\n";
        }
        oss << "\n";
    }
    oss << "KLARNAMENZUORDNUNG → owner_key.txt\n";

    ownerOnlyWrite(cardPath, oss.str());

    // ── F18S step folders ────────────────────────────────────────────────────
    std::string f18sRoot = FileOps::joinPath(mfsRoot, "F18S");
    FileOps::makeDirs(f18sRoot);
    for (auto& s : steps) {
        std::string stepDir = FileOps::joinPath(f18sRoot, sanitiseRegNr(s.stepId));
        FileOps::makeDirs(stepDir);
        std::string stepCard = FileOps::joinPath(stepDir,
                                   sanitiseRegNr(s.stepId) + ".txt");
        std::ostringstream soss;
        soss << "F18S-SCHRITT\n"
             << "=======================================================\n"
             << "SCHRITT-ID     : " << s.stepId           << "\n"
             << "F18 (VORGANG)  : " << v.operationId       << "\n"
             << "TITEL          : " << s.title             << "\n"
             << "TYP            : " << s.stepType          << "\n"
             << "STATUS         : " << f18StepStatusToString(s.status) << "\n"
             << "TRACKING       : " << s.trackingStatus    << "\n"
             << "START-PLAN     : " << fval(s.startDatePlanned) << "\n"
             << "ENDE-PLAN      : " << fval(s.endDatePlanned)   << "\n"
             << "FORTSCHRITT    : " << s.percentComplete << "%\n"
             << "ZUGEWIESEN     : " << fval(s.assignedTo) << "\n"
             << "=======================================================\n";
        if (!s.decision.empty())
            soss << "ENTSCHEIDUNG   : " << s.decision << "\n";
        if (!s.comment.empty())
            soss << "KOMMENTAR      : " << s.comment << "\n";
        ownerOnlyWrite(stepCard, soss.str());
    }

    std::map<std::string,std::string> conn;
    conn["F22"]   = v.taskId;
    conn["OWNER"] = v.ownerId;
    appendOwnerKey(v.operationId, v.title, conn, mfsRoot);

    LOG_DEBUG("MFS F18 written: " + cardPath + " + " +
              std::to_string(steps.size()) + " F18S folders");
    return true;
}

// ─────────────────────────────────────────────────────────────
// DOCUMENT — filed inside parent entity subfolder/DOK/<docId>/
// Parent can be F16, F22, F18, or F77 — each doc in own subfolder
// ─────────────────────────────────────────────────────────────
bool MFSWriter::writeDocument(const Folder& d, const std::string& mfsRoot) {
    // Determine parent entity directory
    std::string parentDir;
    std::string parentRef;

    if (!d.taskId.empty()) {
        auto task = F22::loadById(d.taskId);
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
    std::string docDir = docEntityDir(parentDir, d.folderId);
    FileOps::makeDirs(docDir);

    std::string sane     = sanitiseRegNr(d.folderId);
    std::string safeName = FileOps::sanitizeFilename(d.title);
    if (safeName.size() > 40) safeName = safeName.substr(0, 40);
    std::string cardName = sane + ".txt";
    std::string cardPath = FileOps::joinPath(docDir, cardName);

    std::ostringstream content;
    content << "AKTEN-KARTE\n"
            << "=======================================================\n"
            << "AKTEN-ID      : " << d.folderId          << "\n"
            << "TITEL            : " << d.title               << "\n"
            << "TYP              : " << d.docType             << "\n"
            << "FORMAT           : " << d.format              << "\n"
            << "VERSION          : " << d.version             << "\n"
            << "STATUS           : " << d.currentRevisionState() << "\n"
            << "ERSTELLT         : " << d.dateCreated         << "\n"
            << "GENEHMIGT        : " << d.dateApproved        << "\n"
            << "=======================================================\n\n"
            << "VERWEISE:\n"
            << "  UEBERGEORDNET   : " << parentRef
            << "  [mfs/" << parentRef << "/]\n";
    if (!d.authorId.empty())
        content << "  VERFASSER-REF   : " << d.authorId << "  → owner_key.txt\n";
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
    appendOwnerKey(d.folderId, d.title, conn, mfsRoot);

    LOG_DEBUG("MFS AKT written: " + cardPath);
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



// ── _SCHLUESSEL.txt files ────────────────────────────────────────────────────
// Every entity type gets a folder under mfs/{TYPE}/ with:
//   _SCHLUESSEL.txt  — index of all entities of this type
// Every entity gets its own subfolder with:
//   _SCHLUESSEL.txt  — details of this entity + linked child IDs

static void writeSchluessel(const std::string& path, const std::string& content) {
    { auto pp = path.substr(0,path.rfind("/")); if(!pp.empty()) FileOps::makeDirs(pp); }
    FileOps::writeTextFile(path, content);
}

void MFSWriter::rebuildTypeSchluessel(const std::string& mfsRoot,
                                       const std::string& typeCode)
{
    // Each model class provides its own mfsSchluesselText() — this function
    // only handles the header, routing, and file write.
    // Adding a new entity type = adding a case in the if/else below + a
    // mfsSchluesselText() method on the new model class.

    std::string typeDir = FileOps::joinPath(mfsRoot, typeCode);
    FileOps::makeDirs(typeDir);

    std::ostringstream s;
    s << "ROSENHOLZ PM — SCHLÜSSELDATEI\n"
      << "==============================\n"
      << "Typ      : " << typeCode << "\n"
      << "Erstellt : " << nowIso() << "\n\n"
      << "Diese Datei ermöglicht die Entschlüsselung der Objekte ohne Rosenholz PM.\n"
      << "==============================\n\n";

    if (typeCode == "F16") {
        auto items = F16::loadAll();
        s << "Alle F16-Vorgaenge (" << items.size() << "):\n\n";
        for (auto& p : items) s << p->mfsSchluesselText();

    } else if (typeCode == "F22") {
        std::vector<std::shared_ptr<F22>> items;
        for (auto& p : F16::loadAll()) {
            auto pt = F22::loadForProject(p->projectId);
            items.insert(items.end(), pt.begin(), pt.end());
        }
        s << "Alle F22-Aufgaben (" << items.size() << "):\n\n";
        for (auto& t : items) s << t->mfsSchluesselText();

    } else if (typeCode == "F18") {
        std::vector<std::shared_ptr<F18Operation>> items;
        for (auto& p : F16::loadAll()) {
            auto tasks = F22::loadForProject(p->projectId);
            for (auto& t : tasks) {
                auto tf = F18Operation::loadForTask(t->taskId);
                items.insert(items.end(), tf.begin(), tf.end());
            }
        }
        s << "Alle F18-Vorgaenge (" << items.size() << "):\n\n";
        for (auto& v : items) s << v->mfsSchluesselText();

    } else if (typeCode == "AKT") {
        auto items = Folder::loadRecent(9999);
        s << "Alle Akten (" << items.size() << "):\n\n";
        for (auto& d : items) s << d->mfsSchluesselText();

    } else if (typeCode == "PER") {
        auto items = Person::loadAll();
        s << "Alle Personen (" << items.size() << "):\n\n";
        for (auto& p : items) s << p->mfsSchluesselText();
    }

    writeSchluessel(FileOps::joinPath(typeDir, "_SCHLUESSEL.txt"), s.str());
    LOG_INFO("[MFS] Schluessel updated: " + typeCode);
}


bool MFSWriter::rebuildAll(const std::string& mfsRoot) {
    LOG_INFO("Rebuilding MFS tree at: " + mfsRoot);
    for (auto& sub : {"F16","F22","F18","F77","AKT"})
        FileOps::makeDirs(FileOps::joinPath(mfsRoot, sub));

    // Reset owner key
    FileOps::deleteFile(FileOps::joinPath(mfsRoot, "owner_key.txt"));

    bool ok = true;
    int nProj=0, nTask=0, nF18=0, nDok=0, nF77=0;

    auto projects = F16::loadAll();
    for (auto& p : projects) {
        ok &= writeProject(*p, mfsRoot); ++nProj;

        // Tasks (F22) — filed under F22/<reg>/
        auto tasks = F22::loadForProject(p->projectId);
        for (auto& t : tasks) { ok &= writeTask(*t, mfsRoot); ++nTask; }

        // F18 operations via tasks — filed under F18/<id>/
        for (auto& t : F22::loadForProject(p->projectId)) {
            auto f18s = F18Operation::loadForTask(t->taskId);
            for (auto& v : f18s) { ok &= writeF18(*v, mfsRoot); ++nF18; }
        }

        // Documents filed under their parent F22 task
        for (auto& t : tasks) {
            auto tDocs = Folder::loadForEntity("f22", t->taskId);
            for (auto& d : tDocs) { ok &= writeDocument(*d, mfsRoot); ++nDok; }
        }
        // Documents filed under F18 operations
        for (auto& t : F22::loadForProject(p->projectId)) {
            auto f18s = F18Operation::loadForTask(t->taskId);
            for (auto& v : f18s) {
                auto fDocs = Folder::loadForEntity("f18", v->operationId);
                for (auto& d : fDocs) { ok &= writeDocument(*d, mfsRoot); ++nDok; }
            }
        }
    }

    // F77 active workflows — filed under F77/<id>/
    auto wfs = F77W::loadActive();
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
    // Rebuild type-level Schlüssel index files
    for (const std::string& type : std::vector<std::string>{"F16","F22","F18","AKT","PER"})
        rebuildTypeSchluessel(mfsRoot, type);

    return ok;
}


} // namespace Rosenholz

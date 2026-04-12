// ============================================================
// MFSWriter.cpp  —  MFS-style plaintext file writer
// ============================================================
#include "MFSWriter.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include "../model/ProjectF16.h"
#include "../model/TaskF22.h"
#include "../model/IncidentF18.h"
#include "../model/Risk.h"
#include "../model/Document.h"
#include "../model/Person.h"
#include "../model/Team.h"

#ifndef _WIN32
  #include <sys/stat.h>
#endif
#include <sstream>

namespace RH {

// ── Private: write file with owner-only permissions ──────────
bool MFSWriter::ownerOnlyWrite(const std::string& path, const std::string& content) {
    FileOps::makeDirs(FileOps::dirName(path));
    bool ok = FileOps::writeTextFile(path, content, false);
#ifndef _WIN32
    if (ok) chmod(path.c_str(), S_IRUSR | S_IWUSR);  // 600
#endif
    return ok;
}

// ── Project (F16) ─────────────────────────────────────────────
bool MFSWriter::writeProject(const ProjectF16& p, const std::string& mfsRoot) {
    std::string dir  = FileOps::joinPath(mfsRoot, "F16");
    std::string path = FileOps::joinPath(dir, "F16_" + p.regNumber.toString() + ".txt");

    std::ostringstream oss;
    oss << "=========================================\n";
    oss << "  VORGANG F16  |  " << p.regNumber.toString() << "\n";
    oss << "=========================================\n";
    oss << "VORGANGSART:     " << p.projectType              << "\n";
    oss << "GROESSE:         " << p.sizeClass                << "\n";
    oss << "STATUS:          " << p.status                   << "\n";
    oss << "PHASE:           " << p.phase                    << "\n";
    oss << "ANGELEGT:        " << p.createdAt                << "\n";
    oss << "GEAENDERT:       " << p.updatedAt                << "\n";
    oss << "-----------------------------------------\n";
    // Only opaque cross-references — no real names
    oss << "F22-VORGAENGE:   F22/" << p.regNumber.toString() << "\n";
    oss << "F18-VORFAELLE:   F18/" << p.regNumber.toString() << "\n";
    if (!p.qualityIds.empty()) {
        oss << "QTCS-QUALITAET:";
        for (auto& id : p.qualityIds) oss << " " << id;
        oss << "\n";
    }
    if (!p.costIds.empty()) {
        oss << "QTCS-KOSTEN:   ";
        for (auto& id : p.costIds) oss << " " << id;
        oss << "\n";
    }
    // Intentionally omit title, lead name, team name — in owner key only

    bool ok = ownerOnlyWrite(path, oss.str());
    LOG_DEBUG("MFS F16 written: " + path);

    // Append to owner key
    std::map<std::string, std::string> connections;
    connections["lead_id"]   = p.leadId;
    connections["team_id"]   = p.ownerTeamId;
    connections["sponsor_id"]= p.sponsorId;
    appendOwnerKey(p.regNumber.toString(), p.title, connections, mfsRoot);

    return ok;
}

// ── Task (F22) ────────────────────────────────────────────────
bool MFSWriter::writeTask(const TaskF22& t, const std::string& mfsRoot) {
    std::string dir  = FileOps::joinPath(mfsRoot, "F22");
    std::string path = FileOps::joinPath(dir, "F22_" + t.regNumber.toString() + ".txt");

    std::ostringstream oss;
    oss << "=========================================\n";
    oss << "  AUFGABE F22  |  " << t.regNumber.toString() << "\n";
    oss << "=========================================\n";
    oss << "VORGANG-REF:     F16/" << t.projectId          << "\n";
    oss << "STATUS:          " << t.status                  << "\n";
    oss << "FORTSCHRITT:     " << t.percentComplete << "%"  << "\n";
    oss << "AUFWAND-PLAN:    " << t.effortPlannedHrs << "h" << "\n";
    oss << "AUFWAND-IST:     " << t.effortActualHrs  << "h" << "\n";
    oss << "ANGELEGT:        " << t.createdAt                << "\n";
    if (!t.parentTaskId.empty())
        oss << "PARENT-F22:      F22/" << t.parentTaskId << "\n";

    bool ok = ownerOnlyWrite(path, oss.str());
    LOG_DEBUG("MFS F22 written: " + path);

    std::map<std::string, std::string> connections;
    connections["project_id"]    = t.projectId;
    connections["assignee_id"]   = t.assigneeId;
    connections["parent_task_id"]= t.parentTaskId;
    appendOwnerKey(t.regNumber.toString(), t.title, connections, mfsRoot);
    return ok;
}

// ── Incident (F18) ───────────────────────────────────────────
bool MFSWriter::writeIncident(const IncidentF18& i, const std::string& mfsRoot) {
    std::string dir  = FileOps::joinPath(mfsRoot, "F18");
    std::string path = FileOps::joinPath(dir, "F18_" + i.regNumber.toString() + ".txt");

    std::ostringstream oss;
    oss << "=========================================\n";
    oss << "  VORFALL F18  |  " << i.regNumber.toString() << "\n";
    oss << "=========================================\n";
    oss << "VORGANG-REF:     F16/" << i.projectId          << "\n";
    oss << "SCHWERE:         " << i.severity                << "\n";
    oss << "STATUS:          " << i.status                  << "\n";
    oss << "GEMELDET:        " << i.reportedDate            << "\n";
    oss << "GELÖST:          " << i.resolvedDate            << "\n";
    if (!i.riskId.empty())
        oss << "RISIKO-REF:      " << i.riskId             << "\n";

    bool ok = ownerOnlyWrite(path, oss.str());
    LOG_DEBUG("MFS F18 written: " + path);

    std::map<std::string, std::string> connections;
    connections["project_id"]  = i.projectId;
    connections["owner_id"]    = i.ownerId;
    connections["reported_by"] = i.reportedBy;
    connections["risk_id"]     = i.riskId;
    appendOwnerKey(i.regNumber.toString(), i.title, connections, mfsRoot);
    return ok;
}

// ── Risk ──────────────────────────────────────────────────────
bool MFSWriter::writeRisk(const Risk& r, const std::string& mfsRoot) {
    std::string dir  = FileOps::joinPath(mfsRoot, "RISIKEN");
    std::string path = FileOps::joinPath(dir, "RISK_" + r.riskId + ".txt");

    std::ostringstream oss;
    oss << "=========================================\n";
    oss << "  RISIKO  |  " << r.riskId << "\n";
    oss << "=========================================\n";
    oss << "VORGANG-REF:     F16/" << r.projectId          << "\n";
    oss << "RISIKONIVEAU:    " << r.riskLevel               << "\n";
    oss << "GESAMTSCORE:     " << r.overallRiskScore        << "\n";
    oss << "STRATEGIE:       " << r.responseStrategy        << "\n";
    oss << "STATUS:          " << r.status                  << "\n";

    bool ok = ownerOnlyWrite(path, oss.str());

    std::map<std::string, std::string> connections;
    connections["project_id"] = r.projectId;
    connections["owner_id"]   = r.ownerId;
    appendOwnerKey(r.riskId, r.title, connections, mfsRoot);
    return ok;
}

// ── Document ──────────────────────────────────────────────────
bool MFSWriter::writeDocument(const Document& d, const std::string& mfsRoot) {
    std::string dir  = FileOps::joinPath(mfsRoot, "DOKUMENTE");
    std::string path = FileOps::joinPath(dir, "DOK_" + d.documentId + ".txt");

    std::ostringstream oss;
    oss << "=========================================\n";
    oss << "  DOKUMENT  |  " << d.documentId << "\n";
    oss << "=========================================\n";
    oss << "TYP:             " << d.docType    << "\n";
    oss << "FORMAT:          " << d.format     << "\n";
    oss << "VERSION:         " << d.version    << "\n";
    oss << "STATUS:          " << d.status     << "\n";
    oss << "ERSTELLT:        " << d.dateCreated<< "\n";
    if (!d.fileUrl.empty())
        oss << "QUELLE-URL:      " << d.fileUrl << "\n";

    bool ok = ownerOnlyWrite(path, oss.str());

    std::map<std::string, std::string> connections;
    connections["project_id"] = d.projectId;
    connections["author_id"]  = d.authorId;
    appendOwnerKey(d.documentId, d.title, connections, mfsRoot);
    return ok;
}

// ── Person ────────────────────────────────────────────────────
bool MFSWriter::writePerson(const Person& p, const std::string& mfsRoot) {
    // Persons go in a separate branch — only reg number visible
    std::string dir  = FileOps::joinPath(mfsRoot, "PERSONEN");
    std::string path = FileOps::joinPath(dir, "PER_" + p.regNumber.toString() + ".txt");

    std::ostringstream oss;
    oss << "REGISTRIERNUMMER: " << p.regNumber.toString() << "\n";
    oss << "TYP:              " << p.personType           << "\n";
    oss << "STATUS:           " << p.status               << "\n";

    return ownerOnlyWrite(path, oss.str());
}

// ── Team ─────────────────────────────────────────────────────
bool MFSWriter::writeTeam(const Team& t, const std::string& mfsRoot) {
    std::string dir  = FileOps::joinPath(mfsRoot, "DIENSTEINHEITEN");
    std::string path = FileOps::joinPath(dir, "DE_" + t.teamId + ".txt");

    std::ostringstream oss;
    oss << "ID:    " << t.teamId << "\n";
    oss << "TYP:   " << t.type   << "\n";
    oss << "STAND: " << t.status << "\n";
    if (!t.parentTeamId.empty())
        oss << "UEBERGEORDNET: " << t.parentTeamId << "\n";

    return ownerOnlyWrite(path, oss.str());
}

// ── Owner key ─────────────────────────────────────────────────
bool MFSWriter::appendOwnerKey(
    const std::string& regNumber,
    const std::string& realTitle,
    const std::map<std::string, std::string>& connections,
    const std::string& mfsRoot)
{
    std::string path = FileOps::joinPath(mfsRoot, "owner_key.txt");

    std::ostringstream oss;
    oss << "[" << regNumber << "]\n";
    oss << "KLARNAME: " << realTitle << "\n";
    for (auto& [k, v] : connections)
        if (!v.empty()) oss << k << ": " << v << "\n";
    oss << "\n";

    bool ok = FileOps::writeTextFile(path, oss.str(), true /* append */);
#ifndef _WIN32
    if (ok) chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
    return ok;
}

// ── Batch rebuild ─────────────────────────────────────────────
bool MFSWriter::rebuildAll(const std::string& mfsRoot) {
    LOG_INFO("Rebuilding MFS tree at: " + mfsRoot);
    FileOps::ensureMFSTree(mfsRoot);

    // Remove old owner key and rebuild
    std::string keyPath = FileOps::joinPath(mfsRoot, "owner_key.txt");
    FileOps::deleteFile(keyPath);

    bool ok = true;

    auto projects = ProjectF16::loadAll();
    LOG_INFO("Writing " + std::to_string(projects.size()) + " F16 MFS files");
    for (auto& p : projects) ok &= writeProject(*p, mfsRoot);

    // Tasks — load from DB directly via all projects
    for (auto& proj : projects) {
        auto tasks = TaskF22::loadForProject(proj->projectId);
        for (auto& t : tasks) ok &= writeTask(*t, mfsRoot);
    }

    auto incidents = IncidentF18::loadOpenIncidents();
    LOG_INFO("Writing " + std::to_string(incidents.size()) + " F18 MFS files");
    for (auto& i : incidents) ok &= writeIncident(*i, mfsRoot);

    LOG_INFO("MFS rebuild complete. All OK: " + std::string(ok?"yes":"NO"));
    return ok;
}

} // namespace RH

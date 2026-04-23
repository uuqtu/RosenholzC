// ============================================================
// DocumentMenu.cpp  —  Document detail menu
//
// Lifecycle states (DocumentRevision):
//   in_work → pre_released → released → locked / closed
//   Each document has one current revision (superseded=false).
//   State transitions are driven by the Main Workflow End step.
// ============================================================
#include "cli_common.h"
#include "../workflow/F77Workflow.h"
#include "../repository/DocumentRevision.h"
#include "../workflow/F77Workflow.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include <iomanip>

namespace CLI {
using namespace Rosenholz;

// ── Print document summary ────────────────────────────────────
void printDocument(const Document& d) {
    hdr("DOK — " + d.documentId.substr(0,26) + "  " + d.title.substr(0,24));
    // Show current revision state (the authoritative lifecycle state)
    auto curRev = Rosenholz::DocumentRevision::currentRevision(d.documentId);
    std::string revInfo = curRev
        ? "Rev " + std::to_string(curRev->rev) + " [" + curRev->revState + "]"
        : "(keine Revision)";
    std::cout << "  Revision   : " << revInfo << "\n";
    std::cout << "  Dok-Status : " << d.status << "\n";
    std::cout << "  Typ        : " << d.docType << " / " << d.format << "\n";
    std::cout << "  Version    : " << d.version << "\n";
    std::cout << "  Projekt    : " << (d.projectId.empty() ? "—" : d.projectId.substr(0,32)) << "\n";
    if (!d.taskId.empty())
        std::cout << "  Aufgabe    : " << d.taskId.substr(0,32) << "\n";
    if (!d.releaseWorkflowId.empty())
        std::cout << "  Main WFI   : " << d.releaseWorkflowId.substr(0,36) << "\n";
    // Show current revision
    auto cur = DocumentRevision::currentRevision(d.documentId);
    if (cur)
        std::cout << "  Revision   : Rev " << cur->rev
                  << " [" << cur->revState << "]"
                  << (cur->superseded ? "" : " ← aktiv") << "\n";
    else
        std::cout << "  Revision   : (keine)\n";
    if (!d.filePath.empty())
        std::cout << "  Datei      : " << FileOps::baseName(d.filePath) << "\n";
    std::cout << "\n";
}

// ── List documents ────────────────────────────────────────────
void listDocuments(const std::vector<std::shared_ptr<Document>>& docs,
                   const std::string& title) {
    hdr(title.empty() ? "DOKUMENTE" : title);
    if (docs.empty()) { std::cout << "  (keine Dokumente)\n"; return; }
    int n=1;
    for (auto& d : docs) {
        auto cur = DocumentRevision::currentRevision(d->documentId);
        std::cout << "  " << std::setw(3) << n++ << ". "
                  << std::left << std::setw(28) << d->title.substr(0,26)
                  << "  " << std::setw(12) << d->status
                  << "  v" << d->version;
        if (cur) std::cout << "  [Rev " << cur->rev << " " << cur->revState << "]";
        std::cout << "\n";
    }
    std::cout << "\n";
}

// ── Revision submenu ─────────────────────────────────────────
static void revisionMenu(std::shared_ptr<Document> doc) {
    while (true) {
        hdr("REVISIONEN — " + doc->documentId.substr(0,26));
        auto revs = DocumentRevision::loadAllRevisions(doc->documentId);
        
        if (revs.empty()) {
            std::cout << "  (keine Revisionen vorhanden)\n\n";
            std::cout << "  1.Revision 1 anlegen  0.Zurück\n";
            int ch = readInt("Wahl",0,1); if (ch==0) break;
            if (ch==1) {
                doc->ensureRevision1();
                std::cout << "  >> Revision 1 angelegt (in_work)\n";
            }
            continue;
        }
        
        std::cout << "  Zustandsmaschine: in_work → pre_released → released → locked/closed\n\n";
        for (auto& r : revs) {
            std::cout << "  Rev " << std::setw(3) << r->rev
                      << "  [" << std::left << std::setw(13) << r->revState << "]"
                      << (!r->superseded ? "  ← AKTIV" : "          ")
                      << (r->parentRev ? "  Eltern: Rev " + std::to_string(r->parentRev) : "")
                      << (r->contentHash.empty() ? "" : "  ✓ Inhalt")
                      << "\n";
        }
        
        // Show allowed next transitions for the active revision
        auto cur = DocumentRevision::currentRevision(doc->documentId);
        if (cur) {
            std::cout << "\n  Aktive Revision Rev " << cur->rev
                      << " (" << cur->revState << ") — erlaubte Übergänge:\n";
            for (auto& target : {"in_work","pre_released","released","locked","closed"}) {
                if (DocumentRevision::isTransitionAllowed(cur->revState, target))
                    std::cout << "    → " << target << "\n";
            }
        }
        
        std::cout << "\n"
                  << "  1.Zustand ändern   2.Neue Revision anlegen\n"
                  << "  3.Main Workflow    0.Zurück\n";
        int ch = readInt("Wahl",0,3); if (ch==0) break;
        
        if (ch==1) {
            // State transition
            if (!cur) { std::cout << "  >> Keine aktive Revision.\n"; continue; }
            std::cout << "  Zielzustand:\n"
                      << "  1.in_work  2.pre_released  3.released  4.locked  5.closed\n";
            static const char* ss[] = {"in_work","pre_released","released","locked","closed"};
            int si = readInt("Zustand",1,5);
            std::string target = ss[si-1];
            
            if (!DocumentRevision::isTransitionAllowed(cur->revState, target)) {
                std::cout << "  >> Übergang " << cur->revState << " → " << target
                          << " ist nicht erlaubt.\n";
                continue;
            }
            
            // Warn on irreversible transitions
            if (target == "closed") {
                std::cout << "  ⚠ CLOSED ist terminal — kein Rückweg. Bestätigen? (ja): ";
                std::string conf; std::getline(std::cin, conf);
                if (conf != "ja") { std::cout << "  >> Abgebrochen.\n"; continue; }
            }
            if (target == "released") {
                std::cout << "  ⚠ RELEASED ist unveränderlich — Bestätigen? (ja): ";
                std::string conf; std::getline(std::cin, conf);
                if (conf != "ja") { std::cout << "  >> Abgebrochen.\n"; continue; }
            }
            
            if (cur->transitionState(target)) {
                // Sync document status to match active revision state
                doc->status = target;
                doc->update();
                std::cout << "  >> Rev " << cur->rev << " → " << target << " ✓\n";
            } else {
                std::cout << "  >> Übergang fehlgeschlagen.\n";
            }
            
        } else if (ch==2) {
            // Create new revision
            if (!cur) { std::cout << "  >> Keine Basis-Revision.\n"; continue; }
            // Check if creating from released/locked requires confirmation
            if (cur->revState == "released" || cur->revState == "locked") {
                std::cout << "  Neue Revision basierend auf Rev " << cur->rev
                          << " (" << cur->revState << ") — Bestätigen? (ja): ";
                std::string conf; std::getline(std::cin, conf);
                if (conf != "ja") { std::cout << "  >> Abgebrochen.\n"; continue; }
            }
            std::string note = readOpt("Änderungsnotiz: ");
            auto nr = DocumentRevision::createRevision(
                doc->documentId, cur->rev, doc->authorId, note);
            if (nr) {
                std::cout << "  >> Rev " << nr->rev << " angelegt (in_work)\n";
                // Sync document status back to in_work (new revision is active)
                doc->status = "in_work"; doc->update();
            }
            
        } else if (ch==3) {
            // F77 Freigabe-Workflow for this document
            hdr("F77 FREIGABE-WORKFLOW — " + doc->documentId.substr(0,26));
            std::cout << "  Dokument-Status : " << doc->status << "\n";
            if (doc->releaseWorkflowId.empty()) {
                std::cout << "  (kein Main Workflow — wird angelegt)\n";
                doc->ensureReleaseWorkflow();
                auto rd = Document::loadById(doc->documentId);
                if (rd) *doc = *rd;
            }
            if (!doc->releaseWorkflowId.empty()) {
                int blockers=0;
                F77_Engine::canRelease("dok", doc->documentId, doc->releaseWorkflowId, blockers);
                std::cout << "  Main WFI  : " << doc->releaseWorkflowId.substr(0,36) << "\n";
                std::cout << (blockers>0
                    ? "  ⚠ " + std::to_string(blockers) + " offene Sub-WFI(s)\n"
                    : "  ✓ End-Schritt kann ausgeführt werden\n");
                std::cout << "\n  1.Main WFI öffnen  0.Zurück\n";
                int mch = readInt("Wahl",0,1);
                if (mch==1) instanceMenu(doc->releaseWorkflowId);
            }
        }
    }
}

// ── Main document menu ────────────────────────────────────────
void documentMenu(std::shared_ptr<Document> doc) {
    while (true) {
        if (auto fresh = Document::loadById(doc->documentId)) *doc = *fresh;
        printDocument(*doc);
        
        // Show status warning
        auto cur = DocumentRevision::currentRevision(doc->documentId);
        bool immutable = cur && (cur->revState == "released" ||
                                  cur->revState == "locked" ||
                                  cur->revState == "closed");
        if (immutable)
            std::cout << "  ⚠ Rev " << cur->rev << " ist " << cur->revState
                      << " — unveränderlich\n\n";

        std::cout
            << "  [DOKUMENT]\n"
            << "    1. Bearbeiten (Felder)\n"
            << "    2. Revisionen / 5-State-Workflow\n"
            << "    3. Main Workflow / Freigabe\n"
            << "\n  [DATEI]\n"
            << "    4. Datei öffnen (Lesen)\n"
            << "    5. Datei öffnen (Bearbeiten + Snapshot)\n"
            << "    6. URL neu herunterladen\n"
            << "    7. Versionsverlauf\n"
            << "\n  [SYSTEM]\n"
            << "    8. MFS-Datei schreiben\n"
            << "    9. Dokument löschen\n"
            << "\n    0. Zurück\n";
        hr();
        
        int ch = readInt("Wahl",0,9); if (ch==0) break;
        
        if (ch==1) {
            // Edit submenu
            if (immutable) { std::cout << "  >> " << cur->revState << " — kein Bearbeiten.\n"; continue; }
            hdr("BEARBEITEN");
            std::string t = readOpt("Titel (leer=behalten): ");
            if (!t.empty()) doc->title = t;
            std::string cat = readOpt("Kategorie (leer=behalten): ");
            if (!cat.empty()) doc->docCategory = cat;
            std::string ver = readOpt("Version (leer=behalten): ");
            if (!ver.empty()) doc->version = ver;
            std::string cls = readOpt("Einstufung (intern/vertraulich/öffentlich): ");
            if (!cls.empty()) doc->classification = cls;
            std::string sum = readOpt("Kurzbeschreibung: ");
            if (!sum.empty()) doc->summary = sum;
            std::string auth = readOpt("Autor-ID: ");
            if (!auth.empty()) doc->authorId = auth;
            std::string approv = readOpt("Genehmiger-ID: ");
            if (!approv.empty()) doc->approvedBy = approv;
            doc->update(); std::cout << "  >> Gespeichert.\n";
            
        } else if (ch==2) {
            revisionMenu(doc);
            
        } else if (ch==3) {
            // F77 Freigabe-Workflow (inline)
            hdr("F77 FREIGABE-WORKFLOW — " + doc->documentId.substr(0,26));
            std::cout << "  Status: " << doc->status << "\n";
            if (doc->releaseWorkflowId.empty()) {
                std::cout << "  Anlegen...\n";
                doc->ensureReleaseWorkflow();
                if (auto rd = Document::loadById(doc->documentId)) *doc = *rd;
            }
            if (!doc->releaseWorkflowId.empty()) {
                int bl=0;
                F77_Engine::canRelease("dok",doc->documentId,doc->releaseWorkflowId,bl);
                std::cout << "  Main WFI: " << doc->releaseWorkflowId.substr(0,36) << "\n"
                          << (bl>0 ? "  ⚠ "+std::to_string(bl)+" Sub-WFI(s) offen\n"
                                   : "  ✓ Freigabe möglich\n");
                std::cout << "  1.Öffnen  0.Zurück\n";
                if (readInt("Wahl",0,1)==1) instanceMenu(doc->releaseWorkflowId);
            }
            
        } else if (ch==4) {
            if (!doc->filePath.empty() && FileOps::fileExists(doc->filePath))
                doc->openFile("read");
            else std::cout << "  >> Keine Datei vorhanden.\n";
            
        } else if (ch==5) {
            if (immutable) { std::cout << "  >> " << cur->revState << " — kein Bearbeiten.\n"; continue; }
            if (!doc->filePath.empty() && FileOps::fileExists(doc->filePath)) {
                std::string note = readOpt("Änderungsnotiz: ");
                doc->snapshotVersion(note.empty() ? "Vor Bearbeitung" : note);
                doc->openFile("edit");
                std::cout << "  >> Snapshot gespeichert. Datei geöffnet.\n";
            } else std::cout << "  >> Keine Datei vorhanden.\n";
            
        } else if (ch==6) {
            if (doc->fileUrl.empty()) { doc->fileUrl = readLine("URL: "); doc->update(); }
            if (!doc->fileUrl.empty()) {
                doc->snapshotVersion("Vor URL-Aktualisierung");
                if (doc->refreshFromUrl())
                    std::cout << "  >> Aktualisiert: " << doc->filePath << "\n";
                else std::cout << "  >> Fehler beim Download.\n";
            }
            
        } else if (ch==7) {
            auto vl = doc->loadVersions();
            hdr("VERSIONSVERLAUF (" + std::to_string(vl.size()) + ")");
            for (auto& v : vl)
                std::cout << "  " << v.versionNumber << "  " << v.createdAt.substr(0,16)
                          << "  " << v.changeNote.substr(0,30) << "\n";
                          
        } else if (ch==8) {
            MFSWriter::writeDocument(*doc, Config::instance().mfsPath());
            std::cout << "  >> MFS-Datei geschrieben.\n";
            
        } else if (ch==9) {
            std::cout << "  Dokument " << doc->documentId << " löschen? (ja): ";
            std::string c; std::getline(std::cin, c);
            if (c=="ja") { doc->remove(); std::cout << "  >> Gelöscht.\n"; break; }
        }
    }
}

// ── Document browser ──────────────────────────────────────────
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

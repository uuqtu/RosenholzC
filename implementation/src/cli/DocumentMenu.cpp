// ============================================================
// DocumentMenu.cpp  —  Document (DOK) browser and detail menu
//
// documentBrowserMenu() : list/search/open documents by context
// documentMenu()         : detail view with edit, version, open
// Opts 10–12: open-read, open-edit+snapshot, version history
// ============================================================
#include "cli_common.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include "../core/Logger.h"
#include <sstream>
#include <algorithm>

namespace CLI {

void documentMenu(std::shared_ptr<Rosenholz::Document> doc) {
    while (true) {
        printDocument(*doc);
        std::cout << "  Document actions:\n"
                  << "    1. Edit title / category / version\n"
                  << "    2. Edit status / classification\n"
                  << "    3. Edit dates (created / approved / expires)\n"
                  << "    4. Edit summary / tags\n"
                  << "    5. Set author / approver\n"
                  << "    6. Attach to entity (project / task / incident)\n"
                  << "    7. Re-download / re-archive URL\n"
                  << "    8. Write MFS file\n"
                  << "    9. Delete document record\n"
                  << "   10. Datei öffnen (Lesen)\n"
                  << "   11. Datei öffnen (Bearbeiten + Version)\n"
                  << "   12. Versionsverlauf anzeigen\n"
                  << "    0. Back\n";
        int ch = readInt("Choice", 0, 12);
        if (ch == 0) break;

        else if (ch == 1) {
            std::string t = readOpt("New title (Enter to keep): ");
            if (!t.empty()) doc->title = t;
            std::string c = readOpt("New category (Enter to keep): ");
            if (!c.empty()) doc->docCategory = c;
            std::string v = readOpt("New version (Enter to keep): ");
            if (!v.empty()) doc->version = v;
            std::string f = readOpt("New format (Enter to keep): ");
            if (!f.empty()) doc->format = f;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 2) {
            std::cout << "  Status: draft / review / approved / superseded / archived\n";
            std::string s = readOpt("New status (Enter to keep): ");
            if (!s.empty()) doc->status = s;
            std::string cl = readOpt("New classification (Enter to keep): ");
            if (!cl.empty()) doc->classification = cl;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 3) {
            std::string dc = readOpt("Created  YYYY-MM-DD: ");
            if (!dc.empty()) doc->dateCreated = dc;
            std::string da = readOpt("Approved YYYY-MM-DD: ");
            if (!da.empty()) doc->dateApproved = da;
            std::string de = readOpt("Expires  YYYY-MM-DD: ");
            if (!de.empty()) doc->dateExpires = de;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 4) {
            std::cout << "  Current summary: " << fval(doc->summary) << "\n";
            std::string s = readLine("New summary: ");
            doc->summary = s;
            std::string t = readOpt("New tags comma-separated: ");
            if (!t.empty()) doc->tags = t;
            doc->update(); std::cout << "  >> Saved.\n";
        }
        else if (ch == 5) {
            std::string a = readOpt("Author person-ID (Enter to keep): ");
            if (!a.empty()) doc->reassignAuthor(a);
            std::string ap = readOpt("Approved-by person-ID (Enter to keep): ");
            if (!ap.empty()) { doc->approvedBy = ap; doc->update(); }
            std::cout << "  >> Saved.\n";
        }
        else if (ch == 6) {
            std::cout << "  Entity types: project / task / incident / risk\n";
            std::string et = readLine("Entity type: ");
            std::string ei = readLine("Entity ID: ");
            std::string rel = readOpt("Relationship (attached/reference/evidence, default=attached): ");
            if (rel.empty()) rel = "attached";
            bool ok = doc->attachToEntity(et, ei, rel);
            std::cout << "  >> " << (ok ? "Attached." : "Failed.") << "\n";
        }
        else if (ch == 7) {
            // URL erneuern — snapshot aktuelle Version, dann neu herunterladen
            if (doc->fileUrl.empty()) {
                doc->fileUrl = readLine("URL eingeben: ");
                doc->update();
            }
            if (doc->fileUrl.empty()) { std::cout << "  >> Keine URL.\n"; continue; }
            std::cout << "  Snapshot der aktuellen Version...\n";
            doc->snapshotVersion("Vor URL-Aktualisierung");
            std::cout << "  Herunterladen: " << doc->fileUrl << "\n";
            if (doc->refreshFromUrl()) {
                std::cout << "  >> URL aktualisiert. Neue Version: " << doc->version << "\n"
                          << "  >> MFS-Pfad: " << doc->filePath << "\n";
                Rosenholz::MFSWriter::writeDocument(*doc, Rosenholz::Config::instance().mfsPath());
            } else {
                std::cout << "  >> Aktualisierung fehlgeschlagen (Netzwerkfehler?).\n";
            }
        }
        else if (ch == 8) {
            auto& cfg = Rosenholz::Config::instance();
            Rosenholz::MFSWriter::writeDocument(*doc, cfg.mfsPath());
            std::cout << "  >> MFS file written.\n";
        }
        else if (ch == 9) {
            std::cout << "  Delete document record '" << doc->title << "'? (y/n): ";
            std::string ans; std::getline(std::cin, ans);
            if (!ans.empty() && (ans[0]=='y'||ans[0]=='Y')) {
                doc->remove();
                std::cout << "  >> Deleted.\n";
                break;
            }
        }

        else if (ch == 10) {
            // Öffnen (Lesen) — temporäre Kopie
            if (doc->filePath.empty() || !Rosenholz::FileOps::fileExists(doc->filePath)) {
                std::cout << "  >> Keine Datei vorhanden: " << fval(doc->filePath) << "\n";
            } else {
                std::cout << "  Öffne zur Ansicht (temporäre Kopie)...\n";
                if (doc->openFile("read"))
                    std::cout << "  >> Geöffnet (schreibgeschützt). MFS-Original unverändert.\n";
                else
                    std::cout << "  >> Öffnen fehlgeschlagen.\n";
            }
        }

        else if (ch == 11) {
            // Öffnen (Bearbeiten) — Snapshot + öffne Original
            if (doc->filePath.empty() || !Rosenholz::FileOps::fileExists(doc->filePath)) {
                std::cout << "  >> Keine Datei vorhanden.\n";
            } else {
                std::string note = readOpt("Änderungsnotiz (optional): ");
                std::string by   = readOpt("Geändert von Person-ID (leer=system): ");
                std::string oldVer = doc->version;
                // Bump minor version
                try {
                    size_t dot = doc->version.rfind('.');
                    if (dot != std::string::npos) {
                        int minor = std::stoi(doc->version.substr(dot+1)) + 1;
                        doc->version = doc->version.substr(0, dot+1) + std::to_string(minor);
                    }
                } catch(...) { doc->version += ".1"; }
                // Snapshot
                doc->snapshotVersion(note.empty() ? "Vor Bearbeitung" : note, by);
                doc->importLocalFile(doc->filePath); // update hash + size
                std::cout << "  Snapshot v" << oldVer << " gespeichert. Neue Version: "
                          << doc->version << "\n";
                if (doc->openFile("edit"))
                    std::cout << "  >> Datei geöffnet. Änderungen werden direkt im MFS gespeichert.\n";
                else
                    std::cout << "  >> Öffnen fehlgeschlagen.\n";
            }
        }

        else if (ch == 12) {
            auto versions = doc->loadVersions();
            hdr("VERSIONSVERLAUF — " + doc->documentId.substr(0,20));
            if (versions.empty()) {
                std::cout << "  (keine gespeicherten Versionen)\n\n";
            } else {
                std::cout << "  " << std::left << std::setw(8) << "Ver."
                          << std::setw(22) << "Datum"
                          << std::setw(14) << "Von"
                          << std::setw(10) << "Größe"
                          << "Notiz\n";
                std::cout << "  " << std::string(68,'-') << "\n";
                for (auto& v : versions) {
                    std::cout << "  " << std::left
                              << std::setw(8)  << v.versionNumber.substr(0,7)
                              << std::setw(22) << v.createdAt.substr(0,20)
                              << std::setw(14) << (v.createdBy.empty() ? "system" :
                                                   v.createdBy.substr(0,12))
                              << std::setw(10) << (std::to_string(v.fileSize/1024+1) + " KB")
                              << (v.changeNote.empty() ? "" : v.changeNote.substr(0,30))
                              << "\n";
                }
                std::cout << "\n";
            }
        }
    }
}

void documentBrowserMenu(const std::string& projectId,
                                const std::string& taskId) {
    while (true) {
        // Load docs for given context or all
        std::vector<std::shared_ptr<Rosenholz::Document>> docs;
        if (!projectId.empty())
            docs = Rosenholz::Document::loadForProject(projectId);
        else if (!taskId.empty())
            docs = Rosenholz::Document::loadForEntity("task", taskId);

        listDocuments(docs, projectId.empty() && taskId.empty()
            ? "ALL DOCUMENTS" : "DOCUMENTS");

        std::cout << "  Actions:\n"
                  << "    1. Open document by number\n"
                  << "    2. Create / register new document\n"
                  << "    3. Attach existing document by ID\n";
        if (!projectId.empty())
            std::cout << "    4. List all documents in system\n";
        std::cout << "    0. Back\n";

        int maxch = projectId.empty() ? 3 : 4;
        int ch = readInt("Choice", 0, maxch);
        if (ch == 0) break;

        else if (ch == 1) {
            if (docs.empty()) { std::cout << "  (nothing to open)\n"; continue; }
            int n = readInt("Document number", 1, (int)docs.size());
            documentMenu(docs[n-1]);
        }
        else if (ch == 2) {
            createDocumentWizard(projectId, taskId);
        }
        else if (ch == 3) {
            std::string did = readLine("Document ID to attach: ");
            auto doc = Rosenholz::Document::loadById(did);
            if (!doc) { std::cout << "  >> Not found.\n"; continue; }
            std::string et = !projectId.empty() ? "project"
                           : !taskId.empty()    ? "task" : readLine("Entity type: ");
            std::string ei = !projectId.empty() ? projectId
                           : !taskId.empty()    ? taskId  : readLine("Entity ID: ");
            doc->attachToEntity(et, ei);
            std::cout << "  >> Attached.\n";
        }
        else if (ch == 4) {
            // load everything from documents DB
            auto* db = Rosenholz::DatabasePool::instance().get("documents");
            if (db) {
                auto rows = db->query("SELECT * FROM documents ORDER BY date_created DESC;");
                std::vector<std::shared_ptr<Rosenholz::Document>> all;
                for (auto& r : rows) {
                    auto d = Rosenholz::Document::create("","","");
                    d->load(r.at("document_id"));
                    all.push_back(d);
                }
                listDocuments(all, "ALL DOCUMENTS IN SYSTEM");
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// DISPLAY HELPERS
// ─────────────────────────────────────────────────────────────

} // namespace CLI

// ============================================================
// WatchPoller.cpp -- Background polling for changes
//
// Two responsibilities:
//   1. F77-Task changes: notify user of new/completed tasks
//   2. MFS sync: rewrite MFS files for entities whose DB
//      updatedAt timestamp is newer than their MFS file mtime.
// ============================================================
#include "WatchPoller.h"
#include "Utils.h"
#include "Note.h"
#include "../workflow/F77Task.h"
#include "../workflow/F77Workflow.h"
#include "../model/f16/F16.h"
#include "../model/f22/F22.h"
#include "../model/f18/F18Operation.h"
#include "../model/akt/Folder.h"
#include "../model/akt/FolderRevision.h"
#include "../model/akt/FolderObject.h"
#include "../mfs/MFSWriter.h"
#include "../core/Config.h"
#include "../core/FileOps.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <csignal>
#include <atomic>
#include <map>
#include <sys/stat.h>

namespace Rosenholz {

static std::atomic<bool> s_watchStop{false};
static void watchSigint(int) { s_watchStop.store(true); }

// Returns file modification time as ISO string, or "" if file missing.
static std::string fileMtime(const std::string& path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) return "";
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&st.st_mtime));
    return buf;
}

// Check and rewrite MFS for entities whose DB record is newer than the MFS file.
// Returns number of files written.
// Returns true if a Folder has any FolderObjects currently checked out
// (actively being edited in MFS by a user -- must not be overwritten).
static bool folderHasActiveCheckout(const std::string& folderId) {
    try {
        // Check all revisions for checked-out objects:
        auto revs = FolderRevision::loadAllRevisions(folderId);
        for (auto& rev : revs) {
            if (rev->revState != RevState::IN_WORK) continue;
            auto objs = FolderObject::loadForRevision(folderId, rev->rev);
            for (auto& obj : objs) {
                if (obj->checkedOut) return true;
                // Also protect objects whose MFS file was modified after the DB record:
                if (!obj->filePath.empty()) {
                    std::string fmtime = fileMtime(obj->filePath);
                    if (!fmtime.empty() && fmtime > obj->updatedAt)
                        return true; // file newer than DB ? user may be editing it
                }
            }
        }
    } catch (...) {}
    return false;
}

static int syncMFS(const std::string& mfsRoot,
                   std::function<void(const std::string&)> onEvent) {
    if (mfsRoot.empty()) return 0;
    int written = 0;
    int skipped = 0;

    // F16 projects -- index card files (no binary content, safe to overwrite):
    for (auto& p : F16::loadAll()) {
        std::string sanitised = sanitiseRegNr(p->regNumber.toString());
        std::string mfsFile = FileOps::joinPath(
            FileOps::joinPath(mfsRoot, "F16"), sanitised + ".txt");
        std::string mtime = fileMtime(mfsFile);
        if (mtime.empty() || (!p->updatedAt.empty() && p->updatedAt > mtime)) {
            MFSWriter::writeProject(*p, mfsRoot);
            ++written;
            onEvent("MFS: F16/" + p->regNumber.toString() + " aktualisiert.");
        }
    }

    // F22 tasks -- index card files (safe to overwrite):
    for (auto& t : F22::loadRecent(200)) {
        std::string sanitised = sanitiseRegNr(t->regNumber.toString());
        std::string mfsFile = FileOps::joinPath(
            FileOps::joinPath(mfsRoot, "F22"),
            FileOps::joinPath(sanitised, "00_KARTE.txt"));
        std::string mtime = fileMtime(mfsFile);
        if (mtime.empty() || (!t->updatedAt.empty() && t->updatedAt > mtime)) {
            MFSWriter::writeTask(*t, mfsRoot);
            ++written;
            onEvent("MFS: F22/" + t->regNumber.toString() + " aktualisiert.");
        }
    }

    // F18 operations -- index card files (safe to overwrite):
    for (auto& v : F18Operation::loadRecent(200)) {
        std::string mfsDir = FileOps::joinPath(
            FileOps::joinPath(mfsRoot, "F18"), v->operationId);
        std::string mfsFile = FileOps::joinPath(mfsDir, "00_VORGANG.txt");
        std::string mtime = fileMtime(mfsFile);
        if (mtime.empty() || (!v->updatedAt.empty() && v->updatedAt > mtime)) {
            MFSWriter::writeF18(*v, mfsRoot);
            ++written;
            onEvent("MFS: F18/" + v->operationId + " aktualisiert.");
        }
    }

    // AKT (Folders) -- contain actual document objects that may be open/checked out.
    // PROTECTION: skip any Folder with active checkouts or user-modified files.
    for (auto& d : Folder::loadRecent(200)) {
        auto curRev = FolderRevision::currentRevision(d->folderId);
        if (!curRev) continue;

        // Only AKT index cards (00_SCHLUESSEL.txt) are rewritten here,
        // not the binary document objects. The binary objects are managed
        // by the user directly and never touched by syncMFS.
        std::string mfsDir = d->mfsDir();
        if (mfsDir.empty()) continue;

        std::string cardFile = FileOps::joinPath(mfsDir, "00_SCHLUESSEL.txt");
        std::string mtime = fileMtime(cardFile);

        if (mtime.empty() || (!d->updatedAt.empty() && d->updatedAt > mtime)) {
            // Check for active checkouts BEFORE writing:
            if (folderHasActiveCheckout(d->folderId)) {
                onEvent("MFS: WARN " + d->folderId + " \"" + d->title.substr(0,30) +
                        "\" -- ausgecheckt/bearbeitet, uebersprungen.");
                ++skipped;
                continue;
            }
            MFSWriter::writeDocument(*d, mfsRoot);
            ++written;
            onEvent("MFS: AKT/" + d->folderId + " aktualisiert.");
        }
    }

    if (skipped > 0)
        onEvent("WARNUNG: " + std::to_string(skipped) +
                " Akte(n) uebersprungen (aktive Bearbeitung). Bitte commit ausfuehren.");
    return written;
}

void WatchPoller::run(std::function<void(const std::string&)> onEvent,
                      int intervalSeconds)
{
    s_watchStop.store(false);
    signal(SIGINT, watchSigint);

    // Baseline: current open task count
    int prevOpen = (int)F77Task::loadOpen().size();
    const auto& cfg = Config::instance();

    onEvent("Watch gestartet (" + std::to_string(intervalSeconds) +
            "s Intervall). MFS-Sync: " +
            (cfg.mfs().enabled ? cfg.mfsPath() : "deaktiviert") +
            ". Ctrl+C zum Beenden.");

    while (!s_watchStop.load()) {
        // Sleep in small chunks to respond quickly to Ctrl+C:
        for (int i = 0; i < intervalSeconds * 10 && !s_watchStop.load(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (s_watchStop.load()) break;

        // 1. Check F77-Task changes
        int nowOpen = (int)F77Task::loadOpen().size();
        if (nowOpen != prevOpen) {
            std::ostringstream oss;
            int delta = nowOpen - prevOpen;
            if (delta > 0)
                oss << "+ " << delta << " neue F77-Aufgabe(n) -- " << nowOpen << " offen";
            else
                oss << "OK " << (-delta) << " Aufgabe(n) erledigt -- " << nowOpen << " offen";
            onEvent(oss.str());
            prevOpen = nowOpen;
        }

        // 2. MFS sync
        if (cfg.mfs().enabled && !cfg.mfsPath().empty()) {
            int nWritten = syncMFS(cfg.mfsPath(), onEvent);
            if (nWritten > 0)
                onEvent("MFS-Sync: " + std::to_string(nWritten) + " Datei(en) aktualisiert.");
        }
    }
    signal(SIGINT, SIG_DFL);
    onEvent("Watch beendet.");
}

// One-shot MFS sync (called manually or from AppController)
void WatchPoller::syncMFSNow(std::function<void(const std::string&)> onEvent) {
    const auto& cfg = Config::instance();
    if (!cfg.mfs().enabled || cfg.mfsPath().empty()) {
        onEvent("MFS nicht aktiviert.");
        return;
    }
    int n = syncMFS(cfg.mfsPath(), onEvent);
    onEvent("MFS-Sync abgeschlossen: " + std::to_string(n) + " Datei(en) aktualisiert.");
}

} // namespace Rosenholz

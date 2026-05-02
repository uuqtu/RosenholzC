// ============================================================
// tests/unit/akte/test_akte.cpp  —  AKT: Folder, FolderRevision,
//                                       FolderObject, ArchiveStore
//
// Coverage
// ════════
//   Folder
//     create / save (SQL row with /AKT/ ID)
//     loadById / loadForEntity (f22, f18, f18s)
//     orphan document (no task) still saveable
//     importLocalFile: populates filePath/fileSize/fileHash
//     revise(): creates revision, refuses second while in_work
//     ensureReleaseWorkflow: F77 WFI created
//     attachToEntity / loadForEntity polymorphic (entity_folders table)
//     update() persists title/docType changes
//
//   FolderRevision
//     createRevision: initial state in_work, rev=1, parentRev=0
//     currentRevision: returns newest non-superseded
//     loadByRev / loadAllRevisions
//     isTransitionAllowed: complete state-machine matrix
//     transitionState: persists to DB, terminal closure
//     superseded invariant: exactly one active revision at all times
//     recomputeSuperseded after multiple revisions and releases
//
//   ArchiveStore
//     stageContent → commitContent → retrieveContent
//     lookupRevChunk / chunkExists
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/akt/FolderRevision.h"
#include "../../../src/model/akt/ArchiveStore.h"
#include "../../../src/core/FileOps.h"
#include <filesystem>
#include <fstream>

using namespace Rosenholz;
using namespace RhTest;
namespace fs = std::filesystem;

// ── Folder: create / save ─────────────────────────────────────────────────────

TEST_CASE("AKT/Folder: create() and save() writes row with /AKT/ ID", "[akte][folder][sql]") {
    TempDB db("akt_create");
    auto proj = makeF16("AKT-F16");
    auto task = makeF22(proj->projectId, "AKT-F22");

    // Note: makeF22 may auto-create an Allgemeine Akte — don't assume count=0
    int beforeCount = rowCount("akt","folders");

    auto d = Folder::create("Projektcharter", "report", task->taskId);
    d->format  = "pdf";
    d->version = "1.0";
    REQUIRE(opOk(d->save()));

    SECTION("row in SQL") { CHECK(rowCount("akt","folders") == beforeCount + 1); }
    SECTION("ID contains /AKT/") {
        CHECK(d->folderId.find("/AKT/") != std::string::npos);
    }
    SECTION("SQL title column") {
        CHECK(colValue("akt","folders","title",
                       "folder_id='"+d->folderId+"'") == "Projektcharter");
    }
    SECTION("SQL doc_type column") {
        CHECK(colValue("akt","folders","doc_type",
                       "folder_id='"+d->folderId+"'") == "report");
    }
    SECTION("SQL task_id column") {
        CHECK(colValue("akt","folders","task_id",
                       "folder_id='"+d->folderId+"'") == task->taskId);
    }
}

TEST_CASE("AKT/Folder: orphan (no task) is still saveable", "[akte][folder][model]") {
    TempDB db("akt_orphan");
    auto d = Folder::create("Orphan", "misc");
    REQUIRE(opOk(d->save()));
    CHECK(rowCount("akt","folders") == 1);
}

TEST_CASE("AKT/Folder: loadById round-trip", "[akte][folder][model]") {
    TempDB db("akt_lbi");
    auto proj = makeF16("LBI-F16");
    auto task = makeF22(proj->projectId, "LBI-F22");
    auto d    = Folder::create("LBI-Doc", "spec", task->taskId);
    REQUIRE(opOk(d->save()));

    auto r = Folder::loadById(d->folderId);
    REQUIRE(r != nullptr);
    CHECK(r->folderId == d->folderId);
    CHECK(r->title    == "LBI-Doc");
    CHECK(r->taskId   == task->taskId);
}

TEST_CASE("AKT/Folder: update() persists title change", "[akte][folder][sql]") {
    TempDB db("akt_update");
    auto proj = makeF16("Upd-F16");
    auto task = makeF22(proj->projectId, "Upd-F22");
    auto d    = Folder::create("Original Title", "report", task->taskId);
    REQUIRE(opOk(d->save()));
    d->title = "Updated Title";
    REQUIRE(opOk(d->update()));
    CHECK(colValue("akt","folders","title","folder_id='"+d->folderId+"'") == "Updated Title");
}

// ── Folder: loadForEntity ─────────────────────────────────────────────────────

TEST_CASE("AKT/Folder: loadForEntity(f22) returns task-linked documents",
          "[akte][folder][query]") {
    TempDB db("akt_lfe_f22");
    auto proj = makeF16("LFE-F16");
    auto t1   = makeF22(proj->projectId, "T1");
    auto t2   = makeF22(proj->projectId, "T2");

    auto d1 = Folder::create("Doc-1", "report", t1->taskId); REQUIRE(opOk(d1->save()));
    auto d2 = Folder::create("Doc-2", "spec",   t1->taskId); REQUIRE(opOk(d2->save()));
    auto d3 = Folder::create("Doc-3", "misc",   t2->taskId); REQUIRE(opOk(d3->save()));

    // F22 auto-creates Allgemeine Akte — count may be > 2
    auto docs = Folder::loadForEntity("f22", t1->taskId);
    // At least our two explicitly created documents are present:
    CHECK(docs.size() >= 2);
    for (auto& d : docs) CHECK(d->taskId == t1->taskId);
}

TEST_CASE("AKT/Folder: loadForEntity via entity_folders (polymorphic attach)",
          "[akte][folder][query][sql]") {
    TempDB db("akt_lfe_poly");
    auto proj = makeF16("Poly-F16");
    auto task = makeF22(proj->projectId, "Poly-F22");
    auto op   = makeF18(task->taskId, "Poly-F18");

    // Create Akte attached via entity_folders to f18s entity:
    auto d = Folder::create("F18S-Akte", "general", task->taskId, "");
    REQUIRE(opOk(d->save()));
    std::string stepId = "XV/F18S/9999/26";
    REQUIRE(opOk(d->attachToEntity("f18s", stepId)));

    // Should be findable via loadForEntity("f18s", stepId):
    auto docs = Folder::loadForEntity("f18s", stepId);
    REQUIRE(docs.size() == 1);
    CHECK(docs[0]->folderId == d->folderId);

    // SQL: entity_folders row exists:
    CHECK(rowCount("akt","entity_folders",
                   "entity_type='f18s' AND entity_id='"+stepId+"'") == 1);
}

TEST_CASE("AKT/Folder: loadForEntity returns empty for unknown entity ID", "[akte][folder][query]") {
    TempDB db("akt_lfe_empty");
    // Use a fake ID that no entity matches:
    CHECK(Folder::loadForEntity("f22", "XV/F22/9999/99").empty());
}

// ── Folder: importLocalFile ───────────────────────────────────────────────────

TEST_CASE("AKT/Folder: importLocalFile populates filePath, fileSize, fileHash",
          "[akte][folder][import][filesystem]") {
    TempDB db("akt_import");
    auto proj = makeF16("Imp-F16");
    auto task = makeF22(proj->projectId, "Imp-F22");
    auto d    = Folder::create("Import-Doc", "report", task->taskId);
    d->version = "1.0";
    d->format  = "txt";
    REQUIRE(opOk(d->save()));

    std::string tmpPath = "/tmp/rh_import_" + d->folderId + ".txt";
    FileOps::writeTextFile(tmpPath, "Testinhalt Version 1.0\n");

    auto result = d->importLocalFile(tmpPath);
    CHECK(opOk(result));
    CHECK_FALSE(d->filePath.empty());
    CHECK(d->fileSize > 0);
    CHECK_FALSE(d->fileHash.empty());

    FileOps::deleteFile(tmpPath);
}

// ── Folder: revise() ─────────────────────────────────────────────────────────

TEST_CASE("AKT/Folder: revise() creates first revision", "[akte][folder][revision]") {
    TempDB db("akt_revise1");
    auto d = Folder::create("RevDoc", "report");
    REQUIRE(opOk(d->save()));

    auto rev = d->revise("Initiale Version", "test-user");
    REQUIRE(rev != nullptr);
    CHECK(rev->rev          == 1);
    CHECK(rev->revStateStr() == "in_work");
    CHECK(rev->parentRev    == 0);
    CHECK_FALSE(rev->superseded);
}

TEST_CASE("AKT/Folder: revise() is refused while rev is in_work", "[akte][folder][revision]") {
    TempDB db("akt_revise_block");
    auto d = Folder::create("BlockDoc", "report");
    REQUIRE(opOk(d->save()));
    auto rev1 = d->revise("First", "u1");
    REQUIRE(rev1 != nullptr);

    // Second revise while rev1 is still in_work must fail:
    auto rev1b = d->revise("Second attempt");
    CHECK(rev1b == nullptr);
}

TEST_CASE("AKT/Folder: revise() allowed after predecessor transitions to pre_released",
          "[akte][folder][revision]") {
    TempDB db("akt_revise2");
    auto d    = Folder::create("Rev2Doc", "report");
    REQUIRE(opOk(d->save()));
    auto rev1 = d->revise("Version 1", "u1");
    REQUIRE(rev1 != nullptr);

    REQUIRE(rev1->transitionState("pre_released"));

    auto rev2 = d->revise("Version 2");
    REQUIRE(rev2 != nullptr);
    CHECK(rev2->rev      == 2);
    CHECK(rev2->parentRev == 1);
}

TEST_CASE("AKT/Folder: loadVersions returns all revisions", "[akte][folder][revision]") {
    TempDB db("akt_versions");
    auto d    = Folder::create("VersionDoc", "report");
    REQUIRE(opOk(d->save()));
    auto rev1 = d->revise("V1", "u1");
    REQUIRE(rev1 != nullptr);
    REQUIRE(rev1->transitionState("pre_released"));
    auto rev2 = d->revise("V2");
    REQUIRE(rev2 != nullptr);

    auto versions = d->loadVersions();
    CHECK(versions.size() == 2);
}

// ── FolderRevision: initial state ────────────────────────────────────────────

TEST_CASE("AKT/FolderRevision: createRevision — in_work, rev=1, parentRev=0",
          "[akte][frevision][model]") {
    TempDB db("akt_frev_init");
    auto d = Folder::create("Init-Rev-Doc", "report");
    REQUIRE(opOk(d->save()));

    auto rev = FolderRevision::createRevision(d->folderId, 0, "tester", "init");
    REQUIRE(rev != nullptr);
    CHECK(rev->rev          == 1);
    CHECK(rev->revStateStr() == "in_work");
    CHECK(rev->parentRev    == 0);
    CHECK_FALSE(rev->superseded);
}

TEST_CASE("AKT/FolderRevision: currentRevision finds the active revision",
          "[akte][frevision][query]") {
    TempDB db("akt_frev_cur");
    auto d = Folder::create("Cur-Rev-Doc", "report");
    REQUIRE(opOk(d->save()));
    FolderRevision::createRevision(d->folderId, 0, "u1", "init");

    auto cur = FolderRevision::currentRevision(d->folderId);
    REQUIRE(cur != nullptr);
    CHECK(cur->rev        == 1);
    CHECK_FALSE(cur->superseded);
}

TEST_CASE("AKT/FolderRevision: loadByRev and loadAllRevisions", "[akte][frevision][query]") {
    TempDB db("akt_frev_load");
    auto d  = Folder::create("Load-Rev-Doc", "spec");
    REQUIRE(opOk(d->save()));
    auto r1 = FolderRevision::createRevision(d->folderId, 0, "u1", "rev1");
    auto r2 = FolderRevision::createRevision(d->folderId, 1, "u1", "rev2");
    REQUIRE(r1 != nullptr);
    REQUIRE(r2 != nullptr);

    auto byRev = FolderRevision::loadByRev(d->folderId, 1);
    REQUIRE(byRev != nullptr);
    CHECK(byRev->rev == 1);

    auto all = FolderRevision::loadAllRevisions(d->folderId);
    CHECK(all.size() == 2);
}

// ── FolderRevision: state machine ────────────────────────────────────────────

TEST_CASE("AKT/FolderRevision: isTransitionAllowed — complete matrix",
          "[akte][frevision][statemachine]") {
    TempDB db("akt_frev_sm_matrix");

    auto ok = [](const std::string& f, const std::string& t) {
        return FolderRevision::isTransitionAllowed(
            revStateFromString(f), revStateFromString(t), false);
    };

    // From in_work:
    CHECK( ok("in_work", "pre_released"));
    CHECK( ok("in_work", "locked"));
    CHECK( ok("in_work", "closed"));
    CHECK( ok("in_work", "released"));   // direct allowed

    // From pre_released:
    CHECK( ok("pre_released", "released"));
    CHECK(!ok("pre_released", "locked"));
    CHECK( ok("pre_released", "closed"));
    CHECK(!ok("pre_released", "in_work"));

    // From released:
    CHECK(!ok("released", "locked"));
    CHECK( ok("released", "closed"));
    CHECK(!ok("released", "in_work"));
    CHECK(!ok("released", "pre_released"));

    // From locked:
    CHECK( ok("locked", "pre_released"));
    CHECK( ok("locked", "closed"));
    CHECK( ok("locked", "released"));

    // From closed — terminal:
    CHECK(!ok("closed", "in_work"));
    CHECK(!ok("closed", "released"));
    CHECK(!ok("closed", "locked"));
    CHECK(!ok("closed", "pre_released"));
}

TEST_CASE("AKT/FolderRevision: transitionState persists each step to SQL",
          "[akte][frevision][statemachine][sql]") {
    TempDB db("akt_frev_trans");
    auto d   = Folder::create("Trans-Doc", "report");
    REQUIRE(opOk(d->save()));
    auto rev = FolderRevision::createRevision(d->folderId, 0, "u1", "v1");
    REQUIRE(rev != nullptr);

    // in_work → pre_released
    REQUIRE(rev->transitionState("pre_released"));
    auto r2 = FolderRevision::loadByRev(d->folderId, 1);
    REQUIRE(r2 != nullptr);
    CHECK(r2->revStateStr() == "pre_released");

    // pre_released → released
    REQUIRE(rev->transitionState("released"));
    auto r3 = FolderRevision::loadByRev(d->folderId, 1);
    REQUIRE(r3 != nullptr);
    CHECK(r3->revStateStr() == "released");

    // released → closed
    REQUIRE(rev->transitionState("closed"));

    // closed → anything blocked (terminal):
    CHECK_FALSE(rev->transitionState("in_work"));
    CHECK_FALSE(rev->transitionState("released"));
}

TEST_CASE("AKT/FolderRevision: closed is terminal — no further transitions",
          "[akte][frevision][statemachine]") {
    TempDB db("akt_frev_terminal");
    auto d   = Folder::create("Term-Doc", "report");
    REQUIRE(opOk(d->save()));
    auto rev = FolderRevision::createRevision(d->folderId, 0, "u1", "v1");
    REQUIRE(rev != nullptr);
    REQUIRE(rev->transitionState("closed"));

    CHECK_FALSE(rev->transitionState("in_work"));
    CHECK_FALSE(rev->transitionState("pre_released"));
    CHECK_FALSE(rev->transitionState("released"));
    CHECK_FALSE(rev->transitionState("locked"));
}

// ── FolderRevision: superseded invariant ─────────────────────────────────────

TEST_CASE("AKT/FolderRevision: exactly one active (non-superseded) revision at all times",
          "[akte][frevision][superseded]") {
    TempDB db("akt_frev_super");
    auto d  = Folder::create("Super-Doc", "spec");
    REQUIRE(opOk(d->save()));
    auto r1 = FolderRevision::createRevision(d->folderId, 0, "u1", "rev1");
    auto r2 = FolderRevision::createRevision(d->folderId, 1, "u1", "rev2");
    REQUIRE(r1 != nullptr);
    REQUIRE(r2 != nullptr);

    auto all = FolderRevision::loadAllRevisions(d->folderId);
    REQUIRE(all.size() == 2);
    int active = 0;
    for (auto& r : all) if (!r->superseded) active++;
    CHECK(active == 1);
}

TEST_CASE("AKT/FolderRevision: after releasing rev1, currentRevision switches to rev1",
          "[akte][frevision][superseded]") {
    TempDB db("akt_frev_release_priority");
    auto d  = Folder::create("RelPri-Doc", "spec");
    REQUIRE(opOk(d->save()));
    auto r1 = FolderRevision::createRevision(d->folderId, 0, "u1", "rev1");
    auto r2 = FolderRevision::createRevision(d->folderId, 1, "u1", "rev2");
    REQUIRE(r1 != nullptr);
    REQUIRE(r2 != nullptr);

    // Release rev1 — released status takes priority:
    r1->transitionState("pre_released");
    r1->transitionState("released");

    auto cur = FolderRevision::currentRevision(d->folderId);
    REQUIRE(cur != nullptr);
    CHECK(cur->rev == 1);

    // Superseded invariant still holds:
    auto all = FolderRevision::loadAllRevisions(d->folderId);
    int active = 0;
    for (auto& r : all) if (!r->superseded) active++;
    CHECK(active == 1);
}

// ── Folder: ensureReleaseWorkflow ─────────────────────────────────────────────

TEST_CASE("AKT/Folder: ensureReleaseWorkflow creates F77 WFI", "[akte][folder][lifecycle]") {
    TempDB db("akt_wfi");
    auto proj = makeF16("WFI-F16");
    auto task = makeF22(proj->projectId, "WFI-F22");
    auto d    = Folder::create("WFI-Doc", "spec", task->taskId);
    REQUIRE(opOk(d->save()));
    d->revise("Revision 1 — Test");
    d->ensureReleaseWorkflow();

    auto fresh = Folder::loadById(d->folderId);
    REQUIRE(fresh != nullptr);
    CHECK_FALSE(fresh->workflowId.empty());
}

TEST_CASE("AKT/Folder: revise + ensureReleaseWorkflow is idempotent",
          "[akte][folder][lifecycle]") {
    TempDB db("akt_wfi_idem");
    auto d = Folder::create("Idem-Doc", "report");
    REQUIRE(opOk(d->save()));
    d->revise("Revision 1 — Test");
    d->ensureReleaseWorkflow();
    d->revise("Revision 1 — Test");  // second call with same label
    d->ensureReleaseWorkflow();

    auto all = FolderRevision::loadAllRevisions(d->folderId);
    CHECK(all.size() == 1);   // must not duplicate
}

// ── ArchiveStore ──────────────────────────────────────────────────────────────

TEST_CASE("AKT/ArchiveStore: stage / commit / retrieve / lookup",
          "[akte][archive][filesystem]") {
    TempDB db("akt_archive");
    auto& store = Archive::ArchiveStore::instance();
    if (!store.isOpen()) {
        SUCCEED("ArchiveStore not open in test env — skipping");
        return;
    }

    std::string content = "Hello Rosenholz ArchiveStore Test 12345";
    std::string tmpSrc  = "/tmp/rh_arc_" + std::to_string(std::time(nullptr)) + ".txt";
    { std::ofstream f(tmpSrc); f << content; }

    // Stage:
    std::string stagePath;
    auto ref = store.stageContent(tmpSrc, stagePath);
    REQUIRE(ref.valid());
    CHECK_FALSE(ref.sha256.empty());
    CHECK(ref.size > 0);
    CHECK_FALSE(stagePath.empty());

    // Commit:
    std::string docId = "XV/AKT/8888/26";
    REQUIRE(store.commitContent(stagePath, ref, docId, 1));

    // Retrieve:
    std::string retrievePath = "/tmp/rh_arc_retrieve.txt";
    REQUIRE(store.retrieveContent(docId, 1, retrievePath));
    std::ifstream fin(retrievePath);
    std::string retrieved((std::istreambuf_iterator<char>(fin)), {});
    CHECK(retrieved.find("Hello Rosenholz") != std::string::npos);

    // Lookup:
    CHECK(store.lookupRevChunk(docId, 1) == ref.sha256);
    CHECK(store.chunkExists(ref.sha256));

    std::remove(tmpSrc.c_str());
    std::remove(retrievePath.c_str());
}

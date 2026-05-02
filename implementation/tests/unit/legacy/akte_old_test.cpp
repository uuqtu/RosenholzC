// ============================================================
// tests/unit/legacy/akte_old_test.cpp
//
// Translated from tests/test_model.cpp  (Document / FolderRevision /
// ArchiveStore sections) to Catch2 v3.
// Tests: create/save, importLocalFile, revise(), revision state-machine,
// superseded invariant, ArchiveStore (stage/commit/retrieve/lookup).
// ============================================================
#include "../test_helpers.h"
#include "../../../src/model/akt/FolderRevision.h"
#include "../../../src/model/akt/ArchiveStore.h"
#include "../../../src/core/FileOps.h"
#include <filesystem>
#include <fstream>

using namespace Rosenholz;
using namespace RhTest;

// ── Document create/save ─────────────────────────────────────

TEST_CASE("Legacy/AKT: Folder create, save, loadForEntity", "[legacy][akte][model]") {
    TempDB db("leg_akt_create");
    auto proj = makeF16("AKT-F16");
    auto task = makeF22(proj->projectId, "AKT-F22");

    auto d = Folder::create("Projektcharter", "report", task->taskId);
    d->format  = "pdf";
    d->version = "1.0";
    REQUIRE(opOk(d->save()));

    SECTION("ID contains /AKT/") {
        CHECK(d->folderId.find("/AKT/") != std::string::npos);
    }
    SECTION("loadForEntity(f22) finds it") {
        auto docs = Folder::loadForEntity("f22", task->taskId);
        CHECK_FALSE(docs.empty());
    }
    SECTION("orphan document (no task) is still saveable") {
        auto orphan = Folder::create("Orphan", "misc");
        CHECK(opOk(orphan->save()));
    }
}

// ── importLocalFile ───────────────────────────────────────────

TEST_CASE("Legacy/AKT: importLocalFile populates filePath/fileSize/fileHash",
          "[legacy][akte][import]") {
    TempDB db("leg_akt_import");
    auto proj = makeF16("Import-F16");
    auto task = makeF22(proj->projectId, "Import-F22");
    auto doc  = Folder::create("Import-Doc", "report", task->taskId);
    doc->version = "1.0";
    doc->format  = "txt";
    REQUIRE(opOk(doc->save()));

    std::string tmpPath = "/tmp/rh_doc_import_" + doc->folderId + ".txt";
    FileOps::writeTextFile(tmpPath, "Testinhalt Version 1.0\n");

    auto result = doc->importLocalFile(tmpPath);
    CHECK(opOk(result));
    CHECK_FALSE(doc->filePath.empty());
    CHECK(doc->fileSize > 0);
    CHECK_FALSE(doc->fileHash.empty());
    FileOps::deleteFile(tmpPath);
}

// ── revise() lifecycle ────────────────────────────────────────

TEST_CASE("Legacy/AKT: revise() and revision sequencing", "[legacy][akte][revision]") {
    TempDB db("leg_akt_revise");
    auto doc = Folder::create("RevDoc", "report");
    REQUIRE(opOk(doc->save()));

    auto rev1 = doc->revise("Initiale Version", "system-test");
    REQUIRE(rev1 != nullptr);

    SECTION("rev1 refused while in_work") {
        auto rev1b = doc->revise("Versuch 2 während in_work");
        CHECK(rev1b == nullptr);
    }
    SECTION("rev2 allowed after pre_released") {
        REQUIRE(rev1->transitionState("pre_released"));
        auto rev2 = doc->revise("Aktualisierung");
        REQUIRE(rev2 != nullptr);
        CHECK(rev2->rev     == 2);
        CHECK(rev2->parentRev == 1);
    }
    SECTION("loadVersions non-empty") {
        CHECK_FALSE(doc->loadVersions().empty());
    }
}

// ── FolderRevision initial state ─────────────────────────────

TEST_CASE("Legacy/AKT: FolderRevision initial state is in_work, rev=1",
          "[legacy][akte][frevision]") {
    TempDB db("leg_akt_rev_init");
    auto doc = Folder::create("RevInit-Doc", "report");
    REQUIRE(opOk(doc->save()));
    auto rev = FolderRevision::createRevision(doc->folderId, 0, "test-user", "init");
    REQUIRE(rev != nullptr);
    CHECK(rev->rev         == 1);
    CHECK(rev->revStateStr() == "in_work");
    CHECK(rev->parentRev   == 0);
    CHECK_FALSE(rev->superseded);
}

TEST_CASE("Legacy/AKT: currentRevision finds initial rev", "[legacy][akte][frevision]") {
    TempDB db("leg_akt_cur_rev");
    auto doc = Folder::create("CurRev-Doc", "report");
    REQUIRE(opOk(doc->save()));
    FolderRevision::createRevision(doc->folderId, 0, "u1", "init");
    auto cur = FolderRevision::currentRevision(doc->folderId);
    REQUIRE(cur != nullptr);
    CHECK(cur->rev        == 1);
    CHECK_FALSE(cur->superseded);
}

// ── FolderRevision state machine ─────────────────────────────

TEST_CASE("Legacy/AKT: FolderRevision allowed transitions", "[legacy][akte][statemachine]") {
    TempDB db("leg_akt_sm_allowed");
    auto ok = [](const std::string& f, const std::string& t) {
        return FolderRevision::isTransitionAllowed(
            revStateFromString(f), revStateFromString(t), false);
    };
    // in_work
    CHECK( ok("in_work","pre_released"));
    CHECK( ok("in_work","locked"));
    CHECK( ok("in_work","closed"));
    CHECK( ok("in_work","released"));
    // pre_released
    CHECK( ok("pre_released","released"));
    CHECK(!ok("pre_released","locked"));
    CHECK( ok("pre_released","closed"));
    CHECK(!ok("pre_released","in_work"));
    // released
    CHECK(!ok("released","locked"));
    CHECK( ok("released","closed"));
    CHECK(!ok("released","in_work"));
    CHECK(!ok("released","pre_released"));
    // locked
    CHECK( ok("locked","pre_released"));
    CHECK( ok("locked","closed"));
    CHECK( ok("locked","released"));
    // closed — terminal
    CHECK(!ok("closed","in_work"));
    CHECK(!ok("closed","released"));
    CHECK(!ok("closed","locked"));
}

TEST_CASE("Legacy/AKT: FolderRevision transitionState persists", "[legacy][akte][statemachine]") {
    TempDB db("leg_akt_sm_trans");
    auto doc = Folder::create("Trans-Doc", "report");
    REQUIRE(opOk(doc->save()));
    auto rev = FolderRevision::createRevision(doc->folderId, 0, "u1", "v1");
    REQUIRE(rev != nullptr);

    REQUIRE(rev->transitionState("pre_released"));
    auto r2 = FolderRevision::loadByRev(doc->folderId, 1);
    REQUIRE(r2 != nullptr);
    CHECK(r2->revStateStr() == "pre_released");

    REQUIRE(rev->transitionState("released"));
    auto r3 = FolderRevision::loadByRev(doc->folderId, 1);
    REQUIRE(r3 != nullptr);
    CHECK(r3->revStateStr() == "released");

    REQUIRE(rev->transitionState("closed"));
    CHECK_FALSE(rev->transitionState("in_work"));  // terminal
}

// ── Superseded invariant ──────────────────────────────────────

TEST_CASE("Legacy/AKT: exactly one active revision at all times", "[legacy][akte][superseded]") {
    TempDB db("leg_akt_superseded");
    auto doc = Folder::create("Multi-Rev-Doc", "spec");
    REQUIRE(opOk(doc->save()));

    auto r1 = FolderRevision::createRevision(doc->folderId, 0, "u1", "rev 1");
    REQUIRE(r1 != nullptr);
    auto r2 = FolderRevision::createRevision(doc->folderId, 1, "u1", "rev 2");
    REQUIRE(r2 != nullptr);
    CHECK(r2->rev      == 2);
    CHECK(r2->parentRev == 1);

    auto all = FolderRevision::loadAllRevisions(doc->folderId);
    REQUIRE(all.size() == 2);
    int active = 0;
    for (auto& r : all) if (!r->superseded) active++;
    CHECK(active == 1);

    // After releasing rev1, currentRevision should be rev1 (released wins)
    r1->transitionState("pre_released");
    r1->transitionState("released");
    auto cur = FolderRevision::currentRevision(doc->folderId);
    REQUIRE(cur != nullptr);
    CHECK(cur->rev == 1);

    // Still exactly one active:
    auto all2 = FolderRevision::loadAllRevisions(doc->folderId);
    int active2 = 0;
    for (auto& r : all2) if (!r->superseded) active2++;
    CHECK(active2 == 1);
}

// ── ArchiveStore ──────────────────────────────────────────────

TEST_CASE("Legacy/AKT: ArchiveStore stage/commit/retrieve/lookup", "[legacy][akte][archive]") {
    TempDB db("leg_akt_archive");
    auto& store = Archive::ArchiveStore::instance();
    if (!store.isOpen()) {
        SUCCEED("ArchiveStore not open in test env — skipping");
        return;
    }

    std::string tmpSrc = "/tmp/rh_arc_test_" +
                         std::to_string(std::time(nullptr)) + ".txt";
    {
        std::ofstream f(tmpSrc);
        f << "Hello Rosenholz Archive Test Content 12345";
    }

    std::string stagePath;
    auto ref = store.stageContent(tmpSrc, stagePath);
    CHECK(ref.valid());
    CHECK_FALSE(ref.sha256.empty());
    CHECK(ref.size > 0);
    CHECK_FALSE(stagePath.empty());

    std::string testDocId = "XV/AKT/9999/2026";
    REQUIRE(store.commitContent(stagePath, ref, testDocId, 1));

    std::string retrievePath = "/tmp/rh_arc_retrieve_test.txt";
    REQUIRE(store.retrieveContent(testDocId, 1, retrievePath));

    std::ifstream fin(retrievePath);
    std::string content((std::istreambuf_iterator<char>(fin)), {});
    CHECK(content.find("Hello Rosenholz") != std::string::npos);

    CHECK(store.lookupRevChunk(testDocId, 1) == ref.sha256);
    CHECK(store.chunkExists(ref.sha256));

    std::remove(tmpSrc.c_str());
    std::remove(retrievePath.c_str());
}

// ── ensureReleaseWorkflow + autoCreate ───────────────────────

TEST_CASE("Legacy/AKT: Folder revise + ensureReleaseWorkflow creates WFI",
          "[legacy][akte][workflow]") {
    TempDB db("leg_akt_wfi");
    auto proj = makeF16("WFI-F16");
    auto task = makeF22(proj->projectId, "WFI-F22");
    auto doc  = Folder::create("WFI-Doc", "spec", task->taskId);
    REQUIRE(opOk(doc->save()));
    doc->revise("Revision 1 — Test");
    doc->ensureReleaseWorkflow();

    auto fresh = Folder::loadById(doc->folderId);
    REQUIRE(fresh != nullptr);
    CHECK_FALSE(fresh->workflowId.empty());

    // ensureRevision1 idempotent:
    doc->revise("Revision 1 — Test");
    auto all = FolderRevision::loadAllRevisions(doc->folderId);
    CHECK(all.size() == 1);
}

TEST_CASE("Legacy/AKT: 5-state machine through all valid transitions", "[legacy][akte][statemachine]") {
    TempDB db("leg_akt_5state");
    auto doc = Folder::create("5state-Doc", "report");
    REQUIRE(opOk(doc->save()));
    doc->revise("Revision 1 — Test");
    auto rev = FolderRevision::currentRevision(doc->folderId);
    REQUIRE(rev != nullptr);
    CHECK(rev->transitionState("pre_released"));
    CHECK(rev->revStateStr() == "pre_released");
    CHECK(rev->transitionState("released"));
    CHECK(rev->revStateStr() == "released");
    CHECK(rev->transitionState("closed"));
    CHECK(rev->revStateStr() == "closed");
    CHECK_FALSE(rev->transitionState("in_work"));   // terminal
    CHECK_FALSE(rev->transitionState("released"));  // terminal
}

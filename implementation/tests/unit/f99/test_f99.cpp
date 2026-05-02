// test_f99.cpp  —  F99 Notizen unit tests
#include "../test_helpers.h"

using namespace Rosenholz;
using namespace RhTest;

TEST_CASE("F99: create and load", "[f99][note]") {
    TempDB db("f99");
    auto p = makeF16();
    auto t = makeF22(p->projectId);

    SECTION("basic creation") {
        auto n = Note::create("f22", t->taskId, "Test-Notiz");
        REQUIRE(n != nullptr);
        CHECK(n->entityType == "f22");
        CHECK(n->entityId   == t->taskId);
        CHECK(n->body       == "Test-Notiz");
        CHECK(!n->noteId.empty());
    }

    SECTION("ID contains F99") {
        auto n = Note::create("f22", t->taskId, "ID-Test");
        REQUIRE(n != nullptr);
        CHECK(n->noteId.find("F99") != std::string::npos);
    }

    SECTION("ID uses 6-digit sequence") {
        auto n = Note::create("f22", t->taskId, "Seq-Test");
        REQUIRE(n != nullptr);
        // Format: XV/F99/000001/YY
        auto pos = n->noteId.find("F99/");
        REQUIRE(pos != std::string::npos);
        std::string seq = n->noteId.substr(pos + 4, 6);
        INFO("seq: " << seq);
        CHECK(seq.size() == 6);
        for (char c : seq) CHECK(std::isdigit(c));
        CHECK(seq == "000001");
    }

    SECTION("sequence increments") {
        auto n1 = Note::create("f22", t->taskId, "A");
        auto n2 = Note::create("f22", t->taskId, "B");
        REQUIRE(n1 != nullptr); REQUIRE(n2 != nullptr);
        CHECK(n1->noteId != n2->noteId);
    }
}

TEST_CASE("F99: loadById round-trip", "[f99][note]") {
    TempDB db("f99_load");
    auto p = makeF16();
    auto n = Note::create("f16", p->projectId, "Round-trip");
    REQUIRE(n != nullptr);
    auto loaded = Note::loadById(n->noteId);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->noteId     == n->noteId);
    CHECK(loaded->entityType == "f16");
    CHECK(loaded->body       == "Round-trip");
}

TEST_CASE("F99: loadForEntity", "[f99][note]") {
    TempDB db("f99_entity");
    auto p = makeF16();
    auto t = makeF22(p->projectId);
    Note::create("f22", t->taskId, "A");
    Note::create("f22", t->taskId, "B");
    Note::create("f22", t->taskId, "C");

    auto notes = Note::loadForEntity("f22", t->taskId);
    CHECK(notes.size() == 3);
}

TEST_CASE("F99: search", "[f99][note][wildcard]") {
    TempDB db("f99_srch");
    auto p = makeF16();
    auto t = makeF22(p->projectId);
    Note::create("f22", t->taskId, "Wolfram Studie");
    Note::create("f22", t->taskId, "Wolf am Abend");
    Note::create("f22", t->taskId, "Ganz andere Notiz");

    auto hits = Note::search("Wolf");
    CHECK(hits.size() == 2);

    auto none = Note::search("Nichtvorhanden");
    CHECK(none.empty());
}

TEST_CASE("F99: update()", "[f99][note]") {
    TempDB db("f99_upd");
    auto p = makeF16();
    auto n = Note::create("f16", p->projectId, "Original");
    REQUIRE(n != nullptr);
    n->body = "Updated";
    REQUIRE(opOk(n->update()));
    auto r = Note::loadById(n->noteId);
    REQUIRE(r != nullptr);
    CHECK(r->body == "Updated");
}

TEST_CASE("F99: remove()", "[f99][note]") {
    TempDB db("f99_del");
    auto p = makeF16();
    auto n = Note::create("f16", p->projectId, "Delete-me");
    REQUIRE(n != nullptr);
    auto id = n->noteId;
    REQUIRE(opOk(n->remove()));
    CHECK(Note::loadById(id) == nullptr);
}

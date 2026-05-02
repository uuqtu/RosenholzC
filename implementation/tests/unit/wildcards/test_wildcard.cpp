// test_wildcard.cpp  —  matchesPattern and patternToSQLLike
#include "../test_helpers.h"
#include "../../../src/model/Utils.h"

using Rosenholz::matchesPattern;
using Rosenholz::patternToSQLLike;

TEST_CASE("matchesPattern: no wildcard → substring", "[wildcard]") {
    CHECK( matchesPattern("Test", "est"));
    CHECK( matchesPattern("Test", "Test"));
    CHECK( matchesPattern("Test", "t"));       // case-insensitive
    CHECK(!matchesPattern("Test", "xyz"));
    CHECK(!matchesPattern("Test", "Tests"));
}

TEST_CASE("matchesPattern: * = any number of chars", "[wildcard]") {
    CHECK( matchesPattern("Test",    "T*"));
    CHECK( matchesPattern("Test",    "*t"));
    CHECK( matchesPattern("Test",    "*"));
    CHECK( matchesPattern("",        "*"));
    CHECK( matchesPattern("Test",    "T*t"));
    CHECK( matchesPattern("TestABC", "T*C"));
    CHECK(!matchesPattern("Test",    "Ts*t"));  // spec example
    CHECK(!matchesPattern("Test",    "X*"));
    CHECK( matchesPattern("abcdef",  "a*c*f"));
    CHECK(!matchesPattern("abcdef",  "a*z*f"));
    CHECK( matchesPattern("Test",    "T*est")); // * matches ""
    CHECK( matchesPattern("Test",    "Test*")); // trailing * matches ""
}

TEST_CASE("matchesPattern: % = exactly one char", "[wildcard]") {
    CHECK( matchesPattern("Test",  "T%st"));   // % = e
    CHECK( matchesPattern("Test",  "Te%t"));   // % = s
    CHECK( matchesPattern("Test",  "T%%t"));   // %% = es
    // Spec examples:
    CHECK(!matchesPattern("Test",  "T%t"));    // T + 1char + t ≠ Test (4 chars)
    CHECK(!matchesPattern("Test",  "T%"));     // T + 1char — Test has 3 after T
    CHECK( matchesPattern("Tx",    "T%"));     // T + exactly 1 char ✓
    CHECK( matchesPattern("Test",  "T%%%"));   // T + 3 chars ✓
    CHECK(!matchesPattern("Test",  "T%%%%"));  // T + 4 chars — only 3 after T
    // Case-insensitive:
    CHECK( matchesPattern("test",  "T%st"));
    CHECK( matchesPattern("TEST",  "t%st"));
}

TEST_CASE("matchesPattern: combined * and %", "[wildcard]") {
    CHECK( matchesPattern("TestABC",  "T*A%C"));    // * = est, % = B
    CHECK(!matchesPattern("TestABBC", "T*A%C"));    // % must be exactly 1
    CHECK( matchesPattern("abcXYZ",   "a*%Z"));     // * = bcX, % = Y
}

TEST_CASE("patternToSQLLike: converts to SQL LIKE pattern", "[wildcard]") {
    // No wildcards → wrap with %...%
    CHECK(patternToSQLLike("test")   == "%test%");
    CHECK(patternToSQLLike("Hello")  == "%Hello%");
    // _ without wildcards → substring (% wrap):
    CHECK(patternToSQLLike("T_st")   == "%T_st%");
    // * → %:
    CHECK(patternToSQLLike("T*")     == "T%");
    CHECK(patternToSQLLike("*est")   == "%est");
    CHECK(patternToSQLLike("T*t")    == "T%t");
    // % → _:
    CHECK(patternToSQLLike("T%st")   == "T_st");
    CHECK(patternToSQLLike("T%%t")   == "T__t");
    // Combined:
    CHECK(patternToSQLLike("T*A%C")  == "T%A_C");
}

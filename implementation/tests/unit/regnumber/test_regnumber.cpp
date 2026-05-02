// test_regnumber.cpp  —  RegNumber unit tests
#include "../test_helpers.h"
#include "../../../src/core/RegNumber.h"

using namespace Rosenholz;
using namespace RhTest;

TEST_CASE("RegNumber: format XV/F16/0001/YY", "[regnumber]") {
    TempDB db("rn");
    auto rn = RegNumberGenerator::next("F16");
    std::string s = rn.toString();
    CHECK(s.substr(0,3) == "XV/");
    CHECK(s.substr(3,3) == "F16");
    CHECK(s[6]          == '/');
    CHECK(s.substr(7,4) == "0001");
    CHECK(s[11]         == '/');
}

TEST_CASE("RegNumber: sequence increments per dept", "[regnumber]") {
    TempDB db("rn_seq");
    auto r1 = RegNumberGenerator::next("F22");
    auto r2 = RegNumberGenerator::next("F22");
    CHECK(r2.sequence == r1.sequence + 1);
}

TEST_CASE("RegNumber: departments independent", "[regnumber]") {
    TempDB db("rn_dept");
    auto f16 = RegNumberGenerator::next("F16");
    auto f22 = RegNumberGenerator::next("F22");
    CHECK(f16.sequence == 1);
    CHECK(f22.sequence == 1);
}

TEST_CASE("RegNumber: fromString round-trip", "[regnumber]") {
    TempDB db("rn_rt");
    auto rn  = RegNumberGenerator::next("F18");
    auto rn2 = RegNumber::fromString(rn.toString());
    CHECK(rn2.dept     == rn.dept);
    CHECK(rn2.sequence == rn.sequence);
    CHECK(rn2.year     == rn.year);
}

TEST_CASE("RegNumber: F99 generator works", "[regnumber][f99]") {
    TempDB db("rn_f99");
    auto rn = RegNumberGenerator::next("F99");
    CHECK(rn.dept     == "F99");
    CHECK(rn.sequence == 1);
}

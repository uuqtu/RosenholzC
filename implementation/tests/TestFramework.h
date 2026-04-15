#pragma once
// ============================================================
// TestFramework.h  —  Minimal self-contained test runner
// ============================================================
#include <string>
#include <iostream>
#include <functional>

namespace Rosenholz {

class TestFramework {
public:
    static TestFramework& instance() {
        static TestFramework tf;
        return tf;
    }

    void check(bool condition, const std::string& desc) {
        if (condition) {
            ++pass_;
            std::cout << "  PASS: " << desc << "\n";
        } else {
            ++fail_;
            std::cout << "  FAIL: " << desc << "  [" << section_ << "]\n";
        }
    }

    void fail(const std::string& desc) { check(false, desc); }

    void section(const std::string& name) {
        section_ = name;
        std::cout << "\n=== " << name << " ===\n";
    }

    void summary() const {
        std::cout << "\n=== Results: PASS=" << pass_
                  << "  FAIL=" << fail_ << " ===\n";
    }

    int passed() const { return pass_; }
    int failed() const { return fail_; }

private:
    int pass_{0}, fail_{0};
    std::string section_;
};

// Convenience macros for use in test files
#define TF  Rosenholz::TestFramework::instance()
#define CHECK(cond, desc)  TF.check((cond), (desc))
#define SECTION(name)      TF.section(name)

} // namespace

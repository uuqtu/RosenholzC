// ============================================================
// main_test.cpp  —  rosenholz_test binary entry point
// Runs all test suites using self-contained fixtures.
// ============================================================
#include "TestFramework.h"
#include "../src/core/Config.h"
#include "../src/core/FileOps.h"
using namespace Rosenholz;
#include "../src/app/AppController.h"
#include "../src/core/Migration.h"
#include "../src/workflow/F77Workflow.h"
#include "../src/workflow/F77Workflow.h"

// Forward declarations of all test suites
void testSuiteCore();
void testSuiteModel();
void testSuiteWorkflow();
void testSuiteMFS();
void testSuiteReporting();
void testSuiteMigration();

int main(int argc, char* argv[]) {
    using namespace Rosenholz;

    // Setup
    // Each run starts from a clean DB directory
    const std::string basePath = "/tmp/rosenholz_test_run";
    auto resetDB = [&]() {
        DatabasePool::instance().closeAll();
        FileOps::deleteDir(basePath);
        Config::instance().setBasePath(basePath);
        auto& cfg = Config::instance();
        if (!DatabasePool::instance().initAll(cfg.basePath(),
                cfg.db().walMode, cfg.db().cacheSize)) {
            std::cerr << "FATAL: DB reinit failed\n";
            std::exit(2);
        }
        MigrationEngine::runAll();
        F77_Engine::seedDefaultTemplates();
    };

    // Initial setup: wipe and set test dir, then init
    FileOps::deleteDir(basePath);

    auto& app = AppController::instance();
    if (!app.init("settings.json", "", AppMode::CLI)) {
        std::cerr << "FATAL: AppController::init() failed\n";
        return 1;
    }
    // Override basePath to our isolated test directory (AFTER init reads settings.json)
    Config::instance().setBasePath(basePath);
    DatabasePool::instance().closeAll();
    // Re-init databases at the test path
    {
        auto& cfg = Config::instance();
        if (!DatabasePool::instance().initAll(cfg.basePath(), cfg.db().walMode, cfg.db().cacheSize)) {
            std::cerr << "FATAL: DB init at test path failed\n";
            return 1;
        }
    }
    Logger::instance().setLevel(LogLevel::WARN);
    MigrationEngine::runAll();
    F77_Engine::seedDefaultTemplates();

    std::cout << "\n=== ROSENHOLZ PM TEST SUITE v" << Version::toString() << " ===\n\n";

    try {
        testSuiteCore();
        resetDB();          // fresh DB between suites
        testSuiteModel();
        resetDB();
        testSuiteWorkflow();
        resetDB();
        testSuiteMFS();
        resetDB();
        testSuiteReporting();
        resetDB();
        testSuiteMigration();
    } catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << "\n";
        TestFramework::instance().fail("Uncaught exception: " + std::string(e.what()));
    }

    TestFramework::instance().summary();
    app.shutdown();
    return TestFramework::instance().failed() > 0 ? 1 : 0;
}

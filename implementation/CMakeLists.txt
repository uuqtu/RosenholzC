cmake_minimum_required(VERSION 3.16)
project(Rosenholz VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find dependencies ─────────────────────────────────────────
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 sqlite3)

# ── LMDB (file-content archive) ───────────────────────────
find_path(LMDB_INCLUDE_DIR lmdb.h
    HINTS /usr/include /usr/local/include)
find_library(LMDB_LIBRARY NAMES lmdb
    HINTS /usr/lib /usr/local/lib)
if(LMDB_INCLUDE_DIR AND LMDB_LIBRARY)
    message(STATUS "LMDB: ${LMDB_LIBRARY}")
else()
    message(FATAL_ERROR "LMDB not found. Install liblmdb-dev.")
endif()

endif()
if(NOT SQLITE3_FOUND)
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY sqlite3)
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
        set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
        set(SQLITE3_LIBRARIES    ${SQLITE3_LIBRARY})
        set(SQLITE3_FOUND TRUE)
    endif()
endif()
if(NOT SQLITE3_FOUND)
    message(FATAL_ERROR "SQLite3 not found.")
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── Embed SQL schema files via Python (handles all special chars) ─
set(SQL_DIR "${CMAKE_SOURCE_DIR}/sql")
set(SCHEMA_GEN "${CMAKE_BINARY_DIR}/generated/SchemaFiles.h")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(GLOB SQL_FILES "${SQL_DIR}/*.sql")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Re-generate whenever any .sql file changes
add_custom_command(
    OUTPUT  "${SCHEMA_GEN}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/scripts/gen_schema_files.py"
            "${SQL_DIR}" "${SCHEMA_GEN}"
    DEPENDS ${SQL_FILES} "${CMAKE_SOURCE_DIR}/scripts/gen_schema_files.py"
    COMMENT "Embedding SQL schemas into SchemaFiles.h"
    VERBATIM
)
add_custom_target(generate_schemas DEPENDS "${SCHEMA_GEN}")

# Print which files are embedded (done at configure time by the script via stderr)
foreach(SQL_FILE ${SQL_FILES})
    get_filename_component(SQL_NAME "${SQL_FILE}" NAME_WE)
    message(STATUS "Will embed SQL: ${SQL_NAME}.sql")
endforeach()

# ── Source groups ─────────────────────────────────────────────
set(CORE_SOURCES
    src/core/Logger.cpp
    src/core/Config.cpp
    src/core/FileOps.cpp
    src/core/Database.cpp
    src/core/RegNumber.cpp
    src/core/BackupManager.cpp
    src/core/Migration.cpp
)

set(MODEL_SOURCES
    src/model/Person.cpp
    src/model/Team.cpp
    src/model/f16/ProjectF16.cpp
    src/model/f22/TaskF22.cpp
    src/model/dok/Document.cpp
    src/model/f18/F18Operation.cpp
    src/model/f18/F18OperationStep.cpp
    src/model/f18/Communication.cpp
)

set(WORKFLOW_SOURCES
    src/workflow/F77Workflow.cpp
    src/repository/ArchiveStore.cpp
    src/repository/DocumentRevision.cpp
)

set(MFS_SOURCES
    src/mfs/MFSWriter.cpp
)

set(APP_SOURCES
    src/app/AppController.cpp
)

# ── Shared library: core + model + workflow ───────────────────
add_library(rosenholz_lib STATIC
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
    ${WORKFLOW_SOURCES}
    ${MFS_SOURCES}
    ${APP_SOURCES}
)
add_dependencies(rosenholz_lib generate_schemas)

target_include_directories(rosenholz_lib PUBLIC
    ${LMDB_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ${SQLITE3_INCLUDE_DIRS}
)
target_link_libraries(rosenholz_lib PUBLIC
    ${LMDB_LIBRARY}
    ${SQLITE3_LIBRARIES}
    nlohmann_json::nlohmann_json
)
if(UNIX)
    target_compile_options(rosenholz_lib PRIVATE -Wall -Wextra)
    target_link_libraries(rosenholz_lib PUBLIC
    ${LMDB_LIBRARY} pthread)
endif()

# ── CLI binary ────────────────────────────────────────────────
# Gather all CLI source files
# CLI sources are listed explicitly — this prevents old/renamed files
# from being compiled if they still exist in the directory.
set(CLI_SOURCES
    src/cli/main_cli.cpp
    src/cli/cli_utils.cpp
    src/cli/cli_f16.cpp
    src/cli/cli_f22.cpp
    src/cli/cli_f18.cpp
    src/cli/cli_dok.cpp
    src/cli/cli_f77.cpp
    src/cli/cli_per.cpp
    src/cli/cli_de.cpp
    src/cli/cli_comm.cpp
    src/cli/cli_sys.cpp
)

add_executable(rosenholz_cli
    ${CLI_SOURCES}
)
target_link_libraries(rosenholz_cli PRIVATE rosenholz_lib readline)
target_include_directories(rosenholz_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

# ── Test binary ───────────────────────────────────────────────
file(GLOB TEST_SOURCES "tests/*.cpp")

add_executable(rosenholz_test
    tests/main_test.cpp
    ${TEST_SOURCES}
)
target_link_libraries(rosenholz_test PRIVATE rosenholz_lib)
target_include_directories(rosenholz_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

# ── Install ───────────────────────────────────────────────────
install(TARGETS rosenholz_cli DESTINATION bin)
install(TARGETS rosenholz_test DESTINATION bin)
install(FILES settings.json DESTINATION bin)

message(STATUS "Rosenholz PM v2.0.0")
message(STATUS "SQLite3: ${SQLITE3_LIBRARIES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SQL schemas embedded: ${SQL_FILES}")

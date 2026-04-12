cmake_minimum_required(VERSION 3.16)
project(Rosenholz VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find SQLite3 ─────────────────────────────────────────────
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 sqlite3)
endif()
if(NOT SQLITE3_FOUND)
    # Fallback: look for system sqlite3
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY sqlite3)
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
        set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
        set(SQLITE3_LIBRARIES    ${SQLITE3_LIBRARY})
        set(SQLITE3_FOUND TRUE)
    endif()
endif()
if(NOT SQLITE3_FOUND)
    message(FATAL_ERROR "SQLite3 not found. Install libsqlite3-dev (Linux) or sqlite3 (Windows).")
endif()

# ── Find nlohmann/json ────────────────────────────────────────
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    # Try as header-only via FetchContent
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── Source files ──────────────────────────────────────────────
set(CORE_SOURCES
    src/core/Logger.cpp
    src/core/Config.cpp
    src/core/FileOps.cpp
    src/core/Database.cpp
    src/core/RegNumber.cpp
    src/core/BackupManager.cpp
)

set(MODEL_SOURCES
    src/model/Trackable.cpp
    src/model/Person.cpp
    src/model/Team.cpp
    src/model/ProjectF16.cpp
    src/model/TaskF22.cpp
    src/model/IncidentF18.cpp
    src/model/Risk.cpp
    src/model/Document.cpp
    src/model/Milestone.cpp
    src/model/ReportingModels.cpp
)

set(MFS_SOURCES
    src/mfs/MFSWriter.cpp
)

set(APP_SOURCES
    src/app/AppController.cpp
)

# ── Main executable ───────────────────────────────────────────
add_executable(rosenholz
    main.cpp
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
    ${MFS_SOURCES}
    ${APP_SOURCES}
)

target_include_directories(rosenholz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(rosenholz PRIVATE
    ${SQLITE3_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# ── Platform flags ────────────────────────────────────────────
if(UNIX)
    target_compile_options(rosenholz PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(rosenholz PRIVATE pthread)
endif()
if(WIN32)
    target_compile_definitions(rosenholz PRIVATE
        _WIN32_WINNT=0x0601
        NOMINMAX
    )
    target_link_libraries(rosenholz PRIVATE shlwapi)
endif()

# ── Install ───────────────────────────────────────────────────
install(TARGETS rosenholz DESTINATION bin)
install(FILES settings.json DESTINATION bin)

# ── Status output ─────────────────────────────────────────────
message(STATUS "SQLite3: ${SQLITE3_LIBRARIES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")

cmake_minimum_required(VERSION 3.16)
project(backer VERSION 1.0.0 LANGUAGES CXX)

# ── C++ Standard ──────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ───────────────────────────────────────────────
option(BUILD_TESTING "Build unit tests" ON)

# ── Dependencies (via FetchContent) ───────────────────────
include(FetchContent)

# CLI11 ─ header-only CLI parser
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(cli11)

# spdlog ─ fast logging library
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# Google Test
if(BUILD_TESTING)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# ── Core Library ──────────────────────────────────────────  
add_library(backer_core
    src/core/backup_engine.cpp
    src/core/restore_engine.cpp
    src/fs/fs_abstraction.cpp
    src/fs/path_mapper.cpp
    src/storage/local_storage.cpp
)
target_include_directories(backer_core PUBLIC src)
target_link_libraries(backer_core PUBLIC spdlog::spdlog)

# ── CLI Executable ────────────────────────────────────────
add_executable(backer-cli
    src/main.cpp
    src/cli/commands.cpp
)
target_compile_definitions(backer-cli PRIVATE BACKER_VERSION="${PROJECT_VERSION}")
target_link_libraries(backer-cli PRIVATE backer_core CLI11::CLI11)

# ── Unit Tests ────────────────────────────────────────────
if(BUILD_TESTING)
    add_executable(backer_test
        tests/core/backup_engine_test.cpp
        tests/core/restore_engine_test.cpp
    )
    target_link_libraries(backer_test PRIVATE backer_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(backer_test)
endif()

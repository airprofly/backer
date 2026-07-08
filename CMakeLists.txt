cmake_minimum_required(VERSION 3.16)
project(backer VERSION 1.0.0 LANGUAGES CXX C)

# ── C++ Standard ──────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ───────────────────────────────────────────────
option(BUILD_TESTING "Build unit tests" ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# miniz ─ zip library (single-file C library)
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.1.2
)
FetchContent_GetProperties(miniz)
if(NOT miniz_POPULATED)
    FetchContent_Populate(miniz)

    # miniz 3.1.2+ 需要 miniz_export.h（原本由 CMake GenerateExportHeader 生成），
    # 这里提供一个 stub 供静态库编译使用。
    file(WRITE "${miniz_SOURCE_DIR}/miniz_export.h"
        "#ifndef MINIZ_EXPORT\n#define MINIZ_EXPORT\n#endif\n")

    add_library(miniz STATIC
        "${miniz_SOURCE_DIR}/miniz.c"
        "${miniz_SOURCE_DIR}/miniz_tdef.c"
        "${miniz_SOURCE_DIR}/miniz_tinfl.c"
        "${miniz_SOURCE_DIR}/miniz_zip.c")
    target_include_directories(miniz PUBLIC "${miniz_SOURCE_DIR}")
endif()

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
    src/fs/metadata.cpp
    src/fs/path_mapper.cpp
    src/fs/special_file.cpp
    src/storage/local_storage.cpp
    src/filters/criteria_filter.cpp
    src/pack/tar_packer.cpp
    src/pack/zip_packer.cpp
)
target_include_directories(backer_core PUBLIC src)
target_link_libraries(backer_core PUBLIC spdlog::spdlog miniz)

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
        tests/fs/metadata_test.cpp
        tests/fs/special_file_test.cpp
        tests/filter/criteria_filter_test.cpp
        tests/pack/tar_packer_test.cpp
        tests/pack/zip_packer_test.cpp
    )
    target_link_libraries(backer_test PRIVATE backer_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(backer_test)
endif()

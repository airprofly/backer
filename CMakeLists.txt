cmake_minimum_required(VERSION 3.18)
project(backer VERSION 1.0.0 LANGUAGES CXX C)

# ── C++ Standard ──────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ───────────────────────────────────────────────
option(BUILD_TESTING "Build unit tests" ON)
option(BUILD_GUI "Build Qt6 GUI application (requires Qt6)" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies (via FetchContent) ───────────────────────
include(FetchContent)

# CLI11 ─ header-only CLI parser
FetchContent_Declare(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(cli11)

# spdlog ─ fast logging library
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# miniz ─ zip library (single-file C library, no upstream CMakeLists.txt)
FetchContent_Declare(miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.1.2
    SOURCE_SUBDIR NONE
)
FetchContent_MakeAvailable(miniz)
file(WRITE "${miniz_SOURCE_DIR}/miniz_export.h"
    "#ifndef MINIZ_EXPORT\n#define MINIZ_EXPORT\n#endif\n")
add_library(miniz STATIC
    "${miniz_SOURCE_DIR}/miniz.c"
    "${miniz_SOURCE_DIR}/miniz_tdef.c"
    "${miniz_SOURCE_DIR}/miniz_tinfl.c"
    "${miniz_SOURCE_DIR}/miniz_zip.c")
target_include_directories(miniz PUBLIC "${miniz_SOURCE_DIR}")

# Google Test
if(BUILD_TESTING)
    enable_testing()
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# ── GUI (Qt6) — optional, requires Qt6 prebuilt or system install ────
if(BUILD_GUI)
    include(cmake/FetchQt6.cmake)
endif()

# ── ccronexpr (cron expression parser, single-header C library, no upstream CMakeLists.txt) ──
FetchContent_Declare(ccronexpr
    GIT_REPOSITORY https://github.com/staticlibs/ccronexpr.git
    GIT_TAG 5d7e772df34aadc938f1246ebe2551b9c1c19012
    UPDATE_DISCONNECTED ON
    SOURCE_SUBDIR NONE
)
FetchContent_MakeAvailable(ccronexpr)
# Copy both source and header files to src/scheduler/ for building.
file(COPY
     ${ccronexpr_SOURCE_DIR}/ccronexpr.h
     ${ccronexpr_SOURCE_DIR}/ccronexpr.c
     DESTINATION ${CMAKE_SOURCE_DIR}/src/scheduler/)

# ── Compression libraries (all via FetchContent, no system deps) ──

# zlib (gzip) — has standard CMakeLists.txt
FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
)
# Disable zlib's own test targets (example/minigzip) so ctest doesn't report
# them as "Not Run" when it discovers them without the binaries.
set(ZLIB_BUILD_TESTING OFF CACHE INTERNAL "" FORCE)
FetchContent_MakeAvailable(zlib)

# zstd — has CMake in build/cmake/
FetchContent_Declare(zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.6
    SOURCE_SUBDIR build/cmake
)
set(ZSTD_BUILD_PROGRAMS OFF CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_DOC OFF CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE INTERNAL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE INTERNAL "" FORCE)
FetchContent_MakeAvailable(zstd)
target_compile_definitions(libzstd_static PRIVATE ZSTD_LIB_DEPRECATED=0)

# liblzma (XZ) — XZ Utils has native CMake support since v5.6.0.
# FetchContent_MakeAvailable generates config.h and the liblzma target.
# Dockerfile pre-downloads the release tarball and points
# FETCHCONTENT_SOURCE_DIR_LZMA at it, bypassing the git clone.
FetchContent_Declare(lzma
    GIT_REPOSITORY https://github.com/tukaani-project/xz.git
    GIT_TAG v5.6.3
)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
set(XZ_BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
set(XZ_ENABLE_XZ_DEC OFF CACHE INTERNAL "" FORCE)
set(XZ_ENABLE_XZ OFF CACHE INTERNAL "" FORCE)
set(XZ_ENABLE_LZMAINFO OFF CACHE INTERNAL "" FORCE)
set(XZ_ENABLE_LZMADEC OFF CACHE INTERNAL "" FORCE)
set(XZ_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
set(XZ_BUILD_TESTING OFF CACHE INTERNAL "" FORCE)
FetchContent_MakeAvailable(lzma)

# ── OpenSSL (system library — no CMake FetchContent available) ──
find_package(OpenSSL REQUIRED)

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
    src/compress/build_compressor.cpp
    src/compress/gzip_compressor.cpp
    src/compress/zstd_compressor.cpp
    src/compress/lzma_compressor.cpp
    src/crypto/key_derivation.cpp
    src/crypto/openssl_encryptor.cpp
    src/crypto/build_encryptor.cpp
    src/scheduler/ccronexpr.c
    src/scheduler/backup_scheduler.cpp
    src/scheduler/schedule_store.cpp
    src/scheduler/retention_policy.cpp
)
target_include_directories(backer_core PUBLIC src)
# liblzma's public header (lzma.h) is in api/ but exposed PRIVATE by xz CMake;
# add it here so backer_core sources can include <lzma.h>.
target_include_directories(backer_core PRIVATE ${lzma_SOURCE_DIR}/src/liblzma/api)
# Compression libs are PRIVATE: compressor.h exposes only span/vector, no
# zlib/zstd/lzma types leak through the Compressor interface.
target_link_libraries(backer_core PUBLIC
    spdlog::spdlog
    miniz
    OpenSSL::Crypto
)
target_link_libraries(backer_core PRIVATE
    zlibstatic
    libzstd_static
    liblzma
)

# Static linking on Windows: define OPENSSL_STATIC to avoid DLL imports
if(WIN32 AND OPENSSL_STATIC)
    target_compile_definitions(backer_core PUBLIC OPENSSL_STATIC)
endif()

# ccronexpr needs local-time mode for cron expressions in the user's timezone
target_compile_definitions(backer_core PRIVATE $<$<COMPILE_LANGUAGE:C>:CRON_USE_LOCAL_TIME=1>)

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
        tests/compress/compressor_test.cpp
        tests/compress/compressor_factory_test.cpp
        tests/crypto/openssl_encryptor_test.cpp
        tests/crypto/encryptor_factory_test.cpp
        tests/scheduler/ccronexpr_test.cpp
        tests/scheduler/retention_policy_test.cpp
        tests/scheduler/backup_scheduler_test.cpp
    )
    target_link_libraries(backer_test PRIVATE backer_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(backer_test)
endif()

# ── GUI Executable ─────────────────────────────────────────────
# Requires Qt6. Set BUILD_GUI=ON and provide Qt6_DIR or install Qt6.
#   bash scripts/setup-qt6.sh
#   export Qt6_DIR=<install-dir>/6.5.0/gcc_64
#   cmake -B build -DBUILD_GUI=ON -DQt6_DIR=$Qt6_DIR
if(BUILD_GUI AND HAVE_QT6)
    set(GUI_SOURCES
        src/gui/main.cpp
        src/gui/main_window.cpp
        src/gui/gui_style.cpp
        src/cli/commands.cpp
        src/gui/backup_tab.cpp
        src/gui/restore_tab.cpp
        src/gui/schedule_tab.cpp
        src/gui/backup_worker.cpp
        src/gui/progress_widget.cpp
        src/gui/log_widget.cpp
        src/gui/filter_dialog.cpp
    )
    set(GUI_HEADERS
        src/gui/main_window.h
        src/gui/gui_style.h
        src/gui/backup_tab.h
        src/gui/restore_tab.h
        src/gui/schedule_tab.h
        src/gui/backup_worker.h
        src/gui/progress_widget.h
        src/gui/log_widget.h
        src/gui/filter_dialog.h
        src/gui/gui_utils.h
    )
    set(GUI_RESOURCES
        src/gui/resources/app.qrc
    )

    add_executable(backer-gui ${GUI_SOURCES} ${GUI_HEADERS} ${GUI_RESOURCES})
    target_compile_definitions(backer-gui PRIVATE BACKER_VERSION="${PROJECT_VERSION}")
    target_include_directories(backer-gui PRIVATE src)
    target_link_libraries(backer-gui PRIVATE
        backer_core
        Qt6::Core
        Qt6::Widgets
    )

    # Qt6 自动处理 MOC/uic/rcc
    set_target_properties(backer-gui PROPERTIES
        AUTOMOC ON
        AUTORCC ON
    )

    message(STATUS "GUI: ✅ Enabled (backer-gui target created)")
elseif(BUILD_GUI AND NOT HAVE_QT6)
    message(WARNING "GUI: ⚠ BUILD_GUI=ON but Qt6 not found. "
                    "Set Qt6_DIR or install Qt6 to build the GUI. "
                    "See: bash scripts/setup-qt6.sh")
else()
    message(STATUS "GUI: OFF (set BUILD_GUI=ON and provide Qt6 to enable)")
endif()

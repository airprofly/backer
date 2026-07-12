# FetchQt6.cmake — Qt6 自动下载模块
#
# 完全自包含：当 BUILD_GUI=ON 且 Qt6 未安装时，自动通过 aqtinstall
# 从 Qt 官方源下载 Linux x86_64 预编译二进制，无需系统库。
#
# 控制变量:
#   QT6_AUTO_DOWNLOAD  — BOOL，允许自动下载（默认 ON）
#   QT6_VERSION        — STRING，Qt 版本（默认 6.5.0）
#   Qt6_DIR            — PATH，手动指定路径（绕过自动下载）
#
# 输出变量:
#   HAVE_QT6           — BOOL，Qt6 是否可用

# ── 选项 ──────────────────────────────────────────────────────────────
option(QT6_AUTO_DOWNLOAD "Auto-download Qt6 prebuilt binaries when not found" ON)
set(QT6_VERSION "6.5.0" CACHE STRING "Qt6 version to download")
mark_as_advanced(QT6_AUTO_DOWNLOAD QT6_VERSION)

# ── 查找已存在的 Qt6 ─────────────────────────────────────────────────
# 优先级: Qt6_DIR 缓存变量 > 环境变量 > 系统

if(Qt6_DIR)
    message(STATUS "FetchQt6: using Qt6_DIR: ${Qt6_DIR}")
    find_package(Qt6 QUIET COMPONENTS Core Widgets
        PATHS "${Qt6_DIR}" NO_DEFAULT_PATH)
elseif(DEFINED ENV{Qt6_DIR})
    set(Qt6_DIR "$ENV{Qt6_DIR}" CACHE PATH "Path to Qt6 installation")
    message(STATUS "FetchQt6: using Qt6_DIR from env: ${Qt6_DIR}")
    find_package(Qt6 QUIET COMPONENTS Core Widgets
        PATHS "${Qt6_DIR}" NO_DEFAULT_PATH)
else()
    message(STATUS "FetchQt6: checking system Qt6...")
    find_package(Qt6 QUIET COMPONENTS Core Widgets)
endif()

if(Qt6_FOUND)
    set(HAVE_QT6 ON CACHE BOOL "Qt6 found" FORCE)
    message(STATUS "FetchQt6: ✅ Qt6 found")
    return()
endif()

set(HAVE_QT6 OFF CACHE BOOL "Qt6 not found" FORCE)

# ── Qt6 未找到 → 自动下载预编译二进制 ───────────────────────────────
if(NOT QT6_AUTO_DOWNLOAD)
    message(STATUS "FetchQt6: Qt6 not found and QT6_AUTO_DOWNLOAD=OFF.")
    return()
endif()

message(STATUS "FetchQt6: Qt6 not found, attempting auto-download...")

find_program(_python python3)
if(NOT _python)
    message(WARNING "FetchQt6: python3 not found — cannot auto-download Qt6")
    return()
endif()

# ── Bootstrap pip (if not already available) ──────────────────────────
execute_process(COMMAND ${_python} -m ensurepip --upgrade
    OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _ensure_ret)
execute_process(COMMAND ${_python} -m pip install --user aqtinstall
    OUTPUT_VARIABLE _pip_out ERROR_VARIABLE _pip_err
    RESULT_VARIABLE _pip_ret OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT _pip_ret EQUAL 0)
    message(WARNING "FetchQt6: pip install aqtinstall failed: ${_pip_err}")
    return()
endif()

# ── Download Qt6 prebuilt binaries ────────────────────────────────────
set(_qt_dir "${CMAKE_BINARY_DIR}/_deps/qt6_prebuilt")
file(MAKE_DIRECTORY "${_qt_dir}")

message(STATUS "FetchQt6: downloading Qt ${QT6_VERSION} (gcc_64)...")
message(STATUS "FetchQt6:   target: ${_qt_dir}")
message(STATUS "FetchQt6:   this may take several minutes...")

execute_process(COMMAND ${_python} -m aqt install-qt
    linux desktop "${QT6_VERSION}" gcc_64
    -O "${_qt_dir}" --modules qtbase
    OUTPUT_VARIABLE _aqt_out ERROR_VARIABLE _aqt_err
    RESULT_VARIABLE _aqt_ret TIMEOUT 600)

if(NOT _aqt_ret EQUAL 0)
    message(WARNING "FetchQt6: download failed (${_aqt_ret}): ${_aqt_err}")
    file(REMOVE_RECURSE "${_qt_dir}")
    return()
endif()

set(Qt6_DIR "${_qt_dir}/${QT6_VERSION}/gcc_64"
    CACHE PATH "Path to downloaded Qt6" FORCE)
message(STATUS "FetchQt6: Qt6 downloaded to ${Qt6_DIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets
    PATHS "${Qt6_DIR}" NO_DEFAULT_PATH)

if(Qt6_FOUND)
    set(HAVE_QT6 ON CACHE BOOL "Qt6 found" FORCE)
    message(STATUS "FetchQt6: ✅ Qt6 auto-download successful")
else()
    message(WARNING "FetchQt6: downloaded but find_package failed at ${Qt6_DIR}")
endif()

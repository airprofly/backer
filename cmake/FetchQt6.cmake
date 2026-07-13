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

# Try pip install; Ubuntu 24.04+ may need --break-system-packages (PEP 668)
execute_process(COMMAND ${_python} -m pip install --user aqtinstall
    OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _pip_ret)
if(NOT _pip_ret EQUAL 0)
    execute_process(COMMAND ${_python} -m pip install --user --break-system-packages aqtinstall
        OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _pip_ret)
endif()
if(NOT _pip_ret EQUAL 0)
    execute_process(COMMAND pip3 install --user aqtinstall
        OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _pip_ret)
endif()

if(NOT _pip_ret EQUAL 0)
    message(WARNING "FetchQt6: pip install aqtinstall failed. "
        "Install manually: python3 -m pip install --user --break-system-packages aqtinstall")
    return()
endif()

# ── On Linux, ensure OpenGL is available (Qt6Gui depends on it) ─────
if(UNIX AND NOT APPLE AND NOT OPENGL_FOUND)
    find_package(OpenGL QUIET)
    if(NOT OpenGL_FOUND)
        set(_gl_dir "${CMAKE_BINARY_DIR}/_deps/opengl_local")
        if(NOT EXISTS "${_gl_dir}/usr/lib/x86_64-linux-gnu/libOpenGL.so")
            message(STATUS "FetchQt6: OpenGL not found, downloading...")
            file(MAKE_DIRECTORY "${_gl_dir}")
            execute_process(COMMAND apt-get download libopengl0 libgl1 libglx0 libegl1
                OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _dl_ret
                WORKING_DIRECTORY "${_gl_dir}")
            execute_process(COMMAND apt-get download libgl1-mesa-dev libgl-dev libglx-dev
                libopengl-dev libegl-dev libglvnd-dev
                OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _dl_ret2
                WORKING_DIRECTORY "${_gl_dir}")
            if(_dl_ret EQUAL 0 OR _dl_ret2 EQUAL 0)
                file(GLOB _debs "${_gl_dir}/*.deb")
                foreach(_deb ${_debs})
                    execute_process(COMMAND dpkg-deb -x "${_deb}" "${_gl_dir}"
                        OUTPUT_QUIET ERROR_QUIET)
                endforeach()
            endif()
        endif()
        if(EXISTS "${_gl_dir}/usr/lib/x86_64-linux-gnu/libGL.so")
            set(OPENGL_INCLUDE_DIR "${_gl_dir}/usr/include" CACHE PATH "" FORCE)
            set(OPENGL_gl_LIBRARY
                "${_gl_dir}/usr/lib/x86_64-linux-gnu/libGL.so" CACHE FILEPATH "" FORCE)
            set(OpenGL_GL_PREFERENCE "LEGACY" CACHE STRING "OpenGL preference (LEGACY for Qt6 compatibility)" FORCE)
            message(STATUS "FetchQt6: OpenGL extracted to ${_gl_dir}")
        else()
            message(STATUS "FetchQt6: OpenGL not available — "
                "install: sudo apt install libgl1-mesa-dev")
        endif()
    endif()
endif()

# ── Detect platform for aqtinstall ────────────────────────────────────
if(WIN32)
    set(_aqt_platform "windows")
    set(_aqt_arch "win64_msvc2019_64")
elseif(APPLE)
    set(_aqt_platform "mac")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_aqt_arch "macos_arm64")
    else()
        set(_aqt_arch "macos_x64")
    endif()
else()
    set(_aqt_platform "linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_aqt_arch "gcc_arm64")
    else()
        set(_aqt_arch "gcc_64")
    endif()
endif()

# ── Download Qt6 prebuilt binaries ────────────────────────────────────
set(_qt_dir "${CMAKE_BINARY_DIR}/_deps/qt6_prebuilt")
file(MAKE_DIRECTORY "${_qt_dir}")

message(STATUS "FetchQt6: downloading Qt ${QT6_VERSION} (${_aqt_arch})...")
message(STATUS "FetchQt6:   target: ${_qt_dir}")
message(STATUS "FetchQt6:   this may take several minutes...")

execute_process(COMMAND ${_python} -m aqt install-qt
    ${_aqt_platform} desktop "${QT6_VERSION}" ${_aqt_arch}
    -O "${_qt_dir}"
    OUTPUT_VARIABLE _aqt_out ERROR_VARIABLE _aqt_err
    RESULT_VARIABLE _aqt_ret TIMEOUT 600)

if(NOT _aqt_ret EQUAL 0)
    message(WARNING "FetchQt6: download failed (${_aqt_ret}): ${_aqt_err}")
    file(REMOVE_RECURSE "${_qt_dir}")
    return()
endif()

set(Qt6_DIR "${_qt_dir}/${QT6_VERSION}/${_aqt_arch}"
    CACHE PATH "Path to downloaded Qt6" FORCE)
message(STATUS "FetchQt6: Qt6 downloaded to ${Qt6_DIR}")

# Add Qt6_DIR to CMAKE_PREFIX_PATH so find_dependency calls inside
# Qt6Config can locate sub-modules (Qt6WidgetsTools, Qt6GuiTools, Qt6CoreTools).
list(APPEND CMAKE_PREFIX_PATH "${Qt6_DIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets
    PATHS "${Qt6_DIR}" NO_DEFAULT_PATH)

if(Qt6_FOUND)
    set(HAVE_QT6 ON CACHE BOOL "Qt6 found" FORCE)
    message(STATUS "FetchQt6: ✅ Qt6 auto-download successful")
else()
    message(WARNING "FetchQt6: downloaded but find_package failed at ${Qt6_DIR}")
endif()

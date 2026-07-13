#!/usr/bin/env bash
# setup.sh — 一键环境配置 + 编译脚本
#
# 用法:
#   bash scripts/setup.sh                  # 完整配置 + 编译
#   bash scripts/setup.sh --quick          # 仅编译（跳过依赖安装）
#   bash scripts/setup.sh --cli-only       # 仅 CLI 环境（不含 Qt6 GUI）
#
# 在任何 Ubuntu 22.04+ / Debian 12+ 上均可一键运行。
# 不需要 sudo 权限（Qt6 和 OpenGL 会自动下载到 ~/.local/backer-deps）。

set -euo pipefail

# ── 颜色 ─────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'
BLUE='\033[0;34m'; NC='\033[0m'
info()  { echo -e "${BLUE}::${NC} $*"; }
ok()    { echo -e "${GREEN}✓${NC} $*"; }
warn()  { echo -e "${YELLOW}⚠${NC} $*"; }
err()   { echo -e "${RED}✗${NC} $*"; }

# ── 参数解析 ─────────────────────────────────────────────────────────
QUICK=false
CLI_ONLY=false
for arg in "$@"; do
    case "$arg" in
        --quick) QUICK=true ;;
        --cli-only) CLI_ONLY=true ;;
    esac
done

cd "$(dirname "$0")/.."
PROJECT_DIR="$PWD"
BUILD_DIR="$PWD/build"
DEPS_DIR="$HOME/.local/backer-deps"
QT6_DIR="$DEPS_DIR/6.5.0/gcc_64"
OPENGL_DIR="$DEPS_DIR/usr"

# ═════════════════════════════════════════════════════════════════════
# 阶段 1： 检查前置条件
# ═════════════════════════════════════════════════════════════════════
phase_prereqs() {
    info "检查前置条件..."

    local miss=false
    command -v cmake   >/dev/null 2>&1 || { err "cmake 未安装"; miss=true; }
    command -v g++     >/dev/null 2>&1 || { err "g++ 未安装 (apt install g++)"; miss=true; }
    command -v python3 >/dev/null 2>&1 || { err "python3 未安装"; miss=true; }
    command -v curl    >/dev/null 2>&1 || { err "curl 未安装"; miss=true; }

    # 检查 C++17 编译器
    if command -v g++ &>/dev/null; then
        gpp_ver=$(g++ -dumpversion 2>/dev/null || echo "0")
        if [ "$gpp_ver" -lt 12 ] 2>/dev/null; then
            warn "g++ 版本 $gpp_ver (建议 ≥ 12)，可能不支持完整 C++17"
        fi
    fi

    if [ "$miss" = true ]; then
        err "请安装缺失工具后重试"
        echo "  Ubuntu: sudo apt install cmake g++ python3 curl"
        exit 1
    fi
    ok "前置条件满足"
}

# ═════════════════════════════════════════════════════════════════════
# 阶段 2： 安装 pip + aqtinstall（用于下载 Qt6）
# ═════════════════════════════════════════════════════════════════════
phase_setup_pip() {
    if python3 -m pip --version &>/dev/null; then
        ok "pip 已就绪"
        return
    fi
    info "安装 pip..."
    python3 <(curl -sS https://bootstrap.pypa.io/get-pip.py) --user --break-system-packages 2>/dev/null \
        || python3 <(curl -sS https://bootstrap.pypa.io/get-pip.py) --user 2>/dev/null
    export PATH="$HOME/.local/bin:$PATH"
    if python3 -m pip --version &>/dev/null; then
        ok "pip 安装成功"
    else
        err "pip 安装失败，请手动安装: curl -sS https://bootstrap.pypa.io/get-pip.py | python3"
        exit 1
    fi
}

phase_setup_aqt() {
    if python3 -m aqt version &>/dev/null; then
        ok "aqtinstall 已就绪"
        return
    fi
    info "安装 aqtinstall..."
    python3 -m pip install --user --break-system-packages aqtinstall 2>/dev/null \
        || python3 -m pip install --user aqtinstall
    if python3 -m aqt version &>/dev/null; then
        ok "aqtinstall 安装成功"
    else
        err "aqtinstall 安装失败"
        exit 1
    fi
}

# ═════════════════════════════════════════════════════════════════════
# 阶段 3： 下载 Qt6 预编译包
# ═════════════════════════════════════════════════════════════════════
phase_download_qt6() {
    if [ -f "$QT6_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        ok "Qt6 6.5.0 已下载到 $QT6_DIR"
        return
    fi
    info "下载 Qt6 6.5.0 (Linux x86_64)..."
    info "  目标: $QT6_DIR"
    info "  耗时约 1-2 分钟..."
    mkdir -p "$DEPS_DIR"
    python3 -m aqt install-qt linux desktop 6.5.0 gcc_64 -O "$DEPS_DIR"
    if [ -f "$QT6_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        ok "Qt6 下载成功"
    else
        err "Qt6 下载失败，请检查网络"
        exit 1
    fi
}

# ═════════════════════════════════════════════════════════════════════
# 阶段 4： 下载 OpenGL 库（Qt6Gui 需要）
# ═════════════════════════════════════════════════════════════════════
phase_download_opengl() {
    if [ -f "$OPENGL_DIR/lib/x86_64-linux-gnu/libOpenGL.so" ]; then
        ok "OpenGL 库已下载到 $OPENGL_DIR"
        return
    fi

    info "下载 OpenGL 运行时库（从 Ubuntu 软件源提取）..."

    local dl_dir="/tmp/backer-gl-$$"
    mkdir -p "$dl_dir" "$OPENGL_DIR"

    # 下载必要的 deb 包（apt-get download 不需要 root）
    cd "$dl_dir"
    local pkgs=(
        libopengl0 libgl1 libglx0 libegl1
        libgl1-mesa-dev libgl-dev libglx-dev libopengl-dev
        libegl-dev libglvnd-dev
    )
    for pkg in "${pkgs[@]}"; do
        apt-get download "$pkg" 2>/dev/null || true
    done

    # 提取所有 deb 到目标目录
    for f in *.deb; do
        [ -f "$f" ] && dpkg-deb -x "$f" "$OPENGL_DIR" 2>/dev/null || true
    done

    cd /tmp && rm -rf "$dl_dir"

    # 验证关键文件存在
    if [ -f "$OPENGL_DIR/usr/lib/x86_64-linux-gnu/libOpenGL.so" ]; then
        ok "OpenGL 库就绪"
    else
        warn "OpenGL 库未完全下载。GUI 编译可能失败。"
        warn "可手动安装: sudo apt install libgl1-mesa-dev"
    fi
}

# ═════════════════════════════════════════════════════════════════════
# 阶段 5： 编译项目
# ═════════════════════════════════════════════════════════════════════
phase_build() {
    info "编译项目..."

    local gui_flag=""
    local gui_msg="(仅 CLI)"
    if [ "$CLI_ONLY" = false ] && [ -f "$QT6_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        gui_flag="-DBUILD_GUI=ON"
        gui_msg="(含 GUI)"

        # 设置 Qt6_DIR 和 OpenGL 路径
        gui_flag+=" -DQt6_DIR=$QT6_DIR"
        gui_flag+=" -DOPENGL_INCLUDE_DIR=$OPENGL_DIR/usr/include"
        gui_flag+=" -DOPENGL_opengl_LIBRARY=$OPENGL_DIR/usr/lib/x86_64-linux-gnu/libOpenGL.so"
        gui_flag+=" -DOPENGL_glx_LIBRARY=$OPENGL_DIR/usr/lib/x86_64-linux-gnu/libGLX.so"
        gui_flag+=" -DOPENGL_egl_LIBRARY=$OPENGL_DIR/usr/lib/x86_64-linux-gnu/libEGL.so"
        gui_flag+=" -DCMAKE_PREFIX_PATH=$QT6_DIR;$OPENGL_DIR/usr"
    fi

    cmake -B "$BUILD_DIR" $gui_flag -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -v "^$"
    cmake --build "$BUILD_DIR" -j"$(nproc)"

    ok "编译完成 $gui_msg"
    echo ""
    echo "  CLI 工具:     $BUILD_DIR/backer-cli"
    echo "  GUI 界面:     $BUILD_DIR/backer-gui"
    echo "  单元测试:     $BUILD_DIR/backer_test"
    echo "  测试命令:     ctest --test-dir $BUILD_DIR"
}

# ═════════════════════════════════════════════════════════════════════
# 主流程
# ═════════════════════════════════════════════════════════════════════
main() {
    echo ""
    echo "========================================"
    echo "  Backer — 一键环境配置 + 编译"
    echo "========================================"
    echo ""

    phase_prereqs

    if [ "$QUICK" = true ]; then
        phase_build "$@"
        echo ""
        echo "运行 GUI:  LD_LIBRARY_PATH=$OPENGL_DIR/usr/lib/x86_64-linux-gnu $BUILD_DIR/backer-gui"
        echo "运行 CLI:  $BUILD_DIR/backer-cli --help"
        return
    fi

    if [ "$CLI_ONLY" = false ]; then
        phase_setup_pip
        phase_setup_aqt
        phase_download_qt6
        phase_download_opengl
    fi

    phase_build

    echo ""
    echo "========================================"
    echo "  ✅ 全部完成"
    echo "========================================"
    echo ""
    echo "运行 GUI 界面测试功能:"
    echo ""
    echo "  export LD_LIBRARY_PATH=$OPENGL_DIR/usr/lib/x86_64-linux-gnu:\$LD_LIBRARY_PATH"
    echo "  $BUILD_DIR/backer-gui"
    echo ""
    echo "或直接运行（脚本已封装 LD_LIBRARY_PATH）:"
    echo ""
    echo "  bash scripts/run-gui.sh"
    echo ""
    echo "运行 CLI 备份测试:"
    echo ""
    echo "  bash scripts/test-backup-restore.sh"
    echo ""
    echo "运行全部单元测试:"
    echo ""
    echo "  ctest --test-dir $BUILD_DIR --output-on-failure"
    echo ""
}

main "$@"

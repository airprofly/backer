#!/usr/bin/env bash
# run-gui.sh — 启动 backer-gui（自动设置 LD_LIBRARY_PATH）
#
# 用法: bash scripts/run-gui.sh

set -euo pipefail

cd "$(dirname "$0")/.."
OPENGL_DIR="$HOME/.local/backer-deps/usr"
BUILD_DIR="$PWD/build"

# 设置 OpenGL 运行时路径
if [ -d "$OPENGL_DIR/lib/x86_64-linux-gnu" ]; then
    export LD_LIBRARY_PATH="$OPENGL_DIR/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

if [ ! -f "$BUILD_DIR/backer-gui" ]; then
    echo "✗ backer-gui 未编译，先运行: bash scripts/setup.sh"
    exit 1
fi

echo "启动 backer-gui..."
exec "$BUILD_DIR/backer-gui" "$@"

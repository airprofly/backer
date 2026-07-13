#!/usr/bin/env bash
# Build backer-gui with pre-downloaded Qt6 + OpenGL deps.
# First run: bash scripts/setup-qt6.sh

set -euo pipefail
cd "$(dirname "$0")/.."

DEPS_DIR="$HOME/.local/backer-deps"
export LD_LIBRARY_PATH="$DEPS_DIR/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release \
  -DQt6_DIR="$DEPS_DIR/6.5.0/gcc_64" \
  -DOPENGL_INCLUDE_DIR="$DEPS_DIR/usr/include" \
  -DOPENGL_opengl_LIBRARY="$DEPS_DIR/usr/lib/x86_64-linux-gnu/libOpenGL.so" \
  -DOPENGL_glx_LIBRARY="$DEPS_DIR/usr/lib/x86_64-linux-gnu/libGLX.so" \
  -DOPENGL_egl_LIBRARY="$DEPS_DIR/usr/lib/x86_64-linux-gnu/libEGL.so" \
  -DCMAKE_PREFIX_PATH="$DEPS_DIR/6.5.0/gcc_64;$DEPS_DIR/usr"

cmake --build build -j"$(nproc)"

echo "✅ Build complete: backer-cli, backer-gui, backer_test"

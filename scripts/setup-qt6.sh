#!/usr/bin/env bash
# setup-qt6.sh — Download Qt6 + OpenGL for GUI build (no sudo needed)
#
# Usage: bash scripts/setup-qt6.sh
#
# Downloads Qt6 prebuilt binaries via aqtinstall and extracts system
# OpenGL libraries locally.  After running this, do:
#
#   bash scripts/build-gui.sh
#
# Or manually:
#
#   cmake -B build -DBUILD_GUI=ON -DQt6_DIR="$HOME/.local/backer-deps/6.5.0/gcc_64" ...
#   cmake --build build -j$(nproc)

set -euo pipefail

DEPS_DIR="$HOME/.local/backer-deps"
QT6_DIR="$DEPS_DIR/6.5.0/gcc_64"
OPENGL_DIR="$DEPS_DIR/usr"
PY_USER_SITE="$(python3 -c 'import site; print(site.USER_BASE)/bin' 2>/dev/null || echo "$HOME/.local/bin")"
export PATH="$PY_USER_SITE:$PATH"

echo "=== Backer GUI dependency setup ==="
echo "Target: $DEPS_DIR"

# ── 1. Ensure aqtinstall ────────────────────────────────────────────
if ! python3 -m aqt version &>/dev/null; then
    echo "Installing aqtinstall (Qt downloader)..."
    python3 -m pip install --user --break-system-packages aqtinstall 2>/dev/null \
        || python3 -m pip install --user aqtinstall 2>/dev/null \
        || { echo "ERROR: pip install aqtinstall failed"; exit 1; }
fi

# ── 2. Download Qt6 (if not already present) ─────────────────────────
if [ ! -f "$QT6_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo "Downloading Qt6 6.5.0 (Linux x86_64)..."
    mkdir -p "$DEPS_DIR"
    python3 -m aqt install-qt linux desktop 6.5.0 gcc_64 -O "$DEPS_DIR"
    echo "Qt6 downloaded to $QT6_DIR"
else
    echo "Qt6 already present at $QT6_DIR"
fi

# ── 3. Download OpenGL libraries (if not already present) ────────────
if [ ! -f "$OPENGL_DIR/lib/x86_64-linux-gnu/libOpenGL.so" ]; then
    echo "Downloading OpenGL libraries..."
    mkdir -p /tmp/backer-gl-dl
    cd /tmp/backer-gl-dl
    apt-get download libopengl0 libgl1 libglx0 libegl1 2>/dev/null
    apt-get download libgl1-mesa-dev libgl-dev libglx-dev libopengl-dev libegl-dev libglvnd-dev 2>/dev/null
    for f in *.deb; do
        dpkg-deb -x "$f" "$OPENGL_DIR" 2>/dev/null
    done
    cd /tmp && rm -rf /tmp/backer-gl-dl
    echo "OpenGL extracted to $OPENGL_DIR"
else
    echo "OpenGL already present at $OPENGL_DIR"
fi

echo ""
echo "=== Setup complete ==="
echo "Now build with:  bash scripts/build-gui.sh"
echo "Or:              cmake -B build -DBUILD_GUI=ON -DQt6_DIR=$QT6_DIR ..."

#!/bin/bash
# Setup test data source directory for backup/restore testing
set -euo pipefail

error_handler() {
    echo "Error in script at line ${1}: '${2}' failed with exit code ${3}"
}
trap 'error_handler ${LINENO} "$BASH_COMMAND" $?' ERR

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_DIR="$BASE_DIR/data/source"

echo "Creating test data source directory..."

rm -rf "$TARGET_DIR"
mkdir -p "$TARGET_DIR/subdir"

# Create files with different types: regular, symlink and pipe
echo "regular file" > "$TARGET_DIR/file.txt"
echo "nested file" > "$TARGET_DIR/subdir/nested.txt"
echo "target" > "$TARGET_DIR/target.txt"
ln -sf "target.txt" "$TARGET_DIR/link.txt"
mkfifo "$TARGET_DIR/pipe.fifo"

echo -e "\n\033[1;92m[SUCCESS] Test data created at: $TARGET_DIR\033[0m\n"

ls -la "$TARGET_DIR"
echo ""
ls -la "$TARGET_DIR/subdir"

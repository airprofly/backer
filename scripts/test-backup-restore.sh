#!/bin/bash
# Test backup/restore workflow: setup test data, backup, restore, verify

set -euo pipefail

error_handler() {
    echo -e "\n\033[1;91mError in script at line ${1}: '${2}' failed with exit code ${3}\033[0m"
}
trap 'error_handler ${LINENO} "$BASH_COMMAND" $?' ERR

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"

SOURCE_DIR="$BASE_DIR/data/source"
BACKUP_DIR="$BASE_DIR/data/backup"
RESTORE_DIR="$BASE_DIR/data/restore"

# ── Select mode ──────────────────────────────────────────────
PS3="Choose a mode (1-3): "
echo "Select test mode:"
select MODE in "Local (backer-cli)" "Docker" "Cancel"; do
    case "$MODE" in
        "Local (backer-cli)") DOCKER_MODE=false; break;;
        "Docker")             DOCKER_MODE=true;  break;;
        "Cancel")             echo "Aborted."; exit 0;;
    esac
done

# ── Setup test data ──────────────────────────────────────────
echo -e "\nSetting up test data..."

rm -rf "$SOURCE_DIR" "$BACKUP_DIR" "$RESTORE_DIR" 2>/dev/null || sudo rm -rf "$SOURCE_DIR" "$BACKUP_DIR" "$RESTORE_DIR"
mkdir -p "$SOURCE_DIR/subdir"

echo "regular file" > "$SOURCE_DIR/file.txt"
echo "nested file" > "$SOURCE_DIR/subdir/nested.txt"
echo "target" > "$SOURCE_DIR/target.txt"
ln -sf "target.txt" "$SOURCE_DIR/link.txt"
mkfifo "$SOURCE_DIR/pipe.fifo"

echo -e "\nSource directory: $SOURCE_DIR"
ls -la "$SOURCE_DIR"
echo ""
ls -la "$SOURCE_DIR/subdir"

# ── Run backup ───────────────────────────────────────────────
if [[ "$DOCKER_MODE" == true ]]; then
    echo -e "\nBacking up via Docker..."
    docker compose run --rm backer backup /data/source /data/backup
else
    echo -e "\nBacking up via backer-cli..."
    "$BASE_DIR/build/backer-cli" backup "$SOURCE_DIR" "$BACKUP_DIR"
fi

# ── Run restore ──────────────────────────────────────────────
if [[ "$DOCKER_MODE" == true ]]; then
    echo -e "\nRestoring via Docker..."
    docker compose run --rm backer restore /data/backup /data/restore
else
    echo -e "\nRestoring via backer-cli..."
    "$BASE_DIR/build/backer-cli" restore "$BACKUP_DIR" "$RESTORE_DIR"
fi

# ── Summary ──────────────────────────────────────────────────
echo -e "\n\033[1;92m[SUCCESS] All operations completed.\033[0m\n"

echo "Source:  $SOURCE_DIR"
ls -la "$SOURCE_DIR" | tail -n +2 | grep -v '^total'
echo ""
echo "Backup:  $BACKUP_DIR"
ls -la "$BACKUP_DIR" | tail -n +2 | grep -v '^total'
echo ""
echo "Restore: $RESTORE_DIR"
ls -la "$RESTORE_DIR" | tail -n +2 | grep -v '^total'

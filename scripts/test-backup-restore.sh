#!/bin/bash
# Test backup/restore workflow: setup test data, backup, restore, verify
# Covers: directory mirror, Tar/Zip pack, and gzip/zstd/lzma compression round-trips.

set -euo pipefail

# ── Logging helpers ──────────────────────────────────────────
# Format: [YYYY-MM-DD HH:MM:SS] [LEVEL] message  (color-coded by level)
_log() {
    local level="$1" msg="$2" color="$3"
    printf '\033[%sm[%s] [%s] %s\033[0m\n' \
        "$color" "$(date '+%Y-%m-%d %H:%M:%S')" "$level" "$msg"
}
log_info()  { _log "INFO"  "$1" "1;36"; }   # cyan
log_ok()    { _log "OK"    "$1" "1;32"; }   # green
log_warn()  { _log "WARN"  "$1" "1;33"; }   # yellow
log_err()   { _log "ERROR" "$1" "1;91"; }   # bright red

error_handler() {
    log_err "script line ${1}: '${2}' failed with exit code ${3}"
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

# ── Helper: run backer-cli (or docker) ───────────────────────
run_backer() {
    if [[ "$DOCKER_MODE" == true ]]; then
        docker compose run --rm backer "$@"
    else
        "$BASE_DIR/build/backer-cli" "$@"
    fi
}

# ── Setup test data ──────────────────────────────────────────
log_info "Setting up test data"

rm -rf "$SOURCE_DIR" "$BACKUP_DIR" "$RESTORE_DIR" 2>/dev/null || sudo rm -rf "$SOURCE_DIR" "$BACKUP_DIR" "$RESTORE_DIR"
mkdir -p "$SOURCE_DIR/subdir"

echo "regular file" > "$SOURCE_DIR/file.txt"
echo "nested file" > "$SOURCE_DIR/subdir/nested.txt"
echo "target" > "$SOURCE_DIR/target.txt"
ln -sf "target.txt" "$SOURCE_DIR/link.txt"
mkfifo "$SOURCE_DIR/pipe.fifo"

echo ""
log_info "Source directory: $SOURCE_DIR"
ls -la "$SOURCE_DIR"
echo ""
ls -la "$SOURCE_DIR/subdir"

# ── 1. Directory mirror backup/restore ───────────────────────
log_info "[1/4] Directory mirror backup/restore"
run_backer backup "$SOURCE_DIR" "$BACKUP_DIR"
run_backer restore "$BACKUP_DIR" "$RESTORE_DIR"
log_ok "mirror round-trip"

# ── 2. Tar pack round-trip ───────────────────────────────────
log_info "[2/4] Tar pack round-trip"
rm -rf "$BACKUP_DIR" "$RESTORE_DIR"
run_backer backup "$SOURCE_DIR" "$BACKUP_DIR" --pack tar
run_backer restore "$BACKUP_DIR.tar" "$RESTORE_DIR" --pack tar
log_ok "tar round-trip"

# ── 3. Compression round-trips (gzip/zstd/lzma) ──────────────
log_info "[3/4] Compression round-trips (gzip/zstd/lzma)"
declare -A SUFFIX=( [gzip]=".gz" [zstd]=".zst" [lzma]=".xz" )
# Baseline: uncompressed tar size (what the compressor actually receives).
TAR_BASELINE=$(stat -c%s "$BACKUP_DIR.tar" 2>/dev/null || echo 0)
log_info "baseline (uncompressed tar): ${TAR_BASELINE} B"
for algo in gzip zstd lzma; do
    rm -rf "$BACKUP_DIR" "$RESTORE_DIR" "$BACKUP_DIR.tar"*
    log_info "algorithm: $algo"
    run_backer backup "$SOURCE_DIR" "$BACKUP_DIR" --pack tar --compress "$algo"
    archive="$BACKUP_DIR.tar${SUFFIX[$algo]}"
    if [[ ! -f "$archive" ]]; then
        log_err "compressed archive not found: $archive"
        exit 1
    fi
    comp_size=$(stat -c%s "$archive")
    # Ratio against the uncompressed tar (compressor input), not the raw
    # source dir — tiny test data has tar-header overhead > payload, which
    # would otherwise show nonsensical "inflation" ratios like 1:0.2.
    if [[ "$comp_size" -gt 0 ]]; then
        ratio=$(awk "BEGIN{printf \"%.1f\", $TAR_BASELINE/$comp_size}")
        saved=$(awk "BEGIN{printf \"%.0f\", ($TAR_BASELINE-$comp_size)*100/$TAR_BASELINE}")
    else
        ratio="∞"; saved="100"
    fi
    log_info "$(basename "$archive"): ${comp_size} B ← ${TAR_BASELINE} B [ratio 1:${ratio}, saved ${saved}%]"

    run_backer restore "$archive" "$RESTORE_DIR" --pack tar --decompress "$algo"
    log_ok "$algo round-trip"
done

# ── 4. Verify content integrity across all restores ──────────
log_info "[4/4] Content integrity check"
# Last restore was lzma; verify it matches source
diff_out=$(diff -rq "$SOURCE_DIR" "$RESTORE_DIR" 2>&1 | grep -v 'Fifo\|fifo\|No such file' || true)
if [[ -z "$diff_out" ]]; then
    log_ok "restored content matches source"
else
    log_err "content mismatch:"
    echo "$diff_out" | sed 's/^/    /'
    exit 1
fi

# ── Summary ──────────────────────────────────────────────────
log_ok "All operations completed"

show_listing() {
    local label="$1" path="$2"
    log_info "$label: $path"
    if [[ -e "$path" ]]; then
        ls -la "$path" 2>/dev/null | tail -n +2 | grep -v '^total' || true
    else
        echo "  (not present)"
    fi
}

show_listing "Source"  "$SOURCE_DIR"
show_listing "Backup"  "$BACKUP_DIR"
show_listing "Restore" "$RESTORE_DIR"

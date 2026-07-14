#pragma once

#include "scheduler/backup_scheduler.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace backer::cli {

struct BackupOptions {
    bool preserveMetadata = true;
    bool handleSpecial    = true;

    // ── Filter options ────────────────────────────────────────────────
    std::vector<std::string> includePaths;
    std::vector<std::string> excludePaths;
    std::vector<std::string> includeTypes;
    std::vector<std::string> excludeTypes;
    std::vector<std::string> includeNames;
    std::vector<std::string> excludeNames;
    std::string mtimeBefore;
    std::string mtimeAfter;
    uint64_t sizeMin   = 0;
    uint64_t sizeMax   = 0;
    bool hasSizeMin    = false;
    bool hasSizeMax    = false;
    std::string owner;

    // ── Pack options ──────────────────────────────────────────────────
    std::string packFormat;  // "tar" or empty

    // ── Compress options ─────────────────────────────────────────────
    std::string compressAlgo;  // "gzip", "zstd", "lzma" or empty
    int compressLevel         = 0;  // 0 = default for algorithm

    // ── Encrypt options ──────────────────────────────────────────────
    std::string encryptAlgo;  // "aes256", "sm4" or empty
    std::string password;     // encryption password (empty = prompt)
};

struct RestoreOptions {
    bool preserveMetadata = true;
    bool handleSpecial    = true;

    // ── Pack options (for archive mode) ───────────────────────────────
    std::string packFormat;  // "tar" or empty — source is an archive

    // ── Decompress options ───────────────────────────────────────────
    std::string decompressAlgo;  // "gzip", "zstd", "lzma" or empty

    // ── Decrypt options ──────────────────────────────────────────────
    std::string decryptAlgo;  // "aes256", "sm4" or empty
    std::string password;     // decryption password (empty = prompt)
};

/// ── Auto-naming helpers ──────────────────────────────────────────────────────

/// Generate an auto-named backup path inside @p destDir.
///
///   pack mode:   destDir / srcName_YYYYMMDD_HHMMSS.format
///   mirror mode: destDir / srcName_YYYYMMDD_HHMMSS
///
/// @p destDir is always treated as a directory (the backup repository).
/// The filename is derived from the source directory name + current timestamp.
std::filesystem::path makeBackupPath(
    std::filesystem::path const& destDir,
    std::filesystem::path const& source,
    std::string const& packFormat);

/// Generate an auto-named restore path inside @p destDir.
///
///   destDir / baseName_RYYYYMMDD_HHMMSS
///
/// @p baseName is the source filename with known archive/compress/encrypt
/// extensions stripped.  The trailing restore timestamp avoids collisions
/// when the same backup is restored multiple times.
std::filesystem::path makeRestorePath(
    std::filesystem::path const& destDir,
    std::filesystem::path const& source);

/// ── Command handlers ─────────────────────────────────────────────────────────

/// Execute `backup` command: source → destination.
/// Returns exit code (0 = success).
int handleBackup(std::filesystem::path const& source,
                 std::filesystem::path const& destination,
                 BackupOptions const& options = {});

/// Execute `restore` command: source (backup) → destination (restore target).
/// Returns exit code (0 = success).
int handleRestore(std::filesystem::path const& source,
                  std::filesystem::path const& destination,
                  RestoreOptions const& options = {});

// ── Schedule / Daemon ─────────────────────────────────────────────────────

/// Schedule subcommand dispatch.
/// argv is the remaining args after "schedule" is consumed.
int handleSchedule(std::vector<std::string> const& args);

/// Execute `daemon` command: load saved jobs and run the scheduler loop.
int handleDaemon();

} // namespace backer::cli

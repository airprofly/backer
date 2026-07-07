#pragma once

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
};

struct RestoreOptions {
    bool preserveMetadata = true;
    bool handleSpecial    = true;

    // ── Pack options (for archive mode) ───────────────────────────────
    std::string packFormat;  // "tar" or empty — source is an archive
};

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

} // namespace backer::cli

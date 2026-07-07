#pragma once

#include <filesystem>

namespace backer::cli {

struct BackupOptions {
    bool preserveMetadata = true;
    bool handleSpecial    = true;
};

struct RestoreOptions {
    bool preserveMetadata = true;
    bool handleSpecial    = true;
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

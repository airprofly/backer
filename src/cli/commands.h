#pragma once

#include <filesystem>

namespace backer::cli {

/// Execute `backup` command: source → destination.
/// Returns exit code (0 = success).
int handleBackup(std::filesystem::path const& source,
                 std::filesystem::path const& destination);

/// Execute `restore` command: source (backup) → destination (restore target).
/// Returns exit code (0 = success).
int handleRestore(std::filesystem::path const& source,
                  std::filesystem::path const& destination);

} // namespace backer::cli

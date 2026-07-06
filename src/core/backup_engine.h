#pragma once

#include "core/error_code.h"
#include "core/types.h"

#include <filesystem>
#include <memory>

namespace backer {

class FSAbstraction;

/// Orchestrates a full backup: walk source → copy files to destination
/// preserving the directory structure.
///
/// Phase 1 uses a simplified "directory mirror" approach — each source file
/// is directly copied to the backup location under the same relative path.
class BackupEngine {
public:
    /// @param fs  Filesystem abstraction for accessing source and destination.
    explicit BackupEngine(std::unique_ptr<FSAbstraction> fs);

    /// Execute a full backup from @p source to @p destination.
    ///
    /// @param source       Absolute path to the directory to back up.
    /// @param destination  Absolute path where the backup is written.
    BackupResult backup(
        std::filesystem::path const& source,
        std::filesystem::path const& destination);

private:
    std::unique_ptr<FSAbstraction> fs_;
};

} // namespace backer

#pragma once

#include "core/error_code.h"
#include "core/types.h"

#include <filesystem>
#include <memory>

namespace backer {

class FSAbstraction;

/// Orchestrates a full restore: walk backup → copy files back to destination
/// reconstructing the original directory tree.
///
/// Phase 1 uses the reverse of the simplified "directory mirror" approach.
class RestoreEngine {
public:
    /// @param fs  Filesystem abstraction for accessing backup and restore target.
    explicit RestoreEngine(std::unique_ptr<FSAbstraction> fs);

    /// Execute a full restore from @p source (backup dir) to @p destination.
    ///
    /// @param source       Absolute path to the backup directory.
    /// @param destination  Absolute path where files will be restored.
    BackupResult restore(
        std::filesystem::path const& source,
        std::filesystem::path const& destination);

private:
    std::unique_ptr<FSAbstraction> fs_;
};

} // namespace backer

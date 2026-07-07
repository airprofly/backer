#pragma once

#include "core/error_code.h"
#include "core/types.h"

#include <filesystem>
#include <memory>

namespace backer {

class FSAbstraction;
class Packer;

/// Orchestrates a full restore: (walk backup dir | unpack archive) → copy files back.
///
/// Two modes:
///   - Directory mirror (default): walk the backup directory tree and restore each file.
///   - Archive mode (packer != nullptr): unpack the archive file and restore all entries.
class RestoreEngine {
public:
    /// @param fs  Filesystem abstraction for accessing backup and restore target.
    explicit RestoreEngine(std::unique_ptr<FSAbstraction> fs);

    /// Execute a full restore from @p source (backup) to @p destination.
    ///
    /// @param source       In directory mode: path to the backup directory.
    ///                     In archive mode: path to the archive file.
    /// @param destination  Absolute path where files will be restored.
    /// @param packer       Optional packer for archive mode (null = directory mirror).
    BackupResult restore(
        std::filesystem::path const& source,
        std::filesystem::path const& destination,
        Packer* packer = nullptr);

private:
    std::unique_ptr<FSAbstraction> fs_;
};

} // namespace backer

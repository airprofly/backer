#pragma once

#include "core/error_code.h"
#include "core/types.h"

#include <filesystem>
#include <memory>

namespace backer {

class FSAbstraction;
class Filter;
class Packer;

/// Orchestrates a full backup: walk source → (filter → copy | pack) to destination.
///
/// Two modes:
///   - Directory mirror (default): each source file is copied to the
///     backup location preserving the directory structure.
///   - Archive mode (packer != nullptr): entries are packed into a
///     single archive file (e.g. tar) at the destination.
class BackupEngine {
public:
    /// @param fs  Filesystem abstraction for accessing source and destination.
    explicit BackupEngine(std::unique_ptr<FSAbstraction> fs);

    /// Execute a full backup from @p source to @p destination.
    ///
    /// @param source       Absolute path to the directory to back up.
    /// @param destination  Absolute path where the backup is written.
    ///                     In archive mode this is a file path (e.g. backup.tar).
    /// @param filter       Optional filter applied after walk (null = no filter).
    /// @param packer       Optional packer for archive mode (null = directory mirror).
    BackupResult backup(
        std::filesystem::path const& source,
        std::filesystem::path const& destination,
        Filter* filter = nullptr,
        Packer* packer = nullptr);

private:
    std::unique_ptr<FSAbstraction> fs_;
};

} // namespace backer

#pragma once

#include "core/error_code.h"
#include "core/expected.h"

#include <filesystem>
#include <string>
#include <vector>

namespace backer {

/// An entry discovered when listing a storage location.
struct StorageEntry {
    std::filesystem::path path;
    bool                  isDirectory = false;
    uint64_t              size = 0;
};

/// Abstract interface for storage backends.
///
/// In phase 1 only LocalStorage is implemented; future phases add
/// network/remote storage backends.
class Storage {
public:
    virtual ~Storage() = default;

    /// Check whether a path exists in this storage.
    virtual Expected<bool, ErrorCode> exists(
        std::filesystem::path const& path) = 0;

    /// Check whether a path is a directory.
    virtual Expected<bool, ErrorCode> isDirectory(
        std::filesystem::path const& path) = 0;

    /// Create a directory (including any missing parents).
    virtual Expected<void, ErrorCode> createDirectory(
        std::filesystem::path const& path) = 0;

    /// List the immediate children of a directory.
    virtual Expected<std::vector<StorageEntry>, ErrorCode> listDirectory(
        std::filesystem::path const& path) = 0;

    /// Read the entire contents of a file.
    virtual Expected<std::vector<char>, ErrorCode> readFile(
        std::filesystem::path const& path) = 0;

    /// Write data to a file (creating parent directories as needed).
    virtual Expected<void, ErrorCode> writeFile(
        std::filesystem::path const& path,
        std::vector<char> const& data) = 0;

    /// Get the size of a file in bytes.
    virtual Expected<uint64_t, ErrorCode> fileSize(
        std::filesystem::path const& path) = 0;
};

} // namespace backer

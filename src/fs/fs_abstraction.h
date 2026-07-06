#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/types.h"

#include <filesystem>
#include <vector>

namespace backer {

/// Abstract interface for filesystem operations.
///
/// Provides read, write, walk, and directory creation primitives.
/// Concrete implementations handle local filesystems, and future
/// network or virtual filesystems.
class FSAbstraction {
public:
    virtual ~FSAbstraction() = default;

    /// Recursively walk a directory tree and return all entries.
    /// The root directory itself is not included in the result.
    virtual Expected<std::vector<FileEntry>, ErrorCode>
        walk(std::filesystem::path const& root) = 0;

    /// Read entire contents of a file into memory.
    virtual Expected<std::vector<char>, ErrorCode>
        read(std::filesystem::path const& path) = 0;

    /// Write data to a file, creating parent directories as needed.
    virtual Expected<void, ErrorCode>
        write(std::filesystem::path const& path,
              std::vector<char> const& data) = 0;

    /// Create a directory (including any missing parents).
    virtual Expected<void, ErrorCode>
        mkdir(std::filesystem::path const& path) = 0;
};

// -----------------------------------------------------------------------
// Concrete implementation for the local file system
// -----------------------------------------------------------------------

/// FSAbstraction implementation using std::filesystem and C++ file streams.
///
/// Uses a 64 KiB buffer for file I/O.
class LocalFsAbstraction final : public FSAbstraction {
public:
    static constexpr std::size_t kBufferSize = 65536;  // 64 KiB

    LocalFsAbstraction() = default;

    Expected<std::vector<FileEntry>, ErrorCode>
        walk(std::filesystem::path const& root) override;

    Expected<std::vector<char>, ErrorCode>
        read(std::filesystem::path const& path) override;

    Expected<void, ErrorCode>
        write(std::filesystem::path const& path,
              std::vector<char> const& data) override;

    Expected<void, ErrorCode>
        mkdir(std::filesystem::path const& path) override;

private:
    /// Convert a generic filesystem error code to our ErrorCode.
    static ErrorCode fromErrno(int err) noexcept;
};

} // namespace backer

#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/types.h"

#include <filesystem>
#include <vector>

namespace backer {

/// Abstract interface for filesystem operations.
///
/// Provides read, write, walk, directory creation, special file support,
/// and metadata preservation primitives.
/// Concrete implementations handle local filesystems, and future
/// network or virtual filesystems.
class FSAbstraction {
public:
    virtual ~FSAbstraction() = default;

    /// Recursively walk a directory tree and return all entries.
    /// The root directory itself is not included in the result.
    /// All file types (regular, dir, symlink, fifo, block, char, socket)
    /// are detected and returned with their metadata.
    virtual Expected<std::vector<FileEntry>, ErrorCode>
        walk(std::filesystem::path const& root) = 0;

    /// Read entire contents of a file into memory.
    virtual Expected<std::vector<char>, ErrorCode>
        read(std::filesystem::path const& path) = 0;

    /// Write data to a filesystem object.
    ///
    /// For @p type == kRegular:   writes file content.
    /// For @p type == kSymlink:   creates a symlink; data is the target path.
    /// For @p type == kFifo:      creates a named pipe (data ignored).
    /// For @p type == kBlockDevice/kCharDevice: creates a device node (data ignored).
    /// For other types:           returns kSpecialFileNotSupported.
    virtual Expected<void, ErrorCode>
        write(std::filesystem::path const& path,
              std::vector<char> const& data,
              FileType type = FileType::kRegular) = 0;

    /// Create a directory (including any missing parents).
    virtual Expected<void, ErrorCode>
        mkdir(std::filesystem::path const& path) = 0;

    /// Read metadata (ownership, permissions, timestamps) from a path.
    virtual Expected<Metadata, ErrorCode>
        readMetadata(std::filesystem::path const& path) = 0;

    /// Restore metadata on an already-created filesystem object.
    /// @param restoreOwnership  false to skip lchown (for non-root).
    virtual Expected<void, ErrorCode>
        restoreMetadata(std::filesystem::path const& path,
                        Metadata const& meta,
                        bool restoreOwnership = true) = 0;
};

// -----------------------------------------------------------------------
// Concrete implementation for the local file system
// -----------------------------------------------------------------------

/// FSAbstraction implementation using C++17 std::filesystem,
/// POSIX lstat/chown/utimensat for metadata, and mkfifo/mknod for
/// special file creation.
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
              std::vector<char> const& data,
              FileType type = FileType::kRegular) override;

    Expected<void, ErrorCode>
        mkdir(std::filesystem::path const& path) override;

    Expected<Metadata, ErrorCode>
        readMetadata(std::filesystem::path const& path) override;

    Expected<void, ErrorCode>
        restoreMetadata(std::filesystem::path const& path,
                        Metadata const& meta,
                        bool restoreOwnership = true) override;

private:
    /// Convert a generic filesystem error code to our ErrorCode.
    static ErrorCode fromErrno(int err) noexcept;
};

} // namespace backer

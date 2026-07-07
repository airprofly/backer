#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/types.h"

#include <filesystem>
#include <string>

namespace backer {

/// Detect the FileType of a filesystem object using lstat().
///
/// Unlike std::filesystem::directory_entry helpers, this does not
/// follow symlinks — it reports the type of the entry itself.
Expected<FileType, ErrorCode> detectFileType(std::filesystem::path const& path);

/// Read the target of a symbolic link.
Expected<std::string, ErrorCode> readSymlink(std::filesystem::path const& path);

/// Create a symbolic link at @p link pointing to @p target.
Expected<void, ErrorCode> createSymlink(
    std::filesystem::path const& link,
    std::string const& target);

/// Create a FIFO (named pipe) at @p path with the given permissions.
Expected<void, ErrorCode> createFifo(
    std::filesystem::path const& path,
    uint32_t mode);

/// Create a block or character device node.
///
/// @param path  Filesystem path for the device node.
/// @param type  Either FileType::kBlockDevice or kCharDevice.
/// @param major Device major number.
/// @param minor Device minor number.
/// @param mode  Permission bits.
Expected<void, ErrorCode> createDevice(
    std::filesystem::path const& path,
    FileType type,
    uint64_t major,
    uint64_t minor,
    uint32_t mode);

} // namespace backer

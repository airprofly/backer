#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/types.h"

#include <filesystem>
#include <optional>
#include <string>

namespace backer {

/// Read complete metadata from a path using lstat().
///
/// Captures ownership (uid/gid), permissions (mode bits),
/// and timestamps (atime/mtime/ctime with nanosecond precision).
Expected<Metadata, ErrorCode> readMetadata(std::filesystem::path const& path);

/// Restore metadata on an already-created filesystem object.
///
/// Order of operations (matching Linux kernel expectations):
///   1. Ownership  — lchown() (works for symlinks too)
///   2. Permissions — fchmodat() (skipped for symlinks on Linux)
///   3. Timestamps  — utimensat() with AT_SYMLINK_NOFOLLOW
///
/// @param path              The filesystem path to apply metadata on.
/// @param meta              The metadata to restore.
/// @param restoreOwnership  Set to false when running as non-root to skip
///                          ownership restoration (lchown needs CAP_CHOWN).
Expected<void, ErrorCode> restoreMetadata(
    std::filesystem::path const& path,
    Metadata const& meta,
    bool restoreOwnership = true);

/// Serialize Metadata to a JSON string (for backup index).
std::string metadataToJson(Metadata const& meta);

/// Deserialize Metadata from a JSON string.
std::optional<Metadata> metadataFromJson(std::string const& json);

/// Check whether the current process can restore file ownership
/// (i.e. has CAP_CHOWN or is root).
bool canRestoreOwnership();

} // namespace backer

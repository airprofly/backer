#pragma once

#include <filesystem>

namespace backer {

/// Utility for converting between source paths, relative paths, and backup paths.
///
/// Handles the mapping:
///   source absolute  ↔  relative (under source root)  ↔  backup absolute
class PathMapper {
public:
    /// Convert an absolute path to one relative to sourceRoot.
    /// e.g. sourceToRelative("/home/user/a/b.txt", "/home/user") → "a/b.txt"
    static std::filesystem::path sourceToRelative(
        std::filesystem::path const& absolute,
        std::filesystem::path const& sourceRoot);

    /// Convert a relative path to an absolute backup path under backupRoot.
    /// e.g. relativeToBackup("a/b.txt", "/backup") → "/backup/a/b.txt"
    static std::filesystem::path relativeToBackup(
        std::filesystem::path const& relative,
        std::filesystem::path const& backupRoot);

    /// Convert a backup absolute path back to a relative path.
    /// e.g. backupToRelative("/backup/a/b.txt", "/backup") → "a/b.txt"
    static std::filesystem::path backupToRelative(
        std::filesystem::path const& backupPath,
        std::filesystem::path const& backupRoot);

    /// Convert a relative path to an absolute source path.
    /// e.g. relativeToSource("a/b.txt", "/home/user") → "/home/user/a/b.txt"
    static std::filesystem::path relativeToSource(
        std::filesystem::path const& relative,
        std::filesystem::path const& sourceRoot);
};

} // namespace backer

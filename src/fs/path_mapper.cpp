#include "fs/path_mapper.h"

namespace backer {

// static
std::filesystem::path PathMapper::sourceToRelative(
    std::filesystem::path const& absolute,
    std::filesystem::path const& sourceRoot)
{
    return absolute.lexically_relative(sourceRoot);
}

// static
std::filesystem::path PathMapper::relativeToBackup(
    std::filesystem::path const& relative,
    std::filesystem::path const& backupRoot)
{
    return backupRoot / relative;
}

// static
std::filesystem::path PathMapper::backupToRelative(
    std::filesystem::path const& backupPath,
    std::filesystem::path const& backupRoot)
{
    return backupPath.lexically_relative(backupRoot);
}

// static
std::filesystem::path PathMapper::relativeToSource(
    std::filesystem::path const& relative,
    std::filesystem::path const& sourceRoot)
{
    return sourceRoot / relative;
}

} // namespace backer

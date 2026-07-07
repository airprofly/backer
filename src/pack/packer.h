#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/types.h"

#include <filesystem>
#include <istream>
#include <ostream>
#include <string_view>
#include <vector>

namespace backer {

class FSAbstraction;

/// Abstract interface for archive packers.
///
/// Implementations pack a list of FileEntry objects (plus their content)
/// into a serialized archive format, and unpack archives back to files.
class Packer {
public:
    virtual ~Packer() = default;

    /// Pack a list of file entries into an archive written to @p output.
    ///
    /// @param files      Entries to pack. Relative paths are resolved
    ///                   against @p sourceRoot for reading content.
    /// @param fs         Filesystem abstraction for reading file content.
    /// @param sourceRoot Absolute root path; files are read from
    ///                   @p sourceRoot / entry.relativePath.
    /// @param output     Stream to write the archive to.
    virtual Expected<void, ErrorCode> pack(
        std::vector<FileEntry> const& files,
        FSAbstraction& fs,
        std::filesystem::path const& sourceRoot,
        std::ostream& output) = 0;

    /// Unpack an archive from @p input into @p dest directory.
    ///
    /// @param input  Stream to read the archive from.
    /// @param dest   Destination directory for restored files.
    /// @param fs     Filesystem abstraction for writing files.
    virtual Expected<void, ErrorCode> unpack(
        std::istream& input,
        std::filesystem::path const& dest,
        FSAbstraction& fs) = 0;

    /// Human-readable format name e.g. "tar", "zip".
    virtual std::string_view formatName() const noexcept = 0;
};

} // namespace backer

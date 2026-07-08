#pragma once

#include "pack/packer.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace backer {

/// Packer implementation for Zip format, using miniz library.
///
/// Stores a `.backer_zip_meta` entry in the archive root containing
/// full metadata for all entries (file type, ownership, permissions,
/// timestamps, symlink targets, device numbers).
///
/// | Entry type    | Zip representation                      |
/// |---------------|-----------------------------------------|
/// | Regular file  | Standard zip entry, deflate compressed  |
/// | Directory     | Zip entry with trailing '/'             |
/// | Symlink       | Content = symlink target path           |
/// | FIFO/Device   | Zero-byte entry                         |
///
/// All metadata is restored from `.backer_zip_meta` on unpack, since
/// standard Zip format does not preserve POSIX special file types
/// or nanosecond-precision timestamps.
class ZipPacker final : public Packer {
public:
    ZipPacker() = default;

    Expected<void, ErrorCode> pack(
        std::vector<FileEntry> const& files,
        FSAbstraction& fs,
        std::filesystem::path const& sourceRoot,
        std::ostream& output) override;

    Expected<void, ErrorCode> unpack(
        std::istream& input,
        std::filesystem::path const& dest,
        FSAbstraction& fs) override;

    std::string_view formatName() const noexcept override {
        return "zip";
    }

private:
    static constexpr std::size_t kBufferSize = 65536;

    // ── Metadata JSON helpers ────────────────────────────────────

    /// Build `.backer_zip_meta` content from file entries.
    static std::string buildMetaJson(std::vector<FileEntry> const& files);

    /// Parse `.backer_zip_meta` content into a map: path → JSON body.
    static std::map<std::string, std::string> parseMetaEntries(
        std::string const& json);

    /// Extract a numeric field by key from an entry JSON body.
    static std::optional<int64_t> extractField(
        std::string const& entryJson, std::string const& key);

    /// Extract a string field by key from an entry JSON body.
    static std::string extractStrField(
        std::string const& entryJson, std::string const& key);

    // ── Inner struct for parsed metadata ─────────────────────────

    struct EntryMeta {
        FileType type = FileType::kRegular;
        uint64_t size = 0;
        Metadata metadata;
        std::string symlinkTarget;
        uint64_t devMajor = 0;
        uint64_t devMinor = 0;
    };

    /// Parse a single entry JSON body into EntryMeta.
    static Expected<EntryMeta, ErrorCode> parseEntryMeta(
        std::string const& jsonBody, std::string const& path);
};

} // namespace backer

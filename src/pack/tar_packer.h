#pragma once

#include "pack/packer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace backer {

/// POSIX ustar Tar header — exactly 512 bytes.
#pragma pack(push, 1)
struct TarHeader {
    char name[100];       ///< File name (part of path if prefix is used)
    char mode[8];         ///< File permissions (octal)
    char uid[8];          ///< Owner UID (octal)
    char gid[8];          ///< Group GID (octal)
    char size[12];        ///< File size in bytes (octal)
    char mtime[12];       ///< Modification time (Unix timestamp, octal)
    char chksum[8];       ///< Header checksum (octal)
    char typeflag;        ///< File type indicator
    char linkname[100];   ///< Symlink target name
    char magic[6];        ///< Magic "ustar\0"
    char version[2];      ///< Version "00"
    char uname[32];       ///< Owner user name
    char gname[32];       ///< Owner group name
    char devmajor[8];     ///< Device major number (octal)
    char devminor[8];     ///< Device minor number (octal)
    char prefix[155];     ///< Filename prefix
    char padding[12];     ///< Zero padding
};
#pragma pack(pop)

static_assert(sizeof(TarHeader) == 512,
              "TarHeader must be exactly 512 bytes");

/// Packer implementation for POSIX ustar Tar format.
///
/// Supports: regular files, directories, symlinks, FIFOs, block/char devices.
/// Long paths (up to 255 chars) use the ustar prefix field.
/// End-of-archive is marked by two zero-filled 512-byte blocks.
class TarPacker final : public Packer {
public:
    TarPacker() = default;

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
        return "tar";
    }

    /// Verify checksum against header fields (public for testing).
    bool verifyChecksum(TarHeader const& header) const;

private:
    // ── Tar header encoding/decoding ───────────────────────────────────

    /// Encode a FileEntry into a raw TarHeader (checksum set).
    void encodeHeader(TarHeader& header, FileEntry const& entry) const;

    /// Decode a TarHeader into a FileEntry. Returns kInvalidArchive on error.
    Expected<FileEntry, ErrorCode> decodeHeader(TarHeader const& header) const;

    /// Compute and store the checksum field (spaces → octal sum).
    void setChecksum(TarHeader& header) const;

    // ── I/O helpers ───────────────────────────────────────────────────

    /// Write data and pad to 512-byte boundary with zeros.
    void writeAligned(std::ostream& output,
                      char const* data,
                      std::size_t size) const;

    /// Read exactly @p size bytes; return false on short read.
    bool readExact(std::istream& input, char* buf, std::size_t size) const;

    /// Skip over data blocks for an entry of @p dataSize bytes.
    void skipData(std::istream& input, uint64_t dataSize) const;

    // ── Format helpers ────────────────────────────────────────────────

    /// Format an integer as null-terminated octal string.
    static void formatOctal(char* buf, std::size_t bufSize, uint64_t value);

    /// Parse a null-terminated octal string.
    static uint64_t parseOctal(char const* buf, std::size_t size);

    /// Split path into name (≤100) and prefix (≤155) for ustar.
    /// Returns true if the path fits, false if truncated.
    static bool splitPath(std::string const& path,
                          char* name, std::size_t nameSize,
                          char* prefix, std::size_t prefixSize);

    /// Map FileType → tar typeflag.
    static char typeToFlag(FileType type);

    /// Map tar typeflag → FileType.
    static FileType flagToType(char flag);

    static constexpr std::size_t kBlockSize = 512;
};

} // namespace backer

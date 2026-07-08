#include "pack/tar_packer.h"
#include "fs/fs_abstraction.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <spdlog/spdlog.h>

namespace backer {

// ══════════════════════════════════════════════════════════════════════════════
// Format helpers
// ══════════════════════════════════════════════════════════════════════════════

void TarPacker::formatOctal(char* buf, std::size_t bufSize, uint64_t value) {
    int written = std::snprintf(buf, bufSize, "%0*lo",
                                static_cast<int>(bufSize - 2),
                                static_cast<unsigned long>(value));
    if (written < 0 || static_cast<std::size_t>(written) >= bufSize) {
        std::memset(buf, '0', bufSize - 1);
        buf[bufSize - 1] = '\0';
    }
}

uint64_t TarPacker::parseOctal(char const* buf, std::size_t size) {
    uint64_t result = 0;
    for (std::size_t i = 0; i < size; ++i) {
        char c = buf[i];
        if (c == '\0') break;
        if (c >= '0' && c <= '7') {
            result = (result << 3) | static_cast<uint64_t>(c - '0');
        }
    }
    return result;
}

bool TarPacker::splitPath(std::string const& path,
                           char* name, std::size_t nameSize,
                           char* prefix, std::size_t prefixSize)
{
    std::memset(name,   0, nameSize);
    std::memset(prefix, 0, prefixSize);

    if (path.empty()) return true;

    // Short path fits entirely in name field
    if (path.size() < nameSize) {
        std::strncpy(name, path.c_str(), nameSize - 1);
        name[nameSize - 1] = '\0';
        return true;
    }

    // Long path: split into prefix (≤155) + '/' + name (≤100)
    // Try splits from the rightmost '/' moving left until both parts fit
    auto slashPos = path.rfind('/');
    while (slashPos != std::string::npos && slashPos > 0) {
        std::string const prefixStr = path.substr(0, slashPos);
        std::string const nameStr   = path.substr(slashPos + 1);

        if (prefixStr.size() < prefixSize && nameStr.size() < nameSize) {
            std::strncpy(prefix, prefixStr.c_str(), prefixSize);
            std::strncpy(name,    nameStr.c_str(),   nameSize);
            return true;
        }

        // Move to the next '/' on the left
        if (slashPos == 0) break;
        slashPos = path.rfind('/', slashPos - 1);
    }

    // Cannot split cleanly — truncate name
    spdlog::warn("TarPacker: path too long, truncating name: {}", path);
    std::strncpy(name, path.c_str(), nameSize - 1);
    name[nameSize - 1] = '\0';
    std::memset(prefix, 0, prefixSize);
    return false;
}

char TarPacker::typeToFlag(FileType type) {
    switch (type) {
        case FileType::kRegular:     return '0';
        case FileType::kDirectory:   return '5';
        case FileType::kSymlink:     return '2';
        case FileType::kFifo:        return '6';
        case FileType::kBlockDevice: return '4';
        case FileType::kCharDevice:  return '3';
        case FileType::kSocket:
        default:                     return '0';
    }
}

FileType TarPacker::flagToType(char flag) {
    switch (flag) {
        case '0': case '\0': return FileType::kRegular;
        case '5':            return FileType::kDirectory;
        case '2':            return FileType::kSymlink;
        case '6':            return FileType::kFifo;
        case '4':            return FileType::kBlockDevice;
        case '3':            return FileType::kCharDevice;
        default:             return FileType::kUnknown;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Checksum
// ══════════════════════════════════════════════════════════════════════════════

void TarPacker::setChecksum(TarHeader& header) const {
    // POSIX: checksum field is filled with spaces before summing
    std::memset(header.chksum, ' ', 8);

    unsigned int sum = 0;
    auto const* bytes = reinterpret_cast<unsigned char const*>(&header);
    for (std::size_t i = 0; i < sizeof(TarHeader); ++i) {
        sum += bytes[i];
    }

    std::snprintf(header.chksum, 8, "%06o", sum);
    header.chksum[6] = '\0';
    header.chksum[7] = ' ';
}

bool TarPacker::verifyChecksum(TarHeader const& header) const {
    char saved[8];
    std::memcpy(saved, header.chksum, 8);

    unsigned int expected = 0;
    std::sscanf(saved, "%o", &expected);

    // Create mutable copy with blank checksum
    TarHeader copy;
    std::memcpy(&copy, &header, sizeof(TarHeader));
    std::memset(copy.chksum, ' ', 8);

    unsigned int sum = 0;
    auto const* bytes = reinterpret_cast<unsigned char const*>(&copy);
    for (std::size_t i = 0; i < sizeof(TarHeader); ++i) {
        sum += bytes[i];
    }

    return sum == expected;
}

// ══════════════════════════════════════════════════════════════════════════════
// Header encoding / decoding
// ══════════════════════════════════════════════════════════════════════════════

void TarPacker::encodeHeader(TarHeader& header, FileEntry const& entry) const {
    std::memset(&header, 0, sizeof(TarHeader));

    // Magic + version
    std::memcpy(header.magic,   "ustar", 5);
    std::memcpy(header.version, "00", 2);

    // Path → name + prefix
    auto pathStr = entry.relativePath.generic_string();
    splitPath(pathStr, header.name, 100, header.prefix, 155);

    // Permissions (lower 12 bits)
    formatOctal(header.mode, 8, entry.metadata.permissions & 07777);

    // Uid / gid
    formatOctal(header.uid, 8, entry.metadata.ownerId);
    formatOctal(header.gid, 8, entry.metadata.groupId);

    // Size
    formatOctal(header.size, 12, entry.size);

    // Modification time
    formatOctal(header.mtime, 12,
                static_cast<uint64_t>(entry.metadata.modifyTimeSec));

    // Type
    header.typeflag = typeToFlag(entry.type);

    // Symlink target
    if (entry.type == FileType::kSymlink) {
        auto linkStr = entry.symlinkTarget.generic_string();
        std::strncpy(header.linkname, linkStr.c_str(), 99);
        header.linkname[99] = '\0';
    }

    // Device numbers
    if (entry.type == FileType::kBlockDevice || entry.type == FileType::kCharDevice) {
        formatOctal(header.devmajor, 8, entry.deviceMajor);
        formatOctal(header.devminor, 8, entry.deviceMinor);
    }

    setChecksum(header);
}

Expected<FileEntry, ErrorCode> TarPacker::decodeHeader(
    TarHeader const& header) const
{
    // Check magic
    if (std::memcmp(header.magic, "ustar", 5) != 0) {
        return ErrorCode::kInvalidArchive;
    }

    if (!verifyChecksum(header)) {
        spdlog::error("TarPacker: checksum mismatch");
        return ErrorCode::kInvalidArchive;
    }

    FileEntry entry;

    // Reconstruct path from prefix + name
    std::string const prefix(reinterpret_cast<char const*>(header.prefix));
    std::string const name(reinterpret_cast<char const*>(header.name));
    if (!prefix.empty()) {
        entry.relativePath = std::filesystem::path(prefix) / name;
    } else {
        entry.relativePath = std::filesystem::path(name);
    }

    // Metadata
    entry.metadata.permissions = static_cast<uint32_t>(parseOctal(header.mode, 8));
    entry.metadata.ownerId     = static_cast<uint32_t>(parseOctal(header.uid, 8));
    entry.metadata.groupId     = static_cast<uint32_t>(parseOctal(header.gid, 8));
    entry.metadata.modifyTimeSec = static_cast<int64_t>(parseOctal(header.mtime, 12));

    // Size
    entry.size = parseOctal(header.size, 12);

    // Type
    entry.type = flagToType(header.typeflag);

    // Symlink target
    if (entry.type == FileType::kSymlink) {
        entry.symlinkTarget = std::string(
            reinterpret_cast<char const*>(header.linkname));
    }

    // Device numbers
    if (entry.type == FileType::kBlockDevice || entry.type == FileType::kCharDevice) {
        entry.deviceMajor = parseOctal(header.devmajor, 8);
        entry.deviceMinor = parseOctal(header.devminor, 8);
    }

    return entry;
}

// ══════════════════════════════════════════════════════════════════════════════
// I/O helpers
// ══════════════════════════════════════════════════════════════════════════════

void TarPacker::writeAligned(std::ostream& output,
                              char const* data,
                              std::size_t size) const
{
    output.write(data, static_cast<std::streamsize>(size));

    auto remainder = size % kBlockSize;
    if (remainder > 0) {
        char pad[kBlockSize] = {};
        output.write(pad, static_cast<std::streamsize>(kBlockSize - remainder));
    }
}

bool TarPacker::readExact(std::istream& input, char* buf, std::size_t size) const {
    if (size == 0) return true;
    input.read(buf, static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(input.gcount()) == size;
}

void TarPacker::skipData(std::istream& input, uint64_t dataSize) const {
    // Compute padding needed after dataSize bytes to reach 512-byte boundary
    uint64_t padding = (kBlockSize - (dataSize % kBlockSize)) % kBlockSize;
    uint64_t total = dataSize + padding;

    if (total == 0) return;

    // Try seeking first (works for stringstream / file streams)
    auto pos = input.tellg();
    input.seekg(static_cast<std::streamoff>(total), std::ios::cur);
    if (!input.fail()) return;

    // Fallback: read and discard
    input.clear();
    char discard[kBlockSize];
    while (total > 0) {
        auto chunk = std::min<uint64_t>(total, kBlockSize);
        input.read(discard, static_cast<std::streamsize>(chunk));
        total -= chunk;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Pack
// ══════════════════════════════════════════════════════════════════════════════

Expected<void, ErrorCode> TarPacker::pack(
    std::vector<FileEntry> const& files,
    FSAbstraction& fs,
    std::filesystem::path const& sourceRoot,
    std::ostream& output)
{
    // Sort: directories first so parents exist before content
    auto sorted = files;
    std::sort(sorted.begin(), sorted.end(),
        [](FileEntry const& a, FileEntry const& b) {
            if (a.type != b.type) {
                if (a.type == FileType::kDirectory) return true;
                if (b.type == FileType::kDirectory) return false;
            }
            return a.relativePath < b.relativePath;
        });

    TarHeader header;

    for (auto const& entry : sorted) {
        encodeHeader(header, entry);

        if (entry.type == FileType::kRegular) {
            // Read content from source
            auto absPath = sourceRoot / entry.relativePath;
            auto readResult = fs.read(absPath);

            if (!readResult) {
                // Can't read — write header with size=0
                formatOctal(header.size, 12, 0);
                setChecksum(header);
                output.write(reinterpret_cast<char const*>(&header),
                             static_cast<std::streamsize>(sizeof(TarHeader)));
                spdlog::warn("TarPacker: unreadable file {}, skipping content: {}",
                             absPath.string(), toString(readResult.error()));
                continue;
            }

            auto const& content = readResult.value();
            formatOctal(header.size, 12, content.size());
            setChecksum(header);

            output.write(reinterpret_cast<char const*>(&header),
                         static_cast<std::streamsize>(sizeof(TarHeader)));
            writeAligned(output, content.data(), content.size());
        } else {
            // Directories, symlinks, special files: header only
            output.write(reinterpret_cast<char const*>(&header),
                         static_cast<std::streamsize>(sizeof(TarHeader)));
        }
    }

    // End-of-archive: two zero-filled 512-byte blocks
    char end[kBlockSize * 2] = {};
    output.write(end, static_cast<std::streamsize>(kBlockSize * 2));

    return {};
}

// ══════════════════════════════════════════════════════════════════════════════
// Unpack
// ══════════════════════════════════════════════════════════════════════════════

Expected<void, ErrorCode> TarPacker::unpack(
    std::istream& input,
    std::filesystem::path const& dest,
    FSAbstraction& fs)
{
    TarHeader header;
    int zeroBlocks = 0;

    // Deferred directory metadata: restore AFTER all content is written,
    // sorted deepest-first, so child writes don't overwrite parent mtime.
    std::vector<std::pair<std::filesystem::path, Metadata>> dirsToRestore;

    while (true) {
        // Read header
        if (!readExact(input, reinterpret_cast<char*>(&header), sizeof(TarHeader))) {
            auto const nRead = input.gcount();
            if (input.eof() && nRead == 0) break;  // Empty input — valid
            spdlog::error("TarPacker: truncated or invalid archive");
            return ErrorCode::kInvalidArchive;
        }

        // Check for end-of-archive (two consecutive zero blocks)
        bool isZero = true;
        auto const* hdrBytes = reinterpret_cast<char const*>(&header);
        for (std::size_t i = 0; i < sizeof(TarHeader); ++i) {
            if (hdrBytes[i] != 0) { isZero = false; break; }
        }

        if (isZero) {
            ++zeroBlocks;
            if (zeroBlocks >= 2) break;
            continue;
        }
        zeroBlocks = 0;

        // Decode header
        auto entryResult = decodeHeader(header);
        if (!entryResult) {
            return entryResult.error();
        }

        auto const& entry = entryResult.value();
        uint64_t dataSize = entry.size;
        auto destPath = dest / entry.relativePath;

        switch (entry.type) {

        case FileType::kRegular: {
            // Create parent directories
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) { spdlog::warn("TarPacker: mkdir parent failed: {}", parent.string()); }
            }

            // Read content (handles both empty and non-empty files)
            std::vector<char> content(static_cast<std::size_t>(dataSize));
            if (dataSize > 0) {
                if (!readExact(input, content.data(), content.size())) {
                    spdlog::error("TarPacker: short data for {}", destPath.string());
                    return ErrorCode::kInvalidArchive;
                }
            }

            auto r = fs.write(destPath, content, FileType::kRegular);
            if (!r) {
                spdlog::warn("TarPacker: write failed for {}", destPath.string());
            }

            // Skip padding to next 512-byte boundary
            // (data bytes already consumed by readExact above)
            uint64_t remainder = dataSize % kBlockSize;
            if (remainder > 0) {
                input.ignore(static_cast<std::streamsize>(kBlockSize - remainder));
            }
            break;
        }

        case FileType::kDirectory: {
            auto r = fs.mkdir(destPath);
            if (!r) {
                spdlog::warn("TarPacker: mkdir failed for {}", destPath.string());
            }
            // Defer metadata restoration until all content is unpacked
            dirsToRestore.emplace_back(destPath, entry.metadata);
            // Directory entries in tar have no data, but skip just in case
            if (dataSize > 0) skipData(input, dataSize);
            continue;  // skip immediate metadata restore below
        }

        case FileType::kSymlink: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) { spdlog::warn("TarPacker: mkdir parent failed: {}", parent.string()); }
            }

            // Symlink target is in entry.symlinkTarget (from header linkname)
            std::string target = entry.symlinkTarget.generic_string();
            std::vector<char> symData(target.begin(), target.end());
            auto r = fs.write(destPath, symData, FileType::kSymlink);
            if (!r) {
                spdlog::warn("TarPacker: symlink failed for {}", destPath.string());
            }
            if (dataSize > 0) skipData(input, dataSize);
            break;
        }

        case FileType::kFifo: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) { spdlog::warn("TarPacker: mkdir parent failed: {}", parent.string()); }
            }

            std::vector<char> modeData(sizeof(uint32_t));
            *reinterpret_cast<uint32_t*>(modeData.data()) = entry.metadata.permissions;
            auto r = fs.write(destPath, modeData, FileType::kFifo);
            if (!r) {
                spdlog::warn("TarPacker: FIFO creation failed for {}", destPath.string());
            }
            if (dataSize > 0) skipData(input, dataSize);
            break;
        }

        case FileType::kBlockDevice:
        case FileType::kCharDevice: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) { spdlog::warn("TarPacker: mkdir parent failed: {}", parent.string()); }
            }

            std::vector<char> devData(sizeof(uint32_t));
            *reinterpret_cast<uint32_t*>(devData.data()) = entry.metadata.permissions;
            auto r = fs.write(destPath, devData, entry.type);
            if (!r) {
                spdlog::warn("TarPacker: device creation failed for {}", destPath.string());
            }
            if (dataSize > 0) skipData(input, dataSize);
            break;
        }

        case FileType::kSocket:
        default:
            spdlog::debug("TarPacker: skipping unsupported entry type {}",
                          static_cast<int>(entry.type));
            if (dataSize > 0) skipData(input, dataSize);
            break;
        }

        // Restore metadata for non-directory entries immediately
        auto metaResult = fs.restoreMetadata(destPath, entry.metadata, false);
        if (!metaResult) {
            spdlog::warn("TarPacker: metadata restore partially failed for {}",
                         destPath.string());
        }
    }

    // Deferred directory metadata: deepest first so child writes
    // no longer overwrite parent mtime after this point.
    std::sort(dirsToRestore.begin(), dirsToRestore.end(),
        [](auto const& a, auto const& b) {
            auto depthA = std::distance(a.first.begin(), a.first.end());
            auto depthB = std::distance(b.first.begin(), b.first.end());
            return depthA > depthB;
        });

    for (auto const& [dirPath, meta] : dirsToRestore) {
        auto metaResult = fs.restoreMetadata(dirPath, meta, false);
        if (!metaResult) {
            spdlog::warn("TarPacker: metadata restore partially failed for {}",
                         dirPath.string());
        }
    }

    return {};
}

} // namespace backer

#include "fs/fs_abstraction.h"
#include "fs/metadata.h"
#include "fs/path_mapper.h"
#include "fs/platform.h"
#include "fs/special_file.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <spdlog/spdlog.h>
#include <system_error>

#if BACKER_PLATFORM_POSIX
    #include <sys/stat.h> // lstat, S_IS*
    #if defined(__linux__)
        #include <sys/sysmacros.h> // major(), minor()
    #endif
#endif

namespace backer {

// ── helpers ──────────────────────────────────────────────────────────────

ErrorCode LocalFsAbstraction::fromErrno(int err) noexcept {
    switch (err) {
        case ENOENT:   return ErrorCode::kPathNotFound;
        case EACCES:
        case EPERM:    return ErrorCode::kPermissionDenied;
        case ENOSPC:   return ErrorCode::kDiskFull;
        case EFBIG:
        case EOVERFLOW: return ErrorCode::kFileTooLarge;
        case ELOOP:    return ErrorCode::kSymlinkLoop;
        case EIO:      return ErrorCode::kReadFailed;
        default:       return ErrorCode::kUnknown;
    }
}

// ── file type helpers ───────────────────────────────────────────────────

namespace {

#if BACKER_PLATFORM_POSIX
/// Convert a raw `struct stat` to our FileType enum (POSIX lstat).
FileType fileTypeFromStat(struct stat const& st) noexcept {
    if      (S_ISREG(st.st_mode))  return FileType::kRegular;
    else if (S_ISDIR(st.st_mode))  return FileType::kDirectory;
    else if (S_ISLNK(st.st_mode))  return FileType::kSymlink;
    else if (S_ISFIFO(st.st_mode)) return FileType::kFifo;
    else if (S_ISBLK(st.st_mode))  return FileType::kBlockDevice;
    else if (S_ISCHR(st.st_mode))  return FileType::kCharDevice;
    else if (S_ISSOCK(st.st_mode)) return FileType::kSocket;
    else                           return FileType::kUnknown;
}

/// Build a FileEntry from a struct stat + path.
FileEntry makeFileEntry(
    std::filesystem::path const& absolutePath,
    std::filesystem::path const& root,
    struct stat const& st)
{
    FileEntry fe;
    fe.relativePath = PathMapper::sourceToRelative(absolutePath, root);
    fe.type         = fileTypeFromStat(st);

    // Metadata
    fe.metadata.ownerId       = static_cast<uint32_t>(st.st_uid);
    fe.metadata.groupId       = static_cast<uint32_t>(st.st_gid);
    fe.metadata.permissions   = static_cast<uint32_t>(st.st_mode & static_cast<mode_t>(07777));
    fe.metadata.accessTimeSec  = st.st_atim.tv_sec;
    fe.metadata.accessTimeNsec = st.st_atim.tv_nsec;
    fe.metadata.modifyTimeSec  = st.st_mtim.tv_sec;
    fe.metadata.modifyTimeNsec = st.st_mtim.tv_nsec;
    fe.metadata.changeTimeSec  = st.st_ctim.tv_sec;
    fe.metadata.changeTimeNsec = st.st_ctim.tv_nsec;

    // Size (regular files only)
    if (fe.type == FileType::kRegular) {
        fe.size = static_cast<uint64_t>(st.st_size);
    }

    // Symlink target
    if (fe.type == FileType::kSymlink) {
        std::error_code ec;
        auto target = std::filesystem::read_symlink(absolutePath, ec);
        if (!ec) {
            fe.symlinkTarget = target;
        }
    }

    // Device numbers (block/char devices)
    if (fe.type == FileType::kBlockDevice || fe.type == FileType::kCharDevice) {
        fe.deviceMajor = major(st.st_rdev);
        fe.deviceMinor = minor(st.st_rdev);
    }

    return fe;
}
#endif // BACKER_PLATFORM_POSIX

/// Build a FileEntry from std::filesystem info (non-POSIX fallback).
FileEntry makeFileEntryPortable(
    std::filesystem::path const& absolutePath,
    std::filesystem::path const& root,
    std::filesystem::file_status const& status)
{
    FileEntry fe;
    fe.relativePath = PathMapper::sourceToRelative(absolutePath, root);

    switch (status.type()) {
        case std::filesystem::file_type::regular:   fe.type = FileType::kRegular; break;
        case std::filesystem::file_type::directory: fe.type = FileType::kDirectory; break;
        case std::filesystem::file_type::symlink:   fe.type = FileType::kSymlink; break;
        default:                                    fe.type = FileType::kUnknown; break;
    }

    // Read metadata via std::filesystem (limited on non-POSIX)
    auto perms = status.permissions();
    fe.metadata.permissions = static_cast<uint32_t>(perms);

    if (fe.type == FileType::kRegular) {
        std::error_code ec;
        auto size = std::filesystem::file_size(absolutePath, ec);
        if (!ec) fe.size = static_cast<uint64_t>(size);
    }

    if (fe.type == FileType::kSymlink) {
        std::error_code ec;
        auto target = std::filesystem::read_symlink(absolutePath, ec);
        if (!ec) fe.symlinkTarget = target;
    }

    return fe;
}

} // anonymous namespace

// ── walk ─────────────────────────────────────────────────────────────────

Expected<std::vector<FileEntry>, ErrorCode>
LocalFsAbstraction::walk(std::filesystem::path const& root)
{
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return fromErrno(ec.value() ? ec.value() : ENOENT);
    }
    if (!std::filesystem::is_directory(root, ec)) {
        spdlog::error("walk: root is not a directory: {}", root.string());
        return ErrorCode::kPathNotFound;
    }

    std::vector<FileEntry> entries;

    auto iter = std::filesystem::recursive_directory_iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        ec);

    if (ec) {
        spdlog::error("walk: cannot iterate directory {}: {}", root.string(), ec.message());
        return fromErrno(ec.value());
    }

    for (auto const& dirEntry : iter) {
        auto const& path = dirEntry.path();

#if BACKER_PLATFORM_POSIX
        // Use lstat() for accurate type detection (do not follow symlinks)
        struct stat st;
        if (lstat(path.c_str(), &st) != 0) {
            int err = errno;
            spdlog::warn("walk: lstat({}) failed: {} — skipping", path.string(), std::strerror(err));
            continue;
        }

        auto type = fileTypeFromStat(st);
        if (type == FileType::kUnknown) {
            SPDLOG_TRACE("walk: skipping unknown entry: {}", path.string());
            continue;
        }

        auto fe = makeFileEntry(path, root, st);
        entries.push_back(std::move(fe));
#else
        // Non-POSIX fallback: use std::filesystem status
        auto status = dirEntry.symlink_status(ec);
        if (ec) continue;
        auto fe = makeFileEntryPortable(path, root, status);
        if (fe.type == FileType::kUnknown) {
            SPDLOG_TRACE("walk: skipping unknown entry: {}", path.string());
            continue;
        }
        entries.push_back(std::move(fe));
#endif
    }

    if (ec) {
        spdlog::warn("walk: iteration may be incomplete: {}", ec.message());
    }

    return entries;
}

// ── read ─────────────────────────────────────────────────────────────────

Expected<std::vector<char>, ErrorCode>
LocalFsAbstraction::read(std::filesystem::path const& path)
{
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        spdlog::error("read: cannot get size of {}: {}", path.string(), ec.message());
        return fromErrno(ec.value());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        spdlog::error("read: cannot open {}: {}", path.string(), std::strerror(errno));
        return fromErrno(errno);
    }

    std::vector<char> buffer(size ? size : 1024);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    auto const bytesRead = static_cast<std::size_t>(file.gcount());

    if (!file && !file.eof()) {
        spdlog::error("read: read error on {}: {}", path.string(), std::strerror(errno));
        return ErrorCode::kReadFailed;
    }

    buffer.resize(bytesRead);
    return buffer;
}

// ── write ────────────────────────────────────────────────────────────────

Expected<void, ErrorCode>
LocalFsAbstraction::write(
    std::filesystem::path const& path,
    std::vector<char> const& data,
    FileType type)
{
    // Create parent directories for all types
    auto parent = path.parent_path();
    if (!parent.empty()) {
        auto mkdirResult = mkdir(parent);
        if (!mkdirResult) {
            return mkdirResult.error();
        }
    }

    switch (type) {

    case FileType::kRegular: {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            spdlog::error("write: cannot open {}: {}", path.string(), std::strerror(errno));
            return fromErrno(errno);
        }
        // Write in 64 KiB chunks
        std::size_t offset = 0;
        while (offset < data.size()) {
            auto chunkSize = std::min(kBufferSize, data.size() - offset);
            file.write(data.data() + offset, static_cast<std::streamsize>(chunkSize));
            if (!file) {
                if (errno == ENOSPC) {
                    return ErrorCode::kDiskFull;
                }
                spdlog::error("write: write error on {}: {}", path.string(), std::strerror(errno));
                return ErrorCode::kWriteFailed;
            }
            offset += chunkSize;
        }
        return {};
    }

    case FileType::kSymlink: {
        std::string target(data.begin(), data.end());
        return createSymlink(path, target);
    }

    case FileType::kFifo: {
        mode_t mode = data.size() >= sizeof(mode_t)
                          ? *reinterpret_cast<mode_t const*>(data.data())
                          : 0644;
        return createFifo(path, static_cast<uint32_t>(mode));
    }

    case FileType::kBlockDevice:
    case FileType::kCharDevice: {
        mode_t mode = data.size() >= sizeof(mode_t)
                          ? *reinterpret_cast<mode_t const*>(data.data())
                          : 0644;
        return createDevice(path, type, 0, 0, static_cast<uint32_t>(mode));
    }

    default:
        spdlog::error("write: unsupported file type {} for path {}",
                      static_cast<int>(type), path.string());
        return ErrorCode::kSpecialFileNotSupported;
    }
}

// ── mkdir ────────────────────────────────────────────────────────────────

Expected<void, ErrorCode>
LocalFsAbstraction::mkdir(std::filesystem::path const& path)
{
    std::error_code ec;
    if (std::filesystem::create_directories(path, ec)) {
        return {};
    }
    if (ec) {
        spdlog::error("mkdir: cannot create {}: {}", path.string(), ec.message());
        return fromErrno(ec.value());
    }
    return {};
}

// ── readMetadata ─────────────────────────────────────────────────────────

Expected<Metadata, ErrorCode>
LocalFsAbstraction::readMetadata(std::filesystem::path const& path)
{
    return backer::readMetadata(path);
}

// ── restoreMetadata ──────────────────────────────────────────────────────

Expected<void, ErrorCode>
LocalFsAbstraction::restoreMetadata(
    std::filesystem::path const& path,
    Metadata const& meta,
    bool restoreOwnership)
{
    return backer::restoreMetadata(path, meta, restoreOwnership);
}

} // namespace backer

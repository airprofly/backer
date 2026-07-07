#include "fs/special_file.h"
#include "fs/platform.h"

#include <cerrno>
#include <cstring>
#include <spdlog/spdlog.h>

#if BACKER_PLATFORM_POSIX
    #include <climits>         // PATH_MAX
    #include <sys/stat.h>      // lstat, mknod, mkfifo, S_IS*
    #include <unistd.h>        // readlink, symlink
    #if defined(__linux__)
        #include <sys/sysmacros.h> // major(), minor(), makedev() — Linux only
    #endif
#endif

namespace backer {

// ── detectFileType ─────────────────────────────────────────────────────────

Expected<FileType, ErrorCode> detectFileType(std::filesystem::path const& path)
{
#if BACKER_PLATFORM_POSIX
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        int err = errno;
        spdlog::error("detectFileType: lstat({}) failed: {}",
                      path.string(), std::strerror(err));
        switch (err) {
            case ENOENT:  return ErrorCode::kPathNotFound;
            case EACCES:  return ErrorCode::kPermissionDenied;
            default:      return ErrorCode::kUnknown;
        }
    }

    if      (S_ISREG(st.st_mode))  return FileType::kRegular;
    else if (S_ISDIR(st.st_mode))  return FileType::kDirectory;
    else if (S_ISLNK(st.st_mode))  return FileType::kSymlink;
    else if (S_ISFIFO(st.st_mode)) return FileType::kFifo;
    else if (S_ISBLK(st.st_mode))  return FileType::kBlockDevice;
    else if (S_ISCHR(st.st_mode))  return FileType::kCharDevice;
    else if (S_ISSOCK(st.st_mode)) return FileType::kSocket;
    else                           return FileType::kUnknown;
#else
    // Non-POSIX: fall back to std::filesystem (no special file detection)
    std::error_code ec;
    auto status = std::filesystem::symlink_status(path, ec);
    if (ec) return ErrorCode::kPathNotFound;

    switch (status.type()) {
        case std::filesystem::file_type::regular:    return FileType::kRegular;
        case std::filesystem::file_type::directory:  return FileType::kDirectory;
        case std::filesystem::file_type::symlink:    return FileType::kSymlink;
        default:                                     return FileType::kUnknown;
    }
#endif
}

// ── readSymlink ────────────────────────────────────────────────────────

Expected<std::string, ErrorCode> readSymlink(std::filesystem::path const& path)
{
#if BACKER_PLATFORM_POSIX
    std::string buf;
    buf.resize(PATH_MAX);

    auto len = readlink(path.c_str(), buf.data(), buf.size());
    if (len < 0) {
        int err = errno;
        spdlog::error("readSymlink: readlink({}) failed: {}",
                      path.string(), std::strerror(err));
        switch (err) {
            case ENOENT:  return ErrorCode::kPathNotFound;
            case EACCES:  return ErrorCode::kPermissionDenied;
            case ELOOP:   return ErrorCode::kSymlinkLoop;
            default:      return ErrorCode::kReadFailed;
        }
    }

    buf.resize(static_cast<std::size_t>(len));
    return buf;
#else
    // Non-POSIX: use std::filesystem::read_symlink
    std::error_code ec;
    auto target = std::filesystem::read_symlink(path, ec);
    if (ec) return ErrorCode::kReadFailed;
    return target.string();
#endif
}

// ── createSymlink ──────────────────────────────────────────────────────────

Expected<void, ErrorCode> createSymlink(
    std::filesystem::path const& link,
    std::string const& target)
{
#if BACKER_PLATFORM_POSIX
    if (symlink(target.c_str(), link.c_str()) != 0) {
        int err = errno;
        spdlog::error("createSymlink: symlink({} → {}) failed: {}",
                      link.string(), target, std::strerror(err));
        switch (err) {
            case EEXIST:  return ErrorCode::kWriteFailed;
            case EACCES:
            case EPERM:   return ErrorCode::kPermissionDenied;
            default:      return ErrorCode::kWriteFailed;
        }
    }
    return {};
#else
    // Non-POSIX: use std::filesystem::create_symlink
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    if (ec) return ErrorCode::kWriteFailed;
    return {};
#endif
}

// ── createFifo ─────────────────────────────────────────────────────────────

Expected<void, ErrorCode> createFifo(
    std::filesystem::path const& path,
    uint32_t mode)
{
#if BACKER_PLATFORM_POSIX
    if (mkfifo(path.c_str(), static_cast<mode_t>(mode)) != 0) {
        int err = errno;
        spdlog::error("createFifo: mkfifo({}) failed: {}",
                      path.string(), std::strerror(err));
        switch (err) {
            case EEXIST:  return ErrorCode::kWriteFailed;
            case EACCES:
            case EPERM:   return ErrorCode::kPermissionDenied;
            default:      return ErrorCode::kWriteFailed;
        }
    }
    return {};
#else
    (void)path;
    (void)mode;
    spdlog::warn("createFifo: FIFO not supported on this platform");
    return ErrorCode::kSpecialFileNotSupported;
#endif
}

// ── createDevice ───────────────────────────────────────────────────────────

Expected<void, ErrorCode> createDevice(
    std::filesystem::path const& path,
    FileType type,
    uint64_t major,
    uint64_t minor,
    uint32_t mode)
{
#if BACKER_PLATFORM_POSIX
    if (type != FileType::kBlockDevice && type != FileType::kCharDevice) {
        return ErrorCode::kSpecialFileNotSupported;
    }

    auto dev = makedev(static_cast<unsigned>(major), static_cast<unsigned>(minor));
    auto fileType = (type == FileType::kBlockDevice) ? S_IFBLK : S_IFCHR;
    auto fullMode = fileType | static_cast<mode_t>(mode);

    if (mknod(path.c_str(), fullMode, dev) != 0) {
        int err = errno;
        spdlog::error("createDevice: mknod({}, major={}, minor={}) failed: {}",
                      path.string(), major, minor, std::strerror(err));
        switch (err) {
            case EEXIST:  return ErrorCode::kWriteFailed;
            case EACCES:
            case EPERM:   return ErrorCode::kPermissionDenied;
            default:      return ErrorCode::kWriteFailed;
        }
    }
    return {};
#else
    (void)path;
    (void)type;
    (void)major;
    (void)minor;
    (void)mode;
    spdlog::warn("createDevice: device nodes not supported on this platform");
    return ErrorCode::kSpecialFileNotSupported;
#endif
}

} // namespace backer

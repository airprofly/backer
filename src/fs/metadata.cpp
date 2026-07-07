#include "fs/metadata.h"
#include "fs/platform.h"

#include <cerrno>
#include <cstring>
#include <spdlog/spdlog.h>

#if BACKER_PLATFORM_POSIX
    #include <fcntl.h>    // AT_FDCWD, AT_SYMLINK_NOFOLLOW
    #include <sys/stat.h> // lstat, struct stat
    #include <unistd.h>   // lchown, geteuid
#endif

namespace backer {

// ── helpers ────────────────────────────────────────────────────────────────

namespace {

#if BACKER_PLATFORM_POSIX
/// Attempt to restore ownership (uid/gid) via lchown.
/// Works for all file types including symlinks.
ErrorCode restoreOwner(std::filesystem::path const& path,
                       uid_t uid, gid_t gid) noexcept
{
    if (lchown(path.c_str(), uid, gid) != 0) {
        int err = errno;
        spdlog::warn("restoreMetadata: lchown({}) failed: {}",
                     path.string(), std::strerror(err));
        return err == EPERM ? ErrorCode::kNotRoot : ErrorCode::kMetadataRestoreFailed;
    }
    return ErrorCode::kOk;
}

/// Restore permissions using fchmodat.
/// Skipped for symlinks — Linux ignores symlink permissions.
ErrorCode restorePermissions(std::filesystem::path const& path,
                             mode_t mode, bool isSymlink) noexcept
{
    if (isSymlink) {
        return ErrorCode::kOk;  // symlink permissions are always 0777 on Linux
    }
    if (fchmodat(AT_FDCWD, path.c_str(), mode, 0) != 0) {
        spdlog::warn("restoreMetadata: chmod({}, {:o}) failed: {}",
                     path.string(), static_cast<unsigned>(mode),
                     std::strerror(errno));
        return ErrorCode::kMetadataRestoreFailed;
    }
    return ErrorCode::kOk;
}

/// Restore atime/mtime using utimensat with AT_SYMLINK_NOFOLLOW
/// so symlinks are handled correctly.
ErrorCode restoreTimestamps(std::filesystem::path const& path,
                            timespec atime, timespec mtime) noexcept
{
    timespec times[2] = {atime, mtime};
    if (utimensat(AT_FDCWD, path.c_str(), times, AT_SYMLINK_NOFOLLOW) != 0) {
        spdlog::warn("restoreMetadata: utimensat({}) failed: {}",
                     path.string(), std::strerror(errno));
        return ErrorCode::kMetadataRestoreFailed;
    }
    return ErrorCode::kOk;
}
#endif // BACKER_PLATFORM_POSIX

} // anonymous namespace

// ── readMetadata ───────────────────────────────────────────────────────────

Expected<Metadata, ErrorCode>
readMetadata(std::filesystem::path const& path)
{
#if BACKER_PLATFORM_POSIX
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        int err = errno;
        spdlog::error("readMetadata: lstat({}) failed: {}",
                      path.string(), std::strerror(err));
        switch (err) {
            case ENOENT:  return ErrorCode::kPathNotFound;
            case EACCES:
            case EPERM:   return ErrorCode::kPermissionDenied;
            default:      return ErrorCode::kUnknown;
        }
    }

    Metadata meta;
    meta.ownerId       = static_cast<uint32_t>(st.st_uid);
    meta.groupId       = static_cast<uint32_t>(st.st_gid);
    meta.permissions   = static_cast<uint32_t>(st.st_mode & static_cast<mode_t>(07777));
    meta.accessTimeSec  = st.st_atim.tv_sec;
    meta.accessTimeNsec = st.st_atim.tv_nsec;
    meta.modifyTimeSec  = st.st_mtim.tv_sec;
    meta.modifyTimeNsec = st.st_mtim.tv_nsec;
    meta.changeTimeSec  = st.st_ctim.tv_sec;
    meta.changeTimeNsec = st.st_ctim.tv_nsec;
    return meta;
#else
    // Non-POSIX: use std::filesystem for basic info
    std::error_code ec;
    auto status = std::filesystem::symlink_status(path, ec);
    if (ec) return ErrorCode::kPathNotFound;

    Metadata meta;
    // On Windows, owner/group info is not easily available
    meta.permissions = static_cast<uint32_t>(status.permissions());
    // Use file system clock for timestamps
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        auto dur = ftime.time_since_epoch();
        meta.modifyTimeSec = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
        meta.accessTimeSec = meta.modifyTimeSec;  // best approximation
    }
    return meta;
#endif
}

// ── restoreMetadata ────────────────────────────────────────────────────────

Expected<void, ErrorCode> restoreMetadata(
    std::filesystem::path const& path,
    Metadata const& meta,
    bool restoreOwnership)
{
#if BACKER_PLATFORM_POSIX
    bool const isSymlink = std::filesystem::is_symlink(path);

    // 1. Ownership (optional — needs CAP_CHOWN)
    if (restoreOwnership) {
        auto ec = restoreOwner(path,
                               static_cast<uid_t>(meta.ownerId),
                               static_cast<gid_t>(meta.groupId));
        if (ec != ErrorCode::kOk) {
            // Non-fatal: continue with permissions and timestamps
            spdlog::warn("restoreMetadata: ownership skipped for {} (not root?)",
                         path.string());
        }
    }

    // 2. Permissions (skipped for symlinks on Linux)
    auto permErr = restorePermissions(
        path,
        static_cast<mode_t>(meta.permissions),
        isSymlink);

    // 3. Timestamps (last — best-effort after content is written)
    auto tsErr = restoreTimestamps(
        path,
        timespec{meta.accessTimeSec, meta.accessTimeNsec},
        timespec{meta.modifyTimeSec, meta.modifyTimeNsec});

    if (permErr != ErrorCode::kOk) return permErr;
    if (tsErr  != ErrorCode::kOk) return tsErr;
    return {};
#else
    // Non-POSIX: use std::filesystem for permissions
    (void)restoreOwnership;
    std::error_code ec;
    std::filesystem::permissions(
        path,
        static_cast<std::filesystem::perms>(meta.permissions),
        std::filesystem::perm_options::replace,
        ec);
    if (ec) {
        spdlog::warn("restoreMetadata: chmod({}) failed: {}", path.string(), ec.message());
        return ErrorCode::kMetadataRestoreFailed;
    }
    return {};
#endif
}

// ── JSON serialization ────────────────────────────────────────────────────

std::string metadataToJson(Metadata const& meta)
{
    // Minimal JSON without external dependency.
    return
        "{\"uid\":"       + std::to_string(meta.ownerId) +
        ",\"gid\":"       + std::to_string(meta.groupId) +
        ",\"mode\":"      + std::to_string(meta.permissions) +
        ",\"atime_sec\":" + std::to_string(meta.accessTimeSec) +
        ",\"atime_nsec\":"+ std::to_string(meta.accessTimeNsec) +
        ",\"mtime_sec\":" + std::to_string(meta.modifyTimeSec) +
        ",\"mtime_nsec\":"+ std::to_string(meta.modifyTimeNsec) +
        ",\"ctime_sec\":" + std::to_string(meta.changeTimeSec) +
        ",\"ctime_nsec\":"+ std::to_string(meta.changeTimeNsec) +
        "}";
}

std::optional<Metadata> metadataFromJson(std::string const& json)
{
    // Minimal parser: extracts numeric fields by key prefix.
    auto extract = [&](std::string const& key) -> std::optional<int64_t> {
        auto pos = json.find("\"" + key + "\":");
        if (pos == std::string::npos) return std::nullopt;
        pos += key.size() + 3;  // skip "key":
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
        if (pos >= json.size()) return std::nullopt;
        bool neg = false;
        if (json[pos] == '-') { neg = true; ++pos; }
        if (pos >= json.size() || json[pos] < '0' || json[pos] > '9') return std::nullopt;
        int64_t val = 0;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
            val = val * 10 + (json[pos++] - '0');
        }
        return neg ? -val : val;
    };

    auto uid  = extract("uid");
    auto gid  = extract("gid");
    auto mode = extract("mode");
    auto ats  = extract("atime_sec");
    auto atns = extract("atime_nsec");
    auto mts  = extract("mtime_sec");
    auto mtns = extract("mtime_nsec");
    auto cts  = extract("ctime_sec");
    auto ctns = extract("ctime_nsec");

    if (!uid || !gid || !mode) return std::nullopt;

    Metadata meta;
    meta.ownerId        = static_cast<uint32_t>(*uid);
    meta.groupId        = static_cast<uint32_t>(*gid);
    meta.permissions    = static_cast<uint32_t>(*mode);
    meta.accessTimeSec  = ats.value_or(0);
    meta.accessTimeNsec = atns.value_or(0);
    meta.modifyTimeSec  = mts.value_or(0);
    meta.modifyTimeNsec = mtns.value_or(0);
    meta.changeTimeSec  = cts.value_or(0);
    meta.changeTimeNsec = ctns.value_or(0);
    return meta;
}

// ── canRestoreOwnership ──────────────────────────────────────────────────

bool canRestoreOwnership()
{
#if BACKER_PLATFORM_POSIX
    // Simple heuristic: root can always change ownership.
    return geteuid() == 0;
#else
    return false;
#endif
}

} // namespace backer

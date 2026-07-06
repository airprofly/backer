#include "fs/fs_abstraction.h"
#include "fs/path_mapper.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <spdlog/spdlog.h>
#include <system_error>

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
        std::error_code entryEc;
        auto const& path = dirEntry.path();

        // Determine file type
        FileType type;
        if (dirEntry.is_regular_file(entryEc)) {
            type = FileType::kRegular;
        } else if (dirEntry.is_directory(entryEc)) {
            type = FileType::kDirectory;
        } else if (dirEntry.is_symlink(entryEc)) {
            type = FileType::kSymlink;
        } else {
            SPDLOG_TRACE("walk: skipping unsupported entry: {}", path.string());
            continue;
        }

        FileEntry fe;
        fe.relativePath = PathMapper::sourceToRelative(path, root);
        fe.type = type;

        if (type == FileType::kRegular) {
            fe.size = static_cast<uint64_t>(dirEntry.file_size(entryEc));
        } else if (type == FileType::kSymlink) {
            auto target = std::filesystem::read_symlink(path, entryEc);
            if (!entryEc) {
                fe.symlinkTarget = target;
            }
        }

        entries.push_back(std::move(fe));
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
    std::vector<char> const& data)
{
    // Create parent directories
    auto parent = path.parent_path();
    if (!parent.empty()) {
        auto mkdirResult = mkdir(parent);
        if (!mkdirResult) {
            return mkdirResult.error();
        }
    }

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

} // namespace backer

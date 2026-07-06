#include "storage/local_storage.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <spdlog/spdlog.h>

namespace backer {

// ── helpers ──────────────────────────────────────────────────────────────

namespace {

ErrorCode fromErrno(int err) noexcept {
    switch (err) {
        case ENOENT:   return ErrorCode::kPathNotFound;
        case EACCES:
        case EPERM:    return ErrorCode::kPermissionDenied;
        case ENOSPC:   return ErrorCode::kDiskFull;
        case EIO:      return ErrorCode::kReadFailed;
        default:       return ErrorCode::kUnknown;
    }
}

} // anonymous namespace

// ── exists ───────────────────────────────────────────────────────────────

Expected<bool, ErrorCode> LocalStorage::exists(
    std::filesystem::path const& path)
{
    std::error_code ec;
    bool result = std::filesystem::exists(path, ec);
    if (ec) {
        return fromErrno(ec.value());
    }
    return result;
}

// ── isDirectory ──────────────────────────────────────────────────────────

Expected<bool, ErrorCode> LocalStorage::isDirectory(
    std::filesystem::path const& path)
{
    std::error_code ec;
    bool result = std::filesystem::is_directory(path, ec);
    if (ec) {
        return fromErrno(ec.value());
    }
    return result;
}

// ── createDirectory ──────────────────────────────────────────────────────

Expected<void, ErrorCode> LocalStorage::createDirectory(
    std::filesystem::path const& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        spdlog::error("LocalStorage: cannot create directory {}: {}",
                      path.string(), ec.message());
        return fromErrno(ec.value());
    }
    return {};
}

// ── listDirectory ────────────────────────────────────────────────────────

Expected<std::vector<StorageEntry>, ErrorCode>
LocalStorage::listDirectory(std::filesystem::path const& path)
{
    std::error_code ec;
    std::vector<StorageEntry> entries;

    for (auto const& entry : std::filesystem::directory_iterator(path, ec)) {
        std::error_code entryEc;
        StorageEntry se;
        se.path = entry.path();
        se.isDirectory = entry.is_directory(entryEc);
        if (!entryEc && entry.is_regular_file(entryEc)) {
            se.size = static_cast<uint64_t>(entry.file_size(entryEc));
        }
        entries.push_back(std::move(se));
    }

    if (ec) {
        return fromErrno(ec.value());
    }
    return entries;
}

// ── readFile ─────────────────────────────────────────────────────────────

Expected<std::vector<char>, ErrorCode>
LocalStorage::readFile(std::filesystem::path const& path)
{
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        spdlog::error("LocalStorage: cannot read {}: {}", path.string(), ec.message());
        return fromErrno(ec.value());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        spdlog::error("LocalStorage: cannot open {}: {}", path.string(), std::strerror(errno));
        return fromErrno(errno);
    }

    std::vector<char> buffer(size ? size : 4096);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    auto bytesRead = static_cast<std::size_t>(file.gcount());

    if (!file && !file.eof()) {
        return ErrorCode::kReadFailed;
    }

    buffer.resize(bytesRead);
    return buffer;
}

// ── writeFile ────────────────────────────────────────────────────────────

Expected<void, ErrorCode>
LocalStorage::writeFile(
    std::filesystem::path const& path,
    std::vector<char> const& data)
{
    // Create parent directories
    auto parent = path.parent_path();
    if (!parent.empty()) {
        auto r = createDirectory(parent);
        if (!r) {
            return r.error();
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        spdlog::error("LocalStorage: cannot open {}: {}", path.string(), std::strerror(errno));
        return fromErrno(errno);
    }

    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file) {
        if (errno == ENOSPC) {
            return ErrorCode::kDiskFull;
        }
        return ErrorCode::kWriteFailed;
    }

    return {};
}

// ── fileSize ─────────────────────────────────────────────────────────────

Expected<uint64_t, ErrorCode>
LocalStorage::fileSize(std::filesystem::path const& path)
{
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return fromErrno(ec.value());
    }
    return size;
}

} // namespace backer

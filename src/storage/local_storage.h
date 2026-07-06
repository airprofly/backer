#pragma once

#include "storage/storage.h"

namespace backer {

/// Storage implementation backed by the local filesystem.
class LocalStorage final : public Storage {
public:
    Expected<bool, ErrorCode> exists(
        std::filesystem::path const& path) override;

    Expected<bool, ErrorCode> isDirectory(
        std::filesystem::path const& path) override;

    Expected<void, ErrorCode> createDirectory(
        std::filesystem::path const& path) override;

    Expected<std::vector<StorageEntry>, ErrorCode> listDirectory(
        std::filesystem::path const& path) override;

    Expected<std::vector<char>, ErrorCode> readFile(
        std::filesystem::path const& path) override;

    Expected<void, ErrorCode> writeFile(
        std::filesystem::path const& path,
        std::vector<char> const& data) override;

    Expected<uint64_t, ErrorCode> fileSize(
        std::filesystem::path const& path) override;
};

} // namespace backer

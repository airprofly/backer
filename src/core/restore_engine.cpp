#include "core/restore_engine.h"
#include "fs/fs_abstraction.h"
#include "fs/metadata.h"
#include "fs/path_mapper.h"
#include "fs/special_file.h"

#include <chrono>
#include <spdlog/spdlog.h>

namespace backer {

RestoreEngine::RestoreEngine(std::unique_ptr<FSAbstraction> fs)
    : fs_(std::move(fs))
{
}

BackupResult RestoreEngine::restore(
    std::filesystem::path const& source,
    std::filesystem::path const& destination)
{
    auto const startTime = std::chrono::steady_clock::now();
    BackupResult result;

    // 1. Validate backup source
    std::error_code ec;
    if (!std::filesystem::exists(source, ec)) {
        result.errorCode = ErrorCode::kPathNotFound;
        result.errorMessage = "Backup source does not exist: " + source.string();
        spdlog::error(result.errorMessage);
        return result;
    }
    if (!std::filesystem::is_directory(source, ec)) {
        result.errorCode = ErrorCode::kPathNotFound;
        result.errorMessage = "Backup source is not a directory: " + source.string();
        spdlog::error(result.errorMessage);
        return result;
    }

    // 2. Walk the backup directory tree
    auto walkResult = fs_->walk(source);
    if (!walkResult) {
        result.errorCode = walkResult.error();
        result.errorMessage = std::string(toString(walkResult.error()));
        spdlog::error("RestoreEngine: walk failed for {}: {}", source.string(), result.errorMessage);
        return result;
    }

    auto const& entries = walkResult.value();
    spdlog::info("RestoreEngine: found {} entries in {}", entries.size(), source.string());

    // 3. Create destination root
    auto mkdirResult = fs_->mkdir(destination);
    if (!mkdirResult) {
        result.errorCode = mkdirResult.error();
        result.errorMessage = std::string(toString(mkdirResult.error()));
        spdlog::error("RestoreEngine: cannot create destination root: {}", result.errorMessage);
        return result;
    }

    // Determine if we can restore ownership (root check once)
    bool const canOwn = canRestoreOwnership();

    // 4. Restore each entry
    for (auto const& entry : entries) {
        auto restorePath = PathMapper::relativeToSource(entry.relativePath, destination);

        switch (entry.type) {

        case FileType::kDirectory: {
            auto r = fs_->mkdir(restorePath);
            if (!r) {
                spdlog::warn("RestoreEngine: failed to create dir {}: {}",
                             restorePath.string(), toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalDirs++;
            break;
        }

        case FileType::kRegular: {
            auto backupPath = PathMapper::relativeToBackup(entry.relativePath, source);
            auto readResult = fs_->read(backupPath);
            if (!readResult) {
                spdlog::warn("RestoreEngine: failed to read {}: {}",
                             backupPath.string(), toString(readResult.error()));
                result.stats.skipped++;
                continue;
            }
            auto& content = readResult.value();
            auto writeResult = fs_->write(restorePath, content, FileType::kRegular);
            if (!writeResult) {
                spdlog::warn("RestoreEngine: failed to write {}: {}",
                             restorePath.string(), toString(writeResult.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalBytes += content.size();
            result.stats.totalFiles++;
            break;
        }

        case FileType::kSymlink: {
            auto parent = restorePath.parent_path();
            if (!parent.empty()) {
                fs_->mkdir(parent);
            }
            auto target = entry.symlinkTarget.string();
            auto r = createSymlink(restorePath, target);
            if (!r) {
                spdlog::warn("RestoreEngine: failed to create symlink {} → {}: {}",
                             restorePath.string(), target, toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalFiles++;
            break;
        }

        case FileType::kFifo: {
            auto parent = restorePath.parent_path();
            if (!parent.empty()) {
                fs_->mkdir(parent);
            }
            auto r = createFifo(restorePath, entry.metadata.permissions);
            if (!r) {
                spdlog::warn("RestoreEngine: failed to create FIFO {}: {}",
                             restorePath.string(), toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalFiles++;
            break;
        }

        case FileType::kBlockDevice:
        case FileType::kCharDevice: {
            auto parent = restorePath.parent_path();
            if (!parent.empty()) {
                fs_->mkdir(parent);
            }
            auto r = createDevice(restorePath, entry.type,
                                  entry.deviceMajor, entry.deviceMinor,
                                  entry.metadata.permissions);
            if (!r) {
                spdlog::warn("RestoreEngine: failed to create device {} ({},{}): {}",
                             restorePath.string(), entry.deviceMajor, entry.deviceMinor,
                             toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalFiles++;
            break;
        }

        case FileType::kSocket:
        default:
            spdlog::debug("RestoreEngine: skipping entry {} (type {})",
                          entry.relativePath.string(), static_cast<int>(entry.type));
            result.stats.skipped++;
            continue;  // Skip metadata restore — no entry was created
        }

        // 5. Restore metadata for every successfully created entry
        auto metaResult = fs_->restoreMetadata(restorePath, entry.metadata, canOwn);
        if (!metaResult) {
            spdlog::warn("RestoreEngine: metadata restore partially failed for {}",
                         restorePath.string());
        }
    }

    // 6. Finalize
    auto const endTime = std::chrono::steady_clock::now();
    result.stats.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    result.success = true;
    result.errorCode = ErrorCode::kOk;

    spdlog::info("Restore complete: {} files, {} dirs, {} bytes in {:.1f}s",
                 result.stats.totalFiles,
                 result.stats.totalDirs,
                 result.stats.totalBytes,
                 static_cast<double>(result.stats.elapsed.count()) / 1000.0);

    return result;
}

} // namespace backer

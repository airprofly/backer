#include "core/restore_engine.h"
#include "fs/fs_abstraction.h"
#include "fs/path_mapper.h"

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

    // 4. Restore each entry
    for (auto const& entry : entries) {
        auto restorePath = PathMapper::relativeToSource(entry.relativePath, destination);

        if (entry.type == FileType::kDirectory) {
            auto r = fs_->mkdir(restorePath);
            if (!r) {
                spdlog::warn("RestoreEngine: failed to create dir {}: {}",
                             restorePath.string(), toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalDirs++;

        } else if (entry.type == FileType::kRegular) {
            // Read from backup → write to restore destination
            auto backupPath = PathMapper::relativeToBackup(entry.relativePath, source);
            auto readResult = fs_->read(backupPath);
            if (!readResult) {
                spdlog::warn("RestoreEngine: failed to read {}: {}",
                             backupPath.string(), toString(readResult.error()));
                result.stats.skipped++;
                continue;
            }

            auto& content = readResult.value();
            auto writeResult = fs_->write(restorePath, content);
            if (!writeResult) {
                spdlog::warn("RestoreEngine: failed to write {}: {}",
                             restorePath.string(), toString(writeResult.error()));
                result.stats.skipped++;
                continue;
            }

            result.stats.totalBytes += content.size();
            result.stats.totalFiles++;

        } else if (entry.type == FileType::kSymlink) {
            spdlog::debug("RestoreEngine: skipping symlink {} (metadata support pending)",
                          entry.relativePath.string());
            result.stats.skipped++;
        }
    }

    // 5. Finalize
    auto const endTime = std::chrono::steady_clock::now();
    result.stats.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    result.success = true;
    result.errorCode = ErrorCode::kOk;

    spdlog::info("Restore complete: {} files, {} dirs, {} bytes in {:.1f}s",
                 result.stats.totalFiles,
                 result.stats.totalDirs,
                 result.stats.totalBytes,
                 result.stats.elapsed.count() / 1000.0);

    return result;
}

} // namespace backer

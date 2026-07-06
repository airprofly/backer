#include "core/backup_engine.h"
#include "fs/fs_abstraction.h"
#include "fs/path_mapper.h"

#include <chrono>
#include <spdlog/spdlog.h>

namespace backer {

BackupEngine::BackupEngine(std::unique_ptr<FSAbstraction> fs)
    : fs_(std::move(fs))
{
}

BackupResult BackupEngine::backup(
    std::filesystem::path const& source,
    std::filesystem::path const& destination)
{
    auto const startTime = std::chrono::steady_clock::now();

    BackupResult result;

    // 1. Validate source
    std::error_code ec;
    if (!std::filesystem::exists(source, ec)) {
        result.errorCode = ErrorCode::kPathNotFound;
        result.errorMessage = "Source path does not exist: " + source.string();
        spdlog::error(result.errorMessage);
        return result;
    }
    if (!std::filesystem::is_directory(source, ec)) {
        result.errorCode = ErrorCode::kPathNotFound;
        result.errorMessage = "Source is not a directory: " + source.string();
        spdlog::error(result.errorMessage);
        return result;
    }

    // 2. Walk source directory tree
    auto walkResult = fs_->walk(source);
    if (!walkResult) {
        result.errorCode = walkResult.error();
        result.errorMessage = std::string(toString(walkResult.error()));
        spdlog::error("BackupEngine: walk failed for {}: {}", source.string(), result.errorMessage);
        return result;
    }

    auto const& entries = walkResult.value();
    spdlog::info("BackupEngine: found {} entries in {}", entries.size(), source.string());

    // 3. Create destination root
    auto mkdirResult = fs_->mkdir(destination);
    if (!mkdirResult) {
        result.errorCode = mkdirResult.error();
        result.errorMessage = std::string(toString(mkdirResult.error()));
        spdlog::error("BackupEngine: cannot create destination root: {}", result.errorMessage);
        return result;
    }

    // 4. Copy each entry
    for (auto const& entry : entries) {
        auto destPath = PathMapper::relativeToBackup(entry.relativePath, destination);

        if (entry.type == FileType::kDirectory) {
            // Create directory in backup
            auto r = fs_->mkdir(destPath);
            if (!r) {
                spdlog::warn("BackupEngine: failed to create dir {}: {}",
                             destPath.string(), toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalDirs++;

        } else if (entry.type == FileType::kRegular) {
            // Read source → write to backup
            auto sourcePath = PathMapper::relativeToSource(entry.relativePath, source);
            auto readResult = fs_->read(sourcePath);
            if (!readResult) {
                spdlog::warn("BackupEngine: failed to read {}: {}",
                             sourcePath.string(), toString(readResult.error()));
                result.stats.skipped++;
                continue;
            }

            auto& content = readResult.value();
            auto writeResult = fs_->write(destPath, content);
            if (!writeResult) {
                spdlog::warn("BackupEngine: failed to write {}: {}",
                             destPath.string(), toString(writeResult.error()));
                result.stats.skipped++;
                continue;
            }

            result.stats.totalBytes += content.size();
            result.stats.totalFiles++;

        } else if (entry.type == FileType::kSymlink) {
            // Symlinks: just note them for now (metadata phase will handle fully)
            spdlog::debug("BackupEngine: skipping symlink {} (metadata support pending)",
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

    spdlog::info("Backup complete: {} files, {} dirs, {} bytes in {:.1f}s",
                 result.stats.totalFiles,
                 result.stats.totalDirs,
                 result.stats.totalBytes,
                 result.stats.elapsed.count() / 1000.0);

    return result;
}

} // namespace backer

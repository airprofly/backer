#include "core/restore_engine.h"
#include "fs/fs_abstraction.h"
#include "fs/metadata.h"
#include "fs/path_mapper.h"
#include "fs/special_file.h"
#include "pack/packer.h"

#include <chrono>
#include <sstream>
#include <spdlog/spdlog.h>

namespace backer {

RestoreEngine::RestoreEngine(std::unique_ptr<FSAbstraction> fs)
    : fs_(std::move(fs))
{
}

BackupResult RestoreEngine::restore(
    std::filesystem::path const& source,
    std::filesystem::path const& destination,
    Packer* packer)
{
    auto const startTime = std::chrono::steady_clock::now();
    BackupResult result;

    // 1a. Archive mode: unpack from archive
    if (packer) {
        spdlog::info("RestoreEngine: unpacking {} as {}", source.string(), packer->formatName());

        // Validate source exists
        std::error_code ec;
        if (!std::filesystem::exists(source, ec)) {
            result.errorCode = ErrorCode::kPathNotFound;
            result.errorMessage = "Archive does not exist: " + source.string();
            spdlog::error(result.errorMessage);
            return result;
        }

        // Read the archive file
        auto readResult = fs_->read(source);
        if (!readResult) {
            result.errorCode = readResult.error();
            result.errorMessage = std::string(toString(readResult.error()));
            spdlog::error("RestoreEngine: failed to read archive {}: {}", source.string(),
                          result.errorMessage);
            return result;
        }

        auto const& archiveData = readResult.value();
        std::string archiveStr(archiveData.begin(), archiveData.end());
        std::istringstream archive(archiveStr);

        // Create destination root
        auto mkdirResult = fs_->mkdir(destination);
        if (!mkdirResult) {
            result.errorCode = mkdirResult.error();
            result.errorMessage = std::string(toString(mkdirResult.error()));
            spdlog::error("RestoreEngine: cannot create destination root: {}", result.errorMessage);
            return result;
        }

        // Unpack
        auto unpackResult = packer->unpack(archive, destination, *fs_);
        if (!unpackResult) {
            result.errorCode = unpackResult.error();
            result.errorMessage = std::string(toString(unpackResult.error()));
            spdlog::error("RestoreEngine: unpack failed: {}", result.errorMessage);
            return result;
        }

        auto const endTime = std::chrono::steady_clock::now();
        result.stats.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        result.success = true;
        result.errorCode = ErrorCode::kOk;

        spdlog::info("Restore complete (archive mode): {} in {:.1f}s",
                     source.string(), static_cast<double>(result.stats.elapsed.count()) / 1000.0);
        return result;
    }

    // 1b. Validate backup source (directory mode)
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

    bool const canOwn = canRestoreOwnership();

    // Deferred directory metadata: restore AFTER all content is written,
    // sorted deepest-first, so child writes don't overwrite parent mtime.
    std::vector<std::pair<std::filesystem::path, Metadata>> dirsToRestore;

    // Backup source root metadata (saved separately since walk() only returns children)
    Metadata sourceRootMeta;
    {
        auto metaResult = fs_->readMetadata(source);
        if (metaResult) {
            sourceRootMeta = metaResult.value();
        } else {
            spdlog::warn("RestoreEngine: cannot read backup root metadata: {}",
                         toString(metaResult.error()));
        }
    }

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
            // Defer metadata restoration until all children are written
            dirsToRestore.emplace_back(restorePath, entry.metadata);
            continue;  // skip immediate metadata restore below
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
            continue;
        }

        // 5. Restore metadata for every successfully created non-directory entry
        auto metaResult = fs_->restoreMetadata(restorePath, entry.metadata, canOwn);
        if (!metaResult) {
            spdlog::warn("RestoreEngine: metadata restore partially failed for {}",
                         restorePath.string());
        }
    }

    // 6. Restore deferred directory metadata, deepest-first, so child
    //    writes no longer overwrite parent mtime after this point.
    std::sort(dirsToRestore.begin(), dirsToRestore.end(),
        [](auto const& a, auto const& b) {
            auto depthA = std::distance(a.first.begin(), a.first.end());
            auto depthB = std::distance(b.first.begin(), b.first.end());
            return depthA > depthB;
        });

    for (auto const& [dirPath, meta] : dirsToRestore) {
        auto metaResult = fs_->restoreMetadata(dirPath, meta, canOwn);
        if (!metaResult) {
            spdlog::warn("RestoreEngine: dir metadata partially failed for {}",
                         dirPath.string());
        }
    }

    // Restore backup source root metadata on the destination root directory.
    // Must happen last — destination root is the parent of all children,
    // and any prior write updated its mtime.
    {
        auto metaResult = fs_->restoreMetadata(destination, sourceRootMeta, canOwn);
        if (!metaResult) {
            spdlog::warn("RestoreEngine: root metadata partially failed for {}",
                         destination.string());
        }
    }

    // 7. Finalize
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

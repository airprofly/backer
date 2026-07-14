#include "core/backup_engine.h"
#include "filters/filter.h"
#include "fs/fs_abstraction.h"
#include "fs/metadata.h"
#include "fs/path_mapper.h"
#include "fs/special_file.h"
#include "pack/packer.h"

#include <chrono>
#include <sstream>
#include <spdlog/spdlog.h>

namespace backer {

BackupEngine::BackupEngine(std::unique_ptr<FSAbstraction> fs)
    : fs_(std::move(fs))
{
}

BackupResult BackupEngine::backup(
    std::filesystem::path const& source,
    std::filesystem::path const& destination,
    Filter* filter,
    Packer* packer)
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

    auto entries = std::move(walkResult.value());
    spdlog::info("BackupEngine: found {} entries in {}", entries.size(), source.string());

    // 3. Apply filter (if any) — walk() → filter → copy/pack
    if (filter) {
        entries = filter->apply(entries);
    }

    // 4a. Archive mode (packer is set): pack entries into a single archive
    if (packer) {
        spdlog::info("BackupEngine: packing {} entries as {}", entries.size(), packer->formatName());

        std::ostringstream archive;
        auto packResult = packer->pack(entries, *fs_, source, archive);
        if (!packResult) {
            result.errorCode = packResult.error();
            result.errorMessage = std::string(toString(packResult.error()));
            spdlog::error("BackupEngine: pack failed: {}", result.errorMessage);
            return result;
        }

        // Write the archive to destination (as a regular file)
        auto archiveStr = archive.str();
        std::vector<char> archiveData(archiveStr.begin(), archiveStr.end());
        auto writeResult = fs_->write(destination, archiveData, FileType::kRegular);
        if (!writeResult) {
            result.errorCode = writeResult.error();
            result.errorMessage = std::string(toString(writeResult.error()));
            spdlog::error("BackupEngine: failed to write archive {}: {}", destination.string(),
                          result.errorMessage);
            return result;
        }

        // Count stats
        for (auto const& entry : entries) {
            if (entry.type == FileType::kDirectory) {
                result.stats.totalDirs++;
            } else {
                result.stats.totalFiles++;
                result.stats.totalBytes += entry.size;
            }
        }

        auto const endTime = std::chrono::steady_clock::now();
        result.stats.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        result.success = true;
        result.errorCode = ErrorCode::kOk;

        spdlog::info("Backup complete (archive mode): {} files, {} dirs, {} bytes in {:.1f}s",
                     result.stats.totalFiles, result.stats.totalDirs, result.stats.totalBytes,
                     static_cast<double>(result.stats.elapsed.count()) / 1000.0);
        return result;
    }

    // 4b. Directory mirror mode (default): copy each entry individually
    auto mkdirResult = fs_->mkdir(destination);
    if (!mkdirResult) {
        result.errorCode = mkdirResult.error();
        result.errorMessage = std::string(toString(mkdirResult.error()));
        spdlog::error("BackupEngine: cannot create destination root: {}", result.errorMessage);
        return result;
    }

    bool const canOwn = canRestoreOwnership();

    // Deferred directory metadata: restore AFTER all content is written,
    // sorted deepest-first, so child writes don't overwrite parent mtime.
    std::vector<std::pair<std::filesystem::path, Metadata>> dirsToRestore;

    // Read source root metadata (walk does not include the root itself)
    Metadata sourceRootMeta;
    {
        auto metaResult = fs_->readMetadata(source);
        if (metaResult) {
            sourceRootMeta = metaResult.value();
        } else {
            spdlog::warn("BackupEngine: cannot read source root metadata: {}",
                         toString(metaResult.error()));
        }
    }

    for (auto const& entry : entries) {
        auto destPath = PathMapper::relativeToBackup(entry.relativePath, destination);

        switch (entry.type) {

        case FileType::kDirectory: {
            auto r = fs_->mkdir(destPath);
            if (!r) {
                spdlog::warn("BackupEngine: failed to create dir {}: {}",
                             destPath.string(), toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalDirs++;
            // Defer metadata restoration until all children are written
            dirsToRestore.emplace_back(destPath, entry.metadata);
            continue;  // skip immediate metadata restore below
        }

        case FileType::kRegular: {
            auto sourcePath = PathMapper::relativeToSource(entry.relativePath, source);
            auto readResult = fs_->read(sourcePath);
            if (!readResult) {
                spdlog::warn("BackupEngine: failed to read {}: {}",
                             sourcePath.string(), toString(readResult.error()));
                result.stats.skipped++;
                continue;
            }
            auto& content = readResult.value();
            auto writeResult = fs_->write(destPath, content, FileType::kRegular);
            if (!writeResult) {
                spdlog::warn("BackupEngine: failed to write {}: {}",
                             destPath.string(), toString(writeResult.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalBytes += content.size();
            result.stats.totalFiles++;
            break;
        }

        case FileType::kSymlink: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                fs_->mkdir(parent);
            }
            auto target = entry.symlinkTarget.string();
            auto r = createSymlink(destPath, target);
            if (!r) {
                spdlog::warn("BackupEngine: failed to create symlink {} → {}: {}",
                             destPath.string(), target, toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalFiles++;
            break;
        }

        case FileType::kFifo: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                fs_->mkdir(parent);
            }
            auto r = createFifo(destPath, entry.metadata.permissions);
            if (!r) {
                spdlog::warn("BackupEngine: failed to create FIFO {}: {}",
                             destPath.string(), toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalFiles++;
            break;
        }

        case FileType::kBlockDevice:
        case FileType::kCharDevice: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                fs_->mkdir(parent);
            }
            auto r = createDevice(destPath, entry.type,
                                  entry.deviceMajor, entry.deviceMinor,
                                  entry.metadata.permissions);
            if (!r) {
                spdlog::warn("BackupEngine: failed to create device {} ({},{}): {}",
                             destPath.string(), entry.deviceMajor, entry.deviceMinor,
                             toString(r.error()));
                result.stats.skipped++;
                continue;
            }
            result.stats.totalFiles++;
            break;
        }

        case FileType::kSocket:
        default:
            spdlog::debug("BackupEngine: skipping entry {} (type {})",
                          entry.relativePath.string(), static_cast<int>(entry.type));
            result.stats.skipped++;
            continue;
        }

        // 5. Restore metadata for non-directory entries immediately
        auto metaResult = fs_->restoreMetadata(destPath, entry.metadata, canOwn);
        if (!metaResult) {
            spdlog::warn("BackupEngine: metadata restore partially failed for {}",
                         destPath.string());
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
            spdlog::warn("BackupEngine: dir metadata partially failed for {}",
                         dirPath.string());
        }
    }

    // Restore source root metadata on the destination root directory.
    // Must happen last — destination root is the parent of all children,
    // and any prior write updated its mtime.
    {
        auto metaResult = fs_->restoreMetadata(destination, sourceRootMeta, canOwn);
        if (!metaResult) {
            spdlog::warn("BackupEngine: root metadata partially failed for {}",
                         destination.string());
        }
    }

    // 7. Finalize
    auto const endTime = std::chrono::steady_clock::now();
    result.stats.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    result.success = true;
    result.errorCode = ErrorCode::kOk;

    spdlog::info("Backup complete: {} files, {} dirs, {} bytes in {:.1f}s",
                 result.stats.totalFiles,
                 result.stats.totalDirs,
                 result.stats.totalBytes,
                 static_cast<double>(result.stats.elapsed.count()) / 1000.0);

    return result;
}

} // namespace backer

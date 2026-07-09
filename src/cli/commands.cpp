#include "cli/commands.h"
#include "core/backup_engine.h"
#include "core/restore_engine.h"
#include "core/types.h"
#include "filters/criteria_filter.h"
#include "filters/filter.h"
#include "fs/fs_abstraction.h"
#include "fs/platform.h"
#include "pack/packer.h"
#include "pack/tar_packer.h"
#include "pack/zip_packer.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>

#include <cstring>
#include <regex>
#include <string>

#if BACKER_PLATFORM_POSIX
    #include <pwd.h>
    #include <unistd.h>
#endif

namespace backer::cli {
namespace {

// ══════════════════════════════════════════════════════════════════════════════
// Helper: parse file type string → FileType
// ══════════════════════════════════════════════════════════════════════════════

FileType parseFileType(std::string const& typeStr) {
    if (typeStr == "file"    || typeStr == "regular" || typeStr == "f") return FileType::kRegular;
    if (typeStr == "dir"     || typeStr == "directory" || typeStr == "d") return FileType::kDirectory;
    if (typeStr == "symlink" || typeStr == "link" || typeStr == "l")     return FileType::kSymlink;
    if (typeStr == "fifo"    || typeStr == "pipe" || typeStr == "p")     return FileType::kFifo;
    if (typeStr == "block"   || typeStr == "blockdev" || typeStr == "b") return FileType::kBlockDevice;
    if (typeStr == "char"    || typeStr == "chardev" || typeStr == "c")  return FileType::kCharDevice;
    if (typeStr == "socket"  || typeStr == "sock" || typeStr == "s")     return FileType::kSocket;
    return FileType::kUnknown;
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: resolve username → UID (POSIX only)
// ══════════════════════════════════════════════════════════════════════════════

bool resolveUser(std::string const& userName, uint32_t& uid) {
#if BACKER_PLATFORM_POSIX
    struct passwd pwd;
    struct passwd* result = nullptr;
    char buf[4096];
    int rc = getpwnam_r(userName.c_str(), &pwd, buf, sizeof(buf), &result);
    if (rc == 0 && result != nullptr) {
        uid = static_cast<uint32_t>(result->pw_uid);
        return true;
    }
    spdlog::warn("Cannot resolve user '{}': {}", userName,
                 rc == 0 ? "not found" : std::strerror(rc));
    return false;
#else
    (void)userName;
    (void)uid;
    spdlog::warn("User filtering not supported on this platform");
    return false;
#endif
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: construct Filter from BackupOptions
// ══════════════════════════════════════════════════════════════════════════════

std::unique_ptr<Filter> buildFilter(BackupOptions const& options) {
    std::vector<FilterCriteria> criteria;

    // ── Path includes ─────────────────────────────────────────────────
    for (auto const& pattern : options.includePaths) {
        FilterCriteria c;
        c.pathGlob = pattern;
        criteria.push_back(std::move(c));
    }

    // ── Path excludes ─────────────────────────────────────────────────
    for (auto const& pattern : options.excludePaths) {
        FilterCriteria c;
        c.pathGlob = pattern;
        c.exclude = true;
        criteria.push_back(std::move(c));
    }

    // ── Type includes ─────────────────────────────────────────────────
    for (auto const& typeStr : options.includeTypes) {
        auto ft = parseFileType(typeStr);
        if (ft != FileType::kUnknown) {
            FilterCriteria c;
            c.fileType = ft;
            criteria.push_back(std::move(c));
        } else {
            spdlog::warn("Unknown file type '{}' in --include-type", typeStr);
        }
    }

    // ── Type excludes ─────────────────────────────────────────────────
    for (auto const& typeStr : options.excludeTypes) {
        auto ft = parseFileType(typeStr);
        if (ft != FileType::kUnknown) {
            FilterCriteria c;
            c.fileType = ft;
            c.exclude = true;
            criteria.push_back(std::move(c));
        } else {
            spdlog::warn("Unknown file type '{}' in --exclude-type", typeStr);
        }
    }

    // ── Name includes ─────────────────────────────────────────────────
    for (auto const& pattern : options.includeNames) {
        FilterCriteria c;
        c.nameGlob = pattern;
        criteria.push_back(std::move(c));
    }

    // ── Name excludes ─────────────────────────────────────────────────
    for (auto const& pattern : options.excludeNames) {
        FilterCriteria c;
        c.nameGlob = pattern;
        c.exclude = true;
        criteria.push_back(std::move(c));
    }

    // ── Mtime before/after ────────────────────────────────────────────
    if (!options.mtimeBefore.empty() || !options.mtimeAfter.empty()) {
        FilterCriteria c;
        TimeRange tr;
        if (!options.mtimeBefore.empty()) {
            tr.hasBefore = true;
            tr.beforeSec = std::atol(options.mtimeBefore.c_str());
        }
        if (!options.mtimeAfter.empty()) {
            tr.hasAfter = true;
            tr.afterSec = std::atol(options.mtimeAfter.c_str());
        }
        c.timeRange = tr;
        criteria.push_back(std::move(c));
    }

    // ── Size min/max ──────────────────────────────────────────────────
    if (options.hasSizeMin || options.hasSizeMax) {
        FilterCriteria c;
        SizeRange sr;
        if (options.hasSizeMin) {
            sr.hasMin = true;
            sr.minSize = options.sizeMin;
        }
        if (options.hasSizeMax) {
            sr.hasMax = true;
            sr.maxSize = options.sizeMax;
        }
        c.sizeRange = sr;
        criteria.push_back(std::move(c));
    }

    // ── Owner ─────────────────────────────────────────────────────────
    if (!options.owner.empty()) {
        uint32_t uid = 0;
        if (resolveUser(options.owner, uid)) {
            FilterCriteria c;
            c.ownerId = uid;
            criteria.push_back(std::move(c));
        }
    }

    if (criteria.empty()) {
        return std::make_unique<NoopFilter>();
    }

    return std::make_unique<CriteriaFilter>(std::move(criteria));
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: construct Packer from options
// ══════════════════════════════════════════════════════════════════════════════

std::unique_ptr<Packer> buildPacker(std::string const& format) {
    if (format == "tar") {
        return std::make_unique<TarPacker>();
    }
    if (format == "zip") {
        return std::make_unique<ZipPacker>();
    }
    return nullptr;
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════════
// handleBackup
// ══════════════════════════════════════════════════════════════════════════════

int handleBackup(
    std::filesystem::path const& source,
    std::filesystem::path const& destination,
    BackupOptions const& options)
{
    spdlog::info("Starting backup: {} → {}", source.string(), destination.string());

    if (!options.preserveMetadata) {
        spdlog::warn("Metadata preservation is DISABLED");
    }
    if (!options.handleSpecial) {
        spdlog::warn("Special file handling is DISABLED");
    }

    auto fs = std::make_unique<LocalFsAbstraction>();

    // Build filter
    auto filter = buildFilter(options);

    // Build packer
    auto packer = buildPacker(options.packFormat);
    if (packer) {
        spdlog::info("Packing format: {}", packer->formatName());
    }

    // ── Auto-append file extension for archive mode ────────────────
    // e.g. --pack tar → destination + ".tar", --pack zip → destination + ".zip"
    // Per docs/plans/05-packing.md naming convention.
    auto actualDest = destination;
    if (packer) {
        auto ext = std::string(".") + std::string(packer->formatName());
        auto destStr = destination.string();
        // Avoid double-adding extension if user already typed it
        if (destStr.size() < ext.size() ||
            destStr.compare(destStr.size() - ext.size(), ext.size(), ext) != 0) {
            actualDest = destination;
            actualDest += ext;
        }
        spdlog::info("Output archive: {}", actualDest.string());
    }

    BackupEngine engine(std::move(fs));
    auto result = engine.backup(source, actualDest, filter.get(), packer.get());

    if (result.success) {
        auto const& s = result.stats;
        std::cout
            << "✓ Backup completed successfully\n"
            << "  Archive: " << actualDest.string() << "\n"
            << "  Files:   " << s.totalFiles << "\n"
            << "  Dirs:    " << s.totalDirs << "\n"
            << "  Size:    " << s.totalBytes << " bytes\n"
            << "  Skipped: " << s.skipped << "\n"
            << "  Time:    " << s.elapsed.count() << " ms\n";
        return 0;
    }

    std::cerr
        << "✗ Backup failed: " << result.errorMessage << "\n";
    return 1;
}

// ══════════════════════════════════════════════════════════════════════════════
// handleRestore
// ══════════════════════════════════════════════════════════════════════════════

int handleRestore(
    std::filesystem::path const& source,
    std::filesystem::path const& destination,
    RestoreOptions const& options)
{
    spdlog::info("Starting restore: {} → {}", source.string(), destination.string());

    if (!options.preserveMetadata) {
        spdlog::warn("Metadata preservation is DISABLED");
    }
    if (!options.handleSpecial) {
        spdlog::warn("Special file handling is DISABLED");
    }

    auto fs = std::make_unique<LocalFsAbstraction>();

    // Build packer for archive mode
    auto packer = buildPacker(options.packFormat);
    if (packer) {
        spdlog::info("Unpacking format: {}", packer->formatName());
    }

    RestoreEngine engine(std::move(fs));
    auto result = engine.restore(source, destination, packer.get());

    if (result.success) {
        auto const& s = result.stats;
        std::cout
            << "✓ Restore completed successfully\n"
            << "  Files:   " << s.totalFiles << "\n"
            << "  Dirs:    " << s.totalDirs << "\n"
            << "  Size:    " << s.totalBytes << " bytes\n"
            << "  Skipped: " << s.skipped << "\n"
            << "  Time:    " << s.elapsed.count() << " ms\n";
        return 0;
    }

    std::cerr
        << "✗ Restore failed: " << result.errorMessage << "\n";
    return 1;
}

} // namespace backer::cli

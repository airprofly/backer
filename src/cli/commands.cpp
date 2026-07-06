#include "cli/commands.h"
#include "core/backup_engine.h"
#include "core/restore_engine.h"
#include "core/types.h"
#include "fs/fs_abstraction.h"

#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>

namespace backer::cli {

int handleBackup(
    std::filesystem::path const& source,
    std::filesystem::path const& destination)
{
    spdlog::info("Starting backup: {} → {}", source.string(), destination.string());

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));

    auto result = engine.backup(source, destination);

    if (result.success) {
        auto const& s = result.stats;
        std::cout
            << "✓ Backup completed successfully\n"
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

int handleRestore(
    std::filesystem::path const& source,
    std::filesystem::path const& destination)
{
    spdlog::info("Starting restore: {} → {}", source.string(), destination.string());

    auto fs = std::make_unique<LocalFsAbstraction>();
    RestoreEngine engine(std::move(fs));

    auto result = engine.restore(source, destination);

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

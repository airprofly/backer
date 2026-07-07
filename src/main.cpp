#include "cli/commands.h"

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char** argv)
{
    // ── Logger setup ───────────────────────────────────────────────
    auto console = spdlog::stdout_color_mt("backer");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);

    // ── CLI app ────────────────────────────────────────────────────
    CLI::App app{"Backer — Data backup and restore tool"};

    app.require_subcommand(1);

    // ════════════════════════════════════════════════════════════════
    // backup subcommand
    // ════════════════════════════════════════════════════════════════
    auto* backupCmd = app.add_subcommand("backup", "Back up a directory tree");
    std::string backupSource, backupDest;
    bool backupNoMetadata = false;
    bool backupSkipSpecial = false;

    backupCmd->add_option("source", backupSource, "Source directory to back up")
        ->required()
        ->type_name("PATH");
    backupCmd->add_option("destination", backupDest, "Backup destination (directory or archive)")
        ->required()
        ->type_name("PATH");
    backupCmd->add_flag("--no-metadata", backupNoMetadata,
                        "Skip metadata preservation (ownership, permissions, timestamps)");
    backupCmd->add_flag("--skip-special", backupSkipSpecial,
                        "Skip special file types (FIFO, devices, sockets)");

    // ── Filter arguments ──────────────────────────────────────────
    std::vector<std::string> backupIncludePaths, backupExcludePaths;
    std::vector<std::string> backupIncludeTypes, backupExcludeTypes;
    std::vector<std::string> backupIncludeNames, backupExcludeNames;
    std::string backupMtimeBefore, backupMtimeAfter;
    uint64_t backupSizeMin = 0, backupSizeMax = 0;
    bool backupHasSizeMin = false, backupHasSizeMax = false;
    std::string backupOwner;

    backupCmd->add_option("--include-path", backupIncludePaths,
                          "Glob pattern for paths to include (can be repeated)")
        ->type_name("GLOB");
    backupCmd->add_option("--exclude-path", backupExcludePaths,
                          "Glob pattern for paths to exclude (can be repeated)")
        ->type_name("GLOB");
    backupCmd->add_option("--include-type", backupIncludeTypes,
                          "File type to include: file|dir|symlink|fifo|block|char|socket")
        ->type_name("TYPE");
    backupCmd->add_option("--exclude-type", backupExcludeTypes,
                          "File type to exclude: file|dir|symlink|fifo|block|char|socket")
        ->type_name("TYPE");
    backupCmd->add_option("--include-name", backupIncludeNames,
                          "Glob pattern for filenames to include (can be repeated)")
        ->type_name("GLOB");
    backupCmd->add_option("--exclude-name", backupExcludeNames,
                          "Glob pattern for filenames to exclude (can be repeated)")
        ->type_name("GLOB");
    backupCmd->add_option("--mtime-before", backupMtimeBefore,
                          "Exclude files modified at or after this Unix timestamp")
        ->type_name("UNIX_TIME");
    backupCmd->add_option("--mtime-after", backupMtimeAfter,
                          "Exclude files modified before this Unix timestamp")
        ->type_name("UNIX_TIME");
    backupCmd->add_option("--size-min", backupSizeMin, "Minimum file size in bytes")
        ->type_name("BYTES")
        ->check([&](std::string const& /*val*/) { backupHasSizeMin = true; return std::string{}; });
    backupCmd->add_option("--size-max", backupSizeMax, "Maximum file size in bytes")
        ->type_name("BYTES")
        ->check([&](std::string const& /*val*/) { backupHasSizeMax = true; return std::string{}; });
    backupCmd->add_option("--owner", backupOwner, "Only include files owned by this user")
        ->type_name("USER");

    // ── Pack argument ─────────────────────────────────────────────
    std::string backupPackFormat;
    backupCmd->add_option("--pack", backupPackFormat,
                          "Pack format: tar (creates a single archive file)")
        ->type_name("FMT");

    // ════════════════════════════════════════════════════════════════
    // restore subcommand
    // ════════════════════════════════════════════════════════════════
    auto* restoreCmd = app.add_subcommand("restore", "Restore a directory tree from backup");
    std::string restoreSource, restoreDest;
    bool restoreNoMetadata = false;
    bool restoreSkipSpecial = false;

    restoreCmd->add_option("source", restoreSource, "Backup directory or archive to restore from")
        ->required()
        ->type_name("PATH");
    restoreCmd->add_option("destination", restoreDest, "Restore destination directory")
        ->required()
        ->type_name("PATH");
    restoreCmd->add_flag("--no-metadata", restoreNoMetadata,
                         "Skip metadata preservation");
    restoreCmd->add_flag("--skip-special", restoreSkipSpecial,
                         "Skip special file types (FIFO, devices, sockets)");

    std::string restorePackFormat;
    restoreCmd->add_option("--pack", restorePackFormat,
                           "Pack format: tar (source is an archive to unpack)")
        ->type_name("FMT");

    // --version
    app.set_version_flag("--version", std::string(BACKER_VERSION),
                         "Show version information");

    // ── Parse ──────────────────────────────────────────────────────
    CLI11_PARSE(app, argc, argv);

    // ── Dispatch ───────────────────────────────────────────────────
    if (*backupCmd) {
        backer::cli::BackupOptions opts;
        opts.preserveMetadata = !backupNoMetadata;
        opts.handleSpecial    = !backupSkipSpecial;

        // Filter options
        opts.includePaths  = std::move(backupIncludePaths);
        opts.excludePaths  = std::move(backupExcludePaths);
        opts.includeTypes  = std::move(backupIncludeTypes);
        opts.excludeTypes  = std::move(backupExcludeTypes);
        opts.includeNames  = std::move(backupIncludeNames);
        opts.excludeNames  = std::move(backupExcludeNames);
        opts.mtimeBefore   = std::move(backupMtimeBefore);
        opts.mtimeAfter    = std::move(backupMtimeAfter);
        opts.sizeMin       = backupSizeMin;
        opts.sizeMax       = backupSizeMax;
        opts.hasSizeMin    = backupHasSizeMin;
        opts.hasSizeMax    = backupHasSizeMax;
        opts.owner         = std::move(backupOwner);

        // Pack option
        opts.packFormat = std::move(backupPackFormat);

        return backer::cli::handleBackup(backupSource, backupDest, opts);
    }
    if (*restoreCmd) {
        backer::cli::RestoreOptions opts;
        opts.preserveMetadata = !restoreNoMetadata;
        opts.handleSpecial    = !restoreSkipSpecial;

        // Pack option
        opts.packFormat = std::move(restorePackFormat);

        return backer::cli::handleRestore(restoreSource, restoreDest, opts);
    }

    return EXIT_FAILURE;
}

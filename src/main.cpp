#include "cli/commands.h"

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string_view>
#include <unordered_set>

namespace {

/// Valid compression algorithm names.
constexpr std::string_view kValidAlgos[] = {"gzip", "zstd", "lzma"};

/// Valid encryption algorithm names.
constexpr std::string_view kValidEncAlgos[] = {"aes256", "aes-256-gcm",
                                                "sm4", "sm4-cbc"};

/// Validate @p val against a list of valid algorithm names.
/// Empty value is allowed (option not meaningfully set); returns empty on success.
template <size_t N>
std::string validateAlgo(std::string const& val,
                          std::string_view action,
                          std::string_view const (&valid)[N],
                          std::string_view validList)
{
    if (val.empty()) return std::string{};
    for (auto v : valid) {
        if (v == val) return std::string{};
    }
    return "Invalid " + std::string(action) + " algorithm '" + val +
           "'. Use " + std::string(validList) + ".";
}

} // namespace

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

    // ── Compress argument ─────────────────────────────────────────
    std::string backupCompressAlgo;
    int backupCompressLevel = 0;
    backupCmd->add_option("--compress", backupCompressAlgo,
                          "Compress algorithm: gzip, zstd, lzma")
        ->type_name("ALGO")
        ->check([](std::string const& val) { return validateAlgo(val, "compress", kValidAlgos, "gzip, zstd, or lzma"); });
    backupCmd->add_option("--compress-level", backupCompressLevel,
                          "Compression level (1-9 for gzip/lzma, varies for zstd)")
        ->type_name("LEVEL");

    // ── Encrypt arguments ──────────────────────────────────────────────
    std::string backupEncryptAlgo;
    std::string backupPassword;
    backupCmd->add_option("--encrypt", backupEncryptAlgo,
                          "Encrypt algorithm: aes256, sm4")
        ->type_name("ALGO")
        ->check([](std::string const& val) { return validateAlgo(val, "encrypt", kValidEncAlgos, "aes256, or sm4"); });
    backupCmd->add_option("--password", backupPassword,
                          "Encryption password (omit for interactive prompt)")
        ->type_name("PASSWORD");

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

    std::string restoreDecompressAlgo;
    restoreCmd->add_option("--decompress", restoreDecompressAlgo,
                           "Decompress algorithm: gzip, zstd, lzma")
        ->type_name("ALGO")
        ->check([](std::string const& val) { return validateAlgo(val, "decompress", kValidAlgos, "gzip, zstd, or lzma"); });

    // ── Decrypt arguments ──────────────────────────────────────────────
    std::string restoreDecryptAlgo;
    std::string restorePassword;
    restoreCmd->add_option("--decrypt", restoreDecryptAlgo,
                           "Decrypt algorithm: aes256, sm4")
        ->type_name("ALGO")
        ->check([](std::string const& val) { return validateAlgo(val, "decrypt", kValidEncAlgos, "aes256, or sm4"); });
    restoreCmd->add_option("--password", restorePassword,
                           "Decryption password (omit for interactive prompt)")
        ->type_name("PASSWORD");

    // ════════════════════════════════════════════════════════════════
    // schedule subcommand
    // ════════════════════════════════════════════════════════════════
    auto* scheduleCmd = app.add_subcommand("schedule", "Manage scheduled backup jobs");
    // prefix_command captures all remaining args (including --flags) as-is
    scheduleCmd->prefix_command(true);

    // ════════════════════════════════════════════════════════════════
    // daemon subcommand
    // ════════════════════════════════════════════════════════════════
    auto* daemonCmd = app.add_subcommand("daemon", "Run the backup scheduler daemon");

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

        // Compress options
        opts.compressAlgo = std::move(backupCompressAlgo);
        opts.compressLevel = backupCompressLevel;

        // Encrypt options
        opts.encryptAlgo = std::move(backupEncryptAlgo);
        opts.password    = std::move(backupPassword);

        auto resolvedDest = backer::cli::makeBackupPath(backupDest, backupSource, backupPackFormat);
        return backer::cli::handleBackup(backupSource, resolvedDest, opts);
    }
    if (*restoreCmd) {
        backer::cli::RestoreOptions opts;
        opts.preserveMetadata = !restoreNoMetadata;
        opts.handleSpecial    = !restoreSkipSpecial;

        // Pack option
        opts.packFormat = std::move(restorePackFormat);

        // Decompress option
        opts.decompressAlgo = std::move(restoreDecompressAlgo);

        // Decrypt options
        opts.decryptAlgo = std::move(restoreDecryptAlgo);
        opts.password    = std::move(restorePassword);

        auto resolvedDest = backer::cli::makeRestorePath(restoreDest, restoreSource);
        return backer::cli::handleRestore(restoreSource, resolvedDest, opts);
    }
    if (*scheduleCmd) {
        return backer::cli::handleSchedule(scheduleCmd->remaining());
    }
    if (*daemonCmd) {
        return backer::cli::handleDaemon();
    }

    return EXIT_FAILURE;
}

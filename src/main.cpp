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

    // --backup subcommand
    auto* backupCmd = app.add_subcommand("backup", "Back up a directory tree");
    std::string backupSource, backupDest;
    backupCmd->add_option("source", backupSource, "Source directory to back up")
        ->required()
        ->type_name("PATH");
    backupCmd->add_option("destination", backupDest, "Backup destination directory")
        ->required()
        ->type_name("PATH");

    // --restore subcommand
    auto* restoreCmd = app.add_subcommand("restore", "Restore a directory tree from backup");
    std::string restoreSource, restoreDest;
    restoreCmd->add_option("source", restoreSource, "Backup directory to restore from")
        ->required()
        ->type_name("PATH");
    restoreCmd->add_option("destination", restoreDest, "Restore destination directory")
        ->required()
        ->type_name("PATH");

    // --version
    app.set_version_flag("--version", std::string(BACKER_VERSION),
                         "Show version information");

    // ── Parse ──────────────────────────────────────────────────────
    CLI11_PARSE(app, argc, argv);

    // ── Dispatch ───────────────────────────────────────────────────
    if (*backupCmd) {
        return backer::cli::handleBackup(backupSource, backupDest);
    }
    if (*restoreCmd) {
        return backer::cli::handleRestore(restoreSource, restoreDest);
    }

    return EXIT_FAILURE;
}

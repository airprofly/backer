#pragma once

#include "cli/commands.h"
#include "compress/build_compressor.h"
#include "core/backup_engine.h"
#include "core/restore_engine.h"
#include "core/types.h"
#include "filters/criteria_filter.h"
#include "filters/filter.h"
#include "fs/fs_abstraction.h"
#include "fs/platform.h"
#include "pack/tar_packer.h"
#include "pack/zip_packer.h"

#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>
#include <filesystem>
#include <variant>

namespace backer::gui {

/// Background worker that runs backup or restore in a separate thread.
/// Emits progress and completion signals that the UI can observe.
class BackupWorker : public QThread {
    Q_OBJECT
public:
    enum Mode { Backup, Restore };

    /// Construct a backup worker.
    explicit BackupWorker(Mode mode,
                          std::filesystem::path const& source,
                          std::filesystem::path const& destination,
                          backer::cli::BackupOptions const& opts = {},
                          QObject* parent = nullptr);

    /// Construct a restore worker.
    explicit BackupWorker(Mode mode,
                          std::filesystem::path const& source,
                          std::filesystem::path const& destination,
                          backer::cli::RestoreOptions const& opts = {},
                          QObject* parent = nullptr);

    void run() override;

signals:
    /// Progress update during operation.
    void progressUpdated(int percent, QString const& currentFile,
                         int filesDone, int filesTotal,
                         qint64 bytesDone, qint64 bytesTotal);

    /// Log message at the given severity level (0 = info, 1 = warn, 2 = error).
    void logMessage(QString const& msg, int level);

    /// Operation finished with result.
    void finished(bool success, QString const& message);

public slots:
    /// Request cancellation of the running operation.
    void cancel();

private:
    Mode mode_;
    std::filesystem::path source_;
    std::filesystem::path destination_;
    std::variant<backer::cli::BackupOptions, backer::cli::RestoreOptions> options_;
    std::atomic<bool> cancelled_{false};
};

} // namespace backer::gui

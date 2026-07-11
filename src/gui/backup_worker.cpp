#include "gui/backup_worker.h"

#include <QDebug>

#include <chrono>
#include <cstdint>
#include <thread>

namespace backer::gui {

// -----------------------------------------------------------------------
// Constructors
// -----------------------------------------------------------------------

BackupWorker::BackupWorker(Mode mode,
                           std::filesystem::path const& source,
                           std::filesystem::path const& destination,
                           backer::cli::BackupOptions const& opts,
                           QObject* parent)
    : QThread(parent)
    , mode_(mode)
    , source_(source)
    , destination_(destination)
    , options_(opts)
{
}

BackupWorker::BackupWorker(Mode mode,
                           std::filesystem::path const& source,
                           std::filesystem::path const& destination,
                           backer::cli::RestoreOptions const& opts,
                           QObject* parent)
    : QThread(parent)
    , mode_(mode)
    , source_(source)
    , destination_(destination)
    , options_(opts)
{
}

// -----------------------------------------------------------------------
// Pre-scan: count files and total bytes in a directory tree
// -----------------------------------------------------------------------

namespace {

struct ScanResult {
    int fileCount = 0;
    std::uint64_t totalBytes = 0;
};

ScanResult preScan(std::filesystem::path const& root)
{
    ScanResult result;
    if (!std::filesystem::exists(root)) {
        return result;
    }
    std::error_code ec;
    for (auto const& entry :
         std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec)) {
            ++result.fileCount;
            result.totalBytes += entry.file_size(ec);
        }
    }
    return result;
}

} // anonymous namespace

// -----------------------------------------------------------------------
// run()
// -----------------------------------------------------------------------

void BackupWorker::run()
{
    emit logMessage(QStringLiteral("开始执行..."), 0); // "开始执行..."

    // --- Stage 1: Scanning (0% - 5%) ---
    emit progressUpdated(0, QString(), 0, 0, 0, 0);

    int filesTotal = 0;
    qint64 bytesTotal = 0;

    auto scanResult = preScan(source_);
    filesTotal = scanResult.fileCount;
    bytesTotal = static_cast<qint64>(scanResult.totalBytes);
    if (filesTotal == 0) {
        filesTotal = 1; // avoid division by zero
    }

    emit progressUpdated(5, QStringLiteral("扫描完成"), // "扫描完成"
                         0, filesTotal, 0LL, bytesTotal);
    emit logMessage(
        QStringLiteral("扫描结果: %1 个文件, %2 字节")
            .arg(filesTotal)
            .arg(bytesTotal),
        0);

    if (cancelled_) {
        requestInterruption();
        emit finished(false, QStringLiteral("操作已取消")); // "操作已取消"
        return;
    }

    // --- Stage 2: Executing (5% - 90%) ---
    emit logMessage(QStringLiteral("正在处理..."), 0); // "正在处理..."

    int resultCode = -1;
    std::string errorMsg;

    try {
        if (mode_ == Backup) {
            auto const& opts = std::get<backer::cli::BackupOptions>(options_);
            resultCode = backer::cli::handleBackup(source_, destination_, opts);
        } else {
            auto const& opts = std::get<backer::cli::RestoreOptions>(options_);
            resultCode = backer::cli::handleRestore(source_, destination_, opts);
        }
    } catch (std::exception const& e) {
        errorMsg = e.what();
        resultCode = 1;
    } catch (...) {
        errorMsg = "Unknown exception";
        resultCode = 1;
    }

    if (cancelled_) {
        requestInterruption();
        emit finished(false, QStringLiteral("操作已取消")); // "操作已取消"
        return;
    }

    // --- Stage 3: Finalizing (90% - 100%) ---
    for (int p = 90; p <= 100; p += 2) {
        if (cancelled_) {
            requestInterruption();
            emit finished(false,
                          QStringLiteral("操作已取消"));
            return;
        }
        emit progressUpdated(p, QStringLiteral("正在完成..."), // "正在完成..."
                             filesTotal, filesTotal, bytesTotal, bytesTotal);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // --- Completion ---
    bool success = (resultCode == 0);
    QString msg;
    if (success) {
        msg = QStringLiteral("操作成功完成"); // "操作成功完成"
        emit logMessage(msg, 0);
    } else {
        msg = QStringLiteral("操作失败");
        if (!errorMsg.empty()) {
            msg += QStringLiteral(": %1").arg(QString::fromStdString(errorMsg));
        }
        emit logMessage(msg, 2);
    }

    emit progressUpdated(100, QString(), filesTotal, filesTotal,
                         bytesTotal, bytesTotal);
    emit finished(success, msg);
}

// -----------------------------------------------------------------------
// cancel()
// -----------------------------------------------------------------------

void BackupWorker::cancel()
{
    cancelled_ = true;
    requestInterruption();
}

} // namespace backer::gui

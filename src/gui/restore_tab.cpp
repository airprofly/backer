#include "gui/restore_tab.h"
#include "gui/backup_worker.h"
#include "gui/gui_style.h"
#include "gui/gui_utils.h"
#include "gui/log_widget.h"
#include "gui/progress_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace backer::gui {

RestoreTab::RestoreTab(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

void RestoreTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── Source / Destination ──────────────────────────────────
    auto* srcLayout = new QHBoxLayout();
    srcLayout->addWidget(new QLabel(QStringLiteral("备份源:")));
    sourcePath_ = new QLineEdit();
    sourcePath_->setPlaceholderText(QStringLiteral("选择备份文件或目录..."));
    auto* browseSrcBtn = new QPushButton(QStringLiteral("浏览"));
    srcLayout->addWidget(sourcePath_, 1);
    srcLayout->addWidget(browseSrcBtn);
    mainLayout->addLayout(srcLayout);

    auto* destLayout = new QHBoxLayout();
    destLayout->addWidget(new QLabel(QStringLiteral("还原到:")));
    destPath_ = new QLineEdit();
    destPath_->setPlaceholderText(QStringLiteral("选择还原目标目录..."));
    auto* browseDestBtn = new QPushButton(QStringLiteral("浏览"));
    destLayout->addWidget(destPath_, 1);
    destLayout->addWidget(browseDestBtn);
    mainLayout->addLayout(destLayout);
    connect(browseSrcBtn, &QPushButton::clicked, this, &RestoreTab::onBrowseSource);
    connect(browseDestBtn, &QPushButton::clicked, this, &RestoreTab::onBrowseDest);

    // ── Password (only needed for encrypted backups) ──────────
    auto* pwdRow = new QHBoxLayout();
    pwdRow->addWidget(new QLabel(QStringLiteral("解密密码:")));
    password_ = new QLineEdit();
    password_->setEchoMode(QLineEdit::Password);
    password_->setPlaceholderText(QStringLiteral("加密备份时设置的密码（可选）"));
    pwdRow->addWidget(password_, 1);
    mainLayout->addLayout(pwdRow);

    // ── Restore flags ─────────────────────────────────────────
    auto* flagLayout = new QHBoxLayout();
    preserveMetadata_ = new QCheckBox(QStringLiteral("保留元数据"));
    preserveMetadata_->setChecked(true);
    handleSpecial_ = new QCheckBox(QStringLiteral("处理特殊文件"));
    handleSpecial_->setChecked(true);
    flagLayout->addWidget(preserveMetadata_);
    flagLayout->addWidget(handleSpecial_);
    flagLayout->addStretch();
    mainLayout->addLayout(flagLayout);

    // ── Action buttons ────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    startBtn_ = new QPushButton(QStringLiteral("开始还原"));
    style::styleButton(startBtn_, QColor(style::kAccentBlue));
    cancelBtn_ = new QPushButton(QStringLiteral("取消"));
    style::styleButton(cancelBtn_);
    cancelBtn_->setEnabled(false);
    btnLayout->addStretch();
    btnLayout->addWidget(startBtn_);
    btnLayout->addWidget(cancelBtn_);
    mainLayout->addLayout(btnLayout);

    // ── Progress & Log ────────────────────────────────────────
    progressWidget_ = new ProgressWidget();
    progressWidget_->setRunning(false);
    mainLayout->addWidget(progressWidget_);

    logWidget_ = new LogWidget();
    mainLayout->addWidget(logWidget_, 1);
}

void RestoreTab::setupConnections()
{
    connect(startBtn_, &QPushButton::clicked,
            this, &RestoreTab::onStartRestore);
    connect(cancelBtn_, &QPushButton::clicked,
            this, &RestoreTab::onCancel);
}

void RestoreTab::onBrowseSource()
{
    // Try archive file first, fall back to directory
    QString file = QFileDialog::getOpenFileName(this,
        QStringLiteral("选择备份文件"));
    if (file.isEmpty()) {
        QString dir = QFileDialog::getExistingDirectory(this,
            QStringLiteral("选择备份目录"));
        if (!dir.isEmpty())
            sourcePath_->setText(dir);
    } else {
        sourcePath_->setText(file);
    }
}

void RestoreTab::onBrowseDest()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择还原目标目录"), destPath_->text());
    if (!dir.isEmpty())
        destPath_->setText(dir);
}

void RestoreTab::onStartRestore()
{
    if (sourcePath_->text().isEmpty() || destPath_->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请选择备份源和还原目标目录"));
        return;
    }
    auto srcStr = sourcePath_->text().toStdString();
    if (!std::filesystem::exists(srcStr)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("备份源路径不存在"));
        return;
    }

    auto src = std::filesystem::path(srcStr);
    auto dst = std::filesystem::path(destPath_->text().toStdString());

    // Auto-detect all options from the backup filename
    auto detected = detectRestoreOptions(src);
    if (detected.isEncrypted && password_->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("备份文件已加密，请输入解密密码"));
        return;
    }

    startBtn_->setEnabled(false);
    cancelBtn_->setEnabled(true);
    progressWidget_->setRunning(true);
    logWidget_->clear();

    cli::RestoreOptions opts;
    opts.preserveMetadata = preserveMetadata_->isChecked();
    opts.handleSpecial = handleSpecial_->isChecked();
    opts.decompressAlgo = detected.decompressAlgo;
    opts.packFormat = detected.packFormat;
    if (detected.isEncrypted) {
        // Leave decryptAlgo empty — BackupWorker will auto-try AES then SM4
        opts.password = password_->text().toStdString();
    }

    // Create timestamped subdirectory inside chosen destination.
    auto restorePath = makeRestoreSubPath(dst, src);

    worker_ = new BackupWorker(BackupWorker::Restore, src, restorePath, opts, this);

    connect(worker_, &BackupWorker::progressUpdated,
            this, [this](int pct, QString const& file, int done, int total,
                         qint64 bytesDone, qint64 bytesTotal) {
        progressWidget_->setValue(pct);
        progressWidget_->setCurrentFile(file);
        progressWidget_->setStats(done, total, bytesDone, bytesTotal);
    });
    connect(worker_, &BackupWorker::logMessage,
            logWidget_, &LogWidget::appendMessage);
    connect(worker_, &BackupWorker::finished,
            this, &RestoreTab::onRestoreFinished);

    worker_->start();
    logWidget_->appendMessage(QStringLiteral("还原任务已启动"), 0);
}

void RestoreTab::onCancel()
{
    if (worker_ && worker_->isRunning()) {
        worker_->cancel();
        cancelBtn_->setEnabled(false);
        logWidget_->appendMessage(QStringLiteral("正在取消还原..."), 1);
    }
}

void RestoreTab::onRestoreFinished(bool success, QString const& msg)
{
    startBtn_->setEnabled(true);
    cancelBtn_->setEnabled(false);
    progressWidget_->setRunning(false);

    if (success) {
        progressWidget_->setValue(100);
        logWidget_->appendMessage(QStringLiteral("还原完成"), 0);
    } else {
        logWidget_->appendMessage(QStringLiteral("还原失败: ") + msg, 2);
    }

    emit restoreFinished(success, msg);

    if (worker_) {
        worker_->deleteLater();
        worker_ = nullptr;
    }
}

} // namespace backer::gui

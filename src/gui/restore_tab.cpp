#include "gui/restore_tab.h"
#include "gui/backup_worker.h"
#include "gui/gui_style.h"
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

    // ── Decompress / Pack options ─────────────────────────────
    auto* optionsLayout = new QHBoxLayout();
    enableDecompress_ = new QCheckBox(QStringLiteral("解压缩"));
    decompressAlgo_ = new QComboBox();
    decompressAlgo_->addItems({QStringLiteral("gzip"), QStringLiteral("zstd"), QStringLiteral("lzma")});
    decompressAlgo_->setEnabled(false);
    enablePack_ = new QCheckBox(QStringLiteral("打包格式"));
    packFormat_ = new QComboBox();
    packFormat_->addItems({QStringLiteral("Tar"), QStringLiteral("Zip")});
    packFormat_->setEnabled(false);
    optionsLayout->addWidget(enableDecompress_);
    optionsLayout->addWidget(decompressAlgo_);
    optionsLayout->addSpacing(12);
    optionsLayout->addWidget(enablePack_);
    optionsLayout->addWidget(packFormat_);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

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

    // ── Decrypt options ─────────────────────────────────────────
    auto* decryptLayout = new QHBoxLayout();
    enableDecrypt_ = new QCheckBox(QStringLiteral("解密"));
    decryptAlgo_ = new QComboBox();
    decryptAlgo_->addItems({QStringLiteral("AES-256"), QStringLiteral("SM4")});
    decryptAlgo_->setEnabled(false);
    password_ = new QLineEdit();
    password_->setEchoMode(QLineEdit::Password);
    password_->setPlaceholderText(QStringLiteral("解密密码"));
    password_->setEnabled(false);
    confirmPassword_ = new QLineEdit();
    confirmPassword_->setEchoMode(QLineEdit::Password);
    confirmPassword_->setPlaceholderText(QStringLiteral("确认密码"));
    confirmPassword_->setEnabled(false);
    decryptLayout->addWidget(enableDecrypt_);
    decryptLayout->addWidget(decryptAlgo_);
    decryptLayout->addWidget(password_);
    decryptLayout->addWidget(confirmPassword_);
    decryptLayout->addStretch();
    mainLayout->addLayout(decryptLayout);

    // ── Action buttons ────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    auto* resetBtn = new QPushButton(QStringLiteral("恢复默认"));
    style::styleButton(resetBtn, {}, /*flat=*/true);
    startBtn_ = new QPushButton(QStringLiteral("开始还原"));
    style::styleButton(startBtn_, QColor(style::kAccentBlue));
    cancelBtn_ = new QPushButton(QStringLiteral("取消"));
    style::styleButton(cancelBtn_);
    cancelBtn_->setEnabled(false);
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(startBtn_);
    btnLayout->addWidget(cancelBtn_);
    connect(resetBtn, &QPushButton::clicked,
            this, &RestoreTab::onResetDefaults);
    mainLayout->addLayout(btnLayout);

    // ── Progress & Log ────────────────────────────────────────
    progressWidget_ = new ProgressWidget();
    progressWidget_->setRunning(false);
    mainLayout->addWidget(progressWidget_);

    logWidget_ = new LogWidget();
    logWidget_->setMaximumHeight(150);
    mainLayout->addWidget(logWidget_, 1);
}

void RestoreTab::setupConnections()
{
    connect(startBtn_, &QPushButton::clicked,
            this, &RestoreTab::onStartRestore);
    connect(cancelBtn_, &QPushButton::clicked,
            this, &RestoreTab::onCancel);

    connect(enableDecompress_, &QCheckBox::toggled,
            decompressAlgo_, &QComboBox::setEnabled);
    connect(enablePack_, &QCheckBox::toggled,
            packFormat_, &QComboBox::setEnabled);
    connect(enableDecrypt_, &QCheckBox::toggled, this,
            [this](bool checked) {
                decryptAlgo_->setEnabled(checked);
                password_->setEnabled(checked);
                confirmPassword_->setEnabled(checked);
            });
}

void RestoreTab::onBrowseSource()
{
    // Try directory first (mirror mode), fall back to archive file
    QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择备份目录"));
    if (dir.isEmpty()) {
        QString file = QFileDialog::getOpenFileName(this,
            QStringLiteral("选择备份文件"));
        if (!file.isEmpty())
            sourcePath_->setText(file);
    } else {
        sourcePath_->setText(dir);
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
    if (!std::filesystem::exists(sourcePath_->text().toStdString())) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("备份源路径不存在"));
        return;
    }

    // Validate decrypt password
    if (enableDecrypt_->isChecked()) {
        if (password_->text().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("请输入解密密码"));
            return;
        }
        if (password_->text() != confirmPassword_->text()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("两次输入的密码不一致"));
            return;
        }
    }

    startBtn_->setEnabled(false);
    cancelBtn_->setEnabled(true);
    progressWidget_->setRunning(true);
    logWidget_->clear();

    cli::RestoreOptions opts;
    opts.preserveMetadata = preserveMetadata_->isChecked();
    opts.handleSpecial = handleSpecial_->isChecked();
    if (enableDecompress_->isChecked())
        opts.decompressAlgo = decompressAlgo_->currentText().toStdString();
    if (enablePack_->isChecked())
        opts.packFormat = packFormat_->currentText().toLower().toStdString();
    if (enableDecrypt_->isChecked()) {
        opts.decryptAlgo = decryptAlgo_->currentText().toStdString();
        opts.password = password_->text().toStdString();
    }

    auto src = std::filesystem::path(sourcePath_->text().toStdString());
    auto dst = std::filesystem::path(destPath_->text().toStdString());

    worker_ = new BackupWorker(BackupWorker::Restore, src, dst, opts, this);

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

void RestoreTab::onResetDefaults()
{
    sourcePath_->clear();
    destPath_->clear();
    enableDecompress_->setChecked(false);
    enablePack_->setChecked(false);
    enableDecrypt_->setChecked(false);
    preserveMetadata_->setChecked(true);
    handleSpecial_->setChecked(true);
    password_->clear();
    confirmPassword_->clear();
}

void RestoreTab::onRestoreFinished(bool success, QString const& message)
{
    startBtn_->setEnabled(true);
    cancelBtn_->setEnabled(false);
    progressWidget_->setRunning(false);

    if (success) {
        progressWidget_->setValue(100);
        logWidget_->appendMessage(QStringLiteral("还原完成"), 0);
    } else {
        logWidget_->appendMessage(QStringLiteral("还原失败"), 2);
    }

    emit restoreFinished(success, message);

    if (worker_) {
        worker_->deleteLater();
        worker_ = nullptr;
    }
}

} // namespace backer::gui

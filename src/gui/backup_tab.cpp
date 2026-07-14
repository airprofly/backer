#include "gui/backup_tab.h"
#include "gui/backup_worker.h"
#include "gui/filter_dialog.h"
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
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

namespace backer::gui {

BackupTab::BackupTab(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

void BackupTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── Source / Destination ──────────────────────────────────
    auto* srcLayout = new QHBoxLayout();
    srcLayout->addWidget(new QLabel(QStringLiteral("源目录:")));
    sourcePath_ = new QLineEdit();
    sourcePath_->setPlaceholderText(QStringLiteral("选择要备份的源目录..."));
    browseSourceBtn_ = new QPushButton(QStringLiteral("浏览"));
    srcLayout->addWidget(sourcePath_, 1);
    srcLayout->addWidget(browseSourceBtn_);
    mainLayout->addLayout(srcLayout);

    auto* destLayout = new QHBoxLayout();
    destLayout->addWidget(new QLabel(QStringLiteral("目标目录:")));
    destPath_ = new QLineEdit();
    destPath_->setPlaceholderText(QStringLiteral("选择备份输出目录..."));
    browseDestBtn_ = new QPushButton(QStringLiteral("浏览"));
    destLayout->addWidget(destPath_, 1);
    destLayout->addWidget(browseDestBtn_);
    mainLayout->addLayout(destLayout);

    // ── Pack / Compress / Encrypt ─────────────────────────────
    auto* optionsLayout = new QHBoxLayout();

    enablePack_ = new QCheckBox(QStringLiteral("打包"));
    packFormat_ = new QComboBox();
    packFormat_->addItems({QStringLiteral("Tar"), QStringLiteral("Zip")});
    packFormat_->setEnabled(false);

    enableCompress_ = new QCheckBox(QStringLiteral("压缩"));
    compressAlgo_ = new QComboBox();
    compressAlgo_->addItems({QStringLiteral("gzip"), QStringLiteral("zstd"), QStringLiteral("lzma")});
    compressAlgo_->setEnabled(false);
    compressLevel_ = new QSpinBox();
    compressLevel_->setRange(1, 22);
    compressLevel_->setValue(3);
    compressLevel_->setToolTip(QStringLiteral("1=快速, 22=最佳, 0=默认"));
    compressLevel_->setEnabled(false);

    enableEncrypt_ = new QCheckBox(QStringLiteral("加密"));
    encryptAlgo_ = new QComboBox();
    encryptAlgo_->addItems({QStringLiteral("AES-256"), QStringLiteral("SM4")});
    encryptAlgo_->setEnabled(false);
    password_ = new QLineEdit();
    password_->setEchoMode(QLineEdit::Password);
    password_->setPlaceholderText(QStringLiteral("密码"));
    password_->setEnabled(false);
    confirmPassword_ = new QLineEdit();
    confirmPassword_->setEchoMode(QLineEdit::Password);
    confirmPassword_->setPlaceholderText(QStringLiteral("确认密码"));
    confirmPassword_->setEnabled(false);

    optionsLayout->addWidget(enablePack_);
    optionsLayout->addWidget(packFormat_);
    optionsLayout->addSpacing(12);
    optionsLayout->addWidget(enableCompress_);
    optionsLayout->addWidget(compressAlgo_);
    optionsLayout->addWidget(compressLevel_);
    optionsLayout->addSpacing(12);
    optionsLayout->addWidget(enableEncrypt_);
    optionsLayout->addWidget(encryptAlgo_);
    optionsLayout->addWidget(password_);
    optionsLayout->addWidget(confirmPassword_);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    // ── Filter ────────────────────────────────────────────────
    auto* filterRow = new QHBoxLayout();
    enableFilter_ = new QCheckBox(QStringLiteral("使用筛选"));
    editFilterBtn_ = new QPushButton(QStringLiteral("编辑筛选条件..."));
    editFilterBtn_->setEnabled(false);
    filterRow->addWidget(enableFilter_);
    filterRow->addWidget(editFilterBtn_);
    filterRow->addStretch();
    mainLayout->addLayout(filterRow);

    // ── Filter detail group ───────────────────────────────────
    filterGroup_ = new QGroupBox(QStringLiteral("文件筛选"));
    auto* filterForm = new QFormLayout(filterGroup_);
    includePaths_ = new QLineEdit();
    includePaths_->setPlaceholderText(QStringLiteral("用逗号分隔的包含路径"));
    excludePaths_ = new QLineEdit();
    excludePaths_->setPlaceholderText(QStringLiteral("用逗号分隔的排除路径"));
    includeTypes_ = new QLineEdit();
    includeTypes_->setPlaceholderText(QStringLiteral("file, dir, symlink, fifo, block, char"));
    excludeTypes_ = new QLineEdit();
    excludeTypes_->setPlaceholderText(QStringLiteral("file, dir, symlink, fifo, block, char"));
    filterForm->addRow(QStringLiteral("包含路径:"), includePaths_);
    filterForm->addRow(QStringLiteral("排除路径:"), excludePaths_);
    filterForm->addRow(QStringLiteral("包含类型:"), includeTypes_);
    filterForm->addRow(QStringLiteral("排除类型:"), excludeTypes_);
    filterGroup_->setVisible(false);
    mainLayout->addWidget(filterGroup_);

    // ── Action buttons ────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    auto* resetBtn = new QPushButton(QStringLiteral("恢复默认"));
    style::styleButton(resetBtn, {}, /*flat=*/true);
    startBtn_ = new QPushButton(QStringLiteral("开始备份"));
    style::styleButton(startBtn_, QColor(style::kAccentGreen));
    cancelBtn_ = new QPushButton(QStringLiteral("取消"));
    style::styleButton(cancelBtn_);
    cancelBtn_->setEnabled(false);
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(startBtn_);
    btnLayout->addWidget(cancelBtn_);
    mainLayout->addLayout(btnLayout);
    connect(resetBtn, &QPushButton::clicked,
            this, &BackupTab::onResetDefaults);

    // ── Progress ──────────────────────────────────────────────
    progressWidget_ = new ProgressWidget();
    progressWidget_->setRunning(false);
    mainLayout->addWidget(progressWidget_);

    // ── Log ───────────────────────────────────────────────────
    logWidget_ = new LogWidget();
    logWidget_->setMaximumHeight(150);
    mainLayout->addWidget(logWidget_, 1);
}

void BackupTab::setupConnections()
{
    connect(browseSourceBtn_, &QPushButton::clicked,
            this, &BackupTab::onBrowseSource);
    connect(browseDestBtn_, &QPushButton::clicked,
            this, &BackupTab::onBrowseDest);
    connect(startBtn_, &QPushButton::clicked,
            this, &BackupTab::onStartBackup);
    connect(cancelBtn_, &QPushButton::clicked,
            this, &BackupTab::onCancel);
    connect(editFilterBtn_, &QPushButton::clicked,
            this, &BackupTab::onEditFilter);
    connect(enablePack_, &QCheckBox::toggled,
            this, &BackupTab::onPackToggled);
    connect(enableCompress_, &QCheckBox::toggled,
            this, &BackupTab::onCompressToggled);
    connect(enableEncrypt_, &QCheckBox::toggled,
            this, &BackupTab::onEncryptToggled);
    connect(enableFilter_, &QCheckBox::toggled,
            filterGroup_, &QGroupBox::setVisible);
    connect(enableFilter_, &QCheckBox::toggled,
            editFilterBtn_, &QPushButton::setEnabled);
}

void BackupTab::onBrowseSource()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择源目录"), sourcePath_->text());
    if (!dir.isEmpty())
        sourcePath_->setText(dir);
}

void BackupTab::onBrowseDest()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择目标目录"), destPath_->text());
    if (!dir.isEmpty())
        destPath_->setText(dir);
}

void BackupTab::onPackToggled(bool checked)
{
    packFormat_->setEnabled(checked);
}

void BackupTab::onCompressToggled(bool checked)
{
    compressAlgo_->setEnabled(checked);
    compressLevel_->setEnabled(checked);
}

void BackupTab::onEncryptToggled(bool checked)
{
    encryptAlgo_->setEnabled(checked);
    password_->setEnabled(checked);
    confirmPassword_->setEnabled(checked);
}

void BackupTab::onResetDefaults()
{
    // Clear paths
    sourcePath_->clear();
    destPath_->clear();

    // Uncheck all option toggles
    enablePack_->setChecked(false);
    enableCompress_->setChecked(false);
    enableEncrypt_->setChecked(false);
    enableFilter_->setChecked(false);

    // Clear password fields
    password_->clear();
    confirmPassword_->clear();

    // Clear filter text fields and state
    includePaths_->clear();
    excludePaths_->clear();
    includeTypes_->clear();
    excludeTypes_->clear();
    hasTimeFilter_ = false;
    hasSizeFilter_ = false;
    sizeMin_ = 0;
    sizeMax_ = 0;
    owner_.clear();
}

void BackupTab::onEditFilter()
{
    FilterDialog dlg(this);

    // Pre-fill dialog from current state
    dlg.setIncludePaths(includePaths_->text().split(QStringLiteral(", "),
                        Qt::SkipEmptyParts));
    dlg.setExcludePaths(excludePaths_->text().split(QStringLiteral(", "),
                        Qt::SkipEmptyParts));

    if (dlg.exec() == QDialog::Accepted) {
        // Paths
        includePaths_->setText(dlg.includePaths().join(QStringLiteral(", ")));
        excludePaths_->setText(dlg.excludePaths().join(QStringLiteral(", ")));

        // Types (checkbox → comma-separated text)
        includeTypes_->setText(dlg.includeTypes().join(QStringLiteral(", ")));

        // Time range
        hasTimeFilter_ = dlg.hasTimeFilter();
        mtimeAfter_ = dlg.mtimeAfter();
        mtimeBefore_ = dlg.mtimeBefore();

        // Size range
        hasSizeFilter_ = dlg.hasSizeFilter();
        sizeMin_ = dlg.sizeMin();
        sizeMax_ = dlg.sizeMax();

        // Owner
        owner_ = dlg.owner();
    }
}

backer::cli::BackupOptions BackupTab::collectOptions() const
{
    cli::BackupOptions opts;
    opts.preserveMetadata = true;
    opts.handleSpecial = true;

    if (enablePack_->isChecked())
        opts.packFormat = packFormat_->currentText().toLower().toStdString();

    if (enableCompress_->isChecked()) {
        opts.compressAlgo = compressAlgo_->currentText().toStdString();
        opts.compressLevel = compressLevel_->value();
    }

    if (enableEncrypt_->isChecked()) {
        opts.encryptAlgo = encryptAlgo_->currentText().toStdString();
        opts.password = password_->text().toStdString();
    }

    // Parse filter fields
    if (enableFilter_->isChecked()) {
        auto split = [](QString const& text) {
            return text.split(QRegularExpression(QStringLiteral("[,\\s;]+")),
                              Qt::SkipEmptyParts);
        };
        for (auto const& s : split(includePaths_->text()))
            opts.includePaths.push_back(s.toStdString());
        for (auto const& s : split(excludePaths_->text()))
            opts.excludePaths.push_back(s.toStdString());
        for (auto const& s : split(includeTypes_->text()))
            opts.includeTypes.push_back(s.toStdString());
        for (auto const& s : split(excludeTypes_->text()))
            opts.excludeTypes.push_back(s.toStdString());

        // Time range from FilterDialog
        if (hasTimeFilter_) {
            opts.mtimeAfter = std::to_string(mtimeAfter_.toSecsSinceEpoch());
            opts.mtimeBefore = std::to_string(mtimeBefore_.toSecsSinceEpoch());
        }
        // Size range from FilterDialog
        if (hasSizeFilter_) {
            opts.hasSizeMin = true;
            opts.sizeMin = static_cast<uint64_t>(sizeMin_);
            opts.hasSizeMax = true;
            opts.sizeMax = static_cast<uint64_t>(sizeMax_);
        }
        // Owner filter from FilterDialog
        if (!owner_.isEmpty()) {
            opts.owner = owner_.toStdString();
        }
    }

    return opts;
}

void BackupTab::onStartBackup()
{
    // Validate
    if (sourcePath_->text().isEmpty() || destPath_->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请选择源目录和目标目录"));
        return;
    }
    if (!std::filesystem::exists(sourcePath_->text().toStdString())) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("源目录不存在"));
        return;
    }

    if (enableEncrypt_->isChecked()) {
        if (password_->text().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("请输入加密密码"));
            return;
        }
        if (password_->text() != confirmPassword_->text()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("两次输入的密码不一致"));
            return;
        }
    }

    // Disable UI during backup
    startBtn_->setEnabled(false);
    cancelBtn_->setEnabled(true);
    progressWidget_->setRunning(true);
    logWidget_->clear();

    auto opts = collectOptions();
    auto src = std::filesystem::path(sourcePath_->text().toStdString());
    auto dst = std::filesystem::path(destPath_->text().toStdString());

    // When pack is enabled and destination is an existing directory,
    // put the archive INSIDE that directory (not alongside it).
    if (!opts.packFormat.empty() && std::filesystem::is_directory(dst)) {
        auto name = dst.filename().string();
        if (name.empty() || name == "." || name == "..") {
            name = "backup";
        }
        dst = dst / name;
    }

    worker_ = new BackupWorker(BackupWorker::Backup, src, dst, opts, this);

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
            this, &BackupTab::onBackupFinished);

    worker_->start();
    logWidget_->appendMessage(QStringLiteral("备份任务已启动"), 0);
}

void BackupTab::onCancel()
{
    if (worker_ && worker_->isRunning()) {
        worker_->cancel();
        cancelBtn_->setEnabled(false);
        logWidget_->appendMessage(QStringLiteral("正在取消备份..."), 1);
    }
}

void BackupTab::onBackupFinished(bool success, QString const& msg)
{
    startBtn_->setEnabled(true);
    cancelBtn_->setEnabled(false);
    progressWidget_->setRunning(false);

    if (success) {
        progressWidget_->setValue(100);
        logWidget_->appendMessage(QStringLiteral("备份完成"), 0);
    } else {
        logWidget_->appendMessage(QStringLiteral("备份失败: ") + msg, 2);
    }

    emit backupFinished(success, msg);

    if (worker_) {
        worker_->deleteLater();
        worker_ = nullptr;
    }
}

} // namespace backer::gui

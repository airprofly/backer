#include "gui/backup_tab.h"
#include "gui/backup_worker.h"
#include "gui/filter_dialog.h"
#include "gui/gui_style.h"
#include "gui/log_widget.h"
#include "gui/progress_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
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

    // Row 1-2: Path
    includePaths_ = new QLineEdit();
    includePaths_->setPlaceholderText(QStringLiteral("用逗号分隔的包含路径 (glob)"));
    excludePaths_ = new QLineEdit();
    excludePaths_->setPlaceholderText(QStringLiteral("用逗号分隔的排除路径 (glob)"));
    filterForm->addRow(QStringLiteral("包含路径:"), includePaths_);
    filterForm->addRow(QStringLiteral("排除路径:"), excludePaths_);

    // Row 3-4: Type
    includeTypes_ = new QLineEdit();
    includeTypes_->setPlaceholderText(QStringLiteral("file, dir, symlink, fifo, block, char, socket"));
    excludeTypes_ = new QLineEdit();
    excludeTypes_->setPlaceholderText(QStringLiteral("file, dir, symlink, fifo, block, char, socket"));
    filterForm->addRow(QStringLiteral("包含类型:"), includeTypes_);
    filterForm->addRow(QStringLiteral("排除类型:"), excludeTypes_);

    // Row 5-6: Name
    includeNames_ = new QLineEdit();
    includeNames_->setPlaceholderText(QStringLiteral("用逗号分隔的包含名称 (如 *.txt, *.cpp)"));
    excludeNames_ = new QLineEdit();
    excludeNames_->setPlaceholderText(QStringLiteral("用逗号分隔的排除名称 (如 *.tmp, *.log)"));
    filterForm->addRow(QStringLiteral("包含名称:"), includeNames_);
    filterForm->addRow(QStringLiteral("排除名称:"), excludeNames_);

    // Row 7: Time
    auto* timeRow = new QHBoxLayout();
    enableTimeFilter_ = new QCheckBox(QStringLiteral("启用"));
    mtimeAfter_ = new QDateTimeEdit();
    mtimeAfter_->setCalendarPopup(true);
    mtimeAfter_->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    mtimeAfter_->setDateTime(QDateTime::currentDateTime().addDays(-7));
    mtimeAfter_->setEnabled(false);
    mtimeBefore_ = new QDateTimeEdit();
    mtimeBefore_->setCalendarPopup(true);
    mtimeBefore_->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    mtimeBefore_->setDateTime(QDateTime::currentDateTime());
    mtimeBefore_->setEnabled(false);
    timeRow->addWidget(enableTimeFilter_);
    timeRow->addWidget(new QLabel(QStringLiteral("修改时间:")));
    timeRow->addWidget(mtimeAfter_);
    timeRow->addWidget(new QLabel(QStringLiteral("→")));
    timeRow->addWidget(mtimeBefore_);
    timeRow->addStretch();
    filterForm->addRow(QStringLiteral("时间筛选:"), timeRow);

    // Row 8: Size
    auto* sizeRow = new QHBoxLayout();
    enableSizeFilter_ = new QCheckBox(QStringLiteral("启用"));
    sizeMin_ = new QSpinBox();
    sizeMin_->setRange(0, 999999999);
    sizeMin_->setValue(0);
    sizeMin_->setEnabled(false);
    sizeMax_ = new QSpinBox();
    sizeMax_->setRange(0, 999999999);
    sizeMax_->setValue(0);
    sizeMax_->setSpecialValueText(QStringLiteral("不限"));
    sizeMax_->setEnabled(false);
    sizeUnitMin_ = new QComboBox();
    sizeUnitMin_->addItems({QStringLiteral("B"), QStringLiteral("KB"),
                            QStringLiteral("MB"), QStringLiteral("GB")});
    sizeUnitMin_->setEnabled(false);
    sizeUnitMax_ = new QComboBox();
    sizeUnitMax_->addItems({QStringLiteral("B"), QStringLiteral("KB"),
                            QStringLiteral("MB"), QStringLiteral("GB")});
    sizeUnitMax_->setEnabled(false);
    sizeRow->addWidget(enableSizeFilter_);
    sizeRow->addWidget(new QLabel(QStringLiteral("最小:")));
    sizeRow->addWidget(sizeMin_);
    sizeRow->addWidget(sizeUnitMin_);
    sizeRow->addSpacing(6);
    sizeRow->addWidget(new QLabel(QStringLiteral("最大:")));
    sizeRow->addWidget(sizeMax_);
    sizeRow->addWidget(sizeUnitMax_);
    sizeRow->addStretch();
    filterForm->addRow(QStringLiteral("大小筛选:"), sizeRow);

    // Row 9: Owner
    ownerFilter_ = new QLineEdit();
    ownerFilter_->setPlaceholderText(QStringLiteral("用户名或 UID"));
    filterForm->addRow(QStringLiteral("所有者:"), ownerFilter_);

    filterGroup_->setVisible(false);
    mainLayout->addWidget(filterGroup_);

    // ── Action buttons ────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    startBtn_ = new QPushButton(QStringLiteral("开始备份"));
    style::styleButton(startBtn_, QColor(style::kAccentGreen));
    cancelBtn_ = new QPushButton(QStringLiteral("取消"));
    style::styleButton(cancelBtn_);
    cancelBtn_->setEnabled(false);
    btnLayout->addStretch();
    btnLayout->addWidget(startBtn_);
    btnLayout->addWidget(cancelBtn_);
    mainLayout->addLayout(btnLayout);

    // ── Progress ──────────────────────────────────────────────
    progressWidget_ = new ProgressWidget();
    progressWidget_->setRunning(false);
    mainLayout->addWidget(progressWidget_);

    // ── Log ───────────────────────────────────────────────────
    logWidget_ = new LogWidget();
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
    connect(enableTimeFilter_, &QCheckBox::toggled,
            mtimeAfter_, &QWidget::setEnabled);
    connect(enableTimeFilter_, &QCheckBox::toggled,
            mtimeBefore_, &QWidget::setEnabled);
    connect(enableSizeFilter_, &QCheckBox::toggled,
            sizeMin_, &QWidget::setEnabled);
    connect(enableSizeFilter_, &QCheckBox::toggled,
            sizeMax_, &QWidget::setEnabled);
    connect(enableSizeFilter_, &QCheckBox::toggled,
            sizeUnitMin_, &QWidget::setEnabled);
    connect(enableSizeFilter_, &QCheckBox::toggled,
            sizeUnitMax_, &QWidget::setEnabled);
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

namespace {
/// Byte multiplier for size-unit combobox index.
qint64 unitMult(int index) noexcept
{
    static constexpr qint64 kUnits[] = {1LL, 1024LL, 1024LL * 1024, 1024LL * 1024 * 1024};
    return (index >= 0 && index < 4) ? kUnits[index] : 1;
}
} // anonymous namespace

void BackupTab::onEditFilter()
{
    FilterDialog dlg(this);

    // Pre-fill from current inline values
    dlg.setIncludePaths(includePaths_->text().split(
        QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts));
    dlg.setExcludePaths(excludePaths_->text().split(
        QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts));
    dlg.setIncludeNames(includeNames_->text().split(
        QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts));
    dlg.setExcludeNames(excludeNames_->text().split(
        QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts));

    if (dlg.exec() == QDialog::Accepted) {
        // Sync path/name text fields
        includePaths_->setText(dlg.includePaths().join(QStringLiteral(", ")));
        excludePaths_->setText(dlg.excludePaths().join(QStringLiteral(", ")));
        includeNames_->setText(dlg.includeNames().join(QStringLiteral(", ")));
        excludeNames_->setText(dlg.excludeNames().join(QStringLiteral(", ")));

        // Sync type checkboxes → text field
        includeTypes_->setText(dlg.includeTypes().join(QStringLiteral(", ")));

        // Sync time inline controls
        enableTimeFilter_->setChecked(dlg.hasTimeFilter());
        mtimeAfter_->setDateTime(dlg.mtimeAfter());
        mtimeBefore_->setDateTime(dlg.mtimeBefore());

        // Sync size inline controls
        enableSizeFilter_->setChecked(dlg.hasSizeFilter());
        qint64 szMin = dlg.sizeMin();
        qint64 szMax = dlg.sizeMax();
        // Find best unit (auto-scale to largest unit >= 1)
        int unitIdx = 0;
        for (int i = 3; i > 0; --i) {
            if (szMin > 0 && szMin % unitMult(i) == 0) { unitIdx = i; break; }
            if (szMax > 0 && szMax % unitMult(i) == 0) { unitIdx = i; break; }
        }
        if (szMin > 0 && szMin % unitMult(unitIdx) == 0) {
            sizeMin_->setValue(static_cast<int>(szMin / unitMult(unitIdx)));
            sizeUnitMin_->setCurrentIndex(unitIdx);
        } else {
            sizeMin_->setValue(0);
            sizeUnitMin_->setCurrentIndex(0);
        }
        if (szMax > 0 && szMax % unitMult(unitIdx) == 0) {
            sizeMax_->setValue(static_cast<int>(szMax / unitMult(unitIdx)));
            sizeUnitMax_->setCurrentIndex(unitIdx);
        } else {
            sizeMax_->setValue(0);
            sizeUnitMax_->setCurrentIndex(0);
        }

        // Sync owner
        ownerFilter_->setText(dlg.owner());
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
        for (auto const& s : split(includeNames_->text()))
            opts.includeNames.push_back(s.toStdString());
        for (auto const& s : split(excludeNames_->text()))
            opts.excludeNames.push_back(s.toStdString());

        // Time filter (inline controls)
        if (enableTimeFilter_->isChecked()) {
            opts.mtimeAfter  = mtimeAfter_->dateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")).toStdString();
            opts.mtimeBefore = mtimeBefore_->dateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")).toStdString();
        }

        // Size filter (inline controls)
        if (enableSizeFilter_->isChecked()) {
            qint64 minBytes = sizeMin_->value() * unitMult(sizeUnitMin_->currentIndex());
            qint64 maxBytes = sizeMax_->value() * unitMult(sizeUnitMax_->currentIndex());
            if (minBytes > 0) { opts.hasSizeMin = true; opts.sizeMin = static_cast<uint64_t>(minBytes); }
            if (maxBytes > 0) { opts.hasSizeMax = true; opts.sizeMax = static_cast<uint64_t>(maxBytes); }
        }

        // Owner filter
        if (!ownerFilter_->text().trimmed().isEmpty())
            opts.owner = ownerFilter_->text().trimmed().toStdString();
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

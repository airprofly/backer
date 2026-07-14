#include "gui/settings_tab.h"
#include "gui/gui_style.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace backer::gui {

SettingsTab::SettingsTab(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    loadSettings();
}

void SettingsTab::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    // ── Default paths ─────────────────────────────────────────
    auto* pathGroup = new QGroupBox(QStringLiteral("默认路径"));
    auto* pathForm = new QFormLayout(pathGroup);

    auto* srcRow = new QHBoxLayout();
    defaultSource_ = new QLineEdit();
    defaultSource_->setPlaceholderText(QStringLiteral("默认源目录"));
    auto* browseSrcBtn = new QPushButton(QStringLiteral("浏览"));
    srcRow->addWidget(defaultSource_, 1);
    srcRow->addWidget(browseSrcBtn);
    pathForm->addRow(QStringLiteral("默认源目录:"), srcRow);

    auto* destRow = new QHBoxLayout();
    defaultDest_ = new QLineEdit();
    defaultDest_->setPlaceholderText(QStringLiteral("默认目标目录"));
    auto* browseDestBtn = new QPushButton(QStringLiteral("浏览"));
    destRow->addWidget(defaultDest_, 1);
    destRow->addWidget(browseDestBtn);
    pathForm->addRow(QStringLiteral("默认目标目录:"), destRow);

    layout->addWidget(pathGroup);

    connect(browseSrcBtn, &QPushButton::clicked,
            this, &SettingsTab::onBrowseDefaultSource);
    connect(browseDestBtn, &QPushButton::clicked,
            this, &SettingsTab::onBrowseDefaultDest);

    // ── Default options ───────────────────────────────────────
    auto* optGroup = new QGroupBox(QStringLiteral("默认选项"));
    auto* optForm = new QFormLayout(optGroup);

    defaultPack_ = new QComboBox();
    defaultPack_->addItems({QStringLiteral("无"), QStringLiteral("Tar"), QStringLiteral("Zip")});
    defaultCompress_ = new QComboBox();
    defaultCompress_->addItems({QStringLiteral("无"), QStringLiteral("gzip"),
                                QStringLiteral("zstd"), QStringLiteral("lzma")});
    defaultCompressLevel_ = new QSpinBox();
    defaultCompressLevel_->setRange(0, 22);
    defaultCompressLevel_->setValue(3);
    defaultCompressLevel_->setToolTip(QStringLiteral("0=默认, 1=最快, 22=最佳"));
    defaultEncrypt_ = new QComboBox();
    defaultEncrypt_->addItems({QStringLiteral("无"), QStringLiteral("AES-256"),
                               QStringLiteral("SM4")});

    optForm->addRow(QStringLiteral("默认打包格式:"), defaultPack_);
    optForm->addRow(QStringLiteral("默认压缩算法:"), defaultCompress_);
    optForm->addRow(QStringLiteral("压缩级别:"), defaultCompressLevel_);
    optForm->addRow(QStringLiteral("默认加密算法:"), defaultEncrypt_);

    layout->addWidget(optGroup);

    // ── Advanced ──────────────────────────────────────────────
    auto* advGroup = new QGroupBox(QStringLiteral("高级"));
    auto* advForm = new QFormLayout(advGroup);

    logLevel_ = new QComboBox();
    logLevel_->addItems({QStringLiteral("trace"), QStringLiteral("debug"),
                          QStringLiteral("info"), QStringLiteral("warn"),
                          QStringLiteral("error")});
    logLevel_->setCurrentIndex(2); // info
    threadCount_ = new QSpinBox();
    threadCount_->setRange(1, 16);
    threadCount_->setValue(4);

    advForm->addRow(QStringLiteral("日志级别:"), logLevel_);
    advForm->addRow(QStringLiteral("并发线程数:"), threadCount_);

    layout->addWidget(advGroup);

    // ── About ─────────────────────────────────────────────────
    auto* aboutGroup = new QGroupBox(QStringLiteral("关于"));
    auto* aboutLayout = new QVBoxLayout(aboutGroup);
    auto* aboutLabel = new QLabel(
        QStringLiteral("数据备份软件 v%1\n\n"
                       "基于 C++17 / Qt 6\n\n"
                       "计算机组成与体系结构 / 软件工程 课程项目")
            .arg(QStringLiteral(BACKER_VERSION)));
    aboutLabel->setWordWrap(true);
    aboutLabel->setAlignment(Qt::AlignCenter);
    aboutLayout->addWidget(aboutLabel);
    layout->addWidget(aboutGroup);

    // ── Action buttons ────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    auto* restoreBtn = new QPushButton(QStringLiteral("恢复默认"));
    auto* saveBtn = new QPushButton(QStringLiteral("保存设置"));
    style::styleButton(saveBtn, QColor(style::kAccentGreen));
    btnLayout->addStretch();
    btnLayout->addWidget(restoreBtn);
    btnLayout->addWidget(saveBtn);
    layout->addLayout(btnLayout);
    layout->addStretch();

    connect(restoreBtn, &QPushButton::clicked,
            this, &SettingsTab::onRestoreDefaults);
    connect(saveBtn, &QPushButton::clicked,
            this, &SettingsTab::onSave);
}

void SettingsTab::loadSettings()
{
    QSettings settings(QStringLiteral("backer"), QStringLiteral("backer-gui"));
    settings.beginGroup(QStringLiteral("defaults"));

    defaultSource_->setText(
        settings.value(QStringLiteral("sourcePath")).toString());
    defaultDest_->setText(
        settings.value(QStringLiteral("destPath")).toString());

    auto pack = settings.value(QStringLiteral("packFormat"), QStringLiteral("无")).toString();
    defaultPack_->setCurrentText(pack);
    auto comp = settings.value(QStringLiteral("compressAlgo"), QStringLiteral("无")).toString();
    defaultCompress_->setCurrentText(comp);
    defaultCompressLevel_->setValue(
        settings.value(QStringLiteral("compressLevel"), 3).toInt());
    auto enc = settings.value(QStringLiteral("encryptAlgo"), QStringLiteral("无")).toString();
    defaultEncrypt_->setCurrentText(enc);

    settings.endGroup();

    settings.beginGroup(QStringLiteral("advanced"));
    auto log = settings.value(QStringLiteral("logLevel"), QStringLiteral("info")).toString();
    logLevel_->setCurrentText(log);
    threadCount_->setValue(
        settings.value(QStringLiteral("threadCount"), 4).toInt());
    settings.endGroup();
}

void SettingsTab::saveSettings()
{
    QSettings settings(QStringLiteral("backer"), QStringLiteral("backer-gui"));
    settings.beginGroup(QStringLiteral("defaults"));
    settings.setValue(QStringLiteral("sourcePath"), defaultSource_->text());
    settings.setValue(QStringLiteral("destPath"), defaultDest_->text());
    settings.setValue(QStringLiteral("packFormat"), defaultPack_->currentText());
    settings.setValue(QStringLiteral("compressAlgo"), defaultCompress_->currentText());
    settings.setValue(QStringLiteral("compressLevel"), defaultCompressLevel_->value());
    settings.setValue(QStringLiteral("encryptAlgo"), defaultEncrypt_->currentText());
    settings.endGroup();

    settings.beginGroup(QStringLiteral("advanced"));
    settings.setValue(QStringLiteral("logLevel"), logLevel_->currentText());
    settings.setValue(QStringLiteral("threadCount"), threadCount_->value());
    settings.endGroup();

    settings.sync();
}

void SettingsTab::onBrowseDefaultSource()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择默认源目录"), defaultSource_->text());
    if (!dir.isEmpty())
        defaultSource_->setText(dir);
}

void SettingsTab::onBrowseDefaultDest()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择默认目标目录"), defaultDest_->text());
    if (!dir.isEmpty())
        defaultDest_->setText(dir);
}

void SettingsTab::onSave()
{
    saveSettings();
    QMessageBox::information(this, QStringLiteral("设置"),
        QStringLiteral("设置已保存"));
}

void SettingsTab::onRestoreDefaults()
{
    defaultSource_->clear();
    defaultDest_->clear();
    defaultPack_->setCurrentIndex(0);
    defaultCompress_->setCurrentIndex(0);
    defaultCompressLevel_->setValue(3);
    defaultEncrypt_->setCurrentIndex(0);
    logLevel_->setCurrentIndex(2);
    threadCount_->setValue(4);
}

} // namespace backer::gui

#include "gui/filter_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace backer::gui {

FilterDialog::FilterDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("筛选条件编辑器"));
    setMinimumSize(560, 600);
    setupUi();
}

void FilterDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // ── Path filters ─────────────────────────────────────────
    auto* pathGroup = new QGroupBox(QStringLiteral("路径筛选"));
    auto* pathLayout = new QVBoxLayout(pathGroup);

    // Include paths
    auto* includeLabel = new QLabel(QStringLiteral("包含路径:"));
    auto* includeRow = new QHBoxLayout();
    includePathList_ = new QListWidget();
    auto* includeAddBtn = new QPushButton(QStringLiteral("+"));
    auto* includeRemoveBtn = new QPushButton(QStringLiteral("-"));
    includeAddBtn->setFixedWidth(32);
    includeRemoveBtn->setFixedWidth(32);
    includeRow->addWidget(includePathList_, 1);
    auto* includeBtnLayout = new QVBoxLayout();
    includeBtnLayout->addWidget(includeAddBtn);
    includeBtnLayout->addWidget(includeRemoveBtn);
    includeBtnLayout->addStretch();
    includeRow->addLayout(includeBtnLayout);
    pathLayout->addWidget(includeLabel);
    pathLayout->addLayout(includeRow);

    // Exclude paths
    auto* excludeLabel = new QLabel(QStringLiteral("排除路径:"));
    auto* excludeRow = new QHBoxLayout();
    excludePathList_ = new QListWidget();
    auto* excludeAddBtn = new QPushButton(QStringLiteral("+"));
    auto* excludeRemoveBtn = new QPushButton(QStringLiteral("-"));
    excludeAddBtn->setFixedWidth(32);
    excludeRemoveBtn->setFixedWidth(32);
    excludeRow->addWidget(excludePathList_, 1);
    auto* excludeBtnLayout = new QVBoxLayout();
    excludeBtnLayout->addWidget(excludeAddBtn);
    excludeBtnLayout->addWidget(excludeRemoveBtn);
    excludeBtnLayout->addStretch();
    excludeRow->addLayout(excludeBtnLayout);
    pathLayout->addWidget(excludeLabel);
    pathLayout->addLayout(excludeRow);

    layout->addWidget(pathGroup);

    connect(includeAddBtn, &QPushButton::clicked,
            this, &FilterDialog::onAddIncludePath);
    connect(includeRemoveBtn, &QPushButton::clicked,
            this, &FilterDialog::onRemoveIncludePath);
    connect(excludeAddBtn, &QPushButton::clicked,
            this, &FilterDialog::onAddExcludePath);
    connect(excludeRemoveBtn, &QPushButton::clicked,
            this, &FilterDialog::onRemoveExcludePath);

    // ── Name filters ──────────────────────────────────────────
    auto* nameGroup = new QGroupBox(QStringLiteral("名称筛选"));
    auto* nameLayout = new QVBoxLayout(nameGroup);

    // Include names
    auto* includeNameLabel = new QLabel(QStringLiteral("包含名称:"));
    auto* includeNameRow = new QHBoxLayout();
    includeNameList_ = new QListWidget();
    auto* includeNameAddBtn = new QPushButton(QStringLiteral("+"));
    auto* includeNameRemoveBtn = new QPushButton(QStringLiteral("-"));
    includeNameAddBtn->setFixedWidth(32);
    includeNameRemoveBtn->setFixedWidth(32);
    includeNameRow->addWidget(includeNameList_, 1);
    auto* includeNameBtnLayout = new QVBoxLayout();
    includeNameBtnLayout->addWidget(includeNameAddBtn);
    includeNameBtnLayout->addWidget(includeNameRemoveBtn);
    includeNameBtnLayout->addStretch();
    includeNameRow->addLayout(includeNameBtnLayout);
    nameLayout->addWidget(includeNameLabel);
    nameLayout->addLayout(includeNameRow);

    // Exclude names
    auto* excludeNameLabel = new QLabel(QStringLiteral("排除名称:"));
    auto* excludeNameRow = new QHBoxLayout();
    excludeNameList_ = new QListWidget();
    auto* excludeNameAddBtn = new QPushButton(QStringLiteral("+"));
    auto* excludeNameRemoveBtn = new QPushButton(QStringLiteral("-"));
    excludeNameAddBtn->setFixedWidth(32);
    excludeNameRemoveBtn->setFixedWidth(32);
    excludeNameRow->addWidget(excludeNameList_, 1);
    auto* excludeNameBtnLayout = new QVBoxLayout();
    excludeNameBtnLayout->addWidget(excludeNameAddBtn);
    excludeNameBtnLayout->addWidget(excludeNameRemoveBtn);
    excludeNameBtnLayout->addStretch();
    excludeNameRow->addLayout(excludeNameBtnLayout);
    nameLayout->addWidget(excludeNameLabel);
    nameLayout->addLayout(excludeNameRow);

    layout->addWidget(nameGroup);

    connect(includeNameAddBtn, &QPushButton::clicked,
            this, &FilterDialog::onAddIncludeName);
    connect(includeNameRemoveBtn, &QPushButton::clicked,
            this, &FilterDialog::onRemoveIncludeName);
    connect(excludeNameAddBtn, &QPushButton::clicked,
            this, &FilterDialog::onAddExcludeName);
    connect(excludeNameRemoveBtn, &QPushButton::clicked,
            this, &FilterDialog::onRemoveExcludeName);

    // ── Type filters ─────────────────────────────────────────
    auto* typeGroup = new QGroupBox(QStringLiteral("类型筛选"));
    auto* typeLayout = new QHBoxLayout(typeGroup);

    typeFile_    = new QCheckBox(QStringLiteral("文件"));
    typeDir_     = new QCheckBox(QStringLiteral("目录"));
    typeSymlink_ = new QCheckBox(QStringLiteral("符号链接"));
    typeFifo_    = new QCheckBox(QStringLiteral("管道"));
    typeBlock_   = new QCheckBox(QStringLiteral("块设备"));
    typeChar_    = new QCheckBox(QStringLiteral("字符设备"));
    typeSocket_  = new QCheckBox(QStringLiteral("Socket"));

    typeFile_->setChecked(true);
    typeDir_->setChecked(true);

    typeLayout->addWidget(typeFile_);
    typeLayout->addWidget(typeDir_);
    typeLayout->addWidget(typeSymlink_);
    typeLayout->addWidget(typeFifo_);
    typeLayout->addWidget(typeBlock_);
    typeLayout->addWidget(typeChar_);
    typeLayout->addWidget(typeSocket_);
    typeLayout->addStretch();
    layout->addWidget(typeGroup);

    // ── Time / Size ──────────────────────────────────────────
    auto* tsGroup = new QGroupBox(QStringLiteral("时间 / 大小"));
    auto* tsForm = new QFormLayout(tsGroup);

    enableTimeFilter_ = new QCheckBox(QStringLiteral("启用时间筛选"));
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

    tsForm->addRow(enableTimeFilter_);
    tsForm->addRow(QStringLiteral("修改时间 从:"), mtimeAfter_);
    tsForm->addRow(QStringLiteral("修改时间 到:"), mtimeBefore_);

    enableSizeFilter_ = new QCheckBox(QStringLiteral("启用大小筛选"));
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

    auto* sizeRow = new QHBoxLayout();
    sizeRow->addWidget(new QLabel(QStringLiteral("最小:")));
    sizeRow->addWidget(sizeMin_);
    sizeRow->addWidget(sizeUnitMin_);
    sizeRow->addSpacing(12);
    sizeRow->addWidget(new QLabel(QStringLiteral("最大:")));
    sizeRow->addWidget(sizeMax_);
    sizeRow->addWidget(sizeUnitMax_);
    sizeRow->addStretch();
    tsForm->addRow(enableSizeFilter_);
    tsForm->addRow(QStringLiteral("文件大小:"), sizeRow);

    layout->addWidget(tsGroup);

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

    // ── Owner ────────────────────────────────────────────────
    auto* ownerGroup = new QGroupBox(QStringLiteral("所有者"));
    auto* ownerLayout = new QHBoxLayout(ownerGroup);
    owner_ = new QLineEdit();
    owner_->setPlaceholderText(QStringLiteral("用户名或 UID"));
    ownerLayout->addWidget(owner_);
    layout->addWidget(ownerGroup);

    // ── Buttons ──────────────────────────────────────────────
    buttonBox_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox_);

    connect(buttonBox_, &QDialogButtonBox::accepted, this, [this]() {
        // Basic validation is optional; accept whatever the user set
        accept();
    });
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ── Setters ──────────────────────────────────────────────────────

void FilterDialog::setIncludePaths(QStringList const& paths)
{
    includePathList_->clear();
    for (auto const& p : paths)
        includePathList_->addItem(p);
}

void FilterDialog::setExcludePaths(QStringList const& paths)
{
    excludePathList_->clear();
    for (auto const& p : paths)
        excludePathList_->addItem(p);
}

void FilterDialog::setIncludeNames(QStringList const& names)
{
    includeNameList_->clear();
    for (auto const& n : names)
        includeNameList_->addItem(n);
}

void FilterDialog::setExcludeNames(QStringList const& names)
{
    excludeNameList_->clear();
    for (auto const& n : names)
        excludeNameList_->addItem(n);
}

void FilterDialog::setIncludeTypes(QStringList const& types)
{
    typeFile_->setChecked(types.contains(QStringLiteral("file")));
    typeDir_->setChecked(types.contains(QStringLiteral("dir")));
    typeSymlink_->setChecked(types.contains(QStringLiteral("symlink")));
    typeFifo_->setChecked(types.contains(QStringLiteral("fifo")));
    typeBlock_->setChecked(types.contains(QStringLiteral("block")));
    typeChar_->setChecked(types.contains(QStringLiteral("char")));
    typeSocket_->setChecked(types.contains(QStringLiteral("socket")));
}

void FilterDialog::setTimeFilter(bool enabled, QDateTime const& after, QDateTime const& before)
{
    enableTimeFilter_->setChecked(enabled);
    mtimeAfter_->setDateTime(after);
    mtimeBefore_->setDateTime(before);
}

void FilterDialog::setSizeFilter(bool enabled, qint64 minBytes, qint64 maxBytes)
{
    enableSizeFilter_->setChecked(enabled);
    // Convert bytes back to best-fit unit
    auto guessUnit = [](qint64 bytes) {
        if (bytes == 0) return 0;
        for (int i = 3; i > 0; --i) {
            qint64 mult = unitMultiplier(i);
            if (bytes >= mult && bytes % mult == 0) return i;
        }
        return 0;
    };
    int ui = guessUnit(minBytes > 0 ? minBytes : maxBytes);
    qint64 mult = unitMultiplier(ui);
    sizeMin_->setValue(minBytes > 0 ? static_cast<int>(minBytes / mult) : 0);
    sizeUnitMin_->setCurrentIndex(minBytes > 0 ? ui : 0);
    sizeMax_->setValue(maxBytes > 0 ? static_cast<int>(maxBytes / mult) : 0);
    sizeUnitMax_->setCurrentIndex(maxBytes > 0 ? ui : 0);
}

void FilterDialog::setOwner(QString const& owner)
{
    owner_->setText(owner);
}

// ── Getters ──────────────────────────────────────────────────────

QStringList FilterDialog::includePaths() const
{
    QStringList result;
    for (int i = 0; i < includePathList_->count(); ++i)
        result << includePathList_->item(i)->text();
    return result;
}

QStringList FilterDialog::excludePaths() const
{
    QStringList result;
    for (int i = 0; i < excludePathList_->count(); ++i)
        result << excludePathList_->item(i)->text();
    return result;
}

QStringList FilterDialog::includeNames() const
{
    QStringList result;
    for (int i = 0; i < includeNameList_->count(); ++i)
        result << includeNameList_->item(i)->text();
    return result;
}

QStringList FilterDialog::excludeNames() const
{
    QStringList result;
    for (int i = 0; i < excludeNameList_->count(); ++i)
        result << excludeNameList_->item(i)->text();
    return result;
}

QStringList FilterDialog::includeTypes() const
{
    QStringList result;
    if (typeFile_->isChecked())    result << QStringLiteral("file");
    if (typeDir_->isChecked())     result << QStringLiteral("dir");
    if (typeSymlink_->isChecked()) result << QStringLiteral("symlink");
    if (typeFifo_->isChecked())    result << QStringLiteral("fifo");
    if (typeBlock_->isChecked())   result << QStringLiteral("block");
    if (typeChar_->isChecked())    result << QStringLiteral("char");
    if (typeSocket_->isChecked())  result << QStringLiteral("socket");
    return result;
}

QStringList FilterDialog::excludeTypes() const
{
    // Not exposed as individual exclude checkboxes; return empty.
    return {};
}

bool FilterDialog::hasTimeFilter() const
{
    return enableTimeFilter_->isChecked();
}

QDateTime FilterDialog::mtimeAfter() const
{
    return mtimeAfter_->dateTime();
}

QDateTime FilterDialog::mtimeBefore() const
{
    return mtimeBefore_->dateTime();
}

bool FilterDialog::hasSizeFilter() const
{
    return enableSizeFilter_->isChecked();
}

qint64 FilterDialog::unitMultiplier(int index) noexcept
{
    static constexpr qint64 kUnits[] = {1LL, 1024LL, 1024LL * 1024, 1024LL * 1024 * 1024};
    if (index >= 0 && index < 4) return kUnits[index];
    return 1;
}

qint64 FilterDialog::sizeMin() const
{
    return sizeMin_->value() * unitMultiplier(sizeUnitMin_->currentIndex());
}

qint64 FilterDialog::sizeMax() const
{
    if (sizeMax_->value() == 0) return 0; // unlimited
    return sizeMax_->value() * unitMultiplier(sizeUnitMax_->currentIndex());
}

QString FilterDialog::owner() const
{
    return owner_->text().trimmed();
}

// ── Slots ────────────────────────────────────────────────────

void FilterDialog::onAddIncludePath()
{
    bool ok = false;
    QString text = QInputDialog::getText(this,
        QStringLiteral("添加包含路径"),
        QStringLiteral("路径模式 (支持 glob):"),
        QLineEdit::Normal, {}, &ok);
    if (ok && !text.isEmpty())
        includePathList_->addItem(text);
}

void FilterDialog::onRemoveIncludePath()
{
    auto items = includePathList_->selectedItems();
    for (auto* item : items)
        delete includePathList_->takeItem(includePathList_->row(item));
}

void FilterDialog::onAddExcludePath()
{
    bool ok = false;
    QString text = QInputDialog::getText(this,
        QStringLiteral("添加排除路径"),
        QStringLiteral("路径模式 (支持 glob):"),
        QLineEdit::Normal, {}, &ok);
    if (ok && !text.isEmpty())
        excludePathList_->addItem(text);
}

void FilterDialog::onRemoveExcludePath()
{
    auto items = excludePathList_->selectedItems();
    for (auto* item : items)
        delete excludePathList_->takeItem(excludePathList_->row(item));
}

void FilterDialog::onAddIncludeName()
{
    bool ok = false;
    QString text = QInputDialog::getText(this,
        QStringLiteral("添加包含名称"),
        QStringLiteral("文件名模式 (支持 glob, 如 *.txt):"),
        QLineEdit::Normal, {}, &ok);
    if (ok && !text.isEmpty())
        includeNameList_->addItem(text);
}

void FilterDialog::onRemoveIncludeName()
{
    auto items = includeNameList_->selectedItems();
    for (auto* item : items)
        delete includeNameList_->takeItem(includeNameList_->row(item));
}

void FilterDialog::onAddExcludeName()
{
    bool ok = false;
    QString text = QInputDialog::getText(this,
        QStringLiteral("添加排除名称"),
        QStringLiteral("文件名模式 (支持 glob, 如 *.tmp):"),
        QLineEdit::Normal, {}, &ok);
    if (ok && !text.isEmpty())
        excludeNameList_->addItem(text);
}

void FilterDialog::onRemoveExcludeName()
{
    auto items = excludeNameList_->selectedItems();
    for (auto* item : items)
        delete excludeNameList_->takeItem(excludeNameList_->row(item));
}

} // namespace backer::gui

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
    setMinimumSize(500, 450);
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

void FilterDialog::setIncludeTypes(QStringList const& types)
{
    // Uncheck all first
    typeFile_->setChecked(false);
    typeDir_->setChecked(false);
    typeSymlink_->setChecked(false);
    typeFifo_->setChecked(false);
    typeBlock_->setChecked(false);
    typeChar_->setChecked(false);
    typeSocket_->setChecked(false);

    for (auto const& t : types) {
        auto const key = t.trimmed().toLower();
        if (key == QStringLiteral("file")    || key == QStringLiteral("regular") || key == QStringLiteral("f"))
            typeFile_->setChecked(true);
        else if (key == QStringLiteral("dir") || key == QStringLiteral("directory") || key == QStringLiteral("d"))
            typeDir_->setChecked(true);
        else if (key == QStringLiteral("symlink") || key == QStringLiteral("link") || key == QStringLiteral("l"))
            typeSymlink_->setChecked(true);
        else if (key == QStringLiteral("fifo") || key == QStringLiteral("pipe") || key == QStringLiteral("p"))
            typeFifo_->setChecked(true);
        else if (key == QStringLiteral("block") || key == QStringLiteral("blockdev") || key == QStringLiteral("b"))
            typeBlock_->setChecked(true);
        else if (key == QStringLiteral("char") || key == QStringLiteral("chardev") || key == QStringLiteral("c"))
            typeChar_->setChecked(true);
        else if (key == QStringLiteral("socket") || key == QStringLiteral("sock") || key == QStringLiteral("s"))
            typeSocket_->setChecked(true);
    }
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

} // namespace backer::gui

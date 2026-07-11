#include "gui/schedule_tab.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace backer::gui {

// ═══════════════════════════════════════════════════════════════════════
// ScheduleEditDialog — inline dialog for adding/editing a scheduled task
// ═══════════════════════════════════════════════════════════════════════

class ScheduleEditDialog : public QDialog {
public:
    explicit ScheduleEditDialog(QWidget* parent = nullptr,
                                QString const& name = {},
                                QString const& cron = {},
                                QString const& source = {},
                                QString const& dest = {})
        : QDialog(parent)
    {
        setWindowTitle(name.isEmpty()
            ? QStringLiteral("添加定时任务")
            : QStringLiteral("编辑定时任务"));
        setMinimumWidth(450);

        auto* form = new QFormLayout(this);

        nameEdit_ = new QLineEdit(name);
        nameEdit_->setPlaceholderText(QStringLiteral("任务名称"));
        cronEdit_ = new QLineEdit(cron);
        cronEdit_->setPlaceholderText(QStringLiteral("0 2 * * *  (每天凌晨2点)"));
        sourceEdit_ = new QLineEdit(source);
        sourceEdit_->setPlaceholderText(QStringLiteral("源目录路径"));
        destEdit_ = new QLineEdit(dest);
        destEdit_->setPlaceholderText(QStringLiteral("目标目录路径"));

        auto* browseSrcBtn = new QPushButton(QStringLiteral("浏览"));
        auto* browseDestBtn = new QPushButton(QStringLiteral("浏览"));
        auto* srcRow = new QHBoxLayout();
        srcRow->addWidget(sourceEdit_, 1);
        srcRow->addWidget(browseSrcBtn);
        auto* destRow = new QHBoxLayout();
        destRow->addWidget(destEdit_, 1);
        destRow->addWidget(browseDestBtn);

        form->addRow(QStringLiteral("名称:"), nameEdit_);
        form->addRow(QStringLiteral("Cron 表达式:"), cronEdit_);
        form->addRow(QStringLiteral("源目录:"), srcRow);
        form->addRow(QStringLiteral("目标目录:"), destRow);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(buttons);

        connect(browseSrcBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this,
                QStringLiteral("选择源目录"), sourceEdit_->text());
            if (!dir.isEmpty()) sourceEdit_->setText(dir);
        });
        connect(browseDestBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this,
                QStringLiteral("选择目标目录"), destEdit_->text());
            if (!dir.isEmpty()) destEdit_->setText(dir);
        });
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (nameEdit_->text().isEmpty() || cronEdit_->text().isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("提示"),
                    QStringLiteral("名称和 Cron 表达式不能为空"));
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    QString taskName()  const { return nameEdit_->text(); }
    QString cronExpr()  const { return cronEdit_->text(); }
    QString sourceDir() const { return sourceEdit_->text(); }
    QString destDir()   const { return destEdit_->text(); }

private:
    QLineEdit* nameEdit_{nullptr};
    QLineEdit* cronEdit_{nullptr};
    QLineEdit* sourceEdit_{nullptr};
    QLineEdit* destEdit_{nullptr};
};

// ═══════════════════════════════════════════════════════════════════════
// ScheduleTab
// ═══════════════════════════════════════════════════════════════════════

ScheduleTab::ScheduleTab(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ScheduleTab::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // ── Stacked widget: empty state ↔ task table ────────────
    stack_ = new QStackedWidget(this);

    // Empty state page
    emptyPage_ = new QWidget();
    auto* emptyLayout = new QVBoxLayout(emptyPage_);
    emptyLayout->setAlignment(Qt::AlignCenter);

    QPixmap emptyPix(QStringLiteral(":/icons/empty-state"));
    auto* emptyIcon = new QLabel();
    emptyIcon->setPixmap(emptyPix.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    emptyIcon->setAlignment(Qt::AlignCenter);

    auto* emptyTitle = new QLabel(QStringLiteral("暂无定时任务"));
    emptyTitle->setStyleSheet(QStringLiteral("color: #86868B; font-size: 16px; font-weight: 500; margin-top: 12px;"));
    emptyTitle->setAlignment(Qt::AlignCenter);

    auto* emptyHint = new QLabel(QStringLiteral("点击下方「添加」按钮创建第一个定时备份任务"));
    emptyHint->setStyleSheet(QStringLiteral("color: #C7C7CC; font-size: 13px; margin-top: 4px;"));
    emptyHint->setAlignment(Qt::AlignCenter);

    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyHint);
    stack_->addWidget(emptyPage_);

    // Task table page
    auto* tablePage = new QWidget();
    auto* tableLayout = new QVBoxLayout(tablePage);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    taskTable_ = new QTableWidget(0, 4, this);
    taskTable_->setHorizontalHeaderLabels({
        QStringLiteral("任务名称"),
        QStringLiteral("Cron 表达式"),
        QStringLiteral("源目录"),
        QStringLiteral("目标目录")
    });
    taskTable_->horizontalHeader()->setStretchLastSection(true);
    taskTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    taskTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    taskTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    taskTable_->verticalHeader()->setVisible(false);
    tableLayout->addWidget(taskTable_, 1);

    stack_->addWidget(tablePage);
    layout->addWidget(stack_, 1);

    // ── Buttons ───────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    addBtn_ = new QPushButton(QStringLiteral("添加"));
    editBtn_ = new QPushButton(QStringLiteral("编辑"));
    deleteBtn_ = new QPushButton(QStringLiteral("删除"));
    runNowBtn_ = new QPushButton(QStringLiteral("立即执行"));
    btnLayout->addWidget(addBtn_);
    btnLayout->addWidget(editBtn_);
    btnLayout->addWidget(deleteBtn_);
    btnLayout->addWidget(runNowBtn_);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(addBtn_, &QPushButton::clicked, this, &ScheduleTab::onAddTask);
    connect(editBtn_, &QPushButton::clicked, this, &ScheduleTab::onEditTask);
    connect(deleteBtn_, &QPushButton::clicked, this, &ScheduleTab::onDeleteTask);
    connect(runNowBtn_, &QPushButton::clicked, this, &ScheduleTab::onRunNow);

    updateView();
}

void ScheduleTab::updateView()
{
    bool hasTasks = taskTable_->rowCount() > 0;
    stack_->setCurrentIndex(hasTasks ? 1 : 0);
    editBtn_->setEnabled(hasTasks);
    deleteBtn_->setEnabled(hasTasks);
    runNowBtn_->setEnabled(hasTasks);
}

void ScheduleTab::onAddTask()
{
    ScheduleEditDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        int row = taskTable_->rowCount();
        taskTable_->insertRow(row);
        taskTable_->setItem(row, 0, new QTableWidgetItem(dlg.taskName()));
        taskTable_->setItem(row, 1, new QTableWidgetItem(dlg.cronExpr()));
        taskTable_->setItem(row, 2, new QTableWidgetItem(dlg.sourceDir()));
        taskTable_->setItem(row, 3, new QTableWidgetItem(dlg.destDir()));
        updateView();
    }
}

void ScheduleTab::onEditTask()
{
    int row = taskTable_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请先选择一个任务"));
        return;
    }

    auto name   = taskTable_->item(row, 0)->text();
    auto cron   = taskTable_->item(row, 1)->text();
    auto source = taskTable_->item(row, 2)->text();
    auto dest   = taskTable_->item(row, 3)->text();

    ScheduleEditDialog dlg(this, name, cron, source, dest);
    if (dlg.exec() == QDialog::Accepted) {
        taskTable_->item(row, 0)->setText(dlg.taskName());
        taskTable_->item(row, 1)->setText(dlg.cronExpr());
        taskTable_->item(row, 2)->setText(dlg.sourceDir());
        taskTable_->item(row, 3)->setText(dlg.destDir());
    }
}

void ScheduleTab::onDeleteTask()
{
    int row = taskTable_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请先选择一个任务"));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("确认"),
            QStringLiteral("确定要删除选中的定时任务吗？"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        taskTable_->removeRow(row);
        updateView();
    }
}

void ScheduleTab::onRunNow()
{
    int row = taskTable_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请先选择一个任务"));
        return;
    }

    QMessageBox::information(this, QStringLiteral("提示"),
        QStringLiteral("功能待实现：立即执行定时任务"));
}

} // namespace backer::gui

#include "gui/main_window.h"
#include "gui/backup_tab.h"     // backupTab_ type
#include "gui/restore_tab.h"    // restoreTab_ type
#include "gui/schedule_tab.h"   // scheduleTab_ type
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>

namespace backer::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("数据备份软件 v%1")
                       .arg(QStringLiteral(BACKER_VERSION)));
    setMinimumSize(800, 600);
    resize(1000, 700);

    setupUi();
    setupMenuBar();
    setupStatusBar();
}

void MainWindow::setupUi()
{
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setObjectName(QStringLiteral("mainTabWidget"));

    backupTab_   = new BackupTab();
    restoreTab_  = new RestoreTab();
    scheduleTab_ = new ScheduleTab();

    setWindowIcon(QIcon(QStringLiteral(":/icons/app")));

    tabWidget_->addTab(backupTab_,   QIcon(QStringLiteral(":/icons/backup")),   QStringLiteral("备份"));
    tabWidget_->addTab(restoreTab_,  QIcon(QStringLiteral(":/icons/restore")),  QStringLiteral("还原"));
    tabWidget_->addTab(scheduleTab_, QIcon(QStringLiteral(":/icons/schedule")), QStringLiteral("定时任务"));

    setCentralWidget(tabWidget_);

    connect(backupTab_, &BackupTab::backupFinished,
            this, &MainWindow::onBackupFinished);
    connect(restoreTab_, &RestoreTab::restoreFinished,
            this, [this](bool success, QString const& message) {
        if (success) {
            QMessageBox::information(this,
                QStringLiteral("还原完成"), message);
            tabWidget_->setCurrentWidget(backupTab_);
        } else {
            QMessageBox::critical(this,
                QStringLiteral("还原失败"), message);
            // Stay on restore tab for retry
        }
    });
}

void MainWindow::setupMenuBar()
{
    // ── File menu ───────────────────────────────────────────────────
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("文件"));

    QAction* quitAction = fileMenu->addAction(
        QIcon::fromTheme(QStringLiteral("application-exit")),
        QStringLiteral("退出"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    connect(quitAction, &QAction::triggered,
            qApp, &QApplication::quit);

    // ── Help menu ───────────────────────────────────────────────────
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));

    QAction* aboutAction = helpMenu->addAction(
        QIcon::fromTheme(QStringLiteral("help-about")),
        QStringLiteral("关于"));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this,
            QStringLiteral("关于"),
            QStringLiteral(
                "数据备份软件 v%1\n\n"
                "计算机组成与体系结构 / "
                "软件工程 课程项目\n\n"
                "支持目录树备份与还原、"
                "压缩加密、定时/实时/网络"
                "备份等功能。")
                .arg(QStringLiteral(BACKER_VERSION)));
    });
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::onBackupFinished(bool success, QString message)
{
    if (success) {
        QMessageBox::information(this,
            QStringLiteral("备份完成"),
            message);
    } else {
        QMessageBox::critical(this,
            QStringLiteral("备份失败"),
            message);
    }

    tabWidget_->setCurrentWidget(backupTab_);
}

} // namespace backer::gui

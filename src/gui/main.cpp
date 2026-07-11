// SPDX-License-Identifier: Apache-2.0
//
// GUI 入口 — QApplication 初始化 + MainWindow 启动

#include "gui/gui_style.h"
#include "gui/main_window.h"

#include <QApplication>
#include <QFont>
#include <QScreen>
#include <QSplashScreen>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // ── 应用元信息 ──────────────────────────────────────────────────
    app.setApplicationName(QStringLiteral("数据备份软件"));
    app.setApplicationVersion(QStringLiteral(BACKER_VERSION));
    app.setOrganizationName(QStringLiteral("backer"));
    app.setOrganizationDomain(QStringLiteral("backer.app"));

    // ── 启动画面 ────────────────────────────────────────────────────
    QSplashScreen splash(QPixmap(QStringLiteral(":/icons/splash")));
    splash.setFont(QFont(QStringLiteral("-apple-system, PingFang SC, sans-serif"), 12));
    splash.show();
    app.processEvents();

    splash.showMessage(QStringLiteral("正在加载应用..."),
                       Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    // ── 全局 macOS 简约风格 ─────────────────────────────────────────
    backer::gui::style::applyGlobal(app);
    splash.showMessage(QStringLiteral("正在初始化界面..."),
                       Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    // ── 主窗口 ──────────────────────────────────────────────────────
    backer::gui::MainWindow mainWindow;

    // 居中显示
    auto screen = app.primaryScreen();
    if (screen) {
        auto screenGeom = screen->availableGeometry();
        mainWindow.move(
            (screenGeom.width() - mainWindow.width()) / 2,
            (screenGeom.height() - mainWindow.height()) / 2
        );
    }

    // 启动画面 → 主窗口平滑过渡
    splash.showMessage(QStringLiteral("准备就绪"),
                       Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    mainWindow.show();
    splash.finish(&mainWindow);

    return app.exec();
}

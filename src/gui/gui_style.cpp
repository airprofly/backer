#include "gui/gui_style.h"

#include <QApplication>
#include <QPalette>
#include <QPushButton>

namespace backer::gui::style {

void applyGlobal(QApplication& app)
{
    // QPalette
    QPalette pal;
    pal.setColor(QPalette::Window,        QColor(kBgWindow));
    pal.setColor(QPalette::WindowText,    QColor(kTextPrimary));
    pal.setColor(QPalette::Base,          QColor(kBgCard));
    pal.setColor(QPalette::Text,          QColor(kTextPrimary));
    pal.setColor(QPalette::Button,        QColor(kBgCard));
    pal.setColor(QPalette::ButtonText,    QColor(kTextPrimary));
    pal.setColor(QPalette::Highlight,     QColor(kAccentBlue));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Link,          QColor(kAccentBlue));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(kTextPlaceholder));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(kTextPlaceholder));
    pal.setColor(QPalette::PlaceholderText, QColor(kTextPlaceholder));
    app.setPalette(pal);

    // 全局 QSS
    app.setStyleSheet(globalStylesheet());
}

QString globalStylesheet()
{
    return QStringLiteral(R"(
        /* ── 全局 ────────────────────────────────── */
        QWidget {
            background-color: %1;
            color: %2;
            font-family: -apple-system, "PingFang SC", "Segoe UI",
                         "Helvetica Neue", "Noto Sans CJK SC",
                         "Microsoft YaHei", sans-serif;
            font-size: 13px;
        }

        /* ── 标签 ────────────────────────────────── */
        QLabel {
            background: transparent;
            color: %2;
            border: none;
            padding: 0;
        }

        /* ── 输入框 ──────────────────────────────── */
        QLineEdit, QSpinBox, QComboBox, QDateTimeEdit {
            background-color: %3;
            border: 1px solid %10;
            border-radius: %4px;
            padding: 7px 10px;
            color: %2;
            font-size: 13px;
            selection-background-color: %6;
            selection-color: white;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QDateTimeEdit:focus {
            border: 1px solid %6;
        }
        QLineEdit:disabled, QSpinBox:disabled {
            background-color: %1;
            color: %11;
        }
        QComboBox::drop-down {
            border: none;
            background: transparent;
            width: 24px;
        }
        QComboBox::down-arrow {
            width: 8px;
            height: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: %3;
            border: 1px solid %10;
            border-radius: %4px;
            padding: 4px;
            selection-background-color: %6;
            selection-color: white;
            outline: none;
        }

        /* ── 进度条 ──────────────────────────────── */
        QProgressBar {
            background-color: %5;
            border: none;
            border-radius: 2px;
            height: 4px;
            text-align: center;
            font-size: 0px;
            color: transparent;
        }
        QProgressBar::chunk {
            background-color: %6;
            border-radius: 2px;
        }

        /* ── 表格 ────────────────────────────────── */
        QTableWidget {
            background-color: transparent;
            border: none;
            gridline-color: %5;
            selection-background-color: %5;
            selection-color: %2;
            outline: none;
        }
        QTableWidget::item {
            padding: 6px 10px;
            border: none;
            border-bottom: 1px solid %5;
        }
        QTableWidget::item:selected {
            background-color: %5;
            color: %2;
        }
        QHeaderView::section {
            background-color: %1;
            border: none;
            border-bottom: 1px solid %10;
            padding: 8px 10px;
            font-weight: 500;
            color: %7;
        }

        /* ── 列表 ────────────────────────────────── */
        QListWidget {
            background-color: %3;
            border: 1px solid %10;
            border-radius: %4px;
            padding: 4px;
            outline: none;
        }
        QListWidget::item {
            border-radius: %4px;
            padding: 6px 10px;
        }
        QListWidget::item:selected {
            background-color: %6;
            color: white;
        }

        /* ── 分组框 ──────────────────────────────── */
        QGroupBox {
            background: transparent;
            border: none;
            margin-top: 0;
            padding: 0;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 0 8px 0;
            color: %7;
            font-size: 12px;
            font-weight: 600;
            text-transform: uppercase;
        }

        /* ── 滚动条 ──────────────────────────────── */
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: %10;
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #B8B8BE;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }

        QScrollBar:horizontal {
            background: transparent;
            height: 6px;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background: %10;
            border-radius: 3px;
            min-width: 30px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        /* ── Tab 控件 — macOS Segmented Control ──── */
        QTabWidget::pane {
            background: transparent;
            border: none;
        }
        QTabBar {
            background: transparent;
        }
        QTabBar::tab {
            background: transparent;
            border: none;
            border-radius: 0;
            padding: 8px 18px;
            margin: 0;
            color: %7;
            font-size: 13px;
        }
        QTabBar::tab:selected {
            color: %2;
            font-weight: 600;
            border-bottom: 2px solid %6;
        }
        QTabBar::tab:hover:!selected {
            background: transparent;
            color: %2;
        }

        /* ── 复选框/单选 ──────────────────────────── */
        QCheckBox, QRadioButton {
            background: transparent;
            spacing: 6px;
            color: %2;
            font-size: 13px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 1px solid %10;
            background: %3;
        }
        QCheckBox::indicator:checked {
            background: %6;
            border-color: %6;
        }
        QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 8px;
            border: 1px solid %10;
            background: %3;
        }
        QRadioButton::indicator:checked {
            background: %6;
            border-color: %6;
        }

        /* ── 弹窗 ────────────────────────────────── */
        QMessageBox {
            background-color: %1;
        }
        QMessageBox QLabel {
            color: %2;
            font-size: 13px;
        }

        /* ── 工具提示 ────────────────────────────── */
        QToolTip {
            background-color: %1;
            color: %2;
            border: 1px solid %10;
            border-radius: %4px;
            padding: 6px 10px;
            font-size: 12px;
        }
    )")
    .arg(kBgWindow,           // %1 - 窗口背景
         kTextPrimary,        // %2 - 主文字色
         kBgCard,             // %3 - 卡片/输入框背景
         QString::number(kRadiusSmall),  // %4 - 小圆角
         kBgSidebar,          // %5 - 侧栏/分割线色
         kAccentBlue,         // %6 - 强调色
         kTextSecondary,      // %7 - 次要文字
         kAccentGreen,        // %8 - 绿色
         kAccentRed,          // %9 - 红色
         kBorderLight,        // %10 - 边框
         kTextPlaceholder);   // %11 - 占位文字
}

void styleButton(QPushButton* btn, QColor const& accent, bool flat)
{
    if (flat) {
        // ── 文字按钮 ────────────────────────────────
        QColor c = accent.isValid() ? accent : QColor(kAccentBlue);
        QString qss = QStringLiteral(
            "QPushButton {"
            "  background: transparent;"
            "  border: none;"
            "  color: %1;"
            "  font-size: 13px;"
            "  padding: 4px 8px;"
            "  border-radius: %2px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %3;"
            "}"
            "QPushButton:pressed {"
            "  color: %4;"
            "}")
            .arg(c.name())
            .arg(kRadiusSmall)
            .arg(kBgHover)
            .arg(c.darker(120).name());
        btn->setStyleSheet(qss);
        return;
    }

    if (accent.isValid()) {
        // ── 强调按钮 ────────────────────────────────
        // macOS 风格：填充色 + 白色文字 + 圆角
        QColor hoverColor = accent.lighter(110);
        QColor pressedColor = accent.darker(110);
        QString qss = QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  border: none;"
            "  border-radius: %2px;"
            "  padding: 7px 20px;"
            "  color: white;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "  background-color: %3;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %4;"
            "}"
            "QPushButton:disabled {"
            "  background-color: %5;"
            "  color: white;"
            "}")
            .arg(accent.name())
            .arg(kRadiusSmall)
            .arg(hoverColor.name())
            .arg(pressedColor.name())
            .arg(accent.lighter(140).name());
        btn->setStyleSheet(qss);
    } else {
        // ── 次要按钮 ────────────────────────────────
        // macOS 风格：白底 + 灰边框
        QString qss = QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: %3px;"
            "  padding: 6px 20px;"
            "  color: %4;"
            "  font-size: 13px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %5;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %2;"
            "}"
            "QPushButton:disabled {"
            "  background-color: %1;"
            "  border-color: %6;"
            "  color: %6;"
            "}")
            .arg(kBgCard)
            .arg(kBorderLight)
            .arg(kRadiusSmall)
            .arg(kTextPrimary)
            .arg(kBgHover)
            .arg(kTextPlaceholder);
        btn->setStyleSheet(qss);
    }
}

} // namespace backer::gui::style

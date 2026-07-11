#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QPushButton;

namespace backer::gui::style {

// ═══════════════════════════════════════════════════════════════════════
// macOS 简约设计系统 — 颜色常量
// ═══════════════════════════════════════════════════════════════════════

// 背景色
inline constexpr auto kBgWindow    = "#F5F5F7";  // 窗口背景
inline constexpr auto kBgCard      = "#FFFFFF";  // 卡片/面板背景
inline constexpr auto kBgSidebar   = "#E8E8ED";  // 侧栏/分割线
inline constexpr auto kBgHover     = "#E8E8ED";  // 悬浮高亮

// 强调色 — macOS 标准
inline constexpr auto kAccentBlue  = "#007AFF";
inline constexpr auto kAccentGreen = "#34C759";
inline constexpr auto kAccentOrange = "#FF9500";
inline constexpr auto kAccentRed   = "#FF3B30";

// 文字色
inline constexpr auto kTextPrimary   = "#1D1D1F";
inline constexpr auto kTextSecondary = "#86868B";
inline constexpr auto kTextPlaceholder = "#C7C7CC";

// 边框色
inline constexpr auto kBorderLight  = "#D2D2D7";
inline constexpr auto kBorderFocus  = "#007AFF";

// 窗口按钮色
inline constexpr auto kBtnRed   = "#FF5F57";
inline constexpr auto kBtnGreen = "#28C840";
inline constexpr auto kBtnOrange = "#FEBD2E";

// 圆角
inline constexpr int kRadiusSmall  = 6;
inline constexpr int kRadiusMedium = 8;
inline constexpr int kRadiusLarge  = 12;

// 间距（8px 栅格）
inline constexpr int kSpacingUnit = 8;

// ═══════════════════════════════════════════════════════════════════════
// 样式应用函数
// ═══════════════════════════════════════════════════════════════════════

/// 对整个 QApplication 应用全局 macOS 简约样式
void applyGlobal(QApplication& app);

/// 生成全局 QSS 样式表
QString globalStylesheet();

/// 对 QPushButton 应用 macOS 风格
/// @param accent  强调色（默认透明 = 次要按钮；绿色/蓝色 = 强调按钮）
/// @param flat    是否文字按钮（true = 无边框文字按钮）
void styleButton(QPushButton* btn, QColor const& accent = {},
                 bool flat = false);

} // namespace backer::gui::style

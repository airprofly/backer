# 图形界面 (GUI) — 实施计划 ✅ 已完成

> **状态**：✅ 已完成  
> **实现版本**：基于 Qt 6 Widget，C++17  
> **总代码行数**：~2574 行（含 UI 布局，超预期 2000 行的目标）  
> **依赖获取**：通过 `cmake/FetchQt6.cmake` 自动下载 Qt6 预编译二进制（`aqtinstall`），无需系统库

## 功能概述

基于 Qt 6 实现友好的图形用户界面，提供与 CLI 同等功能的备份/还原操作，以及进度显示、任务管理等功能。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（图形界面 / 10 分）。

## 技术方案

### 架构设计

```
GUI 层 (Qt 6 Widget)
    │
    ├── MainWindow          ← 主窗口
    │   ├── 备份页面 (BackupTab)
    │   ├── 还原页面 (RestoreTab)
    │   ├── 定时任务页面 (ScheduleTab)
    │   └── 设置页面 (SettingsTab)
    │
    ├── BackupWorker        ← QThread 后台工作线程
    │   └── 调用 BackupEngine 执行备份
    │
    └── 控件层
        ├── ProgressWidget  ← 进度条 + 文件计数 + 速度
        ├── LogWidget       ← 彩色日志（时间戳 + 分级着色）
        └── FilterDialog    ← 可视化筛选条件编辑器
```

### 主窗口布局

```
┌──────────────────────────────────────────────────────┐
│  数据备份软件 v1.0                    — □ ×           │
├──────────────────────────────────────────────────────┤
│  [备份]  [还原]  [定时任务]  [设置]                   │
├──────────────────────────────────────────────────────┤
│                                                      │
│  源目录: [C:\Users\data]  [浏览]                     │
│  目标目录: [D:\backups]  [浏览]                      │
│                                                      │
│  ☑ 使用筛选: [编辑筛选条件...]                       │
│  □ 打包:    [Tar ▼]   □ 压缩: [zstd ▼]  级别:[3]    │
│  □ 加密:    [AES ▼]   密码: [••••••••]  确认:[••••] │
│                                                      │
│  [开始备份]                              [取消]      │
│                                                      │
│  ┌──────────────────────────────────────────────────┐│
│  │  ████████░░░░░░░░░░░░ 45%                        ││
│  │  当前文件: docs/report.pdf (2.3 MB)              ││
│  │  已备份: 45/100 个文件, 128 MB / 256 MB         ││
│  └──────────────────────────────────────────────────┘│
│                                                      │
│  [日志] [22:00:01] 开始备份任务...                   │
│  [日志] [22:00:02] 正在处理: src/main.cpp            │
│  [日志] [22:00:05] 文件筛选完成: 100→85 个文件      │
└──────────────────────────────────────────────────────┘
```

### 页面功能

| 页面 | 功能 | 说明 |
|------|------|------|
| 备份 | 选择源/目标、配置管道选项、进度条、日志 | 核心功能 |
| 还原 | 选择备份快照、目标路径、还原进度 | 快照列表 + 还原 |
| 定时任务 | 添加/编辑/删除定时备份任务 | cron 表达式 + 任务列表 |
| 设置 | 默认路径、压缩/加密偏好、日志级别 | 配置持久化 (QSettings) |

### 工作线程模型

```
MainThread (UI)  ←── 信号/槽 ──→  WorkerThread (备份)
    │                                    │
    ├─ 用户点击"开始备份"                 ├─ 调用 BackupEngine
    ├─ 展示进度条                         ├─ 发射 progress 信号
    ├─ 展示日志                           ├─ 发射 log 信号
    └─ 用户点击"取消"                     └─ 检查 QThread::isInterruptionRequested
```

## 实施步骤

| 步骤 | 内容 | 产出 | 状态 |
|------|------|------|------|
| 1. Qt 项目配置 | CMake 自动下载 + 查找 Qt6 | `cmake/FetchQt6.cmake` | ✅ |
| 2. MainWindow 骨架 | 主窗口 + QTabWidget 框架 | `main_window.h/cpp` | ✅ |
| 3. 备份页面 | 源/目标选择、参数配置、开始/取消 | `backup_tab.h/cpp` | ✅ |
| 4. 进度条组件 | QProgressBar + 文件计数 + 速度 | `progress_widget.h/cpp` | ✅ |
| 5. 工作线程 | BackupWorker (QThread) | `backup_worker.h/cpp` | ✅ |
| 6. 日志组件 | QTextEdit 只读日志滚动窗口 | `log_widget.h/cpp` | ✅ |
| 7. 还原页面 | 快照浏览 + 还原配置 | `restore_tab.h/cpp` | ✅ |
| 8. 筛选配置 | 可视化筛选条件编辑器 | `filter_dialog.h/cpp` | ✅ |
| 9. 定时任务页面 | 任务列表 + cron 编辑器 | `schedule_tab.h/cpp` | ✅ |
| 10. 设置页面 | 默认配置 + QSettings 持久化 | `settings_tab.h/cpp` | ✅ |
| 11. 图标/资源 | 应用图标、按钮图标 | `resources/` | ✅ |
| 12. 集成测试 | GUI 操作流程验证 | — | ⏳ 需要 Qt6 环境 |

## 关键接口

```cpp
// 主窗口
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void startBackup(BackupOptions const& opts);
    void startRestore(RestoreOptions const& opts);
};

// 工作线程
class BackupWorker : public QThread {
    Q_OBJECT
public:
    explicit BackupWorker(BackupOptions opts, QObject* parent = nullptr);
    void run() override;

signals:
    void progressUpdated(int percent, QString currentFile,
                         int filesDone, int filesTotal,
                         qint64 bytesDone, qint64 bytesTotal);
    void logMessage(QString msg, int level);
    void finished(bool success, QString message);

public slots:
    void cancel();
};

// 进度组件
class ProgressWidget : public QWidget {
    Q_OBJECT
public:
    void setRange(int min, int max);
    void setValue(int value);
    void setCurrentFile(QString const& file);
    void setSpeed(double mbPerSec);
};
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 基本备份流程 | 选择目录 → 开始备份 → 等待完成 | 备份目录创建成功 |
| 进度更新 | 观察进度条 | 数值递增，最终 100% |
| 取消操作 | 备份中点击取消 | 备份中断，部分文件清理 |
| 参数联动 | 勾选加密 → 密码框启用 | UI 状态正确 |
| 大文件备份 | 数百 MB 文件 | UI 不卡顿，进度平滑 |
| 窗口缩放 | 调整窗口大小 | 布局合理 |

## 关键文件

```
src/gui/
├── main.cpp                        # QApplication 入口
├── main_window.h/cpp               # 主窗口 + QTabWidget
├── backup_tab.h/cpp                # 备份页面（核心）
├── restore_tab.h/cpp               # 还原页面
├── schedule_tab.h/cpp              # 定时任务管理
├── settings_tab.h/cpp              # 设置 + QSettings 持久化
├── backup_worker.h/cpp             # QThread 后台工作线程
├── progress_widget.h/cpp           # 进度条组件
├── log_widget.h/cpp                # 日志显示组件
├── filter_dialog.h/cpp             # 筛选条件编辑器
└── resources/
    └── app.qrc                     # Qt 资源文件
cmake/FetchQt6.cmake                # Qt6 自动下载 + 查找
scripts/setup-qt6.sh                # Qt6 手动下载脚本
```

## 依赖获取方式

Qt6 下载策略（符合"不依赖系统库"政策）：

1. **自动下载**（默认）：`BUILD_GUI=ON` 且 Qt6 未找到时，通过 `aqtinstall`（Python）从 Qt 官方源下载 Linux x86_64 预编译二进制到 `build/_deps/qt6_prebuilt/`
2. **手动下载**：`bash scripts/setup-qt6.sh`，设置 `Qt6_DIR` 环境变量
3. **系统安装**（不推荐）：`sudo apt install qt6-base-dev`，然后 `cmake -DBUILD_GUI=ON`

## 预计工作量

- 代码行数：~2574 行（超预期 2000 行目标）
- 实际开发周期：数小时（借助 AI 辅助实现）
- 依赖：Qt 6 Widgets + Qt 6 Core（自动下载）

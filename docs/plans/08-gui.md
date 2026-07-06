# 图形界面 (GUI) — 实施计划

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
    └── 模型层
        ├── FileListModel   ← 文件列表展示
        └── BackupJobModel  ← 备份任务列表
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

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. Qt 项目配置 | CMake find_package(Qt6 Widgets) | `CMakeLists.txt` |
| 2. MainWindow 骨架 | 主窗口 + QTabWidget 框架 | `main_window.h/cpp` |
| 3. 备份页面 | 源/目标选择、参数配置、开始/取消 | `backup_tab.h/cpp` |
| 4. 进度条组件 | QProgressBar + 文件计数 + 速度 | `progress_widget.h/cpp` |
| 5. 工作线程 | BackupWorker (QThread) | `backup_worker.h/cpp` |
| 6. 日志组件 | QTextEdit 只读日志滚动窗口 | `log_widget.h/cpp` |
| 7. 还原页面 | 快照浏览 + 还原配置 | `restore_tab.h/cpp` |
| 8. 筛选配置 | 可视化筛选条件编辑器 | `filter_dialog.h/cpp` |
| 9. 定时任务页面 | 任务列表 + cron 编辑器 | `schedule_tab.h/cpp` |
| 10. 设置页面 | 默认配置 + QSettings 持久化 | `settings_tab.h/cpp` |
| 11. 图标/资源 | 应用图标、按钮图标 | `resources/` |
| 12. 集成测试 | GUI 操作流程验证 | 测试 |

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
    void backupFinished(bool success, QString message);

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
├── main_window.h
├── main_window.cpp
├── backup_tab.h
├── backup_tab.cpp
├── restore_tab.h
├── restore_tab.cpp
├── schedule_tab.h
├── schedule_tab.cpp
├── settings_tab.h
├── settings_tab.cpp
├── backup_worker.h
├── backup_worker.cpp
├── progress_widget.h
├── progress_widget.cpp
├── log_widget.h
├── log_widget.cpp
├── filter_dialog.h
├── filter_dialog.cpp
└── resources/
    ├── icons/
    └── app.qrc                   # Qt 资源文件
CMakeLists.txt                     # find_package(Qt6)
```

## 依赖安装

```bash
# Ubuntu / Debian
sudo apt install qt6-base-dev qt6-tools-dev

# Fedora / RHEL
sudo dnf install qt6-qtbase-devel

# Arch Linux
sudo pacman -S qt6-base
```

## 预计工作量

- 代码行数：~2000 行（含 UI 布局）
- 开发周期：1.5-2 周（单人）
- 依赖：Qt 6 Widgets + Qt 6 Core

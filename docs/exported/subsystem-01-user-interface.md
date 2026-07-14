# 子系统一：用户接口子系统构件图描述

## 对应源文件

`subsystem-01-user-interface.svg`

## 图概述

本图展示了用户接口子系统的内部结构，包含 CLI 命令行模块和 GUI 图形界面模块两个子模块，以及它们之间的共享调用关系。

## 构件说明

### CLI 命令行模块

| 构件 | 类型 | 说明 |
|------|------|------|
| CLI::App（CLI11 库） | 外部库 | 第三方命令行解析库 CLI11 v2.4.2，header-only 引入。负责解析 4 个子命令（backup/restore/schedule/daemon）及 20+ 选项参数 |
| handleBackup() | 自由函数 | 备份命令的核心调度函数。流程：解析 BackupOptions → buildFilter → buildPacker → 创建 BackupEngine → 执行备份 → 后压缩 → 后加密 → 返回 exit code |
| handleRestore() | 自由函数 | 恢复命令的核心调度函数。流程：解析 RestoreOptions → 预解密 → 预解压（写入临时文件）→ buildPacker → 创建 RestoreEngine → 执行恢复 → 清理临时文件 |
| handleSchedule() | 自由函数 | 计划管理命令函数。调度子命令 list/add/remove/enable/disable，通过 ScheduleStore 读写持久化配置 |
| handleDaemon() | 自由函数 | 守护进程命令函数。加载 ScheduleStore 中的任务 → 构造 BackupScheduler → 设置 lambda 回调（调用 handleBackup）→ 配置 RetentionPolicy → scheduler.run() 进入阻塞循环 |
| BackupOptions | 结构体 | 备份配置的数据载体。包含 preserveMetadata / handleSpecial / 过滤参数（includePaths 等 6 维度）/ packFormat / compressAlgo+Level / encryptAlgo+password 等字段 |
| RestoreOptions | 结构体 | 恢复配置的数据载体。包含 preserveMetadata / handleSpecial / packFormat / decompressAlgo / decryptAlgo+password 等字段 |
| buildFilter() | 工厂函数 | 根据 BackupOptions 中的过滤参数构建 CriteriaFilter 或 NoopFilter，返回 unique_ptr<Filter> |
| buildPacker() | 工厂函数 | 根据格式名字符串构建 TarPacker 或 ZipPacker，返回 unique_ptr<Packer> |
| transformFile() | 工具函数 | 对单个文件执行 compress 或 decompress（one-shot 模式：读取→压缩/解压→写入），调用 Compressor 接口 |
| encryptFile() | 工具函数 | 对单个文件执行 one-shot 加密，调用 Encryptor::encrypt() |
| decryptFile() | 工具函数 | 对单个文件执行 one-shot 解密，调用 Encryptor::decrypt() |
| promptPassword() | 工具函数 | POSIX 终端回显关闭密码输入（termios），非 POSIX 回退到标准输入 |

### GUI 图形界面模块

| 构件 | 类型 | 说明 |
|------|------|------|
| QApplication + QSplashScreen | Qt 框架 | Qt6 应用入口，显示启动闪屏，调用 gui_style::applyGlobal() 应用全局样式 |
| MainWindow | QMainWindow | 主窗口类，承载 QTabWidget 四页签，包含文件菜单（Quit Ctrl+Q）和帮助菜单（About）以及状态栏 |
| QTabWidget | Qt 控件 | 页签容器，容纳 BackupTab / RestoreTab / ScheduleTab / SettingsTab 四个页签 |
| BackupTab | QWidget | 备份页签，配置源/目标路径、打包/压缩/加密选项、过滤条件、进度/日志组件。核心方法 `collectOptions()` 收集 UI 状态到 BackupOptions |
| RestoreTab | QWidget | 恢复页签，配置源/目标路径、解压/解密选项、元数据/特殊文件开关、进度/日志组件 |
| ScheduleTab | QWidget | 计划页签，使用 QStackedWidget 在空状态插图与任务表格间切换。4 列表格（名称/Cron/源/目标），支持添加/编辑/删除/立即运行操作 |
| SettingsTab | QWidget | 设置页签，基于 QSettings("backer", "backer-gui") 实现配置持久化。包含默认路径/格式/算法/级别/日志等级/线程数等设置项 |
| BackupWorker | QThread | 后台备份线程，使用 variant<BackupOptions, RestoreOptions> 存储配置。发出 progressUpdated / logMessage / finished 三个信号 |
| RestoreWorker | QThread | 后台恢复线程，类同 BackupWorker |
| FilterDialog | QDialog | 过滤条件可视化编辑器，包含路径过滤/类型过滤（7 种复选框）/时间过滤/大小过滤/属主过滤 5 个标签页 |
| ScheduleEditDialog | QDialog 内部类 | 计划编辑对话框，编辑任务名称/cron 表达式/源路径/目标路径 |
| ProgressWidget | QWidget | 进度组件，显示 QProgressBar + 当前文件标签 + 统计标签（文件数/字节数）+ 速度标签 |
| LogWidget | QWidget | 日志组件，只读 QTextEdit（HTML 模式），等宽字体，颜色编码（info 黑色 / warn 橙色 / error 红色），10000 行上限自动裁剪 |
| gui_style | 样式命名空间 | macOS 风格设计常量（颜色/间距/圆角），提供 applyGlobal()（设置 QPalette + QSS）和 styleButton()（主/次/扁平按钮）|
| resources/app.qrc | Qt 资源文件 | 图标资源集合，包含 app / backup / restore / schedule / settings / empty-state / splash 共 7 个图标 |

## 模块间关系

1. CLI 与 GUI 在执行层面**共享** handleBackup() / handleRestore() / handleSchedule() 等函数——GUI 的 BackupWorker 在后台线程中直接调用这些 CLI 函数
2. GUI 模块不依赖 CLI 的参数解析（CLI::App），只使用 CLI 模块中的核心调度函数（handleBackup 等）
3. 两个模块均不直接依赖具体实现，通过工厂函数和接口间接使用压缩/加密/打包/过滤等模块

## 设计模式

- **MVC 变体**：BackupTab（View+Controller）→ BackupOptions（Model）→ BackupWorker（后台线程调用 handleBackup）
- **观察者模式**：BackupWorker 通过 Qt 信号槽（progressUpdated / logMessage / finished）通知 UI 更新

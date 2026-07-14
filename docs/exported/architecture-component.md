# 顶层系统构件图描述

## 对应源文件

`architecture-component.svg`

## 图概述

本图为 Backer 备份系统的顶层构件图，展示了系统 8 个子系统的整体划分以及各子系统之间的数据流与调用关系。

## 构件说明

### 用户接口（UI Layer）

| 构件 | 类型 | 说明 |
|------|------|------|
| CLI 命令行 | 可执行构件 | 基于 CLI11 库的命令行接口，提供 backup / restore / schedule / daemon 四个子命令，支持 20+ 参数选项 |
| GUI 图形界面 | 可执行构件 | 基于 Qt6 Widget 的桌面图形界面，包含备份/恢复/计划/设置四个页签，通过 MainWindow + QTabWidget 组织 |

### 核心引擎（Core Engine）

| 构件 | 类型 | 说明 |
|------|------|------|
| BackupEngine | 核心类 | 备份引擎，实现 `backup(source, destination, filter, packer)` 方法。内部流程：遍历源目录 → 过滤 → 逐文件读取写入（或打包）→ 恢复元数据 → 返回 BackupResult |
| RestoreEngine | 核心类 | 恢复引擎，实现 `restore(source, destination, packer)` 方法。支持目录模式（逐文件恢复）和归档模式（Packer::unpack 解包）两种恢复路径 |

### 功能模块（Pipeline Stages）

| 构件 | 类型 | 说明 |
|------|------|------|
| Compressor 压缩模块 | 策略接口 + 实现族 | 定义 Compressor 抽象接口，提供 Gzip（zlib）、Zstd（libzstd）、Lzma（liblzma）三种压缩算法实现，通过 buildCompressor() 工厂创建 |
| Encryptor 加密模块 | 策略接口 + 实现族 | 定义 Encryptor 抽象接口，基于 OpenSSL EVP 实现 AES-256-GCM 和 SM4-CBC 两种加密算法，通过 buildEncryptor() 工厂创建。包含 PBKDF2 密钥派生（SHA-256/SM3） |
| Packer 打包模块 | 策略接口 + 实现族 | 定义 Packer 抽象接口，实现 TarPacker（自实现 POSIX ustar 格式）和 ZipPacker（基于 miniz 库）两种打包格式。ZipPacker 通过 .backer_zip_meta 特殊条目存储文件元数据 |
| Filter 过滤模块 | 策略接口 + 实现类 | 定义 Filter 抽象接口，提供 CriteriaFilter（支持路径/类型/名称/时间/大小/属主六大维度过滤）和 NoopFilter（空过滤器，原样返回） |

### 调度系统（Scheduler）

| 构件 | 类型 | 说明 |
|------|------|------|
| BackupScheduler 调度器 | 核心类 | 线程安全的备份调度器，使用 mutex + condition_variable 实现跨平台定时触发。通过 ccronexpr 库解析 cron 表达式，支持 5/6 字段标准 cron 语法 |
| RetentionPolicy 保留策略 | 核心类 | 快照保留策略管理，基于快照目录名（YYYYMMDD_HHMMSS 格式）实现时间排序，支持按最大数量（maxSnapshots）和保留天数（retentionDays）两种维度清理过期快照 |
| ScheduleStore 计划存储 | 持久化类 | 调度计划的 JSON 持久化存储，默认路径 `~/.config/backer/schedule.json`，使用自定义轻量 JSON 解析器（无外部依赖），支持 CRUD 操作 |

### 基础设施（Infrastructure）

| 构件 | 类型 | 说明 |
|------|------|------|
| FSAbstraction 文件系统抽象 | 抽象接口 | 定义 walk/read/write/mkdir/readMetadata/restoreMetadata 六个文件系统操作接口。LocalFsAbstraction 基于 std::filesystem + POSIX 系统调用实现，通过 platform.h 宏处理跨平台差异 |
| Metadata 元数据 | 工具模块 | 文件元数据的读取（lstat）与恢复（lchown/fchmodat/utimensat），支持 JSON 序列化/反序列化，含 root 权限检测 |

## 数据流关系

1. **备份流**：CLI/GUI → BackupEngine → Filter → Compressor → Encryptor → Packer → 文件存储
2. **恢复流**：CLI/GUI → RestoreEngine → Encryptor → Compressor → Packer → 文件存储（读取）
3. **调度流**：BackupScheduler → BackupEngine（触发备份）→ RetentionPolicy（清理快照）→ ScheduleStore（持久化）

## 设计模式

- **分层架构**：用户接口层 → 核心引擎层 → 功能模块层 → 基础设施层
- **策略模式**：Compressor、Encryptor、Packer、Filter 均采用接口 + 多实现 + 工厂的设计模式，新增算法只需新增实现类并注册工厂
- **依赖注入**：BackupEngine/RestoreEngine 通过构造函数接收 FSAbstraction（unique_ptr），通过方法参数接收 Filter/Packer

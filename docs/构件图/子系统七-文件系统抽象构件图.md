# 子系统七：文件系统抽象子系统构件图描述

## 对应源文件

`subsystem-07-fs-abstraction.svg`

## 图概述

本图展示了文件系统抽象子系统的内部结构，包含 FSAbstraction 抽象接口、LocalFsAbstraction 本地实现、Metadata 元数据模块、SpecialFile 特殊文件处理模块、PathMapper 路径映射工具，以及 platform.h 跨平台适配层。

## 构件说明

### 接口层

| 构件 | 类型 | 说明 |
|------|------|------|
| FSAbstraction | 抽象类（接口） | 文件系统操作统一抽象接口，定义 6 个纯虚方法。所有平台文件系统操作通过此接口接入，隔离上层代码与平台差异 |

接口方法规范：

| 方法 | 签名 | 说明 |
|------|------|------|
| walk() | `path const& root → Expected<vector<FileEntry>, ErrorCode>` | 递归遍历根目录，检测每个条目的文件类型（lstat/symlink_status），返回完整的 FileEntry 列表 |
| read() | `path const& → Expected<vector<char>, ErrorCode>` | 读取整个文件内容到字节数组，预分配 file_size 大小的缓冲区 |
| write() | `path, span<char const> data, FileType → Expected<void, ErrorCode>` | 按文件类型分派写入：普通文件→写内容、符号链接→createSymlink、FIFO→createFifo、设备→createDevice |
| mkdir() | `path const& → Expected<void, ErrorCode>` | 递归创建目录树（create_directories） |
| readMetadata() | `path const& → Expected<Metadata, ErrorCode>` | 读取单个文件的元数据（lstat + 字段提取） |
| restoreMetadata() | `path, Metadata, bool restoreOwnership → Expected<void, ErrorCode>` | 按顺序恢复元数据：lchown（可选）→ fchmodat（跳过符号链接）→ utimensat（含 AT_SYMLINK_NOFOLLOW） |

### 平台检测（platform.h）

| 构件 | 类型 | 说明 |
|------|------|------|
| 平台宏定义 | 编译期宏 | 提供三个维度的平台抽象：① BACKER_PLATFORM_POSIX / BACKER_PLATFORM_WINDOWS（1/0 二元宏，控制 #if 分支）② BACKER_STAT_ATIME / MTIME / CTIME 宏屏蔽 macOS st_atimespec 与 Linux st_atim 结构体成员名差异 ③ 所有 POSIX 专用代码使用 `#if BACKER_PLATFORM_POSIX` 保护，确保 MSVC 编译时不触及 POSIX 符号 |

### LocalFsAbstraction（本地文件系统实现）

| 构件 | 类型 | 说明 |
|------|------|------|
| LocalFsAbstraction | 实现类 | FSAbstraction 的本地文件系统实现。基于 std::filesystem + POSIX 系统调用 + platform.h 条件编译 |
| walk() | 实现 | 使用 std::filesystem::recursive_directory_iterator（含 skip_permission_denied 选项跳过无权限目录）。POSIX：每次迭代调用 lstat() 获取精确类型（不跟随符号链接）；非 POSIX：使用 symlink_status()。通过 makeFileEntry() 提取 UID/GID/权限/纳秒时间戳/设备号 |
| read() | 实现 | std::ifstream 二进制模式读取，file_size 预分配缓冲区 |
| write() | 实现 | 按 FileType 分派：kRegular → std::ofstream 64 KiB 分块写入；kSymlink → createSymlink()；kFifo → createFifo()；kBlockDevice/kCharDevice → createDevice() |
| mkdir() | 实现 | std::filesystem::create_directories 递归创建 |
| makeFileEntry() | 内部辅助 | 从 lstat/symlink_status 结果构建 FileEntry。POSIX：提取 st_uid、st_gid、st_mode、st_atim/mtim/ctim、st_rdev。非 POSIX：UID/GID 固定 0，设备号固定 0 |

### Metadata 模块

| 构件 | 类型 | 说明 |
|------|------|------|
| readMetadata() | 自由函数 | POSIX：lstat() + 提取 st_uid/gid/mode/atim/mtim/ctim 字段到 Metadata 结构体。非 POSIX：symlink_status() + last_write_time() |
| restoreMetadata() | 自由函数 | 三步恢复顺序：① lchown（可选，需 root，仅 POSIX）② fchmodat（跳过符号链接，仅 POSIX）③ utimensat（含 AT_SYMLINK_NOFOLLOW 标志，支持纳秒精度） |
| metadataToJson() | 自由函数 | 将 Metadata 结构体序列化为 JSON 字符串 |
| metadataFromJson() | 自由函数 | 从 JSON 字符串反序列化为 Metadata 结构体 |
| canRestoreOwnership() | 自由函数 | 检查当前进程是否有 root 权限（POSIX：geteuid()==0；非 POSIX：始终返回 false）|

### SpecialFile 模块

| 构件 | 类型 | 说明 |
|------|------|------|
| detectFileType() | 自由函数 | POSIX：lstat() + S_ISREG/DIR/LNK/FIFO/BLK/CHR/SOCK 宏检测文件类型。非 POSIX：symlink_status() 只能区分普通文件/目录/符号链接/其他 |
| readSymlink() | 自由函数 | POSIX：readlink() 分配 PATH_MAX 缓冲区，循环读取直到内容适合。非 POSIX：read_symlink() |
| createSymlink() | 自由函数 | POSIX：symlink()。非 POSIX：create_symlink() |
| createFifo() | 自由函数 | POSIX：mkfifo() 创建命名管道。非 POSIX：返回 kSpecialFileNotSupported 错误 |
| createDevice() | 自由函数 | POSIX：makedev() 组合设备号 + mknod() 创建设备节点。非 POSIX：返回 kSpecialFileNotSupported 错误 |

### PathMapper 模块

| 构件 | 类型 | 说明 |
|------|------|------|
| PathMapper | 静态工具类 | 四向路径转换工具，用于在源路径、相对路径、备份路径之间互相转换 |
| sourceToRelative() | 静态方法 | 将绝对源路径转为相对路径。例：/home/user/a/b.txt → a/b.txt |
| relativeToBackup() | 静态方法 | 将相对路径转为备份目录下的绝对路径。例：a/b.txt → /backup/a/b.txt |
| backupToRelative() | 静态方法 | 将备份路径反转为相对路径（relativeToBackup 的逆操作）|
| relativeToSource() | 静态方法 | 将相对路径映射回源根目录下的绝对路径（sourceToRelative 的逆操作）|

## 平台差异对照

| 功能 | POSIX (Linux/macOS) | Windows |
|------|-------------------|---------|
| 文件类型检测 | lstat() + S_IS* 宏，支持全部 7 种类型 | symlink_status()，仅区分 3 种 |
| 元数据读取 | 完整 UID/GID/权限/纳秒时间戳 | UID/GID=0，时间戳精度受限 |
| 元数据恢复 | lchown + fchmodat + utimensat | 受限（无属主/权限变更） |
| 符号链接 | readlink + symlink | read_symlink + create_symlink |
| 命名管道 | mkfifo | 返回错误 |
| 设备文件 | makedev + mknod | 返回错误 |
| 属主恢复 | 需 root（geteuid==0）| 不支持 |

## 设计模式

- **适配器模式**：LocalFsAbstraction 将 std::filesystem + POSIX API 适配为统一的 FSAbstraction 接口
- **门面模式**：FSAbstraction 将 walk/read/write/mkdir/readMetadata/restoreMetadata 六个原子操作封装为统一门面
- **静态工具类**：PathMapper 仅提供静态方法，无状态，作为纯函数工具使用

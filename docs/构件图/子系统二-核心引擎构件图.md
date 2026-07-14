# 子系统二：核心引擎子系统构件图描述

## 对应源文件

`subsystem-02-core-engine.svg`

## 图概述

本图展示了核心引擎子系统的内部结构，包括类型定义、错误码体系、基础工具类以及 BackupEngine 和 RestoreEngine 两个核心调度类及其内部流程。

## 构件说明

### 类型定义（types.h）

| 构件 | 类型 | 说明 |
|------|------|------|
| FileType | 枚举（uint8_t） | 文件类型枚举，包含 kRegular（普通文件）、kDirectory（目录）、kSymlink（符号链接）、kFifo（命名管道）、kBlockDevice（块设备）、kCharDevice（字符设备）、kSocket（套接字）、kUnknown（未知）共 8 种类型 |
| Metadata | 结构体 | 文件元数据结构体，包含 ownerId（uint32）、groupId（uint32）、permissions（uint32）、accessTimeSec+Nsec（int64）、modifyTimeSec+Nsec（int64）、changeTimeSec+Nsec（int64）|
| FileEntry | 结构体 | 文件条目结构体，包含 relativePath（string）、type（FileType）、size（uint64）、metadata（Metadata）、symlinkTarget（string）、deviceMajor+Minor（uint32）|
| OperationStats | 结构体 | 操作统计，包含 totalFiles、totalDirs、totalBytes、skipped（均为 uint64）、elapsed（milliseconds）|
| BackupResult | 结构体 | 备份/恢复结果，包含 success（bool）、errorCode（ErrorCode）、errorMessage（string）、stats（OperationStats）|

### 错误码（error_code.h）

| 构件 | 类型 | 说明 |
|------|------|------|
| ErrorCode | 枚举（uint32_t） | 统一错误码枚举，按类别分层编码：通用错误 0x00xx（kOk=0, kUnknown, kNotImplemented）、文件系统错误 0x01xx（kPathNotFound, kPermissionDenied, kDiskFull, kFileTooLarge, kSymlinkLoop, kMetadataRestoreFailed, kSpecialFileNotSupported, kNotRoot）、管线错误 0x02xx（kCompressionFailed, kEncryptionFailed, kInvalidArchive）、存储错误 0x03xx（kWriteFailed, kReadFailed, kNetworkError, kAuthFailed）|
| toString() | constexpr 函数 | 编译期错误码到字符串的转换函数 |

### 基础工具

| 构件 | 类型 | 说明 |
|------|------|------|
| Expected\<T,E\> | C++23 polyfill | 期望值类型模板，通过 discriminated union（hasValue_ flag + union storage）实现。支持 has_value()、value()、error()、value_or()。特化 Expected\<void,E\>（无值存储，仅错误）|
| Span\<T\> | C++20 polyfill | 非拥有视图类型模板，轻量级连续数据引用。支持构造自指针+长度、数组、容器。提供 data()、size()、empty()、begin()、end() |

### BackupEngine（备份引擎）

| 构件 | 类型 | 说明 |
|------|------|------|
| BackupEngine | 类 | 备份引擎主类。构造函数 `explicit BackupEngine(unique_ptr<FSAbstraction> fs)` 接收文件系统抽象。核心方法 `backup(source, destination, filter, packer)` |
| backup() | 成员方法 | 备份入口方法。目录镜像模式：walk → filter → 逐文件读写的完整流程。归档模式：所有文件通过 Packer::pack() 写入归档流 |
| walkAndCollect() | 内部步骤 | 调用 FSAbstraction::walk(source) 递归遍历源目录，获取所有文件的 vector<FileEntry> |
| filterEntries() | 内部步骤 | 调用 Filter::apply(files) 按过滤条件筛选文件条目，返回筛选后的文件列表 |
| writeEntries() | 内部步骤 | 按 FileType 分派处理：kDirectory → FSAbstraction::mkdir()（推迟元数据），kRegular → read+write，kSymlink → createSymlink()，kFifo → createFifo()，kBlockDevice/kCharDevice → createDevice()，kSocket → 跳过 |
| restoreMetadata() | 内部步骤 | 先恢复非目录元数据（即时），再恢复目录元数据（按深度降序——最深的目录先恢复，最后恢复根目录元数据）|

### RestoreEngine（恢复引擎）

| 构件 | 类型 | 说明 |
|------|------|------|
| RestoreEngine | 类 | 恢复引擎主类。构造函数同 BackupEngine。核心方法 `restore(source, destination, packer)` |
| restore() | 成员方法 | 恢复入口方法。判断输入是归档文件还是目录，分别走 restoreArchive 或 restoreDirectory |
| restoreDirectory() | 内部步骤 | 目录模式恢复：walk 备份目录 → 建立目标根目录 → 按条目类型创建文件/目录/符号链接 → 按深度降序恢复目录元数据 |
| restoreArchive() | 内部步骤 | 归档模式恢复：fs->read(archive) 读取归档文件 → fs->mkdir(dest) 建立目标根目录 → Packer::unpack(archive, dest, *fs) 解包 |

## 数据流

1. **备份流**：source 路径 → walk → [FileEntry] → filter → [FileEntry] → writeEntries（或 Packer::pack）→ destination
2. **恢复流**：source 路径（目录或归档文件）→ 判断模式 → 目录模式（walk+write）或归档模式（Packer::unpack）→ destination

## 依赖关系

- BackupEngine 和 RestoreEngine 通过 FSAbstraction 接口依赖文件系统抽象（构造函数注入）
- 通过 Filter 接口依赖过滤模块（方法参数传入）
- 通过 Packer 接口依赖打包模块（方法参数传入）
- 两个引擎均不直接依赖压缩和加密模块——压缩/加密由 CLI/GUI 层在调用引擎前后通过 Compressor/Encryptor 接口独立处理

# 系统设计文档 — 数据备份软件

## 1. 总体架构

### 1.1 分层架构

系统采用五层分层架构，上层依赖下层，下层对上层无感知：

```
┌──────────────────────────────────────────────────┐
│                  表现层 (Presentation)              │
│        CLI (CLI11)          GUI (Qt 6 Widget)      │
├──────────────────────────────────────────────────┤
│                  业务层 (Business)                  │
│          BackupEngine          RestoreEngine        │
│              调度器 (Scheduler / Watcher)           │
├──────────────────────────────────────────────────┤
│                管道层 (Pipeline)                    │
│   Filter → Packer → Compressor → Encryptor        │
│   Decryptor → Decompressor → Unpacker → Restore   │
├──────────────────────────────────────────────────┤
│              基础设施层 (Infrastructure)             │
│      文件系统抽象 (FSAbstraction)    元数据管理       │
│      特殊文件支持    路径映射    备份索引 (Index)     │
├──────────────────────────────────────────────────┤
│                存储层 (Storage)                     │
│        本地文件系统       网络 (gRPC 客户端/服务端)   │
└──────────────────────────────────────────────────┘
```

### 1.2 分层职责

| 层级 | 职责 | 关键类 |
|------|------|--------|
| 表现层 | 用户交互、命令解析、参数校验 | `BackupCommand`, `RestoreCommand`, `MainWindow` |
| 业务层 | 编排备份/还原流程、调度策略 | `BackupEngine`, `RestoreEngine`, `BackupScheduler`, `FileWatcher` |
| 管道层 | 数据加工：筛选/打包/压缩/加密 | `Filter`, `Packer`, `Compressor`, `Encryptor` 及对应逆操作 |
| 基础设施层 | 文件系统操作、元数据、索引维护 | `FSAbstraction`, `MetadataManager`, `BackupIndex` |
| 存储层 | 数据持久化与传输 | `LocalStorage`, `RemoteStorage` (gRPC stub) |

### 1.3 模块依赖关系

```
                    ┌─────────────┐
                    │   CLI / GUI  │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ BackupEngine │───→ BackupIndex (元数据索引)
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
         ┌────────┐  ┌──────────┐  ┌────────┐
         │ Filter │  │ Packer   │  │ Watcher │
         └───┬────┘  └────┬─────┘  └────────┘
             │            │
             ▼            ▼
         ┌────────┐  ┌──────────┐
         │Compress│  │Encryptor │
         └───┬────┘  └────┬─────┘
             │            │
             ▼            ▼
         ┌──────────────────────┐
         │  Storage (Local/Net)  │
         └──────────────────────┘
```

---

## 2. 核心模块设计

### 2.1 备份引擎 (BackupEngine)

备份引擎是系统的核心编排器，负责统筹管道的各个阶段。

```
┌────────────────────────────────────────────────────┐
│                  BackupEngine                       │
│                                                     │
│   buildPipeline(filter, packer, compressor, enc)    │
│   backup(source, destination) → BackupResult        │
│   restore(source, destination) → RestoreResult      │
│                                                     │
│   内部流程:                                          │
│   1. FSAbstraction.walk(source) → 文件列表           │
│   2. Filter.apply(文件列表) → 筛选后列表              │
│   3. Packer.pack(文件列表) → 打包流                   │
│   4. Compressor.compress(打包流) → 压缩流            │
│   5. Encryptor.encrypt(压缩流) → 加密流              │
│   6. Storage.write(加密流) → 完成                     │
└────────────────────────────────────────────────────┘
```

**类设计**：

```cpp
class BackupEngine {
public:
    explicit BackupEngine(Storage& storage);

    // 构建备份管道，各阶段可设为 nullptr 跳过
    BackupEngine& setFilter(Filter* filter);
    BackupEngine& setPacker(Packer* packer);
    BackupEngine& setCompressor(Compressor* compressor);
    BackupEngine& setEncryptor(Encryptor* encryptor);

    // 执行备份
    [[nodiscard]] BackupResult backup(
        std::filesystem::path const& source,
        BackupOptions const& options);

    // 执行还原
    [[nodiscard]] RestoreResult restore(
        std::filesystem::path const& destination,
        RestoreOptions const& options);

private:
    Pipeline pipeline_;        // 管道组合
    Storage& storage_;         // 存储后端
    BackupIndex index_;        // 备份索引
};
```

### 2.2 管道接口 (Pipeline)

**管道阶段接口**：

```cpp
// 筛选器
class Filter {
public:
    virtual ~Filter() = default;
    virtual std::vector<FileEntry> apply(
        std::vector<FileEntry> const& files) = 0;
};

// 打包器
class Packer {
public:
    virtual ~Packer() = default;
    virtual void pack(
        std::vector<FileEntry> const& files,
        FSAbstraction& fs,
        std::ostream& output) = 0;
    virtual void unpack(
        std::istream& input,
        std::filesystem::path const& dest,
        FSAbstraction& fs) = 0;
};

// 压缩器 (缓冲区接口，内存到内存转换)
class Compressor {
public:
    virtual ~Compressor() = default;
    virtual std::expected<void, ErrorCode> compress(
        std::span<char const> input,
        std::vector<char>& output) = 0;
    virtual std::expected<void, ErrorCode> decompress(
        std::span<char const> input,
        std::vector<char>& output) = 0;
    virtual std::string_view name() const noexcept = 0;
};

// 加密器 (缓冲区接口，内存到内存转换)
class Encryptor {
public:
    virtual ~Encryptor() = default;
    virtual std::expected<void, ErrorCode> encrypt(
        std::span<char const> input,
        std::vector<char>& output,
        std::string_view password) = 0;
    virtual std::expected<void, ErrorCode> decrypt(
        std::span<char const> input,
        std::vector<char>& output,
        std::string_view password) = 0;
    virtual std::string_view name() const noexcept = 0;
};
```

### 2.3 工厂注册 (可插拔策略)

```cpp
// 压缩器工厂
class CompressorFactory {
public:
    static CompressorFactory& instance();

    // 注册压缩算法
    template <std::derived_from<Compressor> T>
    bool registerCompressor(std::string_view name);

    // 按名称创建
    std::unique_ptr<Compressor> create(std::string_view name);

private:
    std::map<std::string, std::function<std::unique_ptr<Compressor>()>, std::less<>> registry_;
};

// 使用方式：
// CompressorFactory::instance().registerCompressor<GzipCompressor>("gzip");
// auto comp = CompressorFactory::instance().create("gzip");
```

---

## 3. 文件系统抽象 (FSAbstraction)

### 3.1 设计目的

统一屏蔽常规文件与特殊文件的差异，使上层管道无需感知文件类型。

```cpp
class FSAbstraction {
public:
    virtual ~FSAbstraction() = default;

    // 遍历目录树
    virtual std::expected<std::vector<FileEntry>, ErrorCode>
        walk(std::filesystem::path const& root) = 0;

    // 读取文件内容 (常规文件 → bytes; 特殊文件 → 元数据)
    virtual std::expected<std::vector<char>, ErrorCode>
        read(std::filesystem::path const& path) = 0;

    // 写入文件 (含特殊文件的 mknod/mkfifo/symlink)
    virtual std::expected<void, ErrorCode>
        write(std::filesystem::path const& path,
              FileType type,
              std::span<char const> data) = 0;

    // 元数据操作
    virtual std::expected<Metadata, ErrorCode>
        readMetadata(std::filesystem::path const& path) = 0;
    virtual std::expected<void, ErrorCode>
        restoreMetadata(std::filesystem::path const& path,
                        Metadata const& meta) = 0;
};
```

### 3.2 文件条目结构

```cpp
struct FileEntry {
    std::filesystem::path  relativePath;   // 相对于备份根的路径
    FileType                type;           // regular / directory / symlink / fifo / block / char
    uint64_t                size;           // 文件大小
    Metadata                metadata;       // 属主/时间/权限
    std::vector<char>       content;        // 文件内容 (小文件直接加载)
    std::filesystem::path   symlinkTarget;  // 符号链接目标
};

struct Metadata {
    uid_t       ownerId;
    gid_t       groupId;
    mode_t      permissions;    // 含 setuid/setgid/sticky bit
    timespec    accessTime;     // atime
    timespec    modifyTime;     // mtime
    timespec    changeTime;     // ctime (创建时设置)
};
```

### 3.3 特殊文件处理策略

| 文件类型 | 备份方式 | 还原方式 |
|----------|----------|----------|
| 常规文件 | 直接读内容 | 直接写内容 |
| 目录 | 递归遍历 | `mkdir` + 属性设置 |
| 符号链接 | 读 `readlink()` | `symlink()` |
| 命名管道 (FIFO) | 只存元数据 | `mkfifo()` |
| 块设备 / 字符设备 | 存设备号 (major/minor) | `mknod()` |
| Socket | 存元数据 + 重建标记 | 创建空占位 |

---

## 4. 备份格式设计

### 4.1 备份包结构

不使用标准 tar/zip，而是定义紧凑的自描述二进制格式，便于叠加压缩和加密层。

```
┌─────────────────────────────────────────┐
│            Magic Header (8B)             │  "BACKER\0\0\0"
├─────────────────────────────────────────┤
│          Metadata Block                  │
│  ├─ 备份时间戳    (8B)                   │
│  ├─ 文件条目数    (8B)                   │
│  └─ 根路径偏移    (4B)                   │
├─────────────────────────────────────────┤
│          File Entry Table                │
│  ┌─────────────────────────────────────┐│
│  │ FileEntry #1                        ││
│  │  ├ path_len (2B) + path (utf-8)    ││
│  │  ├ type (1B)                        ││
│  │  ├ mode (4B)                        ││
│  │  ├ uid/gid (4B+4B)                  ││
│  │  ├ atime/mtime (8B+8B)              ││
│  │  ├ content_offset (8B)              ││
│  │  └ content_size (8B)                ││
│  │ ...                                  ││
│  └─────────────────────────────────────┘│
├─────────────────────────────────────────┤
│          File Content Data               │
│  ┌─────────────────────────────────────┐│
│  │ content #1 ... bytes                ││
│  │ content #2 ... bytes                ││
│  └─────────────────────────────────────┘│
└─────────────────────────────────────────┘
```

### 4.2 备份索引 (BackupIndex)

每次备份完成后，在主目录生成 `backup.idx`（JSON 格式），用于增量备份和查询：

```json
{
  "version": "1.0",
  "backupTime": "2026-07-06T22:00:00Z",
  "source": "/home/user/data",
  "totalFiles": 1250,
  "totalSize": 5368709120,
  "checksum": {
    "algorithm": "sha256",
    "value": "a1b2c3d4..."
  },
  "entries": [
    {
      "path": "docs/report.md",
      "type": "file",
      "size": 24576,
      "mtime": "2026-07-05T14:30:00Z",
      "checksum": "e5f6..."
    }
  ]
}
```

---

## 5. 扩展模块设计

### 5.1 自定义筛选器 (Filter)

```cpp
struct FilterCriteria {
    std::optional<std::regex>   pathPattern;     // 路径匹配 (glob/regex)
    std::optional<std::string>  fileType;        // 文件类型 (file/dir/symlink/...)
    std::optional<std::string>  namePattern;     // 文件名通配
    std::optional<TimeRange>    timeRange;       // 时间范围 (mtime)
    std::optional<SizeRange>    sizeRange;       // 尺寸范围
    std::optional<std::string>  userName;        // 用户名
    bool                        exclude = false; // true = 排除匹配项
};

class CriteriaFilter : public Filter {
public:
    explicit CriteriaFilter(std::vector<FilterCriteria> criteria);
    std::vector<FileEntry> apply(
        std::vector<FileEntry> const& files) override;
private:
    std::vector<FilterCriteria> criteria_;
};
```

### 5.2 定时备份 (Scheduler)

```
┌────────────────────────────────────────────────────┐
│                  BackupScheduler                     │
│                                                     │
│  schedule(cron_expr, backup_task) → job_id           │
│  cancel(job_id)                                      │
│  listJobs() → 所有定时任务列表                        │
│                                                     │
│  内部: timerfd + epoll 事件循环                       │
│  解析 cron 表达式 (ccronexpr) 计算下次触发时间          │
│  到期自动调用 BackupEngine::backup()                  │
└────────────────────────────────────────────────────┘
```

### 5.3 实时备份 (FileWatcher)

```
┌────────────────────────────────────────────────────┐
│                  FileWatcher                        │
│                                                     │
│  watch(path) → 开始监控目录                          │
│  unwatch(path) → 停止监控                            │
│  setDebounce(ms) → 去抖窗口 (默认 1s)                │
│                                                     │
│  内部: inotify + event queue + worker thread         │
│  事件 → 去重/去抖 → 触发备份                          │
└────────────────────────────────────────────────────┘
```

**事件处理流程**：

```
inotify FD
    │
    ▼
┌─────────────┐   去抖窗口 (1s)   ┌──────────────┐
│ Event Queue  │ ──────────────→  │ Debouncer     │
│ (IN_CREATE)  │                 │ (合并批量事件) │
│ (IN_MODIFY)  │                 └──────┬───────┘
│ (IN_DELETE)  │                        │
└─────────────┘                         ▼
                                ┌──────────────┐
                                │ BackupWorker  │
                                │ (后台线程)     │
                                │ 增量备份/全量  │
                                └──────────────┘
```

### 5.4 网络备份 (Network / gRPC)

**Proto 定义**：

```protobuf
service BackupService {
    rpc Backup(stream BackupRequest) returns (BackupResponse);
    rpc Restore(RestoreRequest) returns (stream RestoreResponse);
    rpc ListSnapshots(ListRequest) returns (ListResponse);
}

message BackupRequest {
    oneof data {
        Metadata metadata = 1;    // 文件元数据
        bytes chunk = 2;          // 文件内容分片
    }
}

message BackupResponse {
    string snapshot_id = 1;
    int64 total_bytes = 2;
    int64 total_files = 3;
}
```

**客户端/服务端架构**：

```
┌──────────────┐       gRPC (TLS)        ┌──────────────────┐
│ Backer Client │ ◄─────────────────────► │  Backer Server    │
│ (本地备份引擎) │                          │                   │
│              │                          │  ┌─────────────┐  │
│  ┌─────────┐ │                          │  │ UserManager  │  │
│  │ Pipeline├─│─ backup stream ────────► │  │ (JWT + SQLite)│  │
│  └─────────┘ │                          │  ├─────────────┤  │
│              │                          │  │MetadataManager│  │
│              │                          │  ├─────────────┤  │
│              │                          │  │ Storage      │  │
│              │                          │  └─────────────┘  │
└──────────────┘                          └──────────────────┘
```

---

## 6. 错误处理策略

### 6.1 错误码体系

```cpp
enum class ErrorCode : uint32_t {
    // 通用 (0x00xx)
    kOk                  = 0,
    kUnknown             = 1,
    kNotImplemented      = 2,

    // 文件系统 (0x01xx)
    kPathNotFound        = 0x0101,
    kPermissionDenied    = 0x0102,
    kDiskFull            = 0x0103,
    kFileTooLarge        = 0x0104,
    kSymlinkLoop         = 0x0105,

    // 管道处理 (0x02xx)
    kCompressionFailed   = 0x0201,
    kEncryptionFailed    = 0x0202,
    kInvalidArchive      = 0x0203,

    // 存储 (0x03xx)
    kWriteFailed         = 0x0301,
    kReadFailed          = 0x0302,
    kNetworkError        = 0x0303,
    kAuthFailed          = 0x0304,
};
```

### 6.2 错误传播

所有可能失败的函数返回 `std::expected<T, ErrorCode>`，管道阶段返回错误后引擎终止并回滚已写入的数据。

---

## 7. 并发模型

| 模块 | 线程模型 | 说明 |
|------|----------|------|
| CLI | 单线程同步 | 命令执行完即退出 |
| GUI | Qt 主线程 + 工作线程 | 备份在 QThread 中执行，不阻塞 UI |
| FileWatcher | 1 监控线程 + 1 工作线程 | epoll 等待 inotify 事件，worker 执行备份 |
| Scheduler | 1 定时线程 | timerfd + epoll_wait |
| gRPC Server | gRPC 内置线程池 | ServerBuilder 自动管理 |

---

## 8. 目录结构

```
├── CMakeLists.txt       # 根构建配置
├── cmake/               # CMake 模块
├── Dockerfile
├── docker-compose.yml
├── docs/                # 课程文档
├── proto/               # gRPC protobuf 定义
├── src/
│   ├── main.cpp         # 入口 (CLI 模式)
│   ├── cli/             # 命令行解析
│   ├── core/            # 备份/还原引擎
│   ├── fs/              # 文件系统抽象（含特殊文件/元数据）
│   ├── filters/         # 自定义筛选
│   ├── pack/            # 打包解包 (Tar/Zip)
│   ├── compress/        # 压缩解压 (gzip/zstd/lzma)
│   ├── crypto/          # 加密解密 (AES/SM4)
│   ├── watch/           # 实时备份 (inotify)
│   ├── scheduler/       # 定时备份
│   ├── network/         # 网络备份 (gRPC)
│   ├── storage/         # 存储抽象
│   └── gui/             # Qt 图形界面
├── tests/               # Google Test 用例
└── scripts/             # 构建/测试/基准脚本
```

完整文件列表见 [`CLAUDE.md`](../CLAUDE.md#目录结构)。

---

## 9. 备份与还原流程

### 9.1 备份流程

```
用户输入 backup /home/data /backup/dir --compress gzip --encrypt
    │
    ▼
CLI 解析参数 → 构建 BackupOptions
    │
    ▼
BackupEngine::backup("/home/data", options)
    │
    ├─ FSAbstraction::walk("/home/data") → FileEntry 列表
    │
    ├─ Filter::apply(entries) → 筛选后的条目
    │
    ├─ (可选) Packer::pack(entries, fs, buffer) → 打包流
    │
    ├─ (可选) Compressor::compress(buffer, compressed) → 压缩流
    │
    ├─ (可选) Encryptor::encrypt(compressed, encrypted, password) → 加密流
    │
    ├─ Storage::write(backup_path, encrypted) → 持久化
    │
    └─ BackupIndex::save() → 记录索引
```

### 9.2 还原流程

```
用户输入 restore /backup/dir /home/data --decrypt --decompress gzip
    │
    ▼
CLI 解析参数 → 构建 RestoreOptions
    │
    ▼
RestoreEngine::restore("/backup/dir", "/home/data", options)
    │
    ├─ Storage::read(backup_path) → 原始数据流
    │
    ├─ (可选) Encryptor::decrypt(stream, decrypted, password) → 解密
    │
    ├─ (可选) Compressor::decompress(decrypted, decompressed) → 解压
    │
    ├─ (可选) Packer::unpack(decompressed, dest, fs) → 解包
    │
    ├─ (略过打包时) 逐个还原文件
    │
    └─ FSAbstraction::restoreMetadata(path, meta) → 还原属性
```

---

## 10. 关键技术决策说明

| 决策 | 方案 | 理由 |
|------|------|------|
| 管道组合方式 | Builder 模式 (`setFilter/setPacker/...`) | 编译期确定管道，无虚函数调用开销；运行期灵活组合 |
| 工厂注册时机 | 静态初始化 (`[[maybe_unused]] bool`) | 新增算法只需包含头文件，不需要改注册代码 |
| 备份索引格式 | JSON | 可读性好，便于调试；第三方工具可直接读取 |
| 去抖策略 | 时间窗口合并 (1s 内的同类事件合并一次备份) | 防止高频修改（如 IDE 自动保存）触发大量备份 |
| 网络传输模型 | gRPC 双向流 | 适合大文件分片传输；流式处理无需加载整个文件到内存 |
| 错误恢复 | 事务式写入 + 索引校验 | 先写临时文件，成功后 rename 覆盖；索引存 checksum |

---

## 11. 接口定义汇总

### 公共 API (CLI)

子命令：`backup`、`restore`、`list`、`watch`、`schedule`、`server`、`client`。

支持筛选（路径/类型/名称/时间/尺寸/属主）、打包（tar/zip）、压缩（gzip/zstd/lzma）、加密（aes/sm4）、增量备份等选项。

详见 [`src/cli/commands.h`](../src/cli/commands.h) 和 [`CLAUDE.md`](../CLAUDE.md#构建与运行)。

---

## 12. 设计验证

| 质量属性 | 验证方式 | 对应设计 |
|----------|----------|----------|
| 正确性 | 备份→还原→diff 全等 | 管道各阶段均有独立单元测试 |
| 健壮性 | Valgrind 无泄漏 + 异常路径覆盖 | 所有错误返回 ErrorCode，引擎支持回滚 |
| 扩展性 | 新增压缩算法 ≤ 3 个文件 | 策略接口 + 工厂注册 |
| 性能 | 大文件/大目录 benchmark | 去抖合并、流式处理、零拷贝 |
| 兼容性 | 旧版本备份文件可还原 | 备份格式含版本号 Magic Header |

# 网络备份 — 实施计划

## 功能概述

将数据备份软件从单机模式扩展为网盘模式，支持通过网络将备份数据发送到远程服务器，包含用户管理、元数据管理、传输加密、增量备份等功能。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（网络备份 / 10+5+5+5+5 分）。

## 技术方案

### 架构设计

```
┌──────────────────────┐          gRPC (TLS)          ┌──────────────────────┐
│   Backer Client       │ ◄─────────────────────────► │   Backer Server       │
│   (本地备份引擎)       │                              │                       │
│                       │   Backup(stream)  request    │  ┌─────────────────┐  │
│  ┌─────────────────┐  │   Restore(stream) request    │  │  UserManager     │  │
│  │ Pipeline         │──┤   ListSnapshots   request   │  │  (JWT + SQLite)   │  │
│  │ Filter→Pack→Comp │  │   Login/Register  request   │  ├─────────────────┤  │
│  │ →Encrypt→Upload  │  │                              │  │MetadataManager  │  │
│  └─────────────────┘  │                              │  ├─────────────────┤  │
│                       │                              │  │  SnapshotStore  │  │
│  ┌─────────────────┐  │                              │  │  (本地文件系统)   │  │
│  │ RemoteStorage    │──┤                              │  └─────────────────┘  │
│  │ (gRPC stub)      │  │                              └──────────────────────┘
│  └─────────────────┘  │
└──────────────────────┘
```

### gRPC 服务定义

```protobuf
service BackupService {
    // 用户认证
    rpc Login(LoginRequest) returns (LoginResponse);
    rpc Register(RegisterRequest) returns (RegisterResponse);
    rpc RefreshToken(RefreshRequest) returns (LoginResponse);

    // 备份操作
    rpc Backup(stream BackupRequest) returns (BackupResponse);
    rpc Restore(RestoreRequest) returns (stream RestoreResponse);

    // 快照管理
    rpc ListSnapshots(ListSnapshotsRequest) returns (ListSnapshotsResponse);
    rpc DeleteSnapshot(DeleteSnapshotRequest) returns (DeleteSnapshotResponse);

    // 元数据
    rpc GetMetadata(GetMetadataRequest) returns (GetMetadataResponse);
    rpc UpdateMetadata(UpdateMetadataRequest) returns (UpdateMetadataResponse);
}

// 备份流: 先发元数据，再发文件内容分片
message BackupRequest {
    oneof data {
        SnapshotMetadata metadata = 1;
        FileChunk file_chunk = 2;
    }
}

message FileChunk {
    string relative_path = 1;
    int64 offset = 2;           // 文件偏移
    bytes data = 3;             // 分片数据 (默认 64 KiB)
    bool is_last = 4;           // 是否最后一个分片
    string checksum = 5;        // SHA256 of this chunk
}
```

### 用户管理

**认证流程**：
```
Client                          Server
  │                               │
  ├── POST /Register(pass, info)──┤
  │          └── 返回 UserID      │
  │                               │
  ├── POST /Login(email, pass) ───┤
  │          └── JWT Token        │
  │                               │
  ├── Backup(stream) + JWT ───────┤
  │          └── 验证 Token       │
  │                               │
  ├── Token 过期 → Refresh ───────┤
  │          └── 新 Token         │
  └───────────────────────────────┘
```

**用户数据存储 (SQLite)**：
```sql
CREATE TABLE users (
    id TEXT PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,   -- bcrypt/scrypt
    display_name TEXT,
    quota_bytes INTEGER DEFAULT 1073741824,  -- 1 GiB 默认配额
    created_at TEXT DEFAULT (datetime('now'))
);

CREATE TABLE snapshots (
    id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    name TEXT,
    source_path TEXT,
    file_count INTEGER,
    total_size INTEGER,
    compressed_size INTEGER,
    checksum TEXT,
    created_at TEXT DEFAULT (datetime('now')),
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

### 元数据管理

服务端维护每份备份的元数据索引（JSON），支持按时间/路径等维度查询：

```json
{
  "snapshotId": "snap_a1b2c3d4",
  "userId": "user_001",
  "createdAt": "2026-07-06T22:00:00Z",
  "sourcePath": "/home/user/data",
  "totalFiles": 1250,
  "totalSize": 5368709120,
  "entries": [
    {
      "path": "docs/report.md",
      "type": "file",
      "size": 24576,
      "mtime": "2026-07-05T14:30:00Z",
      "checksum": "sha256:e5f6..."
    }
  ]
}
```

### 传输加密

- **传输层**：gRPC 内置 TLS/SSL (`grpc::SslServerCredentials`)
- **自签名证书**：开发阶段使用工具生成
- **认证层**：JWT token 放在 gRPC metadata 中传输

### 增量备份

**策略**：基于文件哈希对比 (SHA-256)

```
1. 客户端扫描源目录，计算每文件 SHA-256
2. 向服务端请求上次快照的文件哈希列表
3. 对比哈希 → 仅上传新增/变更的文件
4. 上传变更文件列表 → 服务端生成新快照
```

```cpp
// 增量备份决策
struct IncrementalDecision {
    std::vector<FileEntry> newFiles;        // 新增文件 → 全量上传
    std::vector<FileEntry> modifiedFiles;   // 变更文件 → 全量上传
    std::vector<std::string> unchangedFiles; // 未变文件 → 仅记录路径
    std::vector<std::string> deletedFiles;   // 已删除文件 → 从索引移除
};

class IncrementalEngine {
    IncrementalDecision compare(
        std::vector<FileEntry> const& currentFiles,
        SnapshotIndex const& lastSnapshot);
};
```

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. Protobuf 定义 | 编写 `.proto` 服务定义 | `proto/backup_service.proto` |
| 2. CMake gRPC 集成 | `find_package(gRPC)` + protobuf 生成 | `CMakeLists.txt` |
| 3. SQLite 集成 | 用户/快照表创建 + 基本 CRUD | `user_manager.h/cpp` |
| 4. 用户注册/登录 | 密码哈希 + JWT 生成/验证 | `user_manager.cpp`, `auth.h/cpp` |
| 5. 服务端 gRPC 服务 | BackupService 各 RPC 实现 | `backup_server.h/cpp` |
| 6. 客户端 gRPC 封装 | RemoteStorage (gRPC stub) | `remote_storage.h/cpp`, `backup_client.h/cpp` |
| 7. 传输加密 | TLS 证书 + gRPC SSL 配置 | `tls_config.h/cpp` |
| 8. 元数据服务端管理 | 快照元数据存储/查询 | `metadata_manager.h/cpp` |
| 9. 增量备份引擎 | 文件哈希比较 + 差量上传 | `incremental_engine.h/cpp` |
| 10. 快照管理 | 快照列表/删除/查询 CLI | `commands.cpp` |
| 11. 单元测试 + 集成测试 | 各子模块测试 + client-server 联调 | 测试文件 |

## 关键接口

```cpp
// --- 客户端 ---
class RemoteStorage : public Storage {
public:
    RemoteStorage(std::string const& serverAddr, std::string const& token);

    std::expected<void, ErrorCode> write(std::string const& path,
                                          std::span<char const> data) override;
    std::expected<std::vector<char>, ErrorCode>
        read(std::string const& path) override;
    std::expected<SnapshotList, ErrorCode>
        listSnapshots() override;

private:
    std::unique_ptr<BackupService::Stub> stub_;
    std::string token_;
};

class BackupClient {
public:
    BackupClient(std::string const& serverAddr);

    // 认证
    std::expected<std::string, ErrorCode> login(std::string const& email,
                                                 std::string const& password);
    std::expected<void, ErrorCode> register_(std::string const& email,
                                              std::string const& password,
                                              std::string const& name);

    // 增量备份
    std::expected<std::string, ErrorCode> backupIncremental(
        std::filesystem::path const& source,
        std::optional<std::string> lastSnapshotId);
};

// --- 服务端 ---
class BackupServiceImpl final : public BackupService::Service {
    grpc::Status Backup(grpc::ServerContext* context,
                        grpc::ServerReader<BackupRequest>* reader,
                        BackupResponse* response) override;
    grpc::Status Restore(grpc::ServerContext* context,
                         const RestoreRequest* request,
                         grpc::ServerWriter<RestoreResponse>* writer) override;
    // ...
};

class UserManager {
public:
    std::expected<std::string, ErrorCode> registerUser(
        std::string const& email, std::string const& password);
    std::expected<std::string, ErrorCode> login(
        std::string const& email, std::string const& password);
    bool verifyToken(std::string const& token);
    std::expected<UserInfo, ErrorCode> getUserInfo(std::string const& userId);
};

// --- 增量备份 ---
class IncrementalEngine {
public:
    IncrementalDecision compare(
        std::vector<FileEntry> const& current,
        SnapshotIndex const& lastSnapshot);

    // 计算单个文件哈希
    std::expected<std::string, ErrorCode> fileHash(
        std::filesystem::path const& path);
};
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 用户注册 | 注册新用户 | 数据库中有记录 |
| 用户登录 | 正确/错误密码 | Token 返回/拒绝 |
| 基本备份 | 客户端发文件 → 服务端接收 | 文件内容一致 |
| 基本还原 | 请求还原 → 客户端接收 | 内容一致 |
| 增量备份 | 修改 1 个文件 → 仅上传变更 | 网络传输量少 |
| TLS 加密 | 客户端连接验证 | 无法中间人 |
| 权限控制 | 用户 A 无法访问用户 B 数据 | 返回权限错误 |
| 并发 | 多客户端同时备份 | 服务端稳定 |
| 断线重连 | 传输中切断 → 重连 | 恢复传输 |
| 配额检查 | 超出存储配额 | 拒绝写入 + 提示 |

## 服务端部署

```bash
# 生成自签名 TLS 证书
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes

# 启动服务端
backer server start --port 50051 --cert cert.pem --key key.pem

# 客户端连接
backer client connect 192.168.1.100:50051 backup /home/user/data --user alice
```

## 关键文件

```
proto/
└── backup_service.proto           # gRPC 服务定义
src/network/
├── backup_client.h
├── backup_client.cpp
├── backup_server.h
├── backup_server.cpp
├── remote_storage.h
├── remote_storage.cpp
├── user_manager.h
├── user_manager.cpp
├── auth.h
├── auth.cpp                       # JWT + 密码哈希
├── metadata_manager.h
├── metadata_manager.cpp
├── tls_config.h
├── tls_config.cpp
└── incremental_engine.h
└── incremental_engine.cpp
src/cli/
└── commands.cpp                   # server/client 子命令
tests/network/
├── user_manager_test.cpp
├── auth_test.cpp
├── incremental_engine_test.cpp
└── backup_server_test.cpp
CMakeLists.txt                     # find_package(gRPC protobuf)
```

## 依赖安装

```bash
# Ubuntu / Debian
sudo apt install libgrpc++-dev protobuf-compiler-grpc \
                 libsqlite3-dev libssl-dev

# Fedora / RHEL
sudo dnf install grpc-devel protobuf-compiler \
                 sqlite-devel openssl-devel

# Arch Linux
sudo pacman -S grpc protobuf sqlite openssl
```

## 预计工作量

- 代码行数：~2500 行（含测试）
- 开发周期：2-3 周（单人）或 1-2 周（两人）
- 依赖：gRPC + Protocol Buffers + SQLite + OpenSSL

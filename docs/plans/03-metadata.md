# 元数据支持 — 实施计划

## 功能概述

在备份时保留文件的完整元数据（属主、权限、时间戳），还原时精确恢复，保证备份数据与原数据元数据一致。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（元数据支持 / 10 分）。

## 技术方案

### 元数据结构

`Metadata` 和 `FileEntry` 结构体已在 [architecture-design.md §3.2](../architecture-design.md#32-%E6%96%87%E4%BB%B6%E6%9D%A1%E7%9B%AE%E7%BB%93%E6%9E%84) 中定义，本模块直接复用。核心字段：

```
Metadata:
├─ ownerId / groupId       属主和属组
├─ permissions             权限 (含 setuid/setgid/sticky)
├─ accessTime / modifyTime 时间戳 (纳秒精度)
└─ changeTime              仅记录，不还原
```

### 读取元数据

使用 `lstat()` 系统调用（而非 `stat()`，避免跟随符号链接）：

```cpp
Metadata parseMetadata(struct stat const& st) {
    return Metadata{
        .ownerId     = st.st_uid,
        .groupId     = st.st_gid,
        .permissions = st.st_mode & 07777,  // 仅保留权限位
        .accessTime  = st.st_atim,
        .modifyTime  = st.st_mtim,
        .changeTime  = st.st_ctim
    };
}
```

### 还原元数据

按顺序执行，错误不回滚（尽力而为策略）：

| 操作 | 系统调用 | 说明 |
|------|----------|------|
| 还原属主 | `lchown()` | 符号链接用 `lchown`，防止跟随 |
| 还原权限 | `fchmodat()` / `chmod()` | 符号链接权限不可修改（Linux） |
| 还原时间 | `utimensat()` | 支持纳秒精度，符号链接用 `AT_SYMLINK_NOFOLLOW` |

**重点**：
- `lchown` 需 `CAP_CHOWN` 权限（通常 root），非 root 可降级为记录警告
- 时间戳还原应在文件内容写入完成后，最后一步执行
- 还原顺序：先创建文件/链接 → 写入内容 → 还原属主 → 还原权限 → 还原时间戳

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. Metadata 结构体 | 定义数据结构 + JSON 序列化/反序列化 | `metadata.h` |
| 2. 元数据读取 | `lstat()` → 填充 Metadata | `metadata.cpp` |
| 3. 元数据还原 | `lchown` + `chmod` + `utimensat` | `metadata.cpp` |
| 4. 集成到 FileEntry | FileEntry 增加 metadata 字段 | `fs_abstraction.h` |
| 5. 备份索引记录 | 元数据写入索引文件 `backer_index.json` | `backup_index.cpp` |
| 6. 权限降级处理 | 非 root 时的降级策略 | 错误处理 |
| 7. CLI 选项 | `--preserve-metadata` / `--no-metadata` | `commands.cpp` |
| 8. 单元测试 | 创建文件→设属性→备份→还原→验证属性 | 测试文件 |

## 关键接口

元数据操作方法直接集成在 `FSAbstraction` 中（不在独立 `MetadataManager` 类中，避免职责重叠）。详见 [architecture-design.md §3.1](../architecture-design.md#31-%E8%AE%BE%E8%AE%A1%E7%9B%AE%E7%9A%84)：

```cpp
// FSAbstraction 中的元数据方法
class FSAbstraction {  // 已有 walk/read/write 方法
    // 以下为元数据操作 —— 本模块实现的重点

    // 从文件读取元数据
    std::expected<Metadata, ErrorCode>
        readMetadata(std::filesystem::path const& path);

    // 还原元数据到文件
    std::expected<void, ErrorCode>
        restoreMetadata(std::filesystem::path const& path,
                        Metadata const& meta);
};

// JSON 序列化（辅助函数，放在 metadata.h 中）
std::string metadataToJson(Metadata const& meta);
std::optional<Metadata> metadataFromJson(std::string const& json);
bool canRestoreOwnership();
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 权限保留 | 设 0644/0755/0777 → 备份 → 还原 | `stat.st_mode` 一致 |
| 时间戳保留 | 设特定 atime/mtime | `stat.st_atim/st_mtim` 纳秒一致 |
| 属主/组 | root 运行，设不同 UID/GID | `stat.st_uid/st_gid` 一致 |
| setuid/setgid | 设 4755/2755 | 权限位精确保留 |
| 非 root 降级 | 普通用户运行 | 属主警告 + 其他元数据正常 |
| 符号链接 | 用 `lchown` 而非 `chown` | 不跟随链接 |

## 关键文件

```
src/fs/
├── metadata.h
├── metadata.cpp
├── fs_abstraction.h       # 修改 FileEntry 增加 metadata
└── fs_abstraction.cpp
src/core/
└── backup_index.h/cpp     # 修改索引格式增加元数据
tests/fs/
└── metadata_test.cpp
```

## 预计工作量

- 代码行数：~600 行（含测试）
- 开发周期：3-5 天
- 依赖：`<sys/stat.h>`, `<unistd.h>`, `<fcntl.h>`

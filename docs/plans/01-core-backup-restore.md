# 核心备份与还原 — 实施计划

## 功能概述

实现数据备份软件的核心功能，将指定目录树中的文件数据保存到备份位置，并能完整还原。

- **数据备份**：递归遍历源目录，读取所有常规文件内容，写入目标备份位置
- **数据还原**：从备份位置读取数据，恢复完整的目录树结构和文件内容

## 对应需求

详见 [docs/requirements.md](../requirements.md)（基础分 40 分：目录树备份与还原）。

## 技术方案

### 核心类设计

```
┌──────────────────────┐
│    BackupEngine       │
│  ├ backup() → 遍历→读→写  │
│  └ restore() → 读→写     │
└──────────────────────┘
          │
          ▼
┌──────────────────────┐
│    FSAbstraction      │
│  ├ walk()   遍历目录树  │
│  ├ read()   读文件内容  │
│  ├ write()  写文件      │
│  └ mkdir()  创建目录    │
└──────────────────────┘
```

### 技术选型

- **文件系统遍历**：`std::filesystem::recursive_directory_iterator`
- **文件 I/O**：`std::ifstream` / `std::ofstream`（二进制模式），缓冲区 64 KiB
- **路径映射**：源路径 → 相对路径 → 备份路径（保留目录结构）

### 备份格式（简化版）

第一阶段使用"目录结构镜像"方式：

```
备份目录/
├── file1.txt          ← 直接拷贝文件内容
├── dir/
│   ├── file2.jpg
│   └── subdir/
│       └── file3.pdf
└── .backer_index      ← 可选元数据索引 (JSON)
```

后续扩展（打包功能）时改用自定义二进制打包格式。

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. 项目骨架 | 创建 CMakeLists.txt、src/、tests/ 目录结构 | 可编译的空项目 |
| 2. 路径映射模块 | `PathMapper`：源路径 ↔ 相对路径 ↔ 备份路径转换 | `path_mapper.h/cpp` |
| 3. 目录遍历 | `walk()`：递归扫描目录，返回文件条目列表 | `fs_abstraction.h/cpp` |
| 4. 文件备份 | `backup()`：遍历→读→逐文件写入备份目录 | `backup_engine.h/cpp` |
| 5. 文件还原 | `restore()`：扫描备份目录→重建目录树→写文件 | `restore_engine.h/cpp` |
| 6. CLI 命令 | `backer backup` 和 `backer restore` 子命令 | `commands.h/cpp` |
| 7. 错误处理 | 路径不存在、权限不足、磁盘满等异常处理 | ErrorCode 枚举 |
| 8. 单元测试 | 小型目录树备份→还原→diff 验证 | 测试文件 |
| 9. 集成测试 | 大文件、空目录、嵌套深目录等场景 | 测试脚本 |

## 关键接口

```cpp
// 备份引擎
class BackupEngine {
    BackupResult backup(std::filesystem::path const& source,
                        std::filesystem::path const& destination);
};

// 还原引擎
class RestoreEngine {
    RestoreResult restore(std::filesystem::path const& source,
                          std::filesystem::path const& destination);
};

// 文件系统抽象
class FSAbstraction {
    std::expected<std::vector<FileEntry>, ErrorCode>
        walk(std::filesystem::path const& root);
    std::expected<void, ErrorCode>
        read(std::filesystem::path const& path, std::vector<char>& out);
    std::expected<void, ErrorCode>
        write(std::filesystem::path const& path, std::span<char const> data);
};
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 小型目录树 | 备份→删除源→还原→diff | 文件内容一致 |
| 空目录 | 备份含空目录 | 还原后空目录存在 |
| 大文件 (1GiB+) | 分块读写 | 内容完整、无截断 |
| 深嵌套路径 | 创建深目录树 | 路径正确重建 |
| 错误路径 | 不存在的源路径 | 返回 ErrorCode |
| 权限不足 | chmod 只读目录 | 优雅报错不崩溃 |

> **演进说明**：本阶段为简化实现（直接目录镜像），后续扩展时将演进为 architecture-design.md 定义的完整接口设计——`BackupEngine` 使用 Builder 模式（`setFilter/setPacker/...`）构建管道，各阶段可插拔组合。

## 关键文件

```
src/
├── CMakeLists.txt
├── main.cpp
├── core/
│   ├── backup_engine.h
│   ├── backup_engine.cpp
│   ├── restore_engine.h
│   ├── restore_engine.cpp
│   └── error_code.h
├── fs/
│   ├── fs_abstraction.h
│   ├── fs_abstraction.cpp
│   └── path_mapper.h
├── cli/
│   ├── commands.h
│   └── commands.cpp
└── storage/
    ├── storage.h
    ├── local_storage.h
    └── local_storage.cpp
tests/
├── core/
│   ├── backup_engine_test.cpp
│   └── restore_engine_test.cpp
└── e2e/
    └── backup_restore_test.sh
```

## 预计工作量

- 代码行数：~1500 行（含测试）
- 开发周期：1-2 周（单人）
- 依赖：C++17 标准库 + CLI11

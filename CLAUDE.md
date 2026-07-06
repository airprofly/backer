# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **自维护规则**：当发现代码、设计、或实践中存在本文件未覆盖或记载有误的内容时，应主动更新本文件以保持同步。

## 项目概述

数据备份软件——计算机组成与体系结构/软件工程课程项目，基于 C++17 在 Linux 平台开发。支持目录树的备份与还原，以及特殊文件、元数据、压缩加密、GUI、实时/定时/网络备份等扩展功能。

## 构建与运行

```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build -j$(nproc)

# 运行单元测试
ctest --test-dir build
# 带详细输出
ctest --test-dir build --output-on-failure

# 运行指定测试
./build/backer_test --gtest_filter="*BackupCore*"

# CLI 使用
./build/backer-cli --help
./build/backer-cli backup /path/to/source /path/to/backup
./build/backer-cli restore /path/to/backup /path/to/restore

# 代码风格检查
cpplint --filter=-runtime/references --recursive src/ tests/
clang-tidy-14 src/**/*.cpp -- -std=c++17

# 内存检测
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./build/backer_test

# Docker 构建
docker build -t backer .
docker compose up -d

# 交互式进入容器
docker run --rm -it --entrypoint /bin/bash backer
```

### Docker Compose 挂载约定

```yaml
volumes:
  - ./data/source:/data/source:ro   # 待备份源目录（只读）
  - ./data/backup:/data/backup      # 备份输出目录
  - ./data/restore:/data/restore    # 还原目标目录
```

## 技术栈

| 层面 | 技术 |
|------|------|
| 语言 | C++17 (GCC 9+ / Clang 12+) |
| 构建 | CMake 3.16+ (FetchContent 自动拉取依赖) |
| CLI 解析 | CLI11 v2.4.2 (header-only) |
| 日志 | spdlog v1.14.1 |
| 文件系统 | std::filesystem |
| 测试 | Google Test v1.15.2 |
| 代码检查 | cpplint + clang-tidy-14 |
| 内存检测 | Valgrind (CI 自动校验) |
| 容器 | Docker (multi-stage build: gcc:13-bookworm → debian:bookworm-slim) + Compose |
| CI | GitHub Actions (lint / 双编译器矩阵构建测试 / Valgrind / Docker) |

### 扩展功能选型

| 功能 | 技术 |
|------|------|
| 打包 (Tar) | 自实现 Tar 格式 |
| 打包 (Zip) | miniz |
| 压缩 | zlib / zstd / liblzma |
| 加密 | OpenSSL (AES/SM4) |
| GUI | Qt 6 (Widget) |
| 定时备份 | timerfd + ccronexpr |
| 实时备份 | inotify + 事件队列 |
| 网络备份 | gRPC + Protocol Buffers |
| 增量备份 | 文件哈希比较 |

## 测试要求

**所有逻辑代码必须有对应测试用例**，未覆盖不允许提交。

- 单元测试用 Google Test，每模块一个测试文件，按函数/类组织 `TEST_F`
- 覆盖边界场景（空/大文件、特殊路径）和异常路径（权限不足、磁盘满、路径不存在）
- 测试间隔离，临时目录在 `SetUp`/`TearDown` 中创建和清理

## 代码规范

### 设计原则

- **精炼无冗余**：不写用不到的代码，不提前抽象，不引入用不上的依赖库，消灭死代码和注释掉的代码
- **低耦合**：模块间通过接口（纯虚类）通信，禁止直接依赖具体实现；管道阶段之间只传递 `BackupContext` 数据对象
- **扩展性好**：新压缩/加密算法只需实现对应策略接口，通过工厂注册，不修改现有代码
- **单一职责**：一个类只做一件事，超过 200 行的方法考虑拆分
- **RAII 贯穿**：所有资源（文件句柄、内存、锁）由 RAII 包装器管理，禁止裸 `new`/`delete`

### 编码风格

| 项目 | 规范 |
|------|------|
| 命名 | 类/命名空间 `PascalCase`，函数/变量 `camelCase`，常量 `kPascalCase`，宏全大写 `SNAKE_CASE` |
| 头文件 | 使用 `#pragma once`（vs 传统 header guards），最小化 include，用前向声明代替 include |
| const 正确性 | 成员函数不修改成员变量加 `const`，传参用 `const &`，指针参数判断 `nullptr` |
| 智能指针 | 所有权明确：独有 `std::unique_ptr`，共享 `std::shared_ptr`，观察 `std::weak_ptr` |
| 错误处理 | 返回 `std::expected<T, ErrorCode>` 或抛出异常，禁止忽略返回值或吞掉异常 |
| 注释 | 代码应自注释（自注释：通过合理的命名和结构表达意图）；公共 API 写 `/// doxygen` 注释；实现细节只写 WHY（非显而易见的决策/边界条件/坑），不写 WHAT（代码本身表明了做了什么）；**符合常识的代码不加注释** |
| 格式 | `const` 在类型右侧：`std::string const &name`，指针 `*` 靠类型：`int* p` |
| 魔数 | 禁止魔法数字，使用 `constexpr` 命名常量替代 |
| callback | 用 `std::function` 替代函数指针，lambda 捕获明确写 `[=]` 或 `[&]` 而非隐式默认捕获 |
| 迭代器 | 使用 range-for 而非原始 `for (auto it = ...)`，需要算法时用 `<algorithm>` 库 |

### 接口设计

管道阶段接口定义见 [`docs/architecture-design.md`](docs/architecture-design.md#22-管道接口pipeline)（Compressor、Encryptor、Packer）。

## 架构

分层架构 + 管道模式，详见 [`docs/architecture-design.md`](docs/architecture-design.md)。

## 目录结构

```
├── CMakeLists.txt              # 根构建配置（含测试目标生成）
├── Dockerfile                  # Multi-stage 构建（gcc:13-bookworm → slim）
├── docker-compose.yml          # Compose 编排（含数据卷挂载）
├── .dockerignore               # 构建上下文精简（排除 docs/ testdata/ 等）
├── .gitignore                  # Git 忽略规则
├── .github/
│   └── workflows/
│       └── ci.yml              # GitHub Actions CI（lint + 构建测试 + Valgrind + Docker）
├── .claude/
│   ├── commands/
│   │   ├── update-info.md      # 本项目专用的 /update-info 命令
│   │   └── git/
│   │       ├── create-branch.md # /git:create-branch 命令
│   │       └── git-commit.md    # /git:git-commit 命令
│   └── settings.local.json     # 本地 Claude 权限等配置
├── docs/
│   ├── architecture-design.md  # 分层架构 + 管道模式设计
│   ├── plans/
│   │   ├── README.md           # 分阶段实现路线图（01～11）
│   │   ├── 01-core-backup-restore.md  # ✅ 已完成
│   │   └── 02-*.md ~ 11-*.md          # 🔲 待实施
│   ├── reportDetails.md        # 报告提交要求
│   ├── requirements.md         # 需求规格说明
│   └── technology-selection.md # 技术选型论证
├── src/                        # 全部业务源码
│   ├── main.cpp                # 程序入口，初始化 CLI 并派发命令
│   ├── cli/
│   │   ├── commands.h          # CLI 命令类声明（backup/restore）
│   │   └── commands.cpp        # CLI 命令实现
│   ├── core/                   # ✅ 备份/还原引擎——中心调度逻辑
│   │   ├── backup_engine.h/cpp # 备份引擎
│   │   ├── restore_engine.h/cpp# 还原引擎
│   │   ├── types.h             # 核心数据类型（BackupContext 等）
│   │   ├── error_code.h        # 错误码枚举
│   │   └── expected.h          # std::expected 替代（C++17 polyfill）
│   ├── fs/                     # ✅ 文件系统抽象层
│   │   ├── fs_abstraction.h/cpp# 文件读写、目录遍历
│   │   └── path_mapper.h/cpp   # 路径映射（相对/绝对转换）
│   ├── storage/                # ✅ 存储抽象层
│   │   ├── storage.h           # 存储接口（纯虚类）
│   │   └── local_storage.h/cpp # 本地文件系统实现
│   ├── filters/    🚧          # 自定义筛选（按正则/大小/日期过滤）
│   ├── pack/       🚧          # 打包格式（自实现 Tar + miniz Zip）
│   ├── compress/   🚧          # 压缩（zlib / zstd / liblzma 策略接口）
│   ├── crypto/     🚧          # 加密（AES / SM4 策略接口，基于 OpenSSL）
│   ├── gui/        🚧          # Qt 6 Widgets 图形界面
│   ├── watch/      🚧          # inotify 实时文件监控
│   └── network/    🚧          # gRPC 网络备份
├── tests/
│   └── core/                   # ✅ 备份/还原引擎单元测试（Google Test）
│       ├── backup_engine_test.cpp
│       └── restore_engine_test.cpp
└── testdata/                   # 🧪 测试数据（按场景分类）
    ├── text/                   # 文本文件（hello.txt / empty.txt / large.txt）
    ├── filter/                 # 筛选测试（data.bin / debug.log / tmp/）
    ├── meta/                   # 元数据测试（executable.sh / private.key）
    ├── naming/                 # 特殊命名测试（.hidden / 空格 / 中文）
    ├── nested/                 # 深层嵌套（a/b/c/leaf.txt）
    └── special/                # 特殊文件占位（placeholder.txt / link.txt）
```

> 🚧 = 已规划但尚未实现的模块；✅ = 已完成

## 实现路线

详见 [`docs/plans/README.md`](docs/plans/README.md)。

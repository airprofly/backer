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

# 运行指定测试
./build/tests/backer_test --gtest_filter="*BackupCore*"

# 代码风格检查
cpplint --filter=-runtime/references src/**/*.cpp src/**/*.h
clang-tidy src/**/*.cpp -- -std=c++17

# 内存检测
valgrind --leak-check=full --show-leak-kinds=all ./build/backer-cli backup /path

# 性能分析
perf record ./build/backer-cli backup /path
gprof ./build/backer-cli gmon.out > analysis.txt

# Docker 构建
docker build -t backer .
docker compose up -d
```

## 技术栈

| 层面 | 技术 |
|------|------|
| 语言 | C++17 (GCC 9+ / Clang 12+) |
| 构建 | CMake 3.16+ |
| CLI 解析 | CLI11 (header-only) |
| 日志 | spdlog |
| 文件系统 | std::filesystem + sys/stat.h |
| 测试 | Google Test |
| 代码检查 | cpplint + clang-tidy |
| 性能分析 | Valgrind + gperf + perf |
| 容器 | Docker (multi-stage build) + Compose |
| CI | GitHub Actions |

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
| 增备备份 | 文件哈希比较 |

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
├── CMakeLists.txt        # 根构建配置
├── Dockerfile            # 多阶段构建
├── docker-compose.yml    # 容器编排
├── docs/                 # 课程文档
├── src/
│   ├── core/             # 备份/还原引擎
│   ├── fs/               # 文件系统抽象 (常规/特殊文件/元数据)
│   ├── filters/          # 自定义筛选
│   ├── pack/             # 打包 (Tar/Zip)
│   ├── compress/         # 压缩 (gzip/zstd/lzma)
│   ├── crypto/           # 加密 (AES/SM4)
│   ├── gui/              # Qt 6 图形界面
│   ├── watch/            # inotify 实时监控
│   └── network/          # gRPC 网络备份
└── tests/                # Google Test 用例
```

## 实现路线

详见 [`docs/plans/README.md`](docs/plans/README.md)。

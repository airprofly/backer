# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **自维护规则**：当发现代码、设计、或实践中存在本文件未覆盖或记载有误的内容时，应主动更新本文件以保持同步。

## 项目概述

数据备份软件——计算机组成与体系结构/软件工程课程项目，基于 C++17 在 Linux 平台开发。支持目录树的备份与还原，以及特殊文件、元数据、压缩加密、GUI、实时/定时/网络备份等扩展功能。

## 文档导航

| 文档 | 位置 | 说明 |
|------|------|------|
| 使用文档 | [`docs/usage.md`](docs/usage.md) | 功能使用说明、命令参考、示例 |
| 需求规格说明 | [`docs/requirements.md`](docs/requirements.md) | 课程项目基本要求与扩展要求 |
| 实施计划 | [`docs/plans/README.md`](docs/plans/README.md) | 分阶段实现路线图（01～11） |
| 架构设计 | [`docs/architecture-design.md`](docs/architecture-design.md) | 分层架构 + 管道模式设计 |

## 构建与运行

### 构建准则

- **禁止 `rm -rf build` 全量重编**：始终使用增量编译。若遇到 FetchContent 缓存问题，只清理具体依赖目录（如 `rm -rf build/_deps/spdlog*`），切勿删除整个 build 目录。
- **增量编译流程**：修改源码后直接 `cmake --build build -j$(nproc)`，CMake 会自动检测变更重新编译。只在修改 `CMakeLists.txt` 或新增文件后才需要重新 `cmake -B build`（无需删除 build 目录）。

```bash
# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 测试
ctest --test-dir build --output-on-failure            # 运行所有测试
./build/backer_test --gtest_filter="*BackupCore*"     # 指定测试

# 使用
bash scripts/setup-testdata.sh                        # 生成测试数据
./build/backer-cli backup data/source data/backup     # 备份
./build/backer-cli restore data/backup data/restore   # 还原
bash scripts/test-backup-restore.sh                   # 端到端流程测试

# Docker
docker build -t backer .
docker compose run --rm backer backup /data/source /data/backup
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
| 代码检查 | cpplint（本地）+ clang-tidy-14（CI） |
| 内存检测 | Valgrind (CI 自动校验) |
| 容器 | Docker（multi-stage：gcc:13-bookworm → slim，GH_PROXY 构建参数支持国内加速）+ Compose |
| CI | GitHub Actions（clang-tidy lint / 双编译器矩阵构建测试 / Valgrind / Docker） |

### 扩展功能选型

| 功能 | 技术 |
|------|------|
| 特殊文件 | POSIX lstat/mkfifo/mknod + std::filesystem 回退 |
| 元数据 | POSIX lchown/fchmodat/utimensat + JSON 序列化 |
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

目录树展示规则：
- 单文件/单子目录的目录合入父级注释，不展开
- 只有含多个子项的目录才保持展开

```
├── CMakeLists.txt              # 根构建配置（含测试目标生成）
├── Dockerfile                  # Multi-stage 构建（gcc:13-bookworm → slim）
├── docker-compose.yml          # Compose 编排（含数据卷挂载）
├── .dockerignore               # 构建上下文精简（排除 docs/ testdata/ 等）
├── .gitignore                  # Git 忽略规则
├── LICENSE                     # Apache 2.0 许可证
├── .github/
│   └── workflows/
│       └── ci.yml              # GitHub Actions CI（clang-tidy / 双编译器构建测试 / Valgrind / Docker）
├── .claude/
│   ├── commands/
│   │   ├── update-info.md      # 本项目专用的 /update-info 命令
│   │   ├── implement-feature.md # 新功能实现流水线
│   │   ├── test-features.md     # 功能测试流程
│   │   └── git/
│   │       ├── create-branch.md # /git:create-branch 命令
│   │       └── git-commit.md    # /git:git-commit 命令
│   └── settings.local.json     # 本地 Claude 权限等配置
├── .vscode/
│   └── c_cpp_properties.json   # VS Code C/C++ 配置（路径/标准）
├── docs/
│   ├── architecture-design.md  # 分层架构 + 管道模式设计
│   ├── plans/
│   │   ├── README.md           # 分阶段实现路线图（01～11）
│   │   ├── 01-core-backup-restore.md  # ✅ 已完成
│   │   ├── 02-special-files.md        # ✅ 已完成
│   │   ├── 03-metadata.md             # ✅ 已完成
│   │   ├── 04-filtering.md            # ✅ 已完成
│   │   ├── 05-packing.md              # ✅ 已完成（Tar）
│   │   ├── 06-compression.md
│   │   ├── 07-encryption.md
│   │   ├── 08-gui.md
│   │   ├── 09-scheduled-backup.md
│   │   ├── 10-realtime-backup.md
│   │   └── 11-network-backup.md
│   ├── reportDetails.md        # 报告提交要求
│   ├── requirements.md         # 需求规格说明
│   └── technology-selection.md # 技术选型论证
├── scripts/                    # 辅助脚本
│   ├── setup-testdata.sh       # 生成测试数据（data/source/）
│   └── test-backup-restore.sh  # 端到端备份/还原流程测试（交互式）
├── src/                        # 全部业务源码
│   ├── main.cpp                # 程序入口，初始化 CLI 并派发命令
│   ├── cli/
│   │   ├── commands.h          # CLI 命令类声明（backup/restore）
│   │   └── commands.cpp        # CLI 命令实现
│   ├── core/                   # ✅ 备份/还原引擎——中心调度逻辑
│   │   ├── backup_engine.h/cpp # 备份引擎（含筛选+打包支持）
│   │   ├── restore_engine.h/cpp# 还原引擎（含打包解包支持）
│   │   ├── types.h             # 核心数据类型（FileType/Metadata/FileEntry）
│   │   ├── error_code.h        # 错误码枚举（0x00~0x03 分类）
│   │   └── expected.h          # std::expected 替代（C++17 polyfill）
│   ├── fs/                     # ✅ 文件系统抽象层
│   │   ├── fs_abstraction.h/cpp# 文件读写、目录遍历、元数据、特殊文件
│   │   ├── metadata.h/cpp      # 元数据读取/恢复/JSON 序列化
│   │   ├── path_mapper.h/cpp   # 路径映射（相对/绝对转换）
│   │   ├── platform.h          # 平台检测（POSIX/Windows）
│   │   └── special_file.h/cpp  # 特殊文件检测与创建（symlink/FIFO/device）
│   ├── storage/                # ✅ 存储抽象层
│   │   ├── storage.h           # 存储接口（纯虚类）
│   │   └── local_storage.h/cpp # 本地文件系统实现
│   ├── filters/    ✅          # 自定义筛选（路径/类型/名称/时间/尺寸/用户）
│   ├── pack/       ✅          # 打包格式（自实现 Tar ustar + miniz Zip）
│   ├── compress/   🚧          # 压缩（zlib / zstd / liblzma 策略接口）
│   ├── crypto/     🚧          # 加密（AES / SM4 策略接口，基于 OpenSSL）
│   ├── gui/        🚧          # Qt 6 Widgets 图形界面
│   ├── watch/      🚧          # inotify 实时文件监控
│   └── network/    🚧          # gRPC 网络备份
├── tests/
├── scripts/                    # 辅助脚本（setup-testdata.sh, test-backup-restore.sh）
└── data/                       # 🧪 脚本生成的测试数据（.gitignore，不含于仓库）
    ├── source/                 # 源目录（setup-testdata.sh 生成）
    ├── backup/                 # 备份输出
    └── restore/                # 还原目标
```

> ✅ = 已完成；🚧 = 已规划但尚未实现的模块

## 实现路线

详见 [`docs/plans/README.md`](docs/plans/README.md)。

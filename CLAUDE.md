# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **自维护规则**：当发现代码、设计、或实践中存在本文件未覆盖或记载有误的内容时，应主动更新本文件以保持同步。

## 项目概述

数据备份软件——计算机组成与体系结构/软件工程课程项目，基于 C++17 跨平台（Linux / macOS / Windows）开发。支持目录树的备份与还原，以及特殊文件、元数据、压缩加密、GUI、实时/定时/网络备份等扩展功能。

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
- **依赖零系统化**：所有编译期依赖（CLI11、spdlog、Google Test、miniz、zlib/zstd/liblzma、OpenSSL、Qt、gRPC 等）一律通过 CMake 管理，**禁止依赖系统已安装的库**（不 `find_package` 系统库、不链接 `-l<syslib>`）。目标是 `git clone` 后只需 CMake + 编译器即可直接产出可执行软件，无需预先安装任何开发包。优先用 `FetchContent` 从源码拉取编译；**若该依赖有官方预编译产物（prebuilt binary）且平台/ABI 匹配，可直接拉取产物跳过编译以加速构建**（如 miniz、zlib 等纯库可通过 header-only 或预编译 .a/.so 引入）。性能分析（perf/gprof/gperftools）、内存检测（Valgrind）、lint（clang-tidy）等**非编译工具**不在此约束内，使用本机已安装的即可。
- **跨平台开发**：项目在 CI 中对 Linux（GCC/Clang）、macOS（AppleClang）、Windows（MSVC）三平台执行编译+测试。编写 POSIX 专用代码时使用 `#if BACKER_PLATFORM_POSIX` 保护（见 [`src/fs/platform.h`](src/fs/platform.h)），确保 Windows 编译不触及 POSIX 符号。平台抽象层的 POSIX 类型（如 `mode_t`）须在 Windows include paths 中有对应定义。
> **⚠ FetchContent 依赖自带测试陷阱**：有些库（如 zlib）通过自己的 CMakeLists.txt 注册了测试目标（`example`/`minigzip`），即使 `EXCLUDE_FROM_ALL` 阻止了默认编译，ctest 仍会发现已注册的测试并报告 "Not Run"（视作失败）。**拉取这类依赖时，务必在 `FetchContent_MakeAvailable` 前关闭其测试选项**，例如 `set(ZLIB_BUILD_TESTING OFF CACHE INTERNAL "" FORCE)`。此坑在 CI（ctest 执行全部注册测试）中出现，本地增量 `cmake --build` 则无感。
> **注意**：加密功能依赖系统 OpenSSL（通过 `find_package(OpenSSL REQUIRED)` 接入，不是 FetchContent）。构建前需安装 `libssl-dev`（Ubuntu）或 `openssl-devel`（Fedora）。

> **前置要求**（加密功能需要 OpenSSL）：`sudo apt install libssl-dev`

```bash
# 构建（CLI 版本，需要 OpenSSL）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 构建 GUI 版本（需要 Qt6：sudo apt install qt6-base-dev）
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/backer-gui                                    # 启动图形界面


# 测试（ctest 使用 Mock，无需外部数据）
ctest --test-dir build --output-on-failure            # 运行所有测试
./build/backer_test --gtest_filter="*BackupCore*"     # 指定测试

# 手动使用 CLI / 端到端（需要 setup-testdata.sh 生成真实数据）
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
| 语言 | C++17 (GCC 12+ / Clang 14+ / AppleClang 15+ / MSVC 19.44+) |
| 构建 | CMake 3.16+ (FetchContent 自动拉取依赖) |
| CLI 解析 | CLI11 v2.4.2 (header-only) |
| 日志 | spdlog v1.14.1 |
| 文件系统 | std::filesystem |
| 测试 | Google Test v1.15.2（单元测试） + 端到端脚本（集成测试） |
| 内存检测 | Valgrind（Linux 本地工具，不在 CI 中运行） |
| 性能分析 | perf（内核级采样）/ gprof（`-pg` 编译） |
| 容器 | Docker（multi-stage：gcc:13-bookworm → slim，GH_PROXY 构建参数支持国内加速）+ Compose |
| CI | GitHub Actions（Linux GCC+Clang / macOS AppleClang / Windows MSVC 三平台构建测试 + Docker 验证） |

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
| 定时备份 | ccronexpr + condition_variable (跨平台) |
| 实时备份 | inotify + 事件队列 |
| 网络备份 | gRPC + Protocol Buffers |
| 增量备份 | 文件哈希比较 |

## 测试体系

### 测试分层

| 层级 | 说明 | 工具/方式 |
|------|------|----------|
| 单元测试 | 对函数/类级别进行隔离测试 | Google Test (gtest) |
| 集成测试 | 验证模块间协作（备份→打包→存储、还原→解包→写入） | Mock 文件系统 + gtest `TEST_P` / `TEST_F` |
| 内存测试 | 检测内存泄漏、越界访问 | Valgrind (`--leak-check=full --show-leak-kinds=all`) |
| 性能测试 | 识别热点函数、评估时间复杂度 | perf（内核级采样）/ gprof（`-pg` 编译）/ gperftools（CPU + Heap Profiler） |

### 测试设计方法

**所有逻辑代码必须有对应测试用例**，未覆盖不允许提交。

| 方法 | 应用场景 | 示例 |
|------|----------|------|
| **等价类划分** | 输入空间较大时，选取代表性数据覆盖有效/无效等价类 | 文件尺寸筛选：0字节（有效）、1~1024B（有效）、>10MB（有效）、-1（无效） |
| **边界值分析** | 在等价类边界上取值，捕捉常见错误 | 文件大小 0、1、MAX；路径深度 0、1、1024；过滤器数量 0、1、MAX |
| **白盒测试** | 覆盖语句覆盖、分支覆盖、条件覆盖、路径覆盖 | 对 `if/else`、`switch`、循环边界编写显式用例 |

### 单元测试规范

- 使用 Google Test，每模块一个测试文件，按函数/类组织 `TEST_F`
- 覆盖边界场景（空/大文件、特殊路径）和异常路径（权限不足、磁盘满、路径不存在）
- 测试间隔离，临时目录在 `SetUp`/`TearDown` 中创建和清理
- 对管道接口（Compressor、Encryptor、Packer）使用参数化测试 `TEST_P` 遍历不同算法策略

### 集成测试规范

- 验证完整备份→存储流程：目录遍历 → 筛选 → 元数据采集 → Tar 打包 → 落盘
- 验证完整还原流程：Tar 解包 → 元数据恢复 → 文件写入 → 特殊文件重建
- **使用 mock 文件系统**（如 gtest::TempPath / FakeFileSystem）模拟输入输出，确保测试间隔离且可在无外部数据的 CI 环境中运行
- 端到端流程测试（Local/Docker 模式）由独立脚本 `bash scripts/test-backup-restore.sh` 负责

### 运行命令

详细测试命令见 [`.claude/commands/test-features.md`](.claude/commands/test-features.md)。

> **输出目录约定**：运行日志（Valgrind 等）放 `logs/`；分析产物（perf.data、perf.txt、gmon.out、gprof.txt 等）放 `output/`。两目录均 `.gitignore` 忽略。

简要参考：

```bash
# 运行全部单元测试（带详细输出）
ctest --test-dir build --output-on-failure

# 运行指定测试
./build/backer_test --gtest_filter="*BackupCore*"

# Valgrind 内存检测（必须通过，否则 CI 不合入）
mkdir -p logs output && valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
    ./build/backer_test 2>logs/valgrind.log

# perf 内核级 CPU 采样（无需重新编译）
perf record -g -o output/perf.data -- ./build/backer-cli
perf report --stdio -i output/perf.data > output/perf.txt

# gprof 性能分析（需 -pg 编译）
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_CXX_FLAGS="-pg"
cmake --build build
./build/backer-cli > /dev/null 2>&1 && mv gmon.out output/gmon.out
gprof ./build/backer-cli output/gmon.out > output/gprof.txt
```

### CI/CD 原则

- **全平台架构覆盖**：publish 流水线必须覆盖所有主流 OS×架构组合——Linux x86_64 + ARM64、macOS x86_64 + ARM64、Windows x86_64 + ARM64，任一缺少即阻断发布。
- **CLI+GUI 双产物**：每个平台架构组合都须同时产出 CLI 和 GUI 可执行文件（受限于交叉编译工具链的除外，须显式说明原因）。
- **运行时自包含**：所有运行时依赖（OpenSSL DLL/dylib、Qt 框架等）必须随包分发，确保用户下载解压即可直接运行，不得依赖系统预装库。

### CI 测试流水线（三平台构建测试 + Docker 验证，不含性能分析 / lint / Valgrind）

CI 包含以下 job，全部通过才可合入：

| Job | Runner | 内容 | 失败处理 |
|-----|--------|------|----------|
| `linux-build` | ubuntu-22.04 | GCC-12 + Clang-14 双编译器矩阵构建（CLI+GUI）+ 全量单元测试 | 阻断合入 |
| `linux-arm64-build` | ubuntu-24.04-arm64 | GCC 构建（CLI+GUI）+ 全量单元测试 | 阻断合入 |
| `macos-build` | macos-14 (ARM64) | AppleClang 构建（CLI+GUI）+ 全量单元测试 | 阻断合入 |
| `windows-build` | windows-2022 | MSVC 构建（CLI+GUI）+ 全量单元测试 | 阻断合入 |
| `windows-arm64-build` | windows-2022 (交叉) | MSVC ARM64 交叉编译构建验证（CLI+GUI，仅构建） | 阻断合入 |
| `docker` | ubuntu-22.04 | Docker multi-stage 构建验证 | 阻断合入 |

> **说明**：clang-tidy、Valgrind、gprof/perf 均为本地工具，不在 CI 中运行。
>
> **缓存策略**：`build/_deps` 及 `build/_deps/qt6_prebuilt` 目录通过 `actions/cache@v4` 跨运行缓存。缓存 key 包含 `runner.os` 标签防止跨平台缓存污染——macOS 不会错误恢复 Linux 构建的依赖缓存。

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
├── CMakeLists.txt              # 根构建配置（FetchContent 管理全部依赖）
├── Dockerfile                  # Multi-stage 构建（gcc:13-bookworm → slim）
├── docker-compose.yml          # Compose 编排
├── .github/workflows/          # CI: Linux/macOS/Windows 三平台 + Docker
├── .claude/commands/           # Claude 命令（实现/测试/更新/Git 等）
├── cmake/                      # CMake 模块（FetchQt6）
├── docs/
│   ├── architecture-design.md  # 分层架构 + 管道模式
│   ├── plans/                  # 分阶段实现路线图（01～11）
│   ├── requirements.md         # 需求规格说明
│   └── usage.md                # 功能使用说明
├── src/                        # 全部业务源码
│   ├── main.cpp                # CLI 入口
│   ├── cli/                    # ✅ CLI 命令（backup/restore/schedule/daemon）
│   ├── core/                   # ✅ 备份/还原引擎（中心调度）
│   ├── fs/                     # ✅ 文件系统抽象（含元数据 + 特殊文件）
│   ├── storage/                # ✅ 存储抽象层
│   ├── filters/                # ✅ 6 维度筛选器
│   ├── pack/                   # ✅ Tar ustar + Zip 打包
│   ├── compress/               # ✅ gzip/zstd/lzma 压缩
│   ├── crypto/                 # ✅ AES/SM4 加密（OpenSSL EVP）
│   ├── gui/                    # ✅ Qt 6 图形界面
│   ├── scheduler/              # ✅ 定时备份（ccronexpr）
│   ├── watch/      🚧          # inotify 实时监控
│   └── network/    🚧          # gRPC 网络备份
├── tests/                      # Google Test 单元测试
├── scripts/                    # 辅助脚本
└── data/                       # 🧪 测试数据（.gitignore）
```

> ✅ = 已完成；🚧 = 已规划但尚未实现的模块

## 实现路线

详见 [`docs/plans/README.md`](docs/plans/README.md)。

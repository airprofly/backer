<div align="center">

# 🛡️ Backer — 数据备份还原工具

[![GitHub](https://img.shields.io/badge/GitHub-Repository-black?logo=github)](https://github.com/airprofly/backer) [![Star](https://img.shields.io/github/stars/airprofly/backer?style=social)](https://github.com/airprofly/backer/stargazers) [![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++17](https://img.shields.io/badge/C++-17-00599C?logo=c%2B%2B)](https://isocpp.org/) [![CMake 3.16+](https://img.shields.io/badge/CMake-3.16+-064F8C?logo=cmake)](https://cmake.org/)
[![CI](https://github.com/airprofly/backer/actions/workflows/ci.yml/badge.svg)](https://github.com/airprofly/backer/actions/workflows/ci.yml)

数据备份 · C++17 · 压缩加密 · 实时备份 · 网络备份

</div>

---

## 📖 项目简介

Backer 是计算机组成与体系结构/软件工程课程项目，基于 **C++17** 在 **Linux** 平台开发，使用 **CLI11** 提供命令行接口，**spdlog** 处理日志，**Google Test** 驱动测试。

依赖通过 **CMake FetchContent** 自动拉取（CLI11 v2.4.2、spdlog v1.14.1、GTest v1.15.2），无需手动安装。

**当前状态**：第二阶段（特殊文件 + 元数据）已完成，支持符号链接、命名管道、设备文件的备份还原，以及属主/权限/时间戳元数据的完整保留。后续将通过管道架构扩展打包格式、压缩加密、GUI 界面、实时监控、定时调度及网络备份等功能。

## 🔧 构建与运行

**前置要求**：GCC 9+ / Clang 12+、CMake 3.16+

### Linux

```bash
# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 运行测试
ctest --test-dir build --output-on-failure

# CLI 使用
./build/backer-cli backup /path/to/source /path/to/backup
./build/backer-cli restore /path/to/backup /path/to/restore

# 仅运行特定测试
./build/backer_test --gtest_filter="*RestoreEngine*"
```

### MSYS2 (Windows)

```bash
# 构建（使用 UCRT64 环境）
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 运行测试
./build/backer_test.exe

# CLI 使用
./build/backer-cli.exe backup /path/to/source /path/to/backup
./build/backer-cli.exe restore /path/to/backup /path/to/restore
```

> **注意**：MSYS2 环境下部分特殊文件功能（FIFO、设备文件）不可用，符号链接需管理员权限或开发者模式。元数据功能中权限保留正常，属主/组还原需在 Linux 下以 root 运行。

### Docker 构建

```bash
# 构建镜像（multi-stage 构建，最终镜像约 100MB）
docker build -t backer .

# 使用 Compose（含数据卷挂载）
docker compose up -d

# 交互式运行
docker run --rm backer --help
```

`docker-compose.yml` 默认挂载三个数据卷：
- `./data/source:/data/source:ro` — 待备份源目录
- `./data/backup:/data/backup` — 备份输出目录
- `./data/restore:/data/restore` — 还原目标目录

## 📌 规划特性

- ✅ **核心备份/还原** — 目录树递归扫描，文件内容读写，备份索引
- ✅ **特殊文件处理** — 符号链接、命名管道 (FIFO)、块/字符设备、Socket 完整备份与还原
- ✅ **元数据保留** — 属主 (uid/gid)、权限 (含 setuid/setgid/sticky)、时间戳 (atime/mtime, 纳秒精度)
- 🔲 **灵活筛选** — 支持自定义包含/排除规则、正则表达式过滤
- 🔲 **打包格式** — 自实现 Tar 格式，可选 Zip (miniz) 打包
- 🔲 **压缩算法** — 支持 gzip / zstd / lzma 多级压缩
- 🔲 **加密保护** — AES / SM4 (OpenSSL) 加密备份数据
- 🔲 **图形界面** — Qt 6 桌面 GUI
- 🔲 **实时监控** — inotify 文件变更实时备份
- 🔲 **定时任务** — timerfd + cron 表达式灵活调度
- 🔲 **网络备份** — gRPC + Protocol Buffers 远程备份

## 📁 项目结构

```text
backer/
├── CMakeLists.txt            # 根构建配置（FetchContent 自动拉取依赖）
├── Dockerfile                # Multi-stage Docker 构建
├── docker-compose.yml        # Compose 编排
├── .dockerignore             # 构建上下文精简
├── .github/
│   └── workflows/ci.yml      # CI: lint → 双编译器构建测试 → Valgrind → Docker
├── .claude/                  # Claude Code AI 辅助配置
│   └── commands/             # 斜杠命令（/update-info, /git:*）
├── CLAUDE.md                 # AI 辅助开发指南
├── LICENSE                   # Apache 2.0
├── docs/
│   ├── architecture-design.md     # 分层架构 + 管道模式
│   ├── requirements.md            # 需求规格说明
│   ├── technology-selection.md    # 技术选型记录
│   ├── reportDetails.md           # 课程报告详情
│   └── plans/                     # 分阶段实现计划（01~11）
│       └── README.md
├── src/                      # 🔧 源代码
│   ├── main.cpp              # 程序入口（CLI 初始化 + 命令派发）
│   ├── cli/                  # ✅ 命令行接口 (CLI11)
│   ├── core/                 # ✅ 备份/还原引擎（含特殊文件+元数据）
│   ├── fs/                   # ✅ 文件系统抽象层（含元数据+特殊文件）
│   ├── storage/              # ✅ 存储抽象层（本地文件系统）
│   ├── filters/    🔲       # 筛选器
│   ├── pack/       🔲       # 打包模块
│   ├── compress/   🔲       # 压缩模块
│   ├── crypto/     🔲       # 加密模块
│   ├── gui/        🔲       # Qt 6 图形界面
│   ├── watch/      🔲       # inotify 实时监控
│   └── network/    🔲       # gRPC 网络备份
├── tests/                    # 📝 Google Test 单元测试
│   ├── core/
│   │   ├── backup_engine_test.cpp
│   │   └── restore_engine_test.cpp
│   └── fs/
│       ├── metadata_test.cpp        # 元数据读写/恢复/序列化
│       └── special_file_test.cpp    # 特殊文件检测/创建
└── testdata/                 # 🧪 测试数据（按场景分类）
    ├── text/                 # 常规文本（hello.txt / empty.txt / large.txt）
    ├── filter/               # 筛选测试（data.bin / debug.log / tmp/）
    ├── meta/                 # 元数据测试（executable.sh / private.key）
    ├── naming/               # 特殊命名（.hidden / 空格 / 中文）
    ├── nested/               # 深层嵌套（a/b/c/leaf.txt）
    └── special/              # 特殊文件占位（link.txt / placeholder.txt）
```

> ✅ = 已完成  🔲 = 待实施

## 🏗️ 架构设计

详见 [架构设计文档](docs/architecture-design.md)。

系统采用**五层分层架构**（表现层 → 业务层 → 管道层 → 基础设施层 → 存储层），管道层通过策略接口支持可插拔的压缩/加密算法。

## ⚠️ 开发规范

### 分支管理

- **`main` 分支为保护分支**，禁止直接在上面开发
- 所有开发在**特性分支**上进行，分支命名规则：
  - `feature/<功能名>` — 新功能
  - `fix/<bug描述>` — 缺陷修复
  - `docs/<文档名>` — 文档变更
  - `refactor/<模块名>` — 重构
- **禁止直接 `push` 或 `merge` 到 `main`**，所有变更必须通过 **Pull Request** 合入

### 提交流程

> 提示：以下流程可配合 Claude 命令（`/` 斜杠命令）自动完成部分步骤，详见 [`.claude/commands/`](.claude/commands/)。

```bash
# 1. 用 Claude 创建分支（按规范命名，支持所有分支类型）
/git:create-branch
# → 交互式引导：选择分支类型（feat/fix/docs/refactor...）、输入需求描述、自动生成分支名
# → 效果：feature/airprofly_01-core-backup-restore

# 2. 暂存所有变更
git add .

# 3. 使用 Claude 审查并优化已暂存的变更
/simplify
# → 自动审查变更并应用简化/复用/优化建议

# 4. 将 /simplify 的改动重新暂存，然后生成提交信息
git add .
/git:git-commit
# → 基于 git diff --staged 自动生成 Conventional Commits 格式信息

# 5. 推送到远程 (非 main 分支)
git push origin <分支名>

# 6. 在 GitHub 上创建 Pull Request → 待 review 通过后合入 main（合并时务必选择 **Squash and merge**，不要使用 Create a merge commit）
```

### CI/CD

项目配置了 [GitHub Actions CI](.github/workflows/ci.yml)，每次推送/PR 自动运行：

| 阶段 | 内容 |
|------|------|
| `lint` | cpplint 代码风格 + clang-tidy 静态分析 |
| `build-and-test` | GCC 12 / Clang 14 双编译器矩阵构建 + Google Test |
| `memcheck` | Valgrind 内存泄漏检测 |
| `docker` | Docker 镜像构建验证 |

## 📄 许可证

本项目基于 Apache License 2.0 开源 — 详见 [LICENSE](LICENSE)。

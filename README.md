<div align="center">

# 🛡️ Backer — 数据备份还原工具

[![GitHub](https://img.shields.io/badge/GitHub-Repository-black?logo=github)](https://github.com/airprofly/backer) [![Star](https://img.shields.io/github/stars/airprofly/backer?style=social)](https://github.com/airprofly/backer/stargazers) [![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++17](https://img.shields.io/badge/C++-17-00599C?logo=c%2B%2B)](https://isocpp.org/) [![CMake 3.16+](https://img.shields.io/badge/CMake-3.16+-064F8C?logo=cmake)](https://cmake.org/)
[![CI](https://github.com/airprofly/backer/actions/workflows/ci.yml/badge.svg)](https://github.com/airprofly/backer/actions/workflows/ci.yml)

数据备份 · C++17 · 压缩加密 · 实时备份 · 网络备份

</div>

---

## 📖 项目简介

Backer 是一款 **Linux 平台的数据备份还原工具**，采用 **C++17** 开发，支持目录树备份与还原、特殊文件处理、元数据保留、灵活筛选、多格式打包、多算法压缩加密、图形界面与定时备份等完整功能。

## 📁 项目结构

```text
backer/
├── CMakeLists.txt            # 根构建配置（FetchContent 自动拉取依赖）
├── Dockerfile                # Multi-stage Docker 构建
├── docker-compose.yml        # Compose 编排
├── .dockerignore             # 构建上下文精简
├── .github/                  # CI: workflows/ci.yml（lint → 构建测试 → Valgrind → Docker）
├── .claude/                  # Claude Code AI 辅助配置
├── cmake/                    # CMake 模块（FetchQt6.cmake）
├── .vscode/                  # VS Code 编辑器配置
├── CLAUDE.md                 # AI 辅助开发指南
├── LICENSE                   # Apache 2.0
├── docs/
│   ├── architecture-design.md     # 分层架构 + 管道模式
│   ├── requirements.md            # 需求规格说明
│   ├── technology-selection.md    # 技术选型记录
│   ├── reportDetails.md           # 课程报告详情
│   └── plans/                     # 分阶段实现计划
├── src/                      # 源代码
│   ├── main.cpp              # 程序入口
│   ├── cli/                  # 命令行接口 (CLI11)
│   ├── core/                 # 备份/还原引擎
│   ├── fs/                   # 文件系统抽象层
│   ├── storage/              # 存储抽象层
│   ├── filters/              # 筛选器（6 维度）
│   ├── pack/                 # 打包模块（Tar/Zip）
│   ├── compress/             # 压缩模块（gzip/zstd/lzma）
│   ├── crypto/               # 加密模块（AES/SM4）
│   ├── gui/                  # Qt 6 图形界面
│   ├── scheduler/            # 定时备份
│   ├── watch/                # 实时监控（规划中）
│   └── network/              # 网络备份（规划中）
├── tests/                    # Google Test 单元测试
├── scripts/                  # 辅助脚本
├── logs/                     # 测试日志输出（.gitignore）
└── data/                     # 测试数据（.gitignore）
```

## 🔧 快速开始

**前置要求**：GCC 9+ / Clang 12+、CMake 3.16+

```bash
sudo apt install libssl-dev           # 加密功能依赖 OpenSSL

# 构建（CLI 版本）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 构建 GUI 版本（需要 Qt6：sudo apt install qt6-base-dev）
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 测试（日志输出到 logs/）
ctest --test-dir build --output-on-failure                     &>logs/ctest.log
./build/backer_test                                          &>logs/gtest-all.log
valgrind --leak-check=full ./build/backer_test --gtest_filter="*" &>logs/valgrind.log
perf record -g -o logs/perf.data ./build/backer-cli backup data/source data/backup
perf report -i logs/perf.data --stdio >logs/perf.txt

# CLI 使用（完整命令参考见 [使用文档](docs/usage.md)）
./build/backer-cli --help
```

### Docker

```bash
docker build -t backer .
```

## 📌 规划特性

- ✅ **核心备份/还原** — 目录树递归扫描，文件内容读写，备份索引
- ✅ **特殊文件处理** — 符号链接、命名管道 (FIFO)、块/字符设备、Socket 完整备份与还原
- ✅ **元数据保留** — 属主 (uid/gid)、权限 (含 setuid/setgid/sticky)、时间戳 (atime/mtime, 纳秒精度)
- ✅ **灵活筛选** — 支持自定义包含/排除规则（路径/类型/名称/时间/尺寸/属主 6 维度）
- ✅ **打包格式** — 自实现 Tar 格式，可选 Zip (miniz) 打包
- ✅ **压缩算法** — 支持 gzip / zstd / lzma 多级压缩（归档后压缩，还原前解压）
- ✅ **加密保护** — AES-256-GCM / SM4-CBC (OpenSSL EVP) 加密备份数据
- ✅ **图形界面** — Qt 6 桌面 GUI，macOS 简约风格
- 🔲 **实时监控** — inotify 文件变更实时备份
- ✅ **定时备份** — Cron 表达式调度 + 快照数据淘汰
- 🔲 **网络备份** — gRPC + Protocol Buffers 远程备份

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

## 📄 许可证

本项目基于 Apache License 2.0 开源 — 详见 [LICENSE](LICENSE)。

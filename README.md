<div align="center">

# 🛡️ Backer — 数据备份还原工具

[![GitHub](https://img.shields.io/badge/GitHub-Repository-black?logo=github)](https://github.com/airprofly/backer) [![Star](https://img.shields.io/github/stars/airprofly/backer?style=social)](https://github.com/airprofly/backer/stargazers) [![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++17](https://img.shields.io/badge/C++-17-00599C?logo=c%2B%2B)](https://isocpp.org/) [![CMake 3.16+](https://img.shields.io/badge/CMake-3.16+-064F8C?logo=cmake)](https://cmake.org/)

数据备份 · C++17 · 压缩加密 · 实时备份 · 网络备份

</div>

---

## 📖 项目简介

Backer 是计算机组成与体系结构/软件工程课程项目，基于 **C++17** 在 **Linux** 平台开发。支持目录树的完整备份与还原，并通过管道架构扩展了压缩加密、GUI 界面、实时监控、定时调度及网络备份等功能。

## 📌 规划特性

- 🔲 **核心备份/还原** — 目录树递归扫描，支持增量/差异备份
- 🔲 **特殊文件处理** — 软硬链接、管道、设备文件等无损重建
- 🔲 **元数据保留** — 权限、时间戳、ACL、扩展属性完整保存
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
├── CMakeLists.txt         # 根构建配置
├── Dockerfile             # 多阶段 Docker 构建
├── docker-compose.yml     # 容器编排
├── CLAUDE.md              # AI 辅助开发指南
├── docs/
│   ├── architecture-design.md  # 架构设计文档
│   ├── requirements.md         # 需求规格说明
│   ├── technology-selection.md # 技术选型记录
│   ├── reportDetails.md        # 课程报告详情
│   └── plans/                  # 分阶段实现计划
│       └── README.md           # [计划概览](docs/plans/README.md)
├── src/                   # 🔧 源代码
│   ├── core/              # 备份/还原引擎
│   ├── fs/                # 文件系统抽象层
│   ├── filters/           # 筛选器实现
│   ├── pack/              # 打包模块 (Tar/Zip)
│   ├── compress/          # 压缩模块
│   ├── crypto/            # 加密模块
│   ├── gui/               # Qt 6 图形界面
│   ├── watch/             # inotify 实时监控
│   └── network/           # gRPC 网络备份
└── tests/                 # 📝 Google Test 单元测试
```

## 🔧 构建与运行

### 方式一：本地构建

**前置要求**：GCC 9+ / Clang 12+、CMake 3.16+

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build
```

### 方式二：Docker 构建

依赖已包含在镜像中，无需在宿主机安装：

```bash
docker compose build
```

## 🏗️ 架构设计

详见 [架构设计文档](docs/architecture-design.md)。

## ⚠️ 提交规范

提交前必须执行以下步骤，**禁止直接 `git commit`**：

```bash
# 1. 使用 Claude /simplify 命令审查并优化变更
# 2. 暂存所有改动
git add .
# 3. 使用 Claude 生成 commit 信息并提交（通过 /git:git-commit 命令）
```

## 📄 许可证

本项目基于 Apache License 2.0 开源 — 详见 [LICENSE](LICENSE)。

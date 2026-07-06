# 技术选型方案 — 数据备份软件

## 一、编程语言与编译器

| 项目 | 选型 | 理由 |
|------|------|------|
| 语言标准 | **C++17** | `std::filesystem` 原生支持（无需额外库）；`std::optional`、`std::variant` 适合状态建模；比 C++20 编译器支持更稳定 |
| 编译器 | **GCC 9+ / Clang 12+** | Linux 默认工具链，与课程工具（gdb/gprof/valgrind）兼容良好 |
| 构建系统 | **CMake 3.16+** | 跨平台、生态成熟、自动依赖管理；配合 `CMakePresets` 统一各成员构建配置 |

## 二、核心库选型

| 功能域 | 方案 | 说明 |
|--------|------|------|
| 文件系统操作 | **`std::filesystem`** + **`<sys/stat.h>`**（C++17 标准库） | 目录遍历、路径操作、文件类型检测均覆盖；特殊文件通过系统调用补充 |
| 命令行解析 | **CLI11**（单头文件库） | 轻量、header-only，支持子命令（backup/restore） |
| 日志 | **spdlog** | 高性能、header-only 可选、支持多级别/异步/文件回滚 |
| 序列化/存档 | **自定义二进制格式** | 备份格式体积小、解析高效；配合 packing 模块统一管理 |
| 单元测试 | **Google Test** | 课程指定，CMake 集成成熟，支持 mock |
| 代码检测 | **cpplint + clang-tidy** | 课程指定 cpplint；clang-tidy 增强静态分析 |
| 性能分析 | **Valgrind + gprof + perf** | 课程指定工具链 |

## 三、扩展功能选型矩阵

| 扩展功能 | 推荐技术 | 备选方案 | 评分 |
|----------|----------|----------|------|
| **特殊文件支持** | `lstat()` + `mknod()` / `mkfifo()` / `readlink()` | — | 10分 |
| **元数据支持** | `fchown()`、`fchmod()`、`utimensat()`、ACL（`libacl`） | — | 10分 |
| **自定义筛选** | `glob` pattern + `std::regex` + 时间/尺寸谓词链 | — | 各3分 |
| **打包（Tar）** | 自实现 Tar 格式 | `libarchive`（太重量级） | 10分 |
| **打包（Zip）** | `miniz`（单文件库） | `libzip` | 10分 |
| **压缩（gzip）** | **zlib**（`libz`） | — | 10分 |
| **压缩（zstd）** | **zstd**（`libzstd`） | — | 10分 |
| **压缩（LZMA）** | **liblzma**（XZ Utils） | — | 10分 |
| **加密（AES）** | **OpenSSL**（`libcrypto`） | `libsodium` | 10分 |
| **加密（SM4）** | OpenSSL 3.x（国密支持） | `libsodium` | 10分 |
| **图形界面** | **Qt 6**（Widget + CMake） | GTKmm、FLTK | 10分 |
| **定时备份** | `timerfd` + cron 表达式解析库（`ccronexpr`） | `std::chrono` 轮询 | 10分 |
| **实时备份** | **`inotify`** + 事件队列 | `fanotify`（需 root） | 15分 |
| **网络备份** | **gRPC**（Protocol Buffers） | Boost.Asio、REST | 10+分 |
| 用户管理 | JWT token + SQLite（via `sqlite3`） | — | 5分 |
| 传输加密 | **TLS**（OpenSSL + gRPC 自带） | — | 5分 |
| 增量备份 | **rsync 算法** / 文件哈希对比 | — | 5分 |

## 四、开发工具与环境

| 工具 | 选型 | 用途 |
|------|------|------|
| IDE | **VSCode** + C++ 扩展 | 课程指定，配合 CMake 插件 |
| 版本控制 | **Git** + GitHub | 课程指定 |
| 调试器 | **gdb** + `gdbgui`（可选） | 课程指定 |
| 内存检测 | **Valgrind**（memcheck） | 课程指定 |
| 性能 Profiling | **gprof** + **perf** | 课程指定 |
| 代码检测 | **cpplint** + **clang-tidy** | 课程指定 + 增强 |
| 持续集成 | **GitHub Actions** | 自动构建 + 运行测试 |

## 五、Docker 容器化部署

| 项目 | 选型 | 理由 |
|------|------|------|
| 容器引擎 | **Docker** | 提交要求，统一组员开发环境，消除环境差异 |
| 构建策略 | **多阶段构建（Multi-stage Build）** | 构建环境和运行环境分离，大幅减小最终镜像体积 |
| 编排工具 | **Docker Compose** | 网络备份模式下管理多容器（server + DB），一键启动演示 |
| CI 集成 | **GitHub Actions + Docker** | 每次提交自动构建镜像并运行集成测试 |

## 六、软件架构选型

采用分层架构 + 模块化设计，详见 [`docs/architecture-design.md`](architecture-design.md)。

## 七、建议实现路线

详见 [`docs/plans/README.md`](plans/README.md)。

## 八、验证方式

- **单元测试**：Google Test 覆盖各模块核心逻辑
- **集成测试**：备份→删除原始文件→还原→diff 对比
- **压力测试**：大目录树（10 万+ 文件）性能基准
- **内存检测**：Valgrind 确认无泄漏
- **Docker 验证**：`docker build .` 构建通过 + `docker compose up` 服务正常启动
- **CI**：GitHub Actions 每次 PR 自动构建 + cpplint + 测试 + Docker 构建

## 关键文件

- `CMakeLists.txt` — 根构建配置
- `Dockerfile` — 多阶段构建
- `.dockerignore` — Docker 构建上下文过滤
- `docker-compose.yml` — 容器编排（网络备份模式）
- `src/core/` — 备份/还原引擎
- `src/filters/` — 自定义筛选
- `src/pack/` — 打包解包
- `src/compress/` — 压缩解压
- `src/crypto/` — 加密解密
- `src/fs/` — 文件系统抽象（含特殊文件/元数据）
- `src/gui/` — Qt 图形界面
- `src/watch/` — 实时监控（inotify）
- `src/network/` — 网络备份（gRPC）
- `tests/` — Google Test 用例

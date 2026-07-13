# Backer Linux 环境配置与编译运行指南

> **适用平台**：Ubuntu 22.04 / 24.04（其他 Linux 发行版类似）
> **最后验证**：2026-07-13

---

## 目录

1. [系统要求](#1-系统要求)
2. [一键安装依赖](#2-一键安装依赖)
3. [编译项目](#3-编译项目)
4. [运行 CLI](#4-运行-cli)
5. [运行 GUI](#5-运行-gui)
6. [运行测试](#6-运行测试)
7. [常见问题排查](#7-常见问题排查)
8. [Docker 构建](#8-docker-构建)

---

## 1. 系统要求

| 组件 | 最低版本 | 推荐版本 |
|------|---------|---------|
| 编译器 | GCC 9+ / Clang 12+ | GCC 12+ / Clang 14+ |
| CMake | 3.16+ | 3.28+ |
| OpenSSL | 1.1.1+ (开发库) | 3.0.x (开发库) |
| Python (GUI 自动下载用) | 3.8+ | 3.12+ |
| 操作系统 | Linux x86_64 | Ubuntu 24.04 |

> **注意**：所有编译期依赖（CLI11、spdlog、Google Test、miniz、zlib/zstd/liblzma、Qt6）均通过 CMake FetchContent 自动拉取，**无需手动安装**。
>
> 只有 OpenSSL 需要系统安装开发库（用于加密功能）。

---

## 2. 一键安装依赖

### 2.1 基础编译工具

```bash
sudo apt update
sudo apt install -y cmake g++ build-essential
```

验证安装：

```bash
cmake --version   # 需要 ≥ 3.16
g++ --version     # 需要 ≥ 9
```

### 2.2 OpenSSL（加密功能必需）

```bash
sudo apt install -y libssl-dev
```

验证安装：

```bash
# 检查是否能被 CMake 找到
dpkg -l libssl-dev
```

> 如果不需要加密功能，可以跳过这一步。CMake 会自动检测并禁用加密相关代码。

### 2.3 Python3（GUI 自动下载 Qt6 用）

```bash
# 一般系统自带，验证即可
python3 --version   # 需要 ≥ 3.8
```

> **无需安装 Qt6 系统包！** 项目通过 `cmake/FetchQt6.cmake` 使用 `aqtinstall` 自动下载 Qt6 6.5.0 预编译二进制到构建目录。只需确保 Python3 可用即可。

### 2.4 OpenGL 运行时库（GUI 运行必需）

在 **Ubuntu 桌面版** 上一般已自带：

```bash
# 如果缺失，安装运行时库
sudo apt install -y libgl1 mesa-utils
```

在 **WSL2 (WSLg)** 上一般自带。在其他无桌面环境（如服务器）上运行 GUI 需要 Xvfb：

```bash
sudo apt install -y xvfb
```

---

## 3. 编译项目

### 3.1 快速开始（推荐）

项目根目录下的 `scripts/setup.sh` 可一键完成配置和编译：

```bash
cd ~/projects/backer
bash scripts/setup.sh
```

### 3.2 手动分步编译

#### a) 仅编译 CLI（最快，无需 Qt6/OpenGL）

```bash
cd ~/projects/backer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

产物：`./build/backer-cli`

#### b) 编译 CLI + GUI（完整功能）

```bash
cd ~/projects/backer
cmake -B build -DBUILD_GUI=ON \
    -DOpenGL_GL_PREFERENCE=LEGACY \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

> **为什么需要 `-DOpenGL_GL_PREFERENCE=LEGACY`？**
> Qt6 的 FindWrapOpenGL 模块需要 `OpenGL::GL` target，但 CMake ≥ 3.11 默认使用 GLVND 模式创建 `OpenGL::OpenGL` 而非 `OpenGL::GL`。设置 LEGACY 模式可创建正确的 target。若不加此参数，cmake 会在 generate 步骤报错退出。（详见[常见问题 7.7](#77-gui-编译时-opengl-相关报错)）

产物：
- `./build/backer-cli` — 命令行工具
- `./build/backer-gui` — 图形界面
- `./build/backer_test` — 单元测试

> **第一次编译会很慢**（约 2-3 分钟），因为要下载并编译所有依赖（spdlog、zlib、zstd、lzma、Qt6 等）。后续增量编译很快。

### 3.3 编译参数说明

| CMake 选项 | 默认值 | 说明 |
|-----------|-------|------|
| `-DCMAKE_BUILD_TYPE=Release` | Debug | Release 开启优化 |
| `-DBUILD_GUI=ON` | OFF | 启用 GUI 构建（自动下载 Qt6） |
| `-DBUILD_TESTING=ON` | ON | 构建测试 |
| `-DQT6_AUTO_DOWNLOAD=ON` | ON | 自动下载 Qt6 预编译包 |

### 3.4 增量编译

修改源码后，只需：

```bash
cmake --build build -j$(nproc)
```

CMake 会自动检测变更并只编译修改的文件。

> **不要 `rm -rf build` 全量重编！** 若遇到 FetchContent 缓存问题，只清理具体依赖：`rm -rf build/_deps/spdlog*`

---

## 4. 运行 CLI

### 4.1 查看帮助

```bash
./build/backer-cli --help
```

输出：

```
Backer — Data backup and restore tool
Usage: ./build/backer-cli [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit
  --version                   Show version information

Subcommands:
  backup                      Back up a directory tree
  restore                     Restore a directory tree from backup
  schedule                    Manage scheduled backup jobs
  daemon                      Run the backup scheduler daemon
```

### 4.2 生成测试数据

```bash
bash scripts/setup-testdata.sh
```

这会在 `data/source/` 下创建包含各类文件的测试目录。

### 4.3 备份与还原

```bash
# 备份
./build/backer-cli backup data/source data/backup

# 带 Tar 打包 + gzip 压缩
./build/backer-cli backup data/source data/backup2 --pack tar --compress gzip

# 还原
./build/backer-cli restore data/backup data/restore

# 端到端流程测试
bash scripts/test-backup-restore.sh
```

### 4.4 高级用法

```bash
# 加密备份（需要 libssl-dev）
./build/backer-cli backup data/source data/enc-backup --cipher AES256  # 会提示输入密码

# 定时备份
./build/backer-cli schedule add --name mybackup --cron "0 2 * * *" \
    --source /data/source --dest /data/backup

# 运行定时守护进程
./build/backer-cli daemon
```

---

## 5. 运行 GUI

### 5.1 使用启动脚本（推荐）

```bash
bash scripts/run-gui.sh
```

脚本会自动设置 OpenGL 运行时库路径并启动 GUI。

### 5.2 手动启动

```bash
export LD_LIBRARY_PATH="$HOME/.local/backer-deps/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"
./build/backer-gui
```

> 如果 CMake 的 FetchQt6 自动下载了 OpenGL 库，路径可能不同，这时直接用 `./build/backer-gui` 即可。

### 5.3 无桌面环境运行（SSH / 服务器）

```bash
# 安装虚拟显示
sudo apt install -y xvfb

# 启动虚拟显示
Xvfb :99 -screen 0 1024x768x24 &
export DISPLAY=:99

# 运行 GUI
./build/backer-gui

# 或使用 xvfb-run
xvfb-run ./build/backer-gui
```

### 5.4 GUI 界面说明

启动后可见 4 个标签页：

| 标签页 | 功能 |
|-------|------|
| **备份** | 选择源/目标目录，配置打包/压缩/加密/筛选，执行备份 |
| **还原** | 选择备份源，还原到指定目录 |
| **定时** | 管理 Cron 定时备份任务 |
| **设置** | 配置默认压缩算法、日志级别等 |

详细的 GUI 功能验证见 [`docs/usage-gui-verify.md`](usage-gui-verify.md)。

---

## 6. 运行测试

### 6.1 运行全部测试

```bash
ctest --test-dir build --output-on-failure
```

### 6.2 运行指定测试

```bash
# 按名称筛选
./build/backer_test --gtest_filter="*BackupCore*"

# 运行还原相关测试
./build/backer_test --gtest_filter="*RestoreEngine*"
```

### 6.3 内存检测（Valgrind）

```bash
sudo apt install -y valgrind
mkdir -p logs output
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
    ./build/backer_test 2>logs/valgrind.log
```

---

## 7. 常见问题排查

### 7.1 CMake 找不到 Qt6

**现象**：`FetchQt6: Qt6 not found, attempting auto-download...` 之后报错。

**原因**：自动下载需要 Python3 + pip。

**解决**：
```bash
# 确保 Python3 和 pip 可用
python3 -m pip install --user --break-system-packages aqtinstall
# 或手动指定 Qt6 路径
cmake -B build -DBUILD_GUI=ON -DQt6_DIR=/path/to/qt6
```

### 7.2 编译时报 "OpenSSL NOT found"

**现象**：CMake 输出 `Could NOT find OpenSSL`。

**解决**：
```bash
sudo apt install -y libssl-dev
```

### 7.3 GUI 启动时报 "cannot connect to X server"

**现象**：`qt.qpa.xcb: could not connect to display :0`

**原因**：没有图形桌面环境/没有设置 DISPLAY。

**解决**：
- 在桌面 Linux 上：确保 `echo $DISPLAY` 输出 `:0` 或 `:1`
- 在 WSL2 上：使用 WSLg（Windows 11 自带）或安装 VcXsrv
- 在 SSH/服务器上：使用 Xvfb（见 5.3 节）

### 7.4 GUI 启动时报 EGL/OpenGL 错误

**现象**：`libEGL warning: ...`，但 GUI 仍可运行。

**原因**：缺少硬件 3D 加速（WSL2 或虚拟机中常见）。Qt 会回退到软件渲染，不影响功能。

**解决**：忽略或设置环境变量：
```bash
export LIBGL_ALWAYS_SOFTWARE=1
./build/backer-gui
```

### 7.5 增量编译后测试链接失败

**现象**：链接阶段报 undefined reference。

**解决**：只需重新运行 cmake 配置（不需要删 build）：
```bash
cmake -B build
cmake --build build -j$(nproc)
```

### 7.7 GUI 编译时 OpenGL 相关报错

**现象**：
```
Imported target "Qt6::Widgets" includes non-existent path
".../opengl_local/usr/include" in its INTERFACE_INCLUDE_DIRECTORIES
```

或：

```
The link interface of target "WrapOpenGL::WrapOpenGL" contains:
  OpenGL::GL
but the target was not found.
```

**原因**：CMake ≥ 3.11 默认使用 GLVND 模式处理 OpenGL，创建 `OpenGL::OpenGL` target。但 Qt6 的 `FindWrapOpenGL.cmake` 期望的是 LEGACY 模式的 `OpenGL::GL` target。此外，OpenGL 开发头文件未安装时也会导致此问题。

**解决**：

方法一（推荐）：在 cmake 配置时加上 LEGACY 模式：
```bash
cmake -B build -DBUILD_GUI=ON \
    -DOpenGL_GL_PREFERENCE=LEGACY \
    -DCMAKE_BUILD_TYPE=Release
```

方法二：安装系统 OpenGL 开发库：
```bash
sudo apt install -y libgl1-mesa-dev libgl-dev libglx-dev libopengl-dev
```

方法三：使用 `scripts/setup.sh` 一键脚本（它会自动处理 OpenGL 下载和路径设置）：
```bash
bash scripts/setup.sh
```

### 7.8 FetchContent 依赖下载失败（网络问题）

**现象**：`FetchContent_Populate` 超时。

**解决**：使用代理或手动下载：
```bash
# 为 GitHub 请求设置代理
export https_proxy=http://your-proxy:port
cmake -B build ...
```

国内用户可在 Docker 构建时使用：
```bash
docker build --build-arg GH_PROXY=https://ghproxy.net/ -t backer .
```

---

## 8. Docker 构建

适合不想在宿主机装依赖的用户：

```bash
# 生成测试数据
bash scripts/setup-testdata.sh

# 构建镜像
docker build -t backer .

# 使用 Compose 备份/还原
docker compose run --rm backer backup /data/source /data/backup
docker compose run --rm backer restore /data/backup /data/restore

# 查看结果
ls -la data/backup

# 交互式进入容器
docker run --rm -it --entrypoint /bin/bash backer
```

> Docker 镜像仅包含 CLI 版本（无 GUI）。

---

## 快速参考命令

```bash
# ──────────────────────────────────────────
# 从零开始的完整流程（Ubuntu 24.04）
# ──────────────────────────────────────────

# 1. 安装基础依赖
sudo apt update
sudo apt install -y cmake g++ libssl-dev python3

# 2. 克隆（如果还没有）
git clone https://github.com/airprofly/backer.git
cd backer

# 3. 编译（CLI 版本）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 4. 编译（GUI 版本，可选）
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 5. 生成测试数据
bash scripts/setup-testdata.sh

# 6. 运行
./build/backer-cli backup data/source data/backup
./build/backer-cli restore data/backup data/restore

# 7. 运行测试
ctest --test-dir build --output-on-failure

# 8. 启动 GUI
bash scripts/run-gui.sh
```

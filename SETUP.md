# Backer — Linux 从零搭建指南

> **目标**：你刚刚 `git clone` 了这个仓库，在 Linux 上从零开始配置环境、编译、运行全部功能。
> **适用平台**：Ubuntu 22.04 / 24.04（其他发行版请自行替换包管理器命令）
> **验证日期**：2026-07-13

---

## 目录

- [第一步：安装基础环境](#第一步安装基础环境)
- [第二步：编译项目](#第二步编译项目)
- [第三步：运行命令行工具（CLI）](#第三步运行命令行工具cli)
- [第四步：运行图形界面（GUI）](#第四步运行图形界面gui)
- [第五步：运行测试](#第五步运行测试)
- [常见问题](#常见问题)
- [完整流程速查](#完整流程速查)

---

## 第一步：安装基础环境

打开终端，一条一条执行下面的命令。

### 1.1 编译工具

```bash
sudo apt update
sudo apt install -y cmake g++
```

验证：

```bash
cmake --version   # 输出类似 cmake version 3.28.3
g++ --version     # 输出类似 g++ (Ubuntu 13.3.0) 13.3.0
```

> 最低要求：cmake ≥ 3.16，g++ ≥ 9。推荐 g++ ≥ 12。

### 1.2 OpenSSL（加密功能用）

```bash
sudo apt install -y libssl-dev
```

> 如果不做加密备份，可以跳过这一步。编译会自动识别。

### 1.3 Python3（编译 GUI 用，CLI 不需要）

```bash
# 一般系统自带，验证一下
python3 --version   # 输出类似 Python 3.12.3
```

> GUI 的 Qt6 库通过 Python 工具 `aqtinstall` 自动下载，**不需要** `apt install qt6-base-dev`。

### 1.4 OpenGL（运行 GUI 用）

桌面 Linux 系统一般自带。如果不确定：

```bash
# 安装运行时库
sudo apt install -y libgl1

# 验证
ldconfig -p | grep libGL  # 应该能看到 libGL.so
```

---

## 第二步：编译项目

### 2.1 只编译命令行工具（最快，推荐先做这个）

```bash
# 在项目根目录下执行
cd backer

# 配置
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译（-j$(nproc) 表示用所有 CPU 核心并行编译）
cmake --build build -j$(nproc)
```

预期输出最后几行类似：

```
[100%] Built target backer-cli
[100%] Built target backer_test
```

> 第一次编译需要下载和编译所有依赖（spdlog、zlib、zstd、lzma、Google Test 等），大约 2-3 分钟。后续修改代码后重新编译只需几秒。

验证编译结果：

```bash
ls -lh build/backer-cli
# 输出：-rwxr-xr-x ... 2.7M ... build/backer-cli
```

### 2.2 编译 GUI 版本（含命令行工具）

```bash
# 依然在项目根目录
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

第一次编译 GUI 会比 CLI 版多花约 1-2 分钟（需要下载 Qt6 6.5.0 预编译包 ~200MB）。如果下载慢，可以配置代理：

```bash
export https_proxy=http://你的代理地址:端口
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
```

验证编译结果：

```bash
ls -lh build/backer-{cli,gui} build/backer_test
# 输出类似：
# -rwxr-xr-x ... 2.7M ... build/backer-cli
# -rwxr-xr-x ... 5.1M ... build/backer-gui
# -rwxr-xr-x ... 4.4M ... build/backer_test
```

> 这里第二次 `cmake -B build` 会复用之前已下载的依赖，不会重新下载。

### 2.3 编译参数说明

| 参数 | 默认值 | 作用 |
|------|-------|------|
| `-DCMAKE_BUILD_TYPE=Release` | Debug | Release 模式开启优化，运行更快 |
| `-DBUILD_GUI=ON` | OFF | 编译图形界面 |
| `-DBUILD_TESTING=ON` | ON | 编译单元测试 |

### 2.4 后续增量编译

修改代码后只需运行（不需要重新 `cmake -B build`）：

```bash
cmake --build build -j$(nproc)
```

> 不要 `rm -rf build` 整个删掉重编。如果遇到缓存问题，只清理具体依赖，例如 `rm -rf build/_deps/spdlog*`。

---

## 第三步：运行命令行工具（CLI）

### 3.1 查看帮助

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

### 3.2 生成测试数据

项目自带一个脚本，能在 `data/` 下生成包含各种文件类型的测试目录：

```bash
bash scripts/setup-testdata.sh
```

执行后查看生成了什么：

```bash
ls -la data/source/
# 应该能看到普通文件、子目录、软链接等
```

### 3.3 执行备份

```bash
# 最简单的备份（直接复制文件）
./build/backer-cli backup data/source data/backup

# 带 Tar 打包 + gzip 压缩的备份
./build/backer-cli backup data/source data/backup2 --pack tar --compress gzip

# 带 Zip 打包 + zstd 压缩的备份
./build/backer-cli backup data/source data/backup3 --pack zip --compress zstd
```

查看备份结果：

```bash
ls -la data/backup/
ls -la data/backup2/   # 应该有 .tar.gz 文件
```

### 3.4 执行还原

```bash
# 还原前面创建的备份
./build/backer-cli restore data/backup data/restore
```

验证还原结果：

```bash
ls data/restore/
diff -r data/source data/restore   # 应该没有输出（文件完全一致）
```

### 3.5 端到端测试脚本

一键跑完备份 → 还原 → 对比的完整流程：

```bash
bash scripts/test-backup-restore.sh
```

### 3.6 加密备份（需要第一步安装了 libssl-dev）

```bash
# 创建加密备份（会提示输入密码）
./build/backer-cli backup data/source data/enc-backup --pack tar --cipher AES256

# 还原加密备份（会提示输入密码）
./build/backer-cli restore data/enc-backup data/restore-enc --cipher AES256
```

---

## 第四步：运行图形界面（GUI）

### 4.1 桌面 Linux 系统

确保你是在有桌面环境的 Linux 上（Ubuntu 桌面版、WSL2 + WSLg 等），直接运行：

```bash
# 方法一：使用启动脚本（自动设置库路径）
bash scripts/run-gui.sh

# 方法二：直接运行
./build/backer-gui
```

如果遇到 `cannot connect to X server` 错误，说明没有桌面环境，请跳到 [4.2 节](#42-无桌面环境服务器-ssh)。

启动后你会在窗口中看到 4 个标签页：

| 标签 | 功能 |
|------|------|
| **备份** | 选择源/目标目录，配置打包/压缩/加密/筛选 |
| **还原** | 还原已有的备份 |
| **定时** | 配置 Cron 定时备份任务 |
| **设置** | 默认压缩算法、日志级别等偏好 |

### 4.2 无桌面环境（服务器 / SSH）

需要用虚拟显示模拟屏幕：

```bash
sudo apt install -y xvfb

# 方法一：xvfb-run 自动管理虚拟显示
xvfb-run ./build/backer-gui

# 方法二：手动启动 Xvfb
Xvfb :99 -screen 0 1024x768x24 &
export DISPLAY=:99
./build/backer-gui
```

> 虚拟显示下 GUI 仍然完整运行，只是你看不到窗口。可以通过截屏工具查看画面。

### 4.3 WSL2 特殊说明

如果 EGL 报错但 GUI 能打开，忽略即可：

```
libEGL warning: MESA-LOADER: failed to retrieve device information
```

这是 WSL2 缺少硬件 3D 加速的正常现象，Qt 会自动回退到软件渲染：

```bash
export LIBGL_ALWAYS_SOFTWARE=1   # 强制软件渲染
./build/backer-gui
```

---

## 第五步：运行测试

### 5.1 全部单元测试

```bash
ctest --test-dir build --output-on-failure
```

预期输出：所有测试通过（`100% tests passed`）。

### 5.2 按名字筛选

```bash
# 只跑备份引擎相关的测试
./build/backer_test --gtest_filter="*BackupCore*"

# 只跑还原引擎相关的测试
./build/backer_test --gtest_filter="*RestoreEngine*"
```

### 5.3 内存泄漏检测（可选）

```bash
sudo apt install -y valgrind
mkdir -p logs
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
    ./build/backer_test 2>logs/valgrind.log
```

---

## 常见问题

### Q1：cmake 配置时报错关于 OpenGL

**现象**：

```
Imported target "Qt6::Widgets" includes non-existent path
.../opengl_local/usr/include" in its INTERFACE_INCLUDE_DIRECTORIES
```

或：

```
The link interface of target "WrapOpenGL::WrapOpenGL" contains:
  OpenGL::GL
but the target was not found.
```

**原因**：本项目新的 `cmake/FetchQt6.cmake` 已修复此问题，但如果用旧版本或自定义 cmake 参数会出现。

**解决**：更新代码到最新版本（`git pull`）后重试：

```bash
git pull
rm -rf build
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
```

### Q2：Qt6 自动下载失败

**现象**：`FetchQt6: Qt6 not found, attempting auto-download...` 之后报超时或连接错误。

**原因**：aqtinstall 下载 Qt6 需要连接 `download.qt.io`，网络不通。

**解决**：

```bash
# 方法一：确保 pip 可用
python3 -m pip install --user --break-system-packages aqtinstall

# 方法二：使用代理
export https_proxy=http://你的代理:端口
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release

# 方法三：跳过 GUI，只编译 CLI
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Q3：编译时报 "OpenSSL NOT found"

**现象**：CMake 输出 `Could NOT find OpenSSL`。

**解决**：

```bash
sudo apt install -y libssl-dev
# 重新配置（它会自动检测到）
cmake -B build
```

### Q4：GUI 启动时报 "cannot connect to X server"

**现象**：`qt.qpa.xcb: could not connect to display`

**解决**：

- 桌面 Linux：检查 `echo $DISPLAY`，确保输出不为空
- WSL2：确保使用 Windows 11（自带 WSLg），或安装 [VcXsrv](https://sourceforge.net/projects/vcxsrc/)
- 服务器/SSH：用 Xvfb（见第四步 4.2 节）

### Q5：国内网络下载依赖慢

CMake FetchContent 会从 GitHub 拉取依赖源码。国内用户可以：

```bash
# 方式一：使用 git 代理
git config --global http.proxy http://你的代理:端口

# 方式二：使用 Docker 构建（带国内加速）
docker build --build-arg GH_PROXY=https://ghproxy.net/ -t backer .
```

Docker 方式只编译 CLI 版（无 GUI）。

---

## 完整流程速查

以下是从零开始到全部完成的命令汇总（Ubuntu 24.04）：

```bash
# ── 1. 安装依赖 ──
sudo apt update
sudo apt install -y cmake g++ libssl-dev python3

# ── 2. 克隆（如果还没克隆） ──
git clone https://github.com/airprofly/backer.git
cd backer

# ── 3. 编译 CLI ──
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# ── 4. 编译 GUI（可选） ──
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# ── 5. 生成测试数据并运行 ──
bash scripts/setup-testdata.sh
./build/backer-cli backup data/source data/backup
./build/backer-cli restore data/backup data/restore

# ── 6. 运行测试 ──
ctest --test-dir build --output-on-failure

# ── 7. 启动 GUI（需桌面环境） ──
bash scripts/run-gui.sh
```

> **记住**：之后修改代码只需要 `cmake --build build -j$(nproc)`，不需要重新 `cmake -B build`。

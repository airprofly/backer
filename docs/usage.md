# Backer 使用文档

> 版本 1.0.0 · Linux 平台 · C++17

---

## 目录

- [构建](#构建)
- [功能详解](#功能详解)
  - [图形界面（GUI）](#图形界面gui)
  - [目录镜像备份](#目录镜像备份)
  - [特殊文件处理](#特殊文件处理)
  - [元数据保留](#元数据保留)
  - [筛选器（6 维度）](#筛选器6-维度)
  - [Tar 打包](#tar-打包)
  - [Zip 打包](#zip-打包)
  - [压缩解压](#压缩解压)
- [退出码](#退出码)
- [Docker 使用](#docker-使用)

---

## 构建

```bash
# CLI 版本（无外部依赖）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# GUI 版本（自动下载 Qt6 预编译二进制，无需系统库）
cmake -B build -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## 功能详解
### 目录镜像备份

**命令形式**

```text
backer backup <source> <destination> [OPTIONS]
backer restore <source> <destination> [OPTIONS]
```

**说明**

默认模式。Backer 将源目录递归扫描，在目标位置重建完整的目录结构，每个文件逐一复制：

```text
源目录                          备份目录
data/source/                    data/backup/
├── docs/                       ├── docs/
│   ├── report.pdf              │   ├── report.pdf
│   └── notes.txt               │   └── notes.txt
├── src/                        ├── src/
│   └── main.cpp                │   └── main.cpp
└── README.md                   └── README.md
```

备份完成后输出统计信息：

```text
✓ Backup completed successfully
  Files:   42
  Dirs:    8
  Size:    1234567 bytes
  Skipped: 0
  Time:    123 ms
```

**示例**

```bash
# 备份项目目录
./build/backer-cli backup data/source data/backup

# 还原备份
./build/backer-cli restore data/backup data/restore
```

---

### 特殊文件处理

Backer 使用 POSIX `lstat()` 检测文件类型，支持以下特殊文件的完整备份与还原：

| 类型 | CLI 标识名 | 说明 |
|------|-----------|------|
| 符号链接 | `symlink`, `link`, `l` | 备份链接目标路径，还原时重建软链接 |
| 命名管道 | `fifo`, `pipe`, `p` | 通过 `mkfifo()` 创建 |
| 块设备 | `block`, `blockdev`, `b` | 通过 `mknod()` 创建，记录设备号（major/minor） |
| 字符设备 | `char`, `chardev`, `c` | 通过 `mknod()` 创建，记录设备号 |
| Socket | `socket`, `sock`, `s` | 备份条目存在，还原时创建空占位文件 |

> **注意**：创建块/字符设备节点需要 root 权限。非 root 运行时相关设备会被跳过并记录警告。

---

### 元数据保留

Backer 自动保留并还原以下文件元数据：

| 元数据 | 说明 | 还原条件 |
|--------|------|----------|
| 属主 (UID/GID) | 文件所有者和所属组 | 需要 `CAP_CHOWN` 能力（root） |
| 权限 (mode bits) | rwx + setuid/setgid/sticky | 始终还原（symlink 在 Linux 上跳过） |
| 访问时间 (atime) | 最后访问时间（纳秒精度） | 始终还原 |
| 修改时间 (mtime) | 最后修改时间（纳秒精度） | 始终还原 |
| 变更时间 (ctime) | 状态变更时间 | 记录但不还原 |

元数据通过 JSON 文件（`backer_metadata.json`）存储在备份目录中。

---

### 筛选器（6 维度）

筛选选项仅适用于 `backup` 子命令，`restore` 时不做筛选。

**命令形式**

```text
backer-cli backup <source> <destination> [--include-* ...] [--exclude-* ...]
```

**相关选项**

| 维度 | 选项 | 说明 | 参数格式 |
|------|------|------|----------|
| 路径 | `--include-path <GLOB>` | 仅包含匹配路径 glob 的文件（可重复） | `"src/*.cpp"` |
|      | `--exclude-path <GLOB>` | 排除匹配路径 glob 的文件（可重复） | `"build/*"` |
| 类型 | `--include-type <TYPE>` | 仅包含指定类型的文件（可重复） | `file` / `dir` / `symlink` / `fifo` / `block` / `char` / `socket` |
|      | `--exclude-type <TYPE>` | 排除指定类型的文件（可重复） | 同上 |
| 名称 | `--include-name <GLOB>` | 仅包含匹配文件名 glob 的文件（可重复） | `"*.txt"` |
|      | `--exclude-name <GLOB>` | 排除匹配文件名 glob 的文件（可重复） | `"*.tmp"` |
| 时间 | `--mtime-before <UNIX_TIME>` | 排除修改时间在此时间戳**之后**的文件 | Unix 秒级时间戳 |
|      | `--mtime-after <UNIX_TIME>` | 排除修改时间在此时间戳**之前**的文件 | Unix 秒级时间戳 |
| 尺寸 | `--size-min <BYTES>` | 最小文件大小（字节） | 数字 |
|      | `--size-max <BYTES>` | 最大文件大小（字节） | 数字 |
| 属主 | `--owner <USER>` | 仅包含指定用户拥有的文件 | 系统用户名 |

> **筛选逻辑**：相同维度的多个 `--include-*` 规则为**或**（OR）关系——满足任一即通过该维度；不同维度的规则为**与**（AND）关系——所有维度都必须通过。

筛选器在目录遍历之后、写入存储之前应用，支持 6 种独立的筛选维度：

#### 1. 路径匹配

基于相对路径的 glob 匹配，使用 `fnmatch` 的 `FNM_PATHNAME` 语义——`*` 不匹配路径分隔符 `/`：

```bash
# 仅备份 subdir 目录下的文件
./build/backer-cli backup data/source data/backup --include-path "subdir/*"

# 排除 subdir 目录
./build/backer-cli backup data/source data/backup --exclude-path "subdir/*"
```

#### 2. 文件类型

```bash
# 排除 FIFO 管道
./build/backer-cli backup data/source data/backup --exclude-type fifo

# 排除符号链接
./build/backer-cli backup data/source data/backup --exclude-type symlink

# 仅备份目录和普通文件
./build/backer-cli backup data/source data/backup --include-type dir --include-type file
```

#### 3. 文件名

基于文件名（不含路径部分）的 glob 匹配：

```bash
# 仅备份 .txt 文件
./build/backer-cli backup data/source data/backup --include-name "*.txt"

# 排除 .fifo 文件
./build/backer-cli backup data/source data/backup --exclude-name "*.fifo"
```

#### 4. 修改时间

基于 Unix 时间戳筛选：

```bash
# 仅备份 2026 年 7 月 1 日之后修改过的文件
./build/backer-cli backup data/source data/backup --mtime-after 1818000000

# 排除最近 7 天内修改过的文件
./build/backer-cli backup data/source data/backup --mtime-before $(($(date +%s) - 7*86400))
```

> `--mtime-after` 是包含下限，`--mtime-before` 是排他上限（修改时间在此之前的文件保留）。可用 `$(date +%s)` 获取当前 Unix 时间戳。

#### 5. 文件大小

按字节数筛选：

```bash
# 仅备份 10 字节以上的文件（排除空小文件）
./build/backer-cli backup data/source data/backup --size-min 10

# 排除 100 字节以上的大文件
./build/backer-cli backup data/source data/backup --size-max 100

# 备份大小在 10 到 100 字节之间的文件
./build/backer-cli backup data/source data/backup --size-min 10 --size-max 100
```

> `--size-min 0` 为默认值，表示不设下限。如要筛选空文件，可指定 `--size-min 1`。

#### 6. 属主

按文件所有者用户名筛选（仅 POSIX 平台），通过 `getpwnam_r()` 将用户名解析为 UID 后匹配：

```bash
# 仅备份当前用户拥有的文件
./build/backer-cli backup data/source data/backup --owner $(whoami)
```

**组合示例**

```bash
# 多维度组合：仅备份 subdir 子目录下的普通文件
./build/backer-cli backup data/source data/backup \
    --include-path "subdir/*" \
    --include-type file
```

---

### Tar 打包

**相关选项**

| 选项 | 说明 | 可选值 |
|------|------|--------|
| `--pack tar` | 启用归档模式，生成 `.tar` 文件 | 还原时同理：`--pack tar` |

**说明**

`--pack tar` 启用归档模式，将备份内容打包为 POSIX ustar 格式的 `.tar` 文件。

支持的 tar 条目类型：普通文件、目录、符号链接、FIFO、块设备、字符设备。

**格式细节**：

| 特性 | 支持情况 |
|------|----------|
| 格式标准 | POSIX ustar（`magic = "ustar\0"`） |
| 最大路径长度 | 255 字符（name=100 + prefix=155） |
| 数据块大小 | 512 字节，末尾零填充对齐 |
| 文件大小 | 支持超大型文件（12 位八进制，最大 8 GB） |
| 校验和 | 每个头部包含校验和验证 |
| 归档尾标记 | 两个全零的 512 字节块 |
| 属主名/组名 | 记录在头部（各 32 字符） |

> 元数据直接编码在 tar 头部各字段中。

**示例**

```bash
# 打包备份与还原
./build/backer-cli backup data/source data/backup.tar --pack tar
./build/backer-cli restore data/backup.tar data/restore --pack tar

# 快速快照：仅备份目录和符号链接（不备份文件内容）
./build/backer-cli backup data/source snapshot.tar --pack tar \
    --include-type dir --include-type symlink
```

---

### Zip 打包

**相关选项**

| 选项 | 说明 | 可选值 |
|------|------|--------|
| `--pack zip` | 启用归档模式，生成 `.zip` 文件 | 还原时同理：`--pack zip` |

**说明**

`--pack zip` 启用归档模式，将备份内容打包为标准 Zip 格式的 `.zip` 文件。

基于 miniz 库实现，支持 DEFLATE 压缩。

支持的条目类型：普通文件、目录、符号链接、FIFO、块设备、字符设备。

**特殊文件处理**：

Zip 格式不原生支持特殊文件类型，Backer 通过在归档中嵌入元数据条目来实现完整还原：

| 条目类型 | Zip 中的表示 |
|----------|-------------|
| 普通文件 | 标准 Zip 条目（DEFLATE 压缩） |
| 目录 | Zip 目录条目（名称以 `/` 结尾） |
| 符号链接 | 文件内容为目标路径，由元数据标记为符号链接 |
| FIFO/设备/Socket | 零字节占位条目，由元数据标记实际类型 |

所有元数据（属主、权限、纳秒精度时间戳、设备号等）存储在归档根目录的 `.backer_zip_meta` 条目中。

**格式细节**：

| 特性 | 支持情况 |
|------|----------|
| 格式标准 | 标准 Zip（PKZIP 2.0+） |
| 压缩算法 | DEFLATE（默认级别） |
| 最大路径长度 | 不限（Zip 本身不限制路径长度） |
| 文件大小 | 不限（Zip64 支持 4 GiB+） |
| 数据完整性 | CRC-32 校验（Zip 条目级） |
| 元数据存储 | `.backer_zip_meta` 内部 JSON 条目 |

**示例**

```bash
# Zip 打包备份与还原
./build/backer-cli backup data/source data/backup.zip --pack zip
./build/backer-cli restore data/backup.zip data/restore --pack zip

# 仅备份特定类型的文件
./build/backer-cli backup data/source data/backup.zip --pack zip \
    --include-type file --include-type dir

# 与筛选器组合
./build/backer-cli backup data/source data/backup.zip --pack zip \
    --include-name "*.txt" --include-name "*.md"
```

---

### 图形界面（GUI）

Backer 提供基于 Qt 6 的图形用户界面，macOS 简约风格，功能等价于 CLI 的所有备份/还原操作。

#### 构建

```bash
# 自动下载 Qt6 预编译二进制（首次需联网，数分钟）
cmake -B build -DBUILD_GUI=ON
cmake --build build -j$(nproc)
./build/backer-gui
```

启动后先显示启动画面（盾牌+箭头插图），随后进入主界面。

#### 功能说明

| 页面 | 功能 |
|------|------|
| 备份 | 选择源/目标目录，配置打包/压缩/加密选项，可视化筛选条件编辑，实时进度条和日志 |
| 还原 | 选择备份源（文件或目录），配置解压/解包选项，保留元数据/特殊文件选项 |
| 定时任务 | 添加/编辑/删除定时备份任务（Cron 表达式），空状态插图引导，任务列表管理 |
| 设置 | 默认路径、默认压缩/加密偏好、日志级别、并发线程数，通过 QSettings 持久化 |

---

### 压缩解压

备份归档（`.tar` / `.zip`）后可叠加压缩以节省存储空间，还原时先解压再解包。支持三种算法：

| 算法 | 选项值 | 文件后缀 | 级别范围 | 默认级别 | 特点 |
|------|--------|----------|----------|----------|------|
| gzip | `gzip` | `.gz` | 0–9 | 6 | 通用、解压快 |
| zstd | `zstd` | `.zst` | 1–22 | 3 | 日常备份首选，速度快 |
| LZMA | `lzma` | `.xz` | 0–9 | 6 | 压缩比最高，速度较慢 |

> 压缩作用于**打包后的归档文件**（而非单个文件），因此需配合 `--pack` 使用。

#### 命令

```bash
# 备份 → 打包 (tar) → 压缩 (gzip) 产出 data/backup.tar.gz
./build/backer-cli backup data/source data/backup --pack tar --compress gzip

# 指定压缩级别（1=最快, 9=最高压缩比）
./build/backer-cli backup data/source data/backup --pack tar --compress lzma --compress-level 9

# 还原：先解压 (gzip) 再解包 (tar)
./build/backer-cli restore data/backup.tar.gz data/restore --pack tar --decompress gzip
 
# 三种算法均可
./build/backer-cli backup data/source data/backup --pack tar --compress zstd
./build/backer-cli backup data/source data/backup --pack tar --compress lzma
```

#### 文件命名

备份时自动追加后缀（避免与用户已有的扩展名重复）：

```
--pack tar --compress gzip  → data/backup.tar.gz
--pack tar --compress zstd  → data/backup.tar.zst
--pack tar --compress lzma  → data/backup.tar.xz
```

还原时 `--decompress` 读取压缩归档，解压到临时文件后交给解包器，完成后自动清理临时文件。

#### 算法选型建议

| 场景 | 推荐 | 理由 |
|------|------|------|
| 日常备份，追求速度 | `zstd` | 压缩/解压都极快，压缩比优秀 |
| 通用兼容、跨工具交换 | `gzip` | 最广泛支持，与 `gunzip` 互通 |
| 归档存储，最大化压缩 | `lzma` | 压缩比最高，但耗时与内存占用大 |

---

## 退出码

| 退出码 | 说明 |
|--------|------|
| 0 | 成功 |
| 1 | 操作失败（见 stderr 错误信息） |

---

## Docker 使用

```bash
# 构建镜像（国内用户可添加 GH_PROXY 参数）
docker build -t backer .

# 使用 Compose（自动挂载数据卷）
docker compose run --rm backer backup /data/source /data/backup
docker compose run --rm backer restore /data/backup /data/restore

# 查看帮助
docker run --rm backer --help
```

Compose 挂载约定（默认 `./data/` 各子目录对应容器内 `/data/` 下路径）：

```yaml
volumes:
  - ./data/source:/data/source:ro   # 待备份源目录（只读）
  - ./data/backup:/data/backup      # 备份输出目录
  - ./data/restore:/data/restore    # 还原目标目录
```

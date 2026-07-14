# Backer 使用文档

> 版本 1.0.0 · Linux 平台 · C++17

---

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## 初始化测试数据

```bash
rm -rf data/backup data/restore
./scripts/setup-testdata.sh
```

---

## 目录镜像备份（基本要求 40分）
支持目录树的完整备份与还原，包含普通文件和目录结构。

```bash
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup
ls -l data/backup
./build/backer-cli restore data/backup data/restore
ls -l data/restore
```

---

## 文件类型支持（10分）
支持符号链接、管道（FIFO）、字符设备、块设备等特殊文件的备份与还原。

```bash
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup
ls -l data/backup
./build/backer-cli restore data/backup data/restore
ls -l data/restore
```

---

## 元数据保留（10分）

| 元数据 | 还原条件 |
|--------|----------|
| 属主 (UID/GID) | 需要 root |
| 权限 (mode bits) | 始终还原 |
| 访问/修改时间 | 始终还原（纳秒精度） |

```bash
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup
ls -l data/backup
./build/backer-cli restore data/backup data/restore
ls -l data/restore
```

---

## 筛选器（6维度各3分，共18分）

```bash
./build/backer-cli backup <source> <dest> [--include-* ...] [--exclude-* ...]
```

| 维度 | 选项 | 示例 |
|------|------|------|
| 路径 | `--include-path <GLOB>` | `"src/*.cpp"` |
|      | `--exclude-path <GLOB>` | `"build/*"` |
| 类型 | `--include-type <TYPE>` | `file`, `dir`, `symlink`, `fifo` |
|      | `--exclude-type <TYPE>` | 同上 |
| 名称 | `--include-name <GLOB>` | `"*.txt"` |
|      | `--exclude-name <GLOB>` | `"*.tmp"` |
| 时间 | `--mtime-after <TIME>` | 排除此时间前的文件（Unix 时间戳或 YYYY-MM-DD） |
|      | `--mtime-before <TIME>` | 排除此时间后的文件（同上） |
| 尺寸 | `--size-min <BYTES>` | 最小字节数 |
|      | `--size-max <BYTES>` | 最大字节数 |
| 属主 | `--owner <USER>` | 仅指定用户的文件 |

**示例**

```bash
# 仅备份 .txt 文件                                         ← 名字筛选（3分）
rm -rf data/backup
./build/backer-cli backup data/source data/backup --include-name "*.txt"
ls -l data/backup

# 排除符号链接和设备文件                                     ← 类型筛选（3分）
rm -rf data/backup
./build/backer-cli backup data/source data/backup --exclude-type symlink --exclude-type block
ls -l data/source
ls -l data/backup

# 仅备份 10 字节以上的文件（doc.txt 13B 保留，sub/file.txt 7B 排除）← 尺寸筛选（3分）
rm -rf data/backup
du -ab data/source
./build/backer-cli backup data/source data/backup --size-min 10
ls -l data/backup

# 组合：仅备份 sub 下的普通文件                                ← 路径筛选（3分）+ 类型筛选（3分）
rm -rf data/backup
./build/backer-cli backup data/source data/backup --include-path "sub/*" --include-type file
ls -l data/backup

# 仅备份 2026 年之后修改的文件（doc.txt/fifo/symlink 保留，sub/ 2025年 排除）← 时间筛选（3分）
rm -rf data/backup
ls -l data/source
./build/backer-cli backup data/source data/backup --mtime-after "2026-01-01"
ls -l data/backup

# 仅备份当前用户的文件                                        ← 属主筛选（3分）
rm -rf data/backup
./build/backer-cli backup data/source data/backup --owner $(whoami)
ls -l data/backup
```

---

## 打包（Tar/Zip）（每种10分，共20分）

```bash
# Tar 打包
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup --pack tar
ls -lh data/backup.tar
./build/backer-cli restore data/backup.tar data/restore --pack tar
ls -l data/restore

# Zip 打包
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup --pack zip
ls -lh data/backup.zip
./build/backer-cli restore data/backup.zip data/restore --pack zip
ls -l data/restore
```

---

## 压缩解压（每种10分，共30分）

```bash
# gzip
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup --pack tar --compress gzip
ls -lh data/backup.tar.gz
./build/backer-cli restore data/backup.tar.gz data/restore --pack tar --decompress gzip
ls -l data/restore

# zstd
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup --pack tar --compress zstd
ls -lh data/backup.tar.zst
./build/backer-cli restore data/backup.tar.zst data/restore --pack tar --decompress zstd
ls -l data/restore

# lzma
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup --pack tar --compress lzma
ls -lh data/backup.tar.xz
./build/backer-cli restore data/backup.tar.xz data/restore --pack tar --decompress lzma
ls -l data/restore
```

| 算法 | 后缀 | 特点 |
|------|------|------|
| gzip | `.gz` | 通用、解压快 |
| zstd | `.zst` | 速度快、压缩比好 |
| lzma | `.xz` | 压缩比最高 |

---

## 加密解密（每种10分，共20分）

```bash
# 备份 → 打包 → 压缩 → 加密
rm -rf data/backup data/restore
./build/backer-cli backup data/source data/backup --pack tar --compress gzip --encrypt aes256 --password "pwd"
ls -lh data/backup.tar.gz.enc
head -c 128 data/backup.tar.gz.enc | xxd  # 加密后文件无法直接读取

# 还原：解密 → 解压 → 解包
./build/backer-cli restore data/backup.tar.gz.enc data/restore --pack tar --decompress gzip --decrypt aes256 --password "pwd"
ls -l data/restore
```

| 算法 | 选项 | 说明 |
|------|------|------|
| AES-256-GCM | `aes256` | 认证加密，推荐 |
| SM4-CBC | `sm4` | 国密算法 |

---

## 定时备份（10分）

支持 crontab 表达式设置周期性备份任务，可指定保留快照数量自动淘汰旧备份。

```bash
./build/backer-cli schedule list                                    # 列出所有任务
./build/backer-cli schedule add <name> <cron> <src> <dest> [OPTS]  # 添加任务
./build/backer-cli schedule remove <id>                             # 删除任务
./build/backer-cli schedule enable <id>                             # 启用任务
./build/backer-cli schedule disable <id>                            # 禁用任务
```

**现场演示**

```bash
rm -rf data/backup data/restore
# 添加一个每 30 秒备份一次的定时任务（方便现场观察效果）
./build/backer-cli schedule add "演示任务" "*/30 * * * * *" data/source data/backup --compress zstd --retain-count 3
./build/backer-cli schedule list
sleep 35  # 等待约 30 秒触发一次备份
ls -lh data/backup*.tar.zst
./build/backer-cli schedule list

# 演示完毕后清理
./build/backer-cli schedule remove 1
```

---

## 分数汇总

| 类别 | 功能 | 分数 |
|:-----|:-----|:----:|
| 基本要求 | 目录镜像备份 | 40 |
| 扩展要求 | 文件类型支持 | 10 |
| 扩展要求 | 元数据保留 | 10 |
| 扩展要求 | 筛选器（6 维度） | 18 |
| 扩展要求 | Tar 打包 | 10 |
| 扩展要求 | Zip 打包 | 10 |
| 扩展要求 | gzip 压缩 | 10 |
| 扩展要求 | zstd 压缩 | 10 |
| 扩展要求 | lzma 压缩 | 10 |
| 扩展要求 | AES-256-GCM 加密 | 10 |
| 扩展要求 | SM4-CBC 加密 | 10 |
| 扩展要求 | 定时备份 | 10 |
| **总计** | — | **158** |

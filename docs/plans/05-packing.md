# 打包解包 — 实施计划

## 功能概述

将备份的所有文件拼接为一个或多个打包文件进行存储，支持 Tar 和 Zip 两种打包格式，对应解包还原功能。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（打包解包 / 每种 10 分）。

## 技术方案

### 架构设计

```
Packer 接口
    │
    ├── TarPacker    ← 自实现 Tar 格式 (POSIX.1-2001 / ustar)
    │
    └── ZipPacker    ← 基于 miniz 单文件库
```

### Tar 格式 (自实现)

**选择自实现的原因**：避免引入 `libarchive` 等重型依赖，加深对打包格式的理解。

**支持的 Tar 格式**：POSIX ustar 格式（`prefix` 字段支持长路径）

```
Tar 头部 (512 字节)：
┌─────────────────────┬──────────┐
│ name        (100 B) │ 文件名    │
│ mode         (8 B)  │ 权限      │
│ uid          (8 B)  │ 属主 ID   │
│ gid          (8 B)  │ 属组 ID   │
│ size        (12 B)  │ 文件大小   │
│ mtime       (12 B)  │ 修改时间   │
│ chksum       (8 B)  │ 校验和     │
│ typeflag     (1 B)  │ 文件类型   │
│ linkname   (100 B)  │ 链接目标   │
│ magic        (6 B)  │ "ustar"   │
│ version      (2 B)  │ "00"      │
│ uname       (32 B)  │ 属主名     │
│ gname       (32 B)  │ 属组名     │
│ devmajor     (8 B)  │ 设备号主   │
│ devminor     (8 B)  │ 设备号次   │
│ prefix     (155 B)  │ 路径前缀   │
│ padding     (12 B)  │ 填充       │
└─────────────────────┴──────────┘
数据块 (512 字节对齐)
```

**特殊文件处理**：
| Tar typeflag | 文件类型 |
|-------------|----------|
| '0' | 常规文件 |
| '5' | 目录 |
| '2' | 符号链接 |
| '6' | FIFO |
| '3' | 字符设备 |
| '4' | 块设备 |

### Zip 格式 (miniz)

使用 miniz 库（单文件 C 库，`mz_zip_*` API）：

- 支持流式写入，无需预知总大小
- 每个文件独立压缩（store/deflate）
- 支持注释和额外字段

### 命名约定

```
# 无压缩
backup_20260706_220000.tar
backup_20260706_220000.zip

# 配合压缩
backup_20260706_220000.tar.gz   ← TarPacker + GzipCompressor
backup_20260706_220000.tar.zst  ← TarPacker + ZstdCompressor
```

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. Packer 接口 | 定义 `pack()` / `unpack()` 抽象接口 | `packer.h` |
| 2. Tar 头部结构 | TarHeader 结构体 + 序列化/校验和 | `tar_packer.h` |
| 3. Tar 打包 | 遍历 FileEntry → 写头部 + 写内容 (512B 对齐) | `tar_packer.cpp` |
| 4. Tar 解包 | 读头部 → 校验 → 提取内容 → 重建文件 | `tar_packer.cpp` |
| 5. Tar 长路径处理 | GNU 扩展 `././@LongLink` 或 ustar prefix | `tar_packer.cpp` |
| 6. Tar 特殊文件 | typeflag 映射 FileType | `tar_packer.cpp` |
| 7. miniz 集成 | 添加 miniz 单文件库到项目 | `third_party/miniz/` |
| 8. Zip 打包 | `mz_zip_writer_add_file()` / `mz_zip_writer_add_mem()` | `zip_packer.cpp` |
| 9. Zip 解包 | `mz_zip_reader_extract()` | `zip_packer.cpp` |
| 10. CLI 集成 | `--pack tar` / `--pack zip` | `commands.cpp` |
| 11. 单元测试 | Tar/Zip 打包→解包→diff 验证 | 测试文件 |

## 关键接口

```cpp
class Packer {
public:
    virtual ~Packer() = default;

    // 打包
    virtual std::expected<void, ErrorCode> pack(
        std::vector<FileEntry> const& files,
        FSAbstraction& fs,
        std::ostream& output) = 0;

    // 解包
    virtual std::expected<void, ErrorCode> unpack(
        std::istream& input,
        std::filesystem::path const& dest,
        FSAbstraction& fs) = 0;

    virtual std::string_view formatName() const noexcept = 0;
};

class TarPacker : public Packer { /* ... */ };
class ZipPacker : public Packer { /* ... */ };
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 基本打包解包 | N 个文件打包→解包→diff | 内容一致 |
| 空目录 | 打包含空目录 → 解包 | 目录存在 |
| 符号链接 | 打包符号链接 | typeflag='2', linkname 正确 |
| 长路径 (>100B) | ustar prefix 或 GNU 扩展 | 路径完整保留 |
| 大文件 (2GiB+) | 分块流式处理 | 避免内存溢出 |
| 损坏包 | 修改 tar 校验和 | 解包检测到损坏并报错 |
| Tar.gz 组合 | TarPacker → GzipCompressor | 管道组合正确 |

## 关键文件

```
src/pack/
├── packer.h               # Packer 抽象接口
├── tar_packer.h
├── tar_packer.cpp
├── zip_packer.h
├── zip_packer.cpp
├── miniz.h                # miniz 单头文件
└── miniz.c                # miniz 实现
src/cli/
└── commands.cpp           # --pack 参数
tests/pack/
├── tar_packer_test.cpp
└── zip_packer_test.cpp
```

## 预计工作量

- 代码行数：~1200 行（含测试 + miniz 集成）
- 开发周期：Tar 5-7 天 + Zip 2-3 天（可并行）
- 依赖：无（Tar 自实现）/ miniz（Zip）

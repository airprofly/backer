# 子系统五：打包子系统构件图描述

## 对应源文件

`subsystem-05-packing.svg`

## 图概述

本图展示了打包子系统的内部结构，包含 Packer 抽象接口以及 TarPacker（自实现 POSIX ustar 格式）和 ZipPacker（基于 miniz 库）两个具体实现类。

## 构件说明

### 接口层

| 构件 | 类型 | 说明 |
|------|------|------|
| Packer | 抽象类（接口） | 打包器策略接口，定义 3 个纯虚方法。所有打包格式通过实现此接口接入 |

接口方法规范：

| 方法 | 签名 | 说明 |
|------|------|------|
| pack() | `vector<FileEntry> const& files, FSAbstraction& fs, path const& sourceRoot, ostream& output → Expected<void, ErrorCode>` | 将文件条目列表打包写入输出流 |
| unpack() | `istream& input, path const& dest, FSAbstraction& fs → Expected<void, ErrorCode>` | 从输入流解包恢复到目标目录 |
| formatName() | `→ string_view` | 返回格式名称："tar" / "zip" |

### TarPacker（Tar 打包器）

| 构件 | 类型 | 说明 |
|------|------|------|
| TarPacker | 实现类 | 自实现的 POSIX ustar 格式 Tar 打包器，无外部库依赖。块大小 512 字节，结束标记为 2 个零块。支持路径拆分（100 字符 name + 155 字符 prefix）|
| TarHeader | 结构体（packed, 512 字节） | POSIX ustar 兼容的 tar 头结构体。字段：name[100]、mode[8]、uid[8]、gid[8]、size[12]、mtime[12]、chksum[8]、typeflag（1 字节）、linkname[100]、magic[6]="ustar\0"、version[2]="00"、uname[32]、gname[32]、devmajor[8]、devminor[8]、prefix[155]、padding[12]|
| encodeHeader() | 静态方法 | pack 时调用。填充 TarHeader 各字段：splitPath 拆分路径为 name+prefix、formatOctal 转换数字字段为八进制字符串、typeToFlag 转换文件类型、计算并写入校验和 |
| decodeHeader() | 静态方法 | unpack 时调用。验证 magic=="ustar"、verifyChecksum 验证校验和、parseOctal 解析八进制数字字段、flagToType 还原文件类型、重构 FileEntry |
| formatOctal() | 辅助函数 | 将 uint64_t 格式化为 tar 标准八进制字符串（snprintf 八进制格式，右对齐）|
| parseOctal() | 辅助函数 | 从字节缓冲区解析 tar 八进制数字字符串为 uint64_t |
| splitPath() | 辅助函数 | 将超过 100 字符的路径拆分为 prefix（最长 155）+ name（最长 100）两部分，适配 ustar 格式限制 |
| typeToFlag() | 映射函数 | FileType → tar typeflag 字符（'0'=普通文件、'5'=目录、'2'=符号链接等）|
| flagToType() | 映射函数 | tar typeflag 字符 → FileType |
| writeAligned() | IO 辅助 | 将数据写入 ostream，尾部补齐 0 到 512 字节边界 |
| readExact() | IO 辅助 | 从 istream 精确读取 size 字节到缓冲区，不足返回 false（检测截断或损坏）|
| skipData() | IO 辅助 | unpack 时跳过数据块（实际数据 + 对齐填充）|
| verifyChecksum() | 公开静态方法 | 验证 tar 头的校验和字段，公开供测试使用 |

### ZipPacker（Zip 打包器）

| 构件 | 类型 | 说明 |
|------|------|------|
| ZipPacker | 实现类 | 基于 miniz 库的 ZIP 格式打包器。元数据存储策略：使用 .backer_zip_meta 特殊条目（JSON 格式）存储所有文件的完整元数据 |
| EntryMeta | 内部结构体 | ZIP 条目元数据的中间表示。字段：文件类型（FileType）、大小（uint64）、Metadata、符号链接目标（string）、设备主/次号（uint32）|
| buildMetaJson() | 内部方法 | pack 时调用。遍历所有 FileEntry，将每个条目的完整 Metadata 序列化为嵌套 JSON 对象（路径转义为 JSON key），返回完整 JSON 字符串 |
| parseMetaEntries() | 内部方法 | unpack 时调用。解析 .backer_zip_meta 的 JSON 内容，返回 path → JSON body 的映射表 |
| parseEntryMeta() | 内部方法 | 解析单个条目的 JSON body，反序列化为 EntryMeta 结构体 |
| mz_zip_archive（miniz） | 第三方库 | miniz 单文件 C 库，提供 mz_zip_writer（堆内存模式写入）和 mz_zip_reader（内存模式读取）API |
| jsonEscape() | 内部辅助 | JSON 字符串转义（`"`、`\`、换行等特殊字符 → `\"`、`\\` 等）|
| jsonUnescape() | 内部辅助 | JSON 字符串反转义 |

## Tar 归档结构

```
┌─────────────────┐
│ TarHeader       │ 512 B  (第一个文件)
├─────────────────┤
│ 文件数据        │ n×512 B (尾部 0 填充对齐)
├─────────────────┤
│ TarHeader       │ 512 B  (第二个文件)
├─────────────────┤
│ ...             │
├─────────────────┤
│ 全零块 × 2      │ 1024 B (结束标记)
└─────────────────┘
```

## Zip 文件类型处理策略

| 文件类型 | ZIP 存储方式 |
|---------|-------------|
| 普通文件 | Deflate 压缩存储，内容为原始文件数据 |
| 目录 | 空条目，路径以 "/" 结尾 |
| 符号链接 | 文件内容为链接目标路径字符串 |
| FIFO / 块设备 / 字符设备 / 套接字 | 零字节占位条目（数据在 .backer_zip_meta 的 Metadata 中保留）|

## 依赖关系

| 实现类 | 依赖 | 说明 |
|--------|------|------|
| TarPacker | 无外部库 | 完全自实现，仅依赖 FSAbstraction 接口读写文件内容 |
| ZipPacker | miniz | 单文件嵌入（header-only style），CMake FetchContent 引入 |

## 设计模式

- **策略模式**：Packer 接口 + TarPacker/ZipPacker 双实现
- **模板方法**：pack()/unpack() 定义标准打包/解包流程，具体格式差异在子类实现

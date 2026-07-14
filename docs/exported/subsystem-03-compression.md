# 子系统三：压缩子系统构件图描述

## 对应源文件

`subsystem-03-compression.svg`

## 图概述

本图展示了压缩子系统的内部结构，包含 Compressor 抽象接口、buildCompressor 工厂函数以及 GzipCompressor、ZstdCompressor、LzmaCompressor 三个具体实现类。

## 构件说明

### 接口层

| 构件 | 类型 | 说明 |
|------|------|------|
| Compressor | 抽象类（接口） | 压缩器策略接口，定义 5 个纯虚方法。所有压缩算法通过实现此接口接入系统 |

接口方法规范：

| 方法 | 签名 | 说明 |
|------|------|------|
| compress() | `span<char const> input, vector<char>& output → Expected<void, ErrorCode>` | 将 input 压缩后追加到 output。支持空输入（生成有效空压缩流）|
| decompress() | `span<char const> input, vector<char>& output → Expected<void, ErrorCode>` | 将 input 解压后追加到 output |
| name() | `→ string_view` | 返回算法名称："gzip" / "zstd" / "lzma" |
| defaultLevel() | `→ int` | 返回算法默认压缩级别 |
| suffix() | `→ string_view` | 返回压缩文件后缀：".gz" / ".zst" / ".xz" |
| isValidLevel() | `int → bool` | 验证指定级别是否在有效范围内 |

### 工厂层

| 构件 | 类型 | 说明 |
|------|------|------|
| buildCompressor() | 工厂函数 | 根据算法名称字符串创建对应 Compressor 实例，返回 unique_ptr\<Compressor\>。level=0 时使用算法默认级别。未知名称返回 nullptr。映射："gzip"→GzipCompressor / "zstd"→ZstdCompressor / "lzma"→LzmaCompressor |

### GzipCompressor（Gzip 压缩器）

| 构件 | 类型 | 说明 |
|------|------|------|
| GzipCompressor | 实现类 | zlib 库封装，实现 Compressor 接口。使用 deflateInit2 的 gzip 模式（窗口位 15+16 = 31）压缩，inflateInit2 解压 |
| deflateInit2() | zlib API | 初始化压缩上下文，配置 gzip 窗口位（15+16）。默认压缩级别 6 |
| deflate() | zlib API | 执行压缩，使用 deflateBound 预分配输出缓冲区，deflate(Z_FINISH) 单次完成 |
| inflateInit2() | zlib API | 初始化解压上下文，配置 gzip 窗口位 |
| inflate() | zlib API | 执行解压，分块 inflate 循环（64 KiB 块），4 倍输入大小的预分配启发式 |

### ZstdCompressor（Zstd 压缩器）

| 构件 | 类型 | 说明 |
|------|------|------|
| ZstdCompressor | 实现类 | libzstd 库封装，实现 Compressor 接口。使用单次 API 模式（ZSTD_compress / ZSTD_decompress），默认级别 3 |
| ZSTD_compress() | libzstd API | 单次压缩调用，通过 ZSTD_compressBound 预分配输出缓冲区 |
| ZSTD_decompress() | libzstd API | 单次解压调用。先通过 ZSTD_getFrameContentSize 探测未压缩大小；若大小未知则使用 4 倍启发式预分配 |
| ZSTD_getFrameContentSize() | libzstd API | 获取压缩帧内的原始内容大小信息 |

### LzmaCompressor（Lzma 压缩器）

| 构件 | 类型 | 说明 |
|------|------|------|
| LzmaCompressor | 实现类 | liblzma 库封装，实现 Compressor 接口。使用流式 API 模式（lzma_easy_encoder / lzma_auto_decoder），默认级别 6 |
| lzma_easy_encoder() | liblzma API | 初始化 LZMA2 压缩编码器，配置预设压缩级别。输出缓冲区大小通过 lzma_stream_buffer_bound 计算 |
| lzma_code() | liblzma API | 流式编解码核心函数。压缩时循环调用至 LZMA_STREAM_END；解压时同样循环处理。含死循环防护（对畸形输入）|
| lzma_auto_decoder() | liblzma API | 初始化自动检测解码器，自动识别 LZMA/LZMA2/XZ 格式 |

## 依赖关系

| 实现类 | 底层依赖库 | 引入方式 |
|--------|-----------|---------|
| GzipCompressor | zlib | CMake FetchContent 源码编译 |
| ZstdCompressor | libzstd | CMake FetchContent 源码编译 |
| LzmaCompressor | liblzma | CMake FetchContent 源码编译 |

## 设计模式

- **策略模式**：Compressor 接口 + 3 种算法实现，通过 buildCompressor 工厂根据名称创建实例
- **Span 模式**：compress() / decompress() 均使用 Span\<char const\> 作为输入参数，避免不必要的内存拷贝

## 调用上下文

压缩/解压不在 BackupEngine/RestoreEngine 内部执行，而是由 CLI 层（handleBackup/handleRestore）和 GUI 层（BackupWorker）在引擎调用前后独立处理：
- **备份时**：BackupEngine 先执行目录备份 → 生成归档文件 → transformFile() 对归档文件执行压缩（后压缩）
- **恢复时**：transformFile() 先对归档文件解压（预解压）→ 得到纯归档 → RestoreEngine 执行恢复

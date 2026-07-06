# 压缩解压 — 实施计划

## 功能概述

提供可插拔的压缩算法，在打包后/解包前对备份数据进行压缩和解压，节省存储空间。支持 gzip、zstd、LZMA 三种算法。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（压缩解压 / 每种算法 10 分）。

## 技术方案

### 架构设计

```
Compressor 接口 (compress / decompress)
    │
    ├── GzipCompressor    ← zlib (libz)
    ├── ZstdCompressor   ← zstd (libzstd)
    └── LzmaCompressor   ← liblzma (XZ Utils)
            │
            ▼
CompressorFactory (根据算法名创建)
```

### 基于缓冲区的接口设计

遵循 CLAUDE.md 规范，所有压缩器采用 `std::span` / `std::vector` 的缓冲区接口（非 `istream`/`ostream`），便于零拷贝和内存管理：

**压缩流程**：
```
源数据 (span) → 压缩器 → 压缩后数据 (vector) → 写入输出
```

**解压流程**：
```
压缩数据 (span) → 解压器 → 解压后数据 (vector) → 写入文件
```

### 各算法选型对比

| 特性 | gzip (zlib) | zstd (libzstd) | LZMA (liblzma) |
|------|-------------|----------------|-----------------|
| 压缩比 | 中等 (2-5x) | 中高 (3-8x) | 高 (5-12x) |
| 压缩速度 | 快 | 非常快 | 慢 |
| 解压速度 | 快 | 非常快 | 中等 |
| 内存占用 | 低 (~256KB) | 低 (~1MB) | 高 (~数百MB) |
| 库体积 | 小 | 小 | 中 |
| 适用场景 | 通用兼容 | 日常备份首选 | 高压缩比归档 |

### 压缩级别

| 算法 | 级别范围 | 默认级别 |
|------|---------|---------|
| gzip | 1-9 | 6 |
| zstd | 1-22 | 3 |
| LZMA | 1-9 | 6 |

### 工厂注册模式

```cpp
// 宏注册（静态初始化）
REGISTER_COMPRESSOR("gzip", GzipCompressor);
REGISTER_COMPRESSOR("zstd", ZstdCompressor);
REGISTER_COMPRESSOR("lzma", LzmaCompressor);

// 使用
auto comp = CompressorFactory::instance().create("zstd");
comp->compress(input_data, output_buffer);
```

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. Compressor 接口 | 定义 `compress()` / `decompress()` 流式接口 | `compressor.h` |
| 2. CompressorFactory | 工厂类 + 模板注册方法 | `compressor_factory.h/cpp` |
| 3. Gzip 实现 | zlib `deflate()`/`inflate()` 流式封装 | `gzip_compressor.h/cpp` |
| 4. Zstd 实现 | libzstd `ZSTD_compressStream()` 封装 | `zstd_compressor.h/cpp` |
| 5. LZMA 实现 | liblzma `lzma_stream_encoder`/`lzma_stream_decoder` 封装 | `lzma_compressor.h/cpp` |
| 6. CMake 集成 | find_package(zlib/zstd/liblzma) + 可选链接 | `CMakeLists.txt` |
| 7. CLI 集成 | `--compress gzip/zstd/lzma`, `--compress-level N` | `commands.cpp` |
| 8. 单元测试 | 各算法压缩→解压→验证原始数据 | 测试文件 |

## 关键接口

```cpp
class Compressor {
public:
    virtual ~Compressor() = default;

    // 缓冲区接口：输入 span，输出追加到 vector
    virtual std::expected<void, ErrorCode> compress(
        std::span<char const> input,
        std::vector<char>& output) = 0;
    virtual std::expected<void, ErrorCode> decompress(
        std::span<char const> input,
        std::vector<char>& output) = 0;
    virtual std::string_view name() const noexcept = 0;
    virtual int defaultLevel() const noexcept { return 6; }
};

class CompressorFactory {
public:
    static CompressorFactory& instance();

    template <std::derived_from<Compressor> T>
    bool registerCompressor(std::string_view name);

    std::unique_ptr<Compressor> create(std::string_view name);
    std::vector<std::string_view> availableCompressors() const;
};
```

### Gzip 实现示例

```cpp
class GzipCompressor : public Compressor {
public:
    explicit GzipCompressor(int level = 6) : level_(level) {}

    std::expected<void, ErrorCode> compress(
        std::span<char const> input,
        std::vector<char>& output) override
    {
        z_stream strm = {};
        deflateInit2(&strm, level_, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);

        strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
        strm.avail_in = input.size();

        // 预分配: deflateBound 估算最大输出
        output.resize(output.size() + deflateBound(&strm, input.size()));
        strm.next_out = reinterpret_cast<Bytef*>(output.data() + output.size() - deflateBound(&strm, input.size()));
        strm.avail_out = deflateBound(&strm, input.size());

        int ret = deflate(&strm, Z_FINISH);
        if (ret != Z_STREAM_END) {
            deflateEnd(&strm);
            return std::unexpected(ErrorCode::kCompressionFailed);
        }
        output.resize(output.size() - strm.avail_out); // 截断到实际输出大小
        deflateEnd(&strm);
        return {};
    }

    std::expected<void, ErrorCode> decompress(
        std::span<char const> input,
        std::vector<char>& output) override
    {
        z_stream strm = {};
        inflateInit2(&strm, 15 + 16);

        strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
        strm.avail_in = input.size();

        std::vector<char> buf(64 * 1024);
        do {
            strm.next_out = reinterpret_cast<Bytef*>(buf.data());
            strm.avail_out = buf.size();
            int ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&strm);
                return std::unexpected(ErrorCode::kCompressionFailed);
            }
            output.insert(output.end(), buf.data(), buf.data() + buf.size() - strm.avail_out);
        } while (strm.avail_out == 0);

        inflateEnd(&strm);
        return {};
    }

    std::string_view name() const noexcept override { return "gzip"; }

private:
    int level_;
};
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 基本压缩解压 | 各算法压缩→解压 | 输出 == 输入 |
| 空数据 | 0 字节数据 | 输出为空 |
| 大数据 (1GiB) | 流式处理大数据 | 无 OOM，结果一致 |
| 不同级别 | 每个算法测试 min/max/default 级别 | 正确压缩解压 |
| 损坏数据 | 修改压缩数据 | 解压抛出错误 |
| 管道组合 | Packer → Compressor → File | 管道完整工作 |
| 性能基准 | 相同数据对比三算法 | 记录压缩比/速度 |

## 关键文件

```
src/compress/
├── compressor.h                # Compressor 接口
├── compressor_factory.h
├── compressor_factory.cpp
├── gzip_compressor.h
├── gzip_compressor.cpp
├── zstd_compressor.h
├── zstd_compressor.cpp
├── lzma_compressor.h
└── lzma_compressor.cpp
src/cli/
└── commands.cpp                # --compress, --compress-level 参数
tests/compress/
├── gzip_compressor_test.cpp
├── zstd_compressor_test.cpp
├── lzma_compressor_test.cpp
└── compressor_factory_test.cpp
CMakeLists.txt                  # find_package(zstd) 等
```

## 依赖安装

```bash
# Ubuntu / Debian
sudo apt install zlib1g-dev libzstd-dev liblzma-dev

# Fedora / RHEL
sudo dnf install zlib-devel libzstd-devel xz-devel

# Arch Linux
sudo pacman -S zlib zstd xz
```

## 预计工作量

- 代码行数：~1000 行（含测试）
- 开发周期：5-7 天（三种算法可并行开发）
- 依赖：zlib + libzstd + liblzma

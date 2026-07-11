#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/span.h"

#include <string_view>
#include <vector>

namespace backer {

/// Abstract interface for compression / decompression algorithms.
///
/// All compressors use a buffer-based API (non-istream/ostream) to enable
/// zero-copy integration with the pack layer.
class Compressor {
public:
    virtual ~Compressor() = default;

    /// Compress input data, appending compressed bytes to @p output.
    /// @param input  Source bytes to compress.
    /// @param output Buffer to append compressed data to (grows in place).
    virtual Expected<void, ErrorCode> compress(
        backer::span<char const> input,
        std::vector<char>& output) = 0;

    /// Decompress input data, appending raw bytes to @p output.
    /// @param input  Compressed bytes to decompress.
    /// @param output Buffer to append decompressed data to (grows in place).
    virtual Expected<void, ErrorCode> decompress(
        backer::span<char const> input,
        std::vector<char>& output) = 0;

    /// Human-readable algorithm name, e.g. "gzip", "zstd", "lzma".
    virtual std::string_view name() const noexcept = 0;

    /// Default compression level for this algorithm.
    virtual int defaultLevel() const noexcept = 0;

    /// File-name suffix for this algorithm's compressed output
    /// (e.g. ".gz", ".zst", ".xz").
    virtual std::string_view suffix() const noexcept = 0;

    /// Validate compression level range.
    virtual bool isValidLevel(int level) const = 0;
};

} // namespace backer

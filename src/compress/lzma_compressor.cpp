#include "compress/lzma_compressor.h"

#include <lzma.h>

namespace backer {

LzmaCompressor::LzmaCompressor(int level) : level_(level) {}

bool LzmaCompressor::isValidLevel(int level) const {
    return level >= 0 && level <= 9;
}

Expected<void, ErrorCode> LzmaCompressor::compress(
    backer::span<char const> input, std::vector<char>& output)
{
    // LZMA preset: low 5 bits are the level (0-30), LZMA_PRESET_DEFAULT=6.
    // level_ directly encodes the user-facing 0-9; 0 means caller used default.
    unsigned int preset = static_cast<unsigned int>(level_);

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_easy_encoder(&strm, preset, LZMA_CHECK_CRC64);
    if (ret != LZMA_OK) {
        return ErrorCode::kCompressionFailed;
    }

    constexpr size_t kBufSize = 64 * 1024;
    // Reserve the worst-case compressed bound up front to avoid repeated
    // insert() reallocations as each chunk is flushed.
    output.reserve(output.size() + lzma_stream_buffer_bound(input.size()));
    std::vector<uint8_t> outBuf(kBufSize);

    strm.avail_in = static_cast<uint32_t>(input.size());
    strm.next_in = reinterpret_cast<uint8_t const*>(input.data());

    for (;;) {
        strm.avail_out = static_cast<uint32_t>(outBuf.size());
        strm.next_out = outBuf.data();
        ret = lzma_code(&strm, LZMA_FINISH);
        if (ret != LZMA_OK && ret != LZMA_BUF_ERROR && ret != LZMA_STREAM_END) {
            lzma_end(&strm);
            return ErrorCode::kCompressionFailed;
        }
        size_t wrote = outBuf.size() - static_cast<size_t>(strm.avail_out);
        if (wrote > 0) {
            output.insert(output.end(),
                          reinterpret_cast<char*>(outBuf.data()),
                          reinterpret_cast<char*>(outBuf.data()) + wrote);
        }
        if (ret == LZMA_STREAM_END) break;
    }

    lzma_end(&strm);
    return {};
}

Expected<void, ErrorCode> LzmaCompressor::decompress(
    backer::span<char const> input, std::vector<char>& output)
{
    if (input.empty()) {
        return ErrorCode::kInvalidArchive;
    }

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_auto_decoder(&strm, UINT64_MAX, 0);
    if (ret != LZMA_OK) {
        return ErrorCode::kCompressionFailed;
    }

    constexpr size_t kBufSize = 64 * 1024;
    // Heuristic pre-allocation: decompressed payload is typically several
    // times the compressed input.
    output.reserve(output.size() + input.size() * 4);
    std::vector<uint8_t> outBuf(kBufSize);
    strm.avail_in = static_cast<uint32_t>(input.size());
    strm.next_in = reinterpret_cast<uint8_t const*>(input.data());

    for (;;) {
        strm.avail_out = static_cast<uint32_t>(outBuf.size());
        strm.next_out = outBuf.data();
        ret = lzma_code(&strm, LZMA_FINISH);
        if (ret != LZMA_OK && ret != LZMA_BUF_ERROR && ret != LZMA_STREAM_END) {
            lzma_end(&strm);
            return ErrorCode::kCompressionFailed;
        }
        size_t wrote = outBuf.size() - static_cast<size_t>(strm.avail_out);
        if (wrote > 0) {
            output.insert(output.end(),
                          reinterpret_cast<char*>(outBuf.data()),
                          reinterpret_cast<char*>(outBuf.data()) + wrote);
        }
        if (ret == LZMA_STREAM_END) break;
        // Input exhausted without producing output → truncated/invalid stream.
        // Bail out to avoid an infinite loop on malformed input.
        if (strm.avail_in == 0 && wrote == 0) {
            lzma_end(&strm);
            return ErrorCode::kCompressionFailed;
        }
    }

    lzma_end(&strm);
    return {};
}

} // namespace backer

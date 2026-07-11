#include "compress/zstd_compressor.h"

#include <zstd.h>

namespace backer {

ZstdCompressor::ZstdCompressor(int level) : level_(level) {}

bool ZstdCompressor::isValidLevel(int level) const {
    // zstd's API exposes negative levels (fast presets down to -131072) and
    // positive levels up to 22. For our CLI we expose the user-facing 1..22
    // range and treat 0 (unset) as invalid here; callers pass defaultLevel().
    return level >= 1 && level <= ZSTD_maxCLevel();
}

Expected<void, ErrorCode> ZstdCompressor::compress(
    backer::span<char const> input, std::vector<char>& output)
{
    size_t const compSize = ZSTD_compressBound(input.size());
    size_t offset = output.size();
    output.resize(offset + compSize);

    size_t const ret = ZSTD_compress(output.data() + offset, compSize,
                                      input.data(), input.size(), level_);
    if (ZSTD_isError(ret)) {
        output.resize(offset);
        return ErrorCode::kCompressionFailed;
    }
    output.resize(offset + ret);
    return {};
}

Expected<void, ErrorCode> ZstdCompressor::decompress(
    backer::span<char const> input, std::vector<char>& output)
{
    // Empty zstd frame is represented as a 0-byte or magic-only payload
    if (input.empty()) {
        return {};
    }

    unsigned long long const decompressedSize =
        ZSTD_getFrameContentSize(input.data(), input.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
        return ErrorCode::kInvalidArchive;
    }

    // When the frame does not declare the original size, allocate a buffer
    // sized from a heuristic upper bound (≥4× input, at least the chunk floor)
    // and decompress in a single shot.
    if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        constexpr size_t kMinBufSize = 64 * 1024;
        size_t bufSize = std::max<size_t>(input.size() * 4, kMinBufSize);
        size_t offset = output.size();
        output.resize(offset + bufSize);
        size_t const decoded = ZSTD_decompress(output.data() + offset, bufSize,
                                                input.data(), input.size());
        if (ZSTD_isError(decoded)) {
            output.resize(offset);
            return ErrorCode::kCompressionFailed;
        }
        output.resize(offset + decoded);
        return {};
    }

    size_t offset = output.size();
    output.resize(offset + static_cast<size_t>(decompressedSize));
    size_t const decoded = ZSTD_decompress(output.data() + offset,
                                            static_cast<size_t>(decompressedSize),
                                            input.data(), input.size());
    if (ZSTD_isError(decoded)) {
        output.resize(offset);
        return ErrorCode::kCompressionFailed;
    }
    output.resize(offset + decoded);
    return {};
}

} // namespace backer

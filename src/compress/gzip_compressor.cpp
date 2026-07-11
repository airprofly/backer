#include "compress/gzip_compressor.h"

#include <zlib.h>

namespace backer {

namespace {
// 15-bit zlib window + 16 to request a gzip header/wrapper.
constexpr int kGzipWindowBits = 15 + 16;
constexpr size_t kChunkSize = 64 * 1024;
} // namespace

GzipCompressor::GzipCompressor(int level) : level_(level) {}

bool GzipCompressor::isValidLevel(int level) const {
    return level >= Z_NO_COMPRESSION && level <= Z_BEST_COMPRESSION;
}

Expected<void, ErrorCode> GzipCompressor::compress(
    backer::span<char const> input, std::vector<char>& output)
{
    if (input.empty()) {
        // Empty input → valid gzip stream with empty deflate block
        constexpr unsigned char empty_gzip[] = {
            0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        output.insert(output.end(), std::begin(empty_gzip), std::end(empty_gzip));
        return {};
    }

    z_stream strm{};
    int rc = deflateInit2(&strm, level_, Z_DEFLATED, kGzipWindowBits, 8,
                           Z_DEFAULT_STRATEGY);
    if (rc != Z_OK) {
        deflateEnd(&strm);
        return ErrorCode::kCompressionFailed;
    }

    size_t offset = output.size();
    uLong bound = deflateBound(&strm, static_cast<uLong>(input.size()));
    output.resize(offset + static_cast<size_t>(bound));

    strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());
    strm.next_out = reinterpret_cast<Bytef*>(output.data() + offset);
    strm.avail_out = static_cast<uInt>(bound);

    rc = deflate(&strm, Z_FINISH);
    if (rc != Z_STREAM_END) {
        deflateEnd(&strm);
        output.resize(offset);
        return ErrorCode::kCompressionFailed;
    }

    output.resize(offset + static_cast<size_t>(strm.total_out));
    deflateEnd(&strm);
    return {};
}

Expected<void, ErrorCode> GzipCompressor::decompress(
    backer::span<char const> input, std::vector<char>& output)
{
    if (input.empty()) {
        return ErrorCode::kInvalidArchive;
    }

    z_stream strm{};
    int rc = inflateInit2(&strm, kGzipWindowBits);
    if (rc != Z_OK) {
        inflateEnd(&strm);
        return ErrorCode::kCompressionFailed;
    }

    // Heuristic pre-allocation: decompressed payload is typically several
    // times the compressed input; this avoids repeated insert() reallocations.
    output.reserve(output.size() + input.size() * 4);

    strm.next_in  = const_cast<Bytef*>(reinterpret_cast<Bytef const*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());

    std::vector<char> buf(kChunkSize);
    do {
        strm.next_out = reinterpret_cast<Bytef*>(buf.data());
        strm.avail_out = static_cast<uInt>(buf.size());
        rc = inflate(&strm, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            inflateEnd(&strm);
            return ErrorCode::kCompressionFailed;
        }
        size_t wrote = buf.size() - static_cast<size_t>(strm.avail_out);
        output.insert(output.end(), buf.begin(),
                      buf.begin() + static_cast<std::ptrdiff_t>(wrote));
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    return {};
}

} // namespace backer

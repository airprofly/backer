#include "compress/build_compressor.h"
#include "compress/gzip_compressor.h"
#include "compress/lzma_compressor.h"
#include "compress/zstd_compressor.h"

namespace backer {

std::unique_ptr<Compressor> buildCompressor(std::string_view name, int level) {
    // Level 0 → default; consistent with CLI help text and the Compressor
    // interface's defaultLevel().
    if (name == "gzip") {
        return std::make_unique<GzipCompressor>(
            level == 0 ? GzipCompressor::kDefaultLevel : level);
    }
    if (name == "zstd") {
        return std::make_unique<ZstdCompressor>(
            level == 0 ? ZstdCompressor::kDefaultLevel : level);
    }
    if (name == "lzma") {
        return std::make_unique<LzmaCompressor>(
            level == 0 ? LzmaCompressor::kDefaultLevel : level);
    }
    return nullptr;
}

} // namespace backer

#pragma once

#include "compress/compressor.h"

namespace backer {

/// Zstandard compressor backed by libzstd.
class ZstdCompressor : public Compressor {
public:
    static constexpr int kDefaultLevel = 3;

    explicit ZstdCompressor(int level = kDefaultLevel);

    Expected<void, ErrorCode> compress(backer::span<char const> input,
                                        std::vector<char>& output) override;
    Expected<void, ErrorCode> decompress(backer::span<char const> input,
                                          std::vector<char>& output) override;
    std::string_view name() const noexcept override { return "zstd"; }
    int defaultLevel() const noexcept override { return kDefaultLevel; }
    std::string_view suffix() const noexcept override { return ".zst"; }
    bool isValidLevel(int level) const override;

private:
    int level_;
};

} // namespace backer

#pragma once

#include "compress/compressor.h"

namespace backer {

/// LZMA (XZ) compressor backed by liblzma.
class LzmaCompressor : public Compressor {
public:
    static constexpr int kDefaultLevel = 6;

    explicit LzmaCompressor(int level = kDefaultLevel);

    Expected<void, ErrorCode> compress(backer::span<char const> input,
                                        std::vector<char>& output) override;
    Expected<void, ErrorCode> decompress(backer::span<char const> input,
                                          std::vector<char>& output) override;
    std::string_view name() const noexcept override { return "lzma"; }
    int defaultLevel() const noexcept override { return kDefaultLevel; }
    std::string_view suffix() const noexcept override { return ".xz"; }
    bool isValidLevel(int level) const override;

private:
    int level_;
};

} // namespace backer

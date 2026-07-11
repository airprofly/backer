#pragma once

#include "compress/build_compressor.h"
#include "compress/compressor.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace backer::test {

/// Parameterized test fixture shared by all compressor implementations.
/// Each algorithm's test file instantiates it via INSTANTIATE_TEST_SUITE_P.
class CompressorTest : public ::testing::TestWithParam<std::string> {
protected:
    std::unique_ptr<Compressor> comp_;

    void SetUp() override {
        comp_ = buildCompressor(GetParam());
        ASSERT_NE(comp_, nullptr) << "Unknown compressor: " << GetParam();
    }

    /// Compress then decompress, asserting round-trip equality.
    void expectRoundTrip(std::string const& input) {
        std::vector<char> compressed;
        auto c = comp_->compress(backer::span<char const>(input.data(), input.size()),
                                 compressed);
        ASSERT_TRUE(c.has_value()) << "compress failed";

        std::vector<char> decompressed;
        auto d = comp_->decompress(backer::span<char const>(compressed.data(),
                                                             compressed.size()),
                                   decompressed);
        ASSERT_TRUE(d.has_value()) << "decompress failed";

        std::string result(decompressed.begin(), decompressed.end());
        EXPECT_EQ(result, input)
            << "round-trip mismatch (in=" << input.size() << " bytes)";
    }

    /// Generate a deterministic pseudo-random data blob of @p size bytes.
    static std::string makeBlob(size_t size) {
        std::string s;
        s.reserve(size);
        unsigned int state = 12345;
        for (size_t i = 0; i < size; ++i) {
            state = state * 1103515245u + 12345u;
            s.push_back(static_cast<char>('a' + (state >> 16) % 26));
        }
        return s;
    }

    /// Highly compressible data (repeated byte) of @p size.
    static std::string makeCompressible(size_t size) {
        return std::string(size, 'A');
    }
};

} // namespace backer::test

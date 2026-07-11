#include "compress/build_compressor.h"
#include "compress/gzip_compressor.h"
#include "compress/lzma_compressor.h"
#include "compress/zstd_compressor.h"
#include "compressor_test.h"

#include <gtest/gtest.h>

namespace backer::test {

// ── Basic round-trip: small data ───────────────────────────────────────

TEST_P(CompressorTest, RoundTripSmall) {
    expectRoundTrip("Hello, World!");
}

TEST_P(CompressorTest, RoundTripEmpty) {
    expectRoundTrip("");
}

TEST_P(CompressorTest, RoundTripSingleByte) {
    expectRoundTrip("X");
}

// ── Boundary sizes ─────────────────────────────────────────────────────

TEST_P(CompressorTest, RoundTrip1KB) {
    expectRoundTrip(makeBlob(1024));
}

TEST_P(CompressorTest, RoundTrip64KB) {
    expectRoundTrip(makeBlob(64 * 1024));
}

TEST_P(CompressorTest, RoundTrip1MiB) {
    expectRoundTrip(makeBlob(1024 * 1024));
}

// ── Highly compressible data ───────────────────────────────────────────

TEST_P(CompressorTest, RoundTripCompressible1MiB) {
    expectRoundTrip(makeCompressible(1024 * 1024));
}

// ── Binary data (all byte values) ──────────────────────────────────────

TEST_P(CompressorTest, RoundTripBinary) {
    std::string bin;
    bin.reserve(256 * 64);
    for (int i = 0; i < 256 * 64; ++i) {
        bin.push_back(static_cast<char>(i & 0xFF));
    }
    expectRoundTrip(bin);
}

// ── Level validation ───────────────────────────────────────────────────

TEST_P(CompressorTest, DefaultLevelIsValid) {
    EXPECT_TRUE(comp_->isValidLevel(comp_->defaultLevel()));
}

TEST_P(CompressorTest, NegativeLevelInvalid) {
    EXPECT_FALSE(comp_->isValidLevel(-1));
}

TEST_P(CompressorTest, ExtremeLevelInvalid) {
    EXPECT_FALSE(comp_->isValidLevel(1000));
}

// ── Factory respects level: higher level yields smaller output ──────────
// Regression guard: --compress-level must actually reach the compressor.

TEST_P(CompressorTest, FactoryAppliesCompressionLevel) {
    // Realistic text: has repetition but requires search effort to exploit,
    // so higher levels can still find more matches than level 1.
    // (Uniform data bottoms out at level 1, which would mask the effect.)
    std::string data;
    data.reserve(256 * 1024);
    for (int i = 0; i < 20000; ++i) {
        data += "The quick brown fox jumps over the lazy dog. line ";
        data += std::to_string(i % 64);
        data += '\n';
    }

    auto low = buildCompressor(GetParam(), 1);
    auto high = buildCompressor(GetParam(),
        (GetParam() == "zstd") ? 19 : 9);
    ASSERT_NE(low, nullptr);
    ASSERT_NE(high, nullptr);

    std::vector<char> outLow, outHigh;
    ASSERT_TRUE(low->compress(
        backer::span<char const>(data.data(), data.size()), outLow).has_value());
    ASSERT_TRUE(high->compress(
        backer::span<char const>(data.data(), data.size()), outHigh).has_value());

    // Higher level must produce a strictly smaller stream than level 1.
    // If equal, the level was ignored (regression).
    EXPECT_LT(outHigh.size(), outLow.size())
        << GetParam() << ": high level should compress better than level 1";
}

// ── Decompress corrupt data fails ──────────────────────────────────────

TEST_P(CompressorTest, DecompressGarbageFails) {
    std::string garbage = "not a valid compressed stream!!";
    std::vector<char> out;
    auto r = comp_->decompress(
        backer::span<char const>(garbage.data(), garbage.size()), out);
    EXPECT_FALSE(r.has_value());
}

TEST_P(CompressorTest, DecompressTruncatedFails) {
    std::vector<char> compressed;
    auto c = comp_->compress(
        backer::span<char const>(makeBlob(4096).data(), 4096), compressed);
    ASSERT_TRUE(c.has_value());
    ASSERT_GT(compressed.size(), 10u);

    // Truncate to half the size → should fail or produce garbage (but not crash)
    compressed.resize(compressed.size() / 2);
    std::vector<char> out;
    comp_->decompress(
        backer::span<char const>(compressed.data(), compressed.size()), out);
    // We only assert no crash; correctness is not guaranteed for truncated input.
}

// ── Appending: output grows in place ───────────────────────────────────

TEST_P(CompressorTest, CompressAppendsToExistingOutput) {
    std::vector<char> output;
    output.push_back('P'); // pre-existing content
    output.push_back('R');
    size_t prefix = output.size();

    std::string data = "appending test data for compression";
    auto c = comp_->compress(
        backer::span<char const>(data.data(), data.size()), output);
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(output[0], 'P');
    EXPECT_EQ(output[1], 'R');
    EXPECT_GT(output.size(), prefix);
}

INSTANTIATE_TEST_SUITE_P(AllCompressors, CompressorTest,
    ::testing::Values("gzip", "zstd", "lzma"));

} // namespace backer::test

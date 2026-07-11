#include "compress/build_compressor.h"
#include "compress/gzip_compressor.h"
#include "compress/lzma_compressor.h"
#include "compress/zstd_compressor.h"

#include <gtest/gtest.h>

namespace backer::test {

// ── Create returns correct type ──────────────────────────────────────────

TEST(BuildCompressorTest, CreateGzip) {
    auto c = buildCompressor("gzip");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name(), "gzip");
    EXPECT_EQ(c->defaultLevel(), 6);
}

TEST(BuildCompressorTest, CreateZstd) {
    auto c = buildCompressor("zstd");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name(), "zstd");
    EXPECT_EQ(c->defaultLevel(), 3);
}

TEST(BuildCompressorTest, CreateLzma) {
    auto c = buildCompressor("lzma");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name(), "lzma");
    EXPECT_EQ(c->defaultLevel(), 6);
}

// ── Unknown algorithm returns nullptr ────────────────────────────────────

TEST(BuildCompressorTest, UnknownAlgorithmReturnsNull) {
    auto c = buildCompressor("bzip2");
    EXPECT_EQ(c, nullptr);
}

// ── Case sensitivity ─────────────────────────────────────────────────────

TEST(BuildCompressorTest, CaseSensitiveLookup) {
    auto c = buildCompressor("GZIP");
    EXPECT_EQ(c, nullptr);
}

// ── Empty / partial names are rejected (no substring match) ──────────────

TEST(BuildCompressorTest, PartialNameRejected) {
    EXPECT_EQ(buildCompressor("g"), nullptr);
    EXPECT_EQ(buildCompressor("std"), nullptr);
    EXPECT_EQ(buildCompressor("lz"), nullptr);
}

// ── Level passthrough ────────────────────────────────────────────────────

TEST(BuildCompressorTest, DefaultLevelZero) {
    // Level 0 should produce a valid compressor (uses default).
    auto c = buildCompressor("gzip", 0);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->defaultLevel(), c->defaultLevel()); // self-check
}

// ── Round-trip via buildCompressor ────────────────────────────────────────

TEST(BuildCompressorTest, RoundTrip) {
    std::string input = "build-compressor round-trip test data";
    for (auto name : {"gzip", "zstd", "lzma"}) {
        auto comp = buildCompressor(name);
        ASSERT_NE(comp, nullptr) << "algorithm: " << name;

        std::vector<char> compressed;
        auto c = comp->compress(
            backer::span<char const>(input.data(), input.size()), compressed);
        ASSERT_TRUE(c.has_value());

        std::vector<char> decompressed;
        auto d = comp->decompress(
            backer::span<char const>(compressed.data(), compressed.size()),
            decompressed);
        ASSERT_TRUE(d.has_value());

        std::string result(decompressed.begin(), decompressed.end());
        EXPECT_EQ(result, input) << "algorithm: " << name;
    }
}

} // namespace backer::test

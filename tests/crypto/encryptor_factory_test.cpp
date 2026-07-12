#include "crypto/build_encryptor.h"

#include <gtest/gtest.h>

namespace backer::test {

// ── Create returns correct type ──────────────────────────────────────────────

TEST(BuildEncryptorTest, CreateAes256) {
    auto e = buildEncryptor("aes256");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->name(), "aes256");
    EXPECT_EQ(e->suffix(), ".enc");
}

TEST(BuildEncryptorTest, CreateAes256FullName) {
    auto e = buildEncryptor("aes-256-gcm");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->name(), "aes256");
}

TEST(BuildEncryptorTest, CreateSm4) {
    auto e = buildEncryptor("sm4");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->name(), "sm4");
    EXPECT_EQ(e->suffix(), ".enc");
}

// ── Unknown algorithm returns nullptr ────────────────────────────────────────

TEST(BuildEncryptorTest, UnknownAlgorithmReturnsNull) {
    auto e = buildEncryptor("nonexistent");
    EXPECT_EQ(e, nullptr);
}

TEST(BuildEncryptorTest, UnknownAlgorithmEmpty) {
    auto e = buildEncryptor("");
    EXPECT_EQ(e, nullptr);
}

// ── Case sensitivity ─────────────────────────────────────────────────────────

TEST(BuildEncryptorTest, CaseSensitiveLookup) {
    auto e = buildEncryptor("AES256");
    EXPECT_EQ(e, nullptr);
}

// ── Partial names are rejected ───────────────────────────────────────────────

TEST(BuildEncryptorTest, PartialNameRejected) {
    EXPECT_EQ(buildEncryptor("aes"), nullptr);
    EXPECT_EQ(buildEncryptor("sm"), nullptr);
    EXPECT_EQ(buildEncryptor("aes-256"), nullptr);
}

// ── Round-trip via buildEncryptor ────────────────────────────────────────────

TEST(BuildEncryptorTest, RoundTrip) {
    std::string input = "build-encryptor round-trip test data";
    std::string password = "factory-test-pwd";

    for (auto name : {"aes256", "sm4"}) {
        auto enc = buildEncryptor(name);
        ASSERT_NE(enc, nullptr) << "algorithm: " << name;

        std::vector<char> encrypted;
        auto e = enc->encrypt(
            backer::span<char const>(input.data(), input.size()),
            encrypted, password);
        ASSERT_TRUE(e.has_value()) << "encrypt failed for " << name;

        std::vector<char> decrypted;
        auto d = enc->decrypt(
            backer::span<char const>(encrypted.data(), encrypted.size()),
            decrypted, password);
        ASSERT_TRUE(d.has_value()) << "decrypt failed for " << name;

        std::string result(decrypted.begin(), decrypted.end());
        EXPECT_EQ(result, input) << "algorithm: " << name;
    }
}

} // namespace backer::test

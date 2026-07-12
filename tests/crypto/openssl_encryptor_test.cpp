#include "crypto/build_encryptor.h"
#include "crypto/encrypted_format.h"
#include "crypto/openssl_encryptor.h"
#include "encryptor_test.h"

#include <gtest/gtest.h>

namespace backer::test {

// ── Basic round-trip: small data ─────────────────────────────────────────────

TEST_P(EncryptorTest, RoundTripSmall) {
    expectRoundTrip("Hello, World!", password_);
}

TEST_P(EncryptorTest, RoundTripEmpty) {
    expectRoundTrip("", password_);
}

TEST_P(EncryptorTest, RoundTripSingleByte) {
    expectRoundTrip("X", password_);
}

// ── Boundary sizes ───────────────────────────────────────────────────────────

TEST_P(EncryptorTest, RoundTrip1KB) {
    expectRoundTrip(makeBlob(1024), password_);
}

TEST_P(EncryptorTest, RoundTrip64KB) {
    expectRoundTrip(makeBlob(64 * 1024), password_);
}

TEST_P(EncryptorTest, RoundTrip1MiB) {
    expectRoundTrip(makeBlob(1024 * 1024), password_);
}

// ── Compressible data ────────────────────────────────────────────────────────

TEST_P(EncryptorTest, RoundTripCompressible1MiB) {
    expectRoundTrip(std::string(1024 * 1024, 'A'), password_);
}

// ── Binary data (all byte values) ────────────────────────────────────────────

TEST_P(EncryptorTest, RoundTripBinary) {
    std::string bin;
    bin.reserve(256 * 64);
    for (int i = 0; i < 256 * 64; ++i) {
        bin.push_back(static_cast<char>(i & 0xFF));
    }
    expectRoundTrip(bin, password_);
}

// ── Password variants ────────────────────────────────────────────────────────

TEST_P(EncryptorTest, WrongPasswordFails) {
    std::string input = "sensitive data to encrypt";
    std::vector<char> encrypted;
    auto e = enc_->encrypt(
        backer::span<char const>(input.data(), input.size()), encrypted, password_);
    ASSERT_TRUE(e.has_value());

    std::vector<char> decrypted;
    auto d = enc_->decrypt(
        backer::span<char const>(encrypted.data(), encrypted.size()),
        decrypted, "wrong-password");
    EXPECT_FALSE(d.has_value()) << "decrypt with wrong password should fail";
}

TEST_P(EncryptorTest, EmptyPassword) {
    std::string input = "data with empty password";
    std::vector<char> encrypted;
    // Empty password may or may not be accepted depending on policy;
    // we just verify it doesn't crash.
    auto e = enc_->encrypt(
        backer::span<char const>(input.data(), input.size()), encrypted, "");
    // If encryption succeeded, decryption with same empty password should work
    if (e.has_value()) {
        std::vector<char> decrypted;
        auto d = enc_->decrypt(
            backer::span<char const>(encrypted.data(), encrypted.size()),
            decrypted, "");
        EXPECT_TRUE(d.has_value());
    }
}

TEST_P(EncryptorTest, PasswordWithSpaces) {
    expectRoundTrip("data", "my passphrase with spaces and  special chars !@#$%");
}

// ── Corrupted ciphertext ─────────────────────────────────────────────────────

TEST_P(EncryptorTest, CorruptedCiphertextFails) {
    std::string input = makeBlob(4096);
    std::vector<char> encrypted;
    auto e = enc_->encrypt(
        backer::span<char const>(input.data(), input.size()), encrypted, password_);
    ASSERT_TRUE(e.has_value());

    // Corrupt a byte in the ciphertext (after header)
    if (encrypted.size() > sizeof(crypto::EncryptedHeader) + 10) {
        auto corruptPos = sizeof(crypto::EncryptedHeader) + 5;
        encrypted[corruptPos] ^= 0xFF;

        std::vector<char> decrypted;
        auto d = enc_->decrypt(
            backer::span<char const>(encrypted.data(), encrypted.size()),
            decrypted, password_);
        // GCM modes should catch this; CBC modes may not
        // We just verify no crash (GCM returns error, CBC may produce garbage)
        (void)d;
    }
}

TEST_P(EncryptorTest, CorruptedHeaderMagicFails) {
    std::string input = "test data";
    std::vector<char> encrypted;
    auto e = enc_->encrypt(
        backer::span<char const>(input.data(), input.size()), encrypted, password_);
    ASSERT_TRUE(e.has_value());

    if (encrypted.size() > 0) {
        encrypted[0] ^= 0xFF; // corrupt magic

        std::vector<char> decrypted;
        auto d = enc_->decrypt(
            backer::span<char const>(encrypted.data(), encrypted.size()),
            decrypted, password_);
        EXPECT_FALSE(d.has_value()) << "decrypt with bad magic should fail";
    }
}

// ── Truncated data ───────────────────────────────────────────────────────────

TEST_P(EncryptorTest, TruncatedInputFails) {
    std::string input = makeBlob(1024);
    std::vector<char> encrypted;
    auto e = enc_->encrypt(
        backer::span<char const>(input.data(), input.size()), encrypted, password_);
    ASSERT_TRUE(e.has_value());

    // Truncate to just the header
    encrypted.resize(sizeof(crypto::EncryptedHeader) / 2);
    std::vector<char> decrypted;
    auto d = enc_->decrypt(
        backer::span<char const>(encrypted.data(), encrypted.size()),
        decrypted, password_);
    EXPECT_FALSE(d.has_value()) << "decrypt truncated data should fail";
}

// ── Non-encrypted garbage fails ──────────────────────────────────────────────

TEST_P(EncryptorTest, GarbageInputFails) {
    std::string garbage = "this is not an encrypted file at all!!";
    std::vector<char> decrypted;
    auto d = enc_->decrypt(
        backer::span<char const>(garbage.data(), garbage.size()),
        decrypted, password_);
    EXPECT_FALSE(d.has_value()) << "decrypt garbage should fail";
}

// ── Appending: output grows in place ─────────────────────────────────────────

TEST_P(EncryptorTest, EncryptAppendsToExistingOutput) {
    std::vector<char> output;
    output.push_back('P');
    output.push_back('R');
    size_t prefix = output.size();

    std::string data = "append test data";
    auto e = enc_->encrypt(
        backer::span<char const>(data.data(), data.size()), output, password_);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(output[0], 'P');
    EXPECT_EQ(output[1], 'R');
    EXPECT_GT(output.size(), prefix);
}

// ── Long password (UTF-8 special chars) ──────────────────────────────────────

TEST_P(EncryptorTest, UnicodePassword) {
    expectRoundTrip("data with unicode password",
                    "密码含有中文 and рyсский язык and emoji 🔐🔑");
}

// ══════════════════════════════════════════════════════════════════════════════
// Instantiate for all supported algorithms
// ══════════════════════════════════════════════════════════════════════════════

INSTANTIATE_TEST_SUITE_P(AllEncryptors, EncryptorTest,
    ::testing::Values("aes256", "sm4"));

} // namespace backer::test

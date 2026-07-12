#pragma once

#include "crypto/build_encryptor.h"
#include "crypto/encryptor.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace backer::test {

/// Parameterized test fixture shared by all encryptor implementations.
class EncryptorTest : public ::testing::TestWithParam<std::string> {
protected:
    std::unique_ptr<Encryptor> enc_;
    std::string password_ = "test-password-123";

    void SetUp() override {
        enc_ = buildEncryptor(GetParam());
        ASSERT_NE(enc_, nullptr) << "Unknown encryptor: " << GetParam();
    }

    /// Encrypt then decrypt, asserting round-trip equality.
    void expectRoundTrip(std::string const& input, std::string_view password) {
        std::vector<char> encrypted;
        auto e = enc_->encrypt(backer::span<char const>(input.data(), input.size()),
                                encrypted, password);
        ASSERT_TRUE(e.has_value()) << "encrypt failed for " << enc_->name();

        std::vector<char> decrypted;
        auto d = enc_->decrypt(backer::span<char const>(encrypted.data(), encrypted.size()),
                                decrypted, password);
        ASSERT_TRUE(d.has_value()) << "decrypt failed for " << enc_->name();

        std::string result(decrypted.begin(), decrypted.end());
        EXPECT_EQ(result, input)
            << "round-trip mismatch (in=" << input.size() << " bytes) for " << enc_->name();
    }

    /// Generate a deterministic pseudo-random data blob.
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
};

} // namespace backer::test

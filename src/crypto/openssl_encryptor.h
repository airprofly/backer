#pragma once

#include "crypto/encryptor.h"
#include "crypto/encrypted_format.h"

#include <openssl/evp.h>

#include <string>
#include <string_view>

namespace backer {

/// Encryptor implementation backed by OpenSSL EVP.
///
/// Supports AES-256-GCM (default) and SM4-CBC / SM4-GCM (OpenSSL 3.0+).
/// Key derivation uses PBKDF2 with SHA-256 (AES) or SM3 (SM4).
class OpenSslEncryptor : public Encryptor {
public:
    /// @param algorithm  Algorithm name: "aes256" (AES-256-GCM),
    ///                   "sm4" (SM4-CBC), "sm4-gcm" (SM4-GCM).
    explicit OpenSslEncryptor(std::string_view algorithm);

    Expected<void, ErrorCode> encrypt(
        backer::span<char const> input,
        std::vector<char>& output,
        std::string_view password) override;

    Expected<void, ErrorCode> decrypt(
        backer::span<char const> input,
        std::vector<char>& output,
        std::string_view password) override;

    std::string_view name() const noexcept override { return name_; }
    std::string_view suffix() const noexcept override { return ".enc"; }

private:
    crypto::Algorithm algo_;
    std::string name_;

    /// Key length in bytes (32 for AES-256, 16 for SM4).
    static size_t keyLength(crypto::Algorithm algo) noexcept;

    /// IV length in bytes (12 for GCM, 16 for CBC).
    static size_t ivLength(crypto::Algorithm algo) noexcept;

    /// Whether this algorithm uses GCM mode (has authentication tag).
    static bool isGcmMode(crypto::Algorithm algo) noexcept;

    /// Whether to use SM3 digest instead of SHA-256 for PBKDF2.
    static bool useSm3(crypto::Algorithm algo) noexcept;

    /// Get the EVP_CIPHER for this algorithm.
    static EVP_CIPHER const* cipher(crypto::Algorithm algo) noexcept;
};

} // namespace backer

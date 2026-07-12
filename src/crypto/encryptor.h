#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/span.h"

#include <string_view>
#include <vector>

namespace backer {

/// Abstract interface for encryption / decryption algorithms.
///
/// All encryptors use a buffer-based API to integrate with the pack/compress
/// pipeline. Passwords are provided per-call; the implementation derives the
/// key via PBKDF2 on each invocation.
class Encryptor {
public:
    virtual ~Encryptor() = default;

    /// Encrypt @p input, appending ciphertext (with header + auth tag)
    /// to @p output.
    /// @param input   Plaintext bytes to encrypt.
    /// @param output  Buffer to append ciphertext to (grows in place).
    /// @param password User-provided passphrase for key derivation.
    virtual Expected<void, ErrorCode> encrypt(
        backer::span<char const> input,
        std::vector<char>& output,
        std::string_view password) = 0;

    /// Decrypt @p input, appending recovered plaintext to @p output.
    /// @param input   Ciphertext (with header) to decrypt.
    /// @param output  Buffer to append plaintext to (grows in place).
    /// @param password User-provided passphrase for key derivation.
    virtual Expected<void, ErrorCode> decrypt(
        backer::span<char const> input,
        std::vector<char>& output,
        std::string_view password) = 0;

    /// Human-readable algorithm name, e.g. "aes256", "sm4".
    virtual std::string_view name() const noexcept = 0;

    /// File-name suffix for this algorithm's encrypted output (e.g. ".enc").
    virtual std::string_view suffix() const noexcept = 0;
};

} // namespace backer

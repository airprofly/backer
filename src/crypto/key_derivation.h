#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "core/span.h"

#include <string_view>
#include <vector>

namespace backer::crypto {

/// PBKDF2-based key derivation using OpenSSL.
///
/// All derived keys are zeroed from memory via OPENSSL_cleanse when the
/// returned vector goes out of scope.
class KeyDerivation {
public:
    /// Derive a key of @p keyLength bytes from @p password and @p salt.
    ///
    /// @param password   User-provided passphrase.
    /// @param salt       Cryptographic salt (random per encryption).
    /// @param iterations PBKDF2 iteration count (≥ 100000 recommended).
    /// @param keyLength  Desired output key length in bytes.
    /// @param useSm3     If true, use SM3 digest; otherwise use SHA-256.
    /// @return           Derived key on success, ErrorCode on failure.
    static Expected<std::vector<char>, ErrorCode> deriveKey(
        std::string_view password,
        backer::span<char const> salt,
        int iterations,
        size_t keyLength,
        bool useSm3 = false);

    /// Generate @p length cryptographically-secure random bytes.
    static Expected<std::vector<char>, ErrorCode> randomBytes(size_t length);
};

} // namespace backer::crypto

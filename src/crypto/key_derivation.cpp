#include "crypto/key_derivation.h"
#include "crypto/encrypted_format.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>
#include <spdlog/spdlog.h>

namespace backer::crypto {

Expected<std::vector<char>, ErrorCode> KeyDerivation::deriveKey(
    std::string_view password,
    backer::span<char const> salt,
    int iterations,
    size_t keyLength,
    bool useSm3)
{
    if (password.empty()) {
        spdlog::error("Key derivation: password is empty");
        return ErrorCode::kEncryptionFailed;
    }
    if (salt.size() < 1) {
        spdlog::error("Key derivation: salt is empty");
        return ErrorCode::kEncryptionFailed;
    }
    if (iterations < 1) {
        spdlog::error("Key derivation: iterations must be >= 1");
        return ErrorCode::kEncryptionFailed;
    }

    const EVP_MD* digest = useSm3 ? EVP_sm3() : EVP_sha256();
    if (digest == nullptr) {
        spdlog::error("Key derivation: unsupported digest (SM3 not available?)");
        return ErrorCode::kEncryptionFailed;
    }

    std::vector<char> key(keyLength);

    int rc = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.size()),
        reinterpret_cast<unsigned char const*>(salt.data()),
        static_cast<int>(salt.size()),
        iterations,
        digest,
        static_cast<int>(keyLength),
        reinterpret_cast<unsigned char*>(key.data()));

    if (rc != 1) {
        spdlog::error("Key derivation: PBKDF2 failed");
        return ErrorCode::kEncryptionFailed;
    }

    return key;
}

Expected<std::vector<char>, ErrorCode> KeyDerivation::randomBytes(size_t length) {
    std::vector<char> buf(length);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(buf.data()),
                   static_cast<int>(buf.size())) != 1) {
        spdlog::error("Key derivation: RAND_bytes failed");
        return ErrorCode::kEncryptionFailed;
    }
    return buf;
}

} // namespace backer::crypto

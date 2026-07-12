#include "crypto/openssl_encryptor.h"
#include "crypto/key_derivation.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <cstring>
#include <spdlog/spdlog.h>

namespace backer {

// ── RAII wrapper for EVP_CIPHER_CTX ────────────────────────────────────────────

namespace {
struct CipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX* ctx) const noexcept {
        if (ctx) EVP_CIPHER_CTX_free(ctx);
    }
};
using CipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, CipherCtxDeleter>;

/// Log the last OpenSSL error and return kEncryptionFailed.
ErrorCode logOpenSslError() {
    std::array<char, 256> buf{};
    unsigned long err = ERR_get_error();
    if (err != 0) {
        ERR_error_string_n(err, buf.data(), buf.size());
        spdlog::error("OpenSSL error: {}", buf.data());
    } else {
        spdlog::error("OpenSSL operation failed (no queued error)");
    }
    return ErrorCode::kEncryptionFailed;
}

/// Create a random-filled EncryptedHeader for the given algorithm.
crypto::EncryptedHeader createHeader(crypto::Algorithm algo) {
    crypto::EncryptedHeader header{};

    // Magic
    std::memcpy(header.magic, crypto::kMagic.data(), crypto::kMagicSize);

    // Algorithm
    header.algorithm = static_cast<uint8_t>(algo);

    // Random salt
    RAND_bytes(header.salt, static_cast<int>(sizeof(header.salt)));

    // Iterations (little-endian)
    header.iterations = static_cast<uint32_t>(crypto::kDefaultIterations);

    // Random IV — full 16 bytes; GCM modes use first 12, CBC uses all 16
    RAND_bytes(header.iv, static_cast<int>(sizeof(header.iv)));

    return header;
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════════
// OpenSslEncryptor
// ══════════════════════════════════════════════════════════════════════════════

OpenSslEncryptor::OpenSslEncryptor(std::string_view algorithm) {
    bool ok = false;
    algo_ = crypto::algorithmFromName(algorithm, ok);
    if (!ok) {
        // Default to AES-256-GCM for unknown names
        algo_ = crypto::Algorithm::kAes256Gcm;
        name_ = "aes256";
    } else {
        name_ = std::string(crypto::algorithmName(algo_));
    }
}

size_t OpenSslEncryptor::keyLength(crypto::Algorithm algo) noexcept {
    switch (algo) {
        case crypto::Algorithm::kAes256Gcm: return crypto::kAes256KeyLength; // 32
        case crypto::Algorithm::kSm4Cbc:    return crypto::kSm4KeyLength;    // 16
    }
    return crypto::kAes256KeyLength;
}

size_t OpenSslEncryptor::ivLength(crypto::Algorithm algo) noexcept {
    switch (algo) {
        case crypto::Algorithm::kAes256Gcm: return crypto::kGcmIvLength; // 12
        case crypto::Algorithm::kSm4Cbc:    return crypto::kCbcIvLength; // 16
    }
    return crypto::kGcmIvLength;
}

bool OpenSslEncryptor::isGcmMode(crypto::Algorithm algo) noexcept {
    return algo == crypto::Algorithm::kAes256Gcm;
}

bool OpenSslEncryptor::useSm3(crypto::Algorithm algo) noexcept {
    return algo == crypto::Algorithm::kSm4Cbc;
}

EVP_CIPHER const* OpenSslEncryptor::cipher(crypto::Algorithm algo) noexcept {
    switch (algo) {
        case crypto::Algorithm::kAes256Gcm: return EVP_aes_256_gcm();
        case crypto::Algorithm::kSm4Cbc:    return EVP_sm4_cbc();
    }
    return EVP_aes_256_gcm();
}

Expected<void, ErrorCode> OpenSslEncryptor::encrypt(
    backer::span<char const> input,
    std::vector<char>& output,
    std::string_view password)
{
    // ── 1. Generate header with random salt + IV ──────────────────────────
    auto const algo = algo_; // local copy for helpers
    auto header = createHeader(algo);
    auto const ivLen = ivLength(algo);
    auto const keyLen = keyLength(algo);

    // ── 2. Derive key via PBKDF2 ──────────────────────────────────────────
    auto saltSpan = backer::span<char const>(
        reinterpret_cast<char const*>(header.salt), sizeof(header.salt));

    std::vector<char> key;
    auto keyResult = crypto::KeyDerivation::deriveKey(
        password, saltSpan, crypto::kDefaultIterations,
        keyLen, useSm3(algo));

    if (!keyResult.has_value()) {
        return keyResult.error();
    }
    key = std::move(keyResult.value());

    // ── 3. Initialize encryption context ──────────────────────────────────
    CipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        return logOpenSslError();
    }

    auto ciph = cipher(algo);
    if (ciph == nullptr) {
        spdlog::error("Encryption: unsupported cipher (not available in this OpenSSL build)");
        return ErrorCode::kEncryptionFailed;
    }

    // For GCM, set IV length (default is 12, but be explicit)
    if (isGcmMode(algo)) {
        if (EVP_EncryptInit_ex(ctx.get(), ciph, nullptr, nullptr, nullptr) != 1) {
            return logOpenSslError();
        }
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(ivLen), nullptr) != 1) {
            return logOpenSslError();
        }
        if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
                               reinterpret_cast<unsigned char const*>(key.data()),
                               reinterpret_cast<unsigned char const*>(header.iv)) != 1) {
            return logOpenSslError();
        }
    } else {
        // CBC mode — initialize in one shot
        if (EVP_EncryptInit_ex(ctx.get(), ciph, nullptr,
                               reinterpret_cast<unsigned char const*>(key.data()),
                               reinterpret_cast<unsigned char const*>(header.iv)) != 1) {
            return logOpenSslError();
        }
    }

    // ── 4. Write header to output ─────────────────────────────────────────
    auto headerBytes = reinterpret_cast<char const*>(&header);
    output.insert(output.end(), headerBytes, headerBytes + sizeof(header));

    // ── 5. Encrypt input data ─────────────────────────────────────────────
    std::vector<char> ciphertext(input.size() + EVP_MAX_BLOCK_LENGTH);
    int outLen = 0;

    if (!input.empty()) {
        if (EVP_EncryptUpdate(ctx.get(),
                              reinterpret_cast<unsigned char*>(ciphertext.data()),
                              &outLen,
                              reinterpret_cast<unsigned char const*>(input.data()),
                              static_cast<int>(input.size())) != 1) {
            return logOpenSslError();
        }
        output.insert(output.end(), ciphertext.data(), ciphertext.data() + outLen);
    }

    // ── 6. Finalize (flush remaining blocks) ──────────────────────────────
    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx.get(),
                            reinterpret_cast<unsigned char*>(ciphertext.data()),
                            &finalLen) != 1) {
        return logOpenSslError();
    }
    if (finalLen > 0) {
        output.insert(output.end(), ciphertext.data(), ciphertext.data() + finalLen);
    }

    // ── 7. Append GCM authentication tag (GCM modes only) ─────────────────
    if (isGcmMode(algo)) {
        std::array<unsigned char, crypto::kGcmTagLength> tag{};
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
                                static_cast<int>(tag.size()), tag.data()) != 1) {
            return logOpenSslError();
        }
        output.insert(output.end(),
                      reinterpret_cast<char const*>(tag.data()),
                      reinterpret_cast<char const*>(tag.data()) + tag.size());
    }

    // Clear key material
    std::fill(key.begin(), key.end(), '\0');

    return {};
}

Expected<void, ErrorCode> OpenSslEncryptor::decrypt(
    backer::span<char const> input,
    std::vector<char>& output,
    std::string_view password)
{
    // ── 1. Minimum size check ─────────────────────────────────────────────
    if (input.size() < static_cast<ptrdiff_t>(sizeof(crypto::EncryptedHeader))) {
        spdlog::error("Decryption: input too small for header ({} bytes)", input.size());
        return ErrorCode::kEncryptionFailed;
    }

    // ── 2. Parse header ──────────────────────────────────────────────────
    auto const* header = reinterpret_cast<crypto::EncryptedHeader const*>(input.data());
    if (!crypto::isValidMagic(*header)) {
        spdlog::error("Decryption: invalid magic bytes — not an encrypted file");
        return ErrorCode::kEncryptionFailed;
    }

    auto headerAlgo = static_cast<crypto::Algorithm>(header->algorithm);
    // Verify algorithm is known
    switch (headerAlgo) {
        case crypto::Algorithm::kAes256Gcm:
        case crypto::Algorithm::kSm4Cbc:
            break;
        default:
            spdlog::error("Decryption: unknown algorithm byte {}", header->algorithm);
            return ErrorCode::kEncryptionFailed;
    }

    auto const ivLen = ivLength(headerAlgo);
    auto const keyLen = keyLength(headerAlgo);
    bool const gcmMode = isGcmMode(headerAlgo);

    // ── 3. Separate header, ciphertext, and tag ──────────────────────────
    auto headerSize = static_cast<ptrdiff_t>(sizeof(crypto::EncryptedHeader));
    auto cipherAndTag = input.data() + headerSize;
    auto cipherAndTagLen = input.size() - headerSize;

    ptrdiff_t tagSize = gcmMode ? static_cast<ptrdiff_t>(crypto::kGcmTagLength) : 0;
    if (tagSize > cipherAndTagLen) {
        spdlog::error("Decryption: input too short for GCM auth tag");
        return ErrorCode::kEncryptionFailed;
    }

    auto cipherLen = cipherAndTagLen - tagSize;
    auto const* tag = gcmMode ? cipherAndTag + cipherLen : nullptr;

    // ── 4. Derive key via PBKDF2 ──────────────────────────────────────────
    auto saltSpan = backer::span<char const>(
        reinterpret_cast<char const*>(header->salt), sizeof(header->salt));

    std::vector<char> key;
    auto keyResult = crypto::KeyDerivation::deriveKey(
        password, saltSpan, static_cast<int>(header->iterations),
        keyLen, useSm3(headerAlgo));

    if (!keyResult.has_value()) {
        return keyResult.error();
    }
    key = std::move(keyResult.value());

    // ── 5. Initialize decryption context ──────────────────────────────────
    CipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        return logOpenSslError();
    }

    auto ciph = cipher(headerAlgo);

    if (gcmMode) {
        if (EVP_DecryptInit_ex(ctx.get(), ciph, nullptr, nullptr, nullptr) != 1) {
            return logOpenSslError();
        }
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(ivLen), nullptr) != 1) {
            return logOpenSslError();
        }
        if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
                               reinterpret_cast<unsigned char const*>(key.data()),
                               reinterpret_cast<unsigned char const*>(header->iv)) != 1) {
            return logOpenSslError();
        }
    } else {
        if (EVP_DecryptInit_ex(ctx.get(), ciph, nullptr,
                               reinterpret_cast<unsigned char const*>(key.data()),
                               reinterpret_cast<unsigned char const*>(header->iv)) != 1) {
            return logOpenSslError();
        }
    }

    // ── 6. Decrypt ciphertext ─────────────────────────────────────────────
    std::vector<char> plaintext(cipherLen + EVP_MAX_BLOCK_LENGTH);
    int outLen = 0;

    if (cipherLen > 0) {
        if (EVP_DecryptUpdate(ctx.get(),
                              reinterpret_cast<unsigned char*>(plaintext.data()),
                              &outLen,
                              reinterpret_cast<unsigned char const*>(cipherAndTag),
                              static_cast<int>(cipherLen)) != 1) {
            return logOpenSslError();
        }
        output.insert(output.end(), plaintext.data(), plaintext.data() + outLen);
    }

    // ── 7. Set expected GCM tag before finalize (GCM only) ─────────────────
    if (gcmMode && tag != nullptr) {
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG,
                                static_cast<int>(crypto::kGcmTagLength),
                                const_cast<unsigned char*>(
                                    reinterpret_cast<unsigned char const*>(tag))) != 1) {
            return logOpenSslError();
        }
    }

    // ── 8. Finalize (verifies GCM tag for GCM modes) ──────────────────────
    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx.get(),
                            reinterpret_cast<unsigned char*>(plaintext.data()),
                            &finalLen) != 1) {
        spdlog::error("Decryption failed: wrong password or corrupted data");
        return ErrorCode::kEncryptionFailed;
    }
    if (finalLen > 0) {
        output.insert(output.end(), plaintext.data(), plaintext.data() + finalLen);
    }

    // Clear key material
    std::fill(key.begin(), key.end(), '\0');

    return {};
}

} // namespace backer

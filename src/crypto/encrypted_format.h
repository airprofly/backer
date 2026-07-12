#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>

namespace backer::crypto {

// ════════════════════════════════════════════════════════════════════════════
// Encryption file format
//
// ┌──────────────────────────────────────┐
// │      加密文件头部 (47 bytes)          │
// │  ├─ Magic: "BACKERENC\0" (10B)      │
// │  ├─ 算法标识 (1B)                   │
// │  ├─ PBKDF2 salt (16B)               │
// │  ├─ PBKDF2 iterations (4B, LE)      │
// │  └─ IV / nonce (16B, pad w/ zeros)  │
// ├──────────────────────────────────────┤
// │      加密后的密文数据                  │
// ├──────────────────────────────────────┤
// │  GCM 认证标签 (16B, GCM only)        │
// └──────────────────────────────────────┘
// ════════════════════════════════════════════════════════════════════════════

/// Magic identifier written at the start of every encrypted file.
inline constexpr std::string_view kMagic = "BACKERENC";
/// Total size of the magic field including the NUL terminator.
inline constexpr size_t kMagicSize = 10;

/// Supported encryption algorithms.
enum class Algorithm : uint8_t {
    kAes256Gcm = 0,  // AES-256-GCM
    kSm4Cbc    = 1,  // SM4-CBC
    // SM4-GCM requires OpenSSL ≥ 3.2 — not available on 3.0.x
};

/// PBKDF2 iteration count (security-sensitive — do not lower).
inline constexpr int kDefaultIterations = 100000;
/// Salt length in bytes.
inline constexpr size_t kSaltLength = 16;
/// Standard IV length for GCM modes.
inline constexpr size_t kGcmIvLength = 12;
/// Standard IV length for CBC modes.
inline constexpr size_t kCbcIvLength = 16;
/// GCM authentication tag length.
inline constexpr size_t kGcmTagLength = 16;
/// Key length for AES-256.
inline constexpr size_t kAes256KeyLength = 32;
/// Key length for SM4.
inline constexpr size_t kSm4KeyLength = 16;

/// Packed header structure — safe to read/write directly.
#pragma pack(push, 1)
struct EncryptedHeader {
    char     magic[10];        // "BACKERENC\0"
    uint8_t  algorithm;         // Algorithm enum value
    uint8_t  salt[16];          // PBKDF2 salt
    uint32_t iterations;        // PBKDF2 iterations (little-endian)
    uint8_t  iv[16];            // Nonce/IV (12B for GCM, 16B for CBC)
};
#pragma pack(pop)

static_assert(sizeof(EncryptedHeader) == 10 + 1 + 16 + 4 + 16,
              "EncryptedHeader: unexpected padding — check #pragma pack");

/// Check whether @p header begins with the expected magic bytes.
inline bool isValidMagic(EncryptedHeader const& header) noexcept {
    return std::memcmp(header.magic, kMagic.data(), kMagicSize) == 0;
}

/// Convert Algorithm enum to a human-readable string.
inline constexpr std::string_view algorithmName(Algorithm algo) noexcept {
    switch (algo) {
        case Algorithm::kAes256Gcm: return "aes256";
        case Algorithm::kSm4Cbc:    return "sm4";
    }
    return "unknown";
}

/// Convert string algorithm name to Algorithm enum value.
/// Returns std::nullopt_t equivalent by setting @p ok = false on failure.
inline Algorithm algorithmFromName(std::string_view name, bool& ok) noexcept {
    ok = true;
    if (name == "aes256"    || name == "aes-256-gcm")      return Algorithm::kAes256Gcm;
    if (name == "sm4"       || name == "sm4-cbc")          return Algorithm::kSm4Cbc;
    ok = false;
    return Algorithm::kAes256Gcm; // fallback, ignored when ok == false
}

} // namespace backer::crypto

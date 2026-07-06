#pragma once

#include <cstdint>
#include <string_view>

namespace backer {

/// Unified error codes covering all backup/restore phases.
enum class ErrorCode : uint32_t {
    // General (0x00xx)
    kOk = 0,
    kUnknown = 1,
    kNotImplemented = 2,

    // File system (0x01xx)
    kPathNotFound = 0x0101,
    kPermissionDenied = 0x0102,
    kDiskFull = 0x0103,
    kFileTooLarge = 0x0104,
    kSymlinkLoop = 0x0105,

    // Pipeline processing (0x02xx)
    kCompressionFailed = 0x0201,
    kEncryptionFailed = 0x0202,
    kInvalidArchive = 0x0203,

    // Storage (0x03xx)
    kWriteFailed = 0x0301,
    kReadFailed = 0x0302,
    kNetworkError = 0x0303,
    kAuthFailed = 0x0304,
};

/// Convert an ErrorCode to a human-readable string.
constexpr std::string_view toString(ErrorCode ec) noexcept {
    switch (ec) {
        case ErrorCode::kOk:                return "OK";
        case ErrorCode::kUnknown:           return "Unknown error";
        case ErrorCode::kNotImplemented:    return "Not implemented";
        case ErrorCode::kPathNotFound:      return "Path not found";
        case ErrorCode::kPermissionDenied:  return "Permission denied";
        case ErrorCode::kDiskFull:          return "Disk full";
        case ErrorCode::kFileTooLarge:      return "File too large";
        case ErrorCode::kSymlinkLoop:       return "Symlink loop detected";
        case ErrorCode::kCompressionFailed: return "Compression failed";
        case ErrorCode::kEncryptionFailed:  return "Encryption failed";
        case ErrorCode::kInvalidArchive:    return "Invalid archive";
        case ErrorCode::kWriteFailed:       return "Write failed";
        case ErrorCode::kReadFailed:        return "Read failed";
        case ErrorCode::kNetworkError:      return "Network error";
        case ErrorCode::kAuthFailed:        return "Authentication failed";
    }
    return "Unknown error code";
}

} // namespace backer

#pragma once

#include "core/error_code.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace backer {

/// Supported file types for backup/restore.
enum class FileType : uint8_t {
    kRegular,
    kDirectory,
    kSymlink,
    kFifo,
    kBlockDevice,
    kCharDevice,
    kSocket,
    kUnknown
};

/// File metadata — ownership, permissions, and timestamps.
struct Metadata {
    uint32_t ownerId        = 0;
    uint32_t groupId        = 0;
    uint32_t permissions    = 0;  // mode_t bits (lower 12: rwx for user/group/other + setuid/setgid/sticky)

    int64_t accessTimeSec   = 0;
    int64_t accessTimeNsec  = 0;
    int64_t modifyTimeSec   = 0;
    int64_t modifyTimeNsec  = 0;
    int64_t changeTimeSec   = 0;  // ctime — recorded but not restored
    int64_t changeTimeNsec  = 0;
};

/// An entry discovered during directory traversal.
struct FileEntry {
    std::filesystem::path relativePath;   ///< Path relative to traversal root
    FileType              type = FileType::kRegular;
    uint64_t              size = 0;
    Metadata              metadata;       ///< Ownership, permissions, timestamps
    std::filesystem::path symlinkTarget;  ///< For symbolic links
    uint64_t              deviceMajor = 0;///< Major device number (block/char)
    uint64_t              deviceMinor = 0;///< Minor device number (block/char)
};

/// Statistics accumulated during backup or restore.
struct OperationStats {
    uint64_t  totalFiles  = 0;
    uint64_t  totalDirs   = 0;
    uint64_t  totalBytes  = 0;
    uint64_t  skipped     = 0;
    std::chrono::milliseconds elapsed{0};
};

/// Result returned by BackupEngine::backup().
struct BackupResult {
    bool        success = false;
    ErrorCode   errorCode = ErrorCode::kOk;
    std::string errorMessage;
    OperationStats stats;
};

} // namespace backer

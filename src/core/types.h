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
};

/// An entry discovered during directory traversal.
struct FileEntry {
    std::filesystem::path relativePath;  ///< Path relative to traversal root
    FileType              type = FileType::kRegular;
    uint64_t              size = 0;
    std::filesystem::path symlinkTarget;      ///< For symbolic links
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

#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>

namespace backer::gui {

/// Current local time as YYYYMMDD_HHMMSS string.
inline std::string currentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if BACKER_PLATFORM_POSIX
    localtime_r(&tt, &tm);
#else
    localtime_s(&tm, &tt);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return os.str();
}

/// Strip known archive/compress/encrypt extensions from a filename.
///   "data_20260714.tar.gz.enc" → "data_20260714"
inline std::string stripExtensions(std::filesystem::path const& path)
{
    static std::unordered_set<std::string> const kKnownExts = {
        ".enc", ".gz", ".zst", ".lzma", ".xz", ".tar", ".zip"
    };
    auto s = path.string();
    while (s.size() > 1 && (s.back() == '/' || s.back() == '\\'))
        s.pop_back();
    auto filename = std::filesystem::path(s).filename().string();
    auto p = std::filesystem::path(filename);
    while (true) {
        auto ext = p.extension().string();
        if (!ext.empty() && kKnownExts.count(ext)) {
            p = p.stem();
        } else {
            break;
        }
    }
    return p.string();
}

/// Build a subdirectory name for a backup inside the user-chosen dest dir.
///   makeBackupSubPath("data", "home/user/docs", "tar")
///   → "data/docs_20260715_143000"
inline std::filesystem::path makeBackupSubPath(
    std::filesystem::path const& destDir,
    std::filesystem::path const& source)
{
    auto srcName = source.filename().string();
    if (srcName.empty() || srcName == "." || srcName == "..")
        srcName = "backup";
    auto ts = currentTimestamp();
    return destDir / (srcName + "_" + ts);
}

/// Build a subdirectory name for a restore inside the user-chosen dest dir.
///   makeRestoreSubPath("data", "backups/docs_20260715_143000.tar.gz")
///   → "data/docs_R20260715_143000"
inline std::filesystem::path makeRestoreSubPath(
    std::filesystem::path const& destDir,
    std::filesystem::path const& source)
{
    auto baseName = stripExtensions(source);
    if (baseName.empty() || baseName == "." || baseName == "..")
        baseName = "restore";
    auto ts = currentTimestamp();
    return destDir / (baseName + "_R" + ts);
}

} // namespace backer::gui

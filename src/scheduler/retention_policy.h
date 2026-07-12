#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace backer {

/// Retention policy configuration.
struct RetentionConfig {
    /// Maximum number of snapshots to keep (0 = unlimited).
    int maxSnapshots = 0;
    /// Maximum age in days for snapshots (0 = unlimited).
    int retentionDays = 0;

    bool enabled() const noexcept {
        return maxSnapshots > 0 || retentionDays > 0;
    }
};

/// Determines which backup snapshots should be removed according to
/// a configured retention policy.
///
/// The policy considers two dimensions:
///   - **Count-based**: keep only the N most recent snapshots.
///   - **Age-based**:  remove snapshots older than N days.
///
/// Both dimensions are applied independently (a snapshot is removed if
/// it violates *either* constraint).
class RetentionPolicy {
public:
    /// Given a list of snapshot directories (full paths), return the
    /// subset that should be deleted under @p config.
    ///
    /// @param snapshots  List of snapshot paths.  Each path's filename
    ///                   should be a timestamp in YYYYMMDD_HHMMSS format
    ///                   (as produced by BackupScheduler::makeSnapshotPath).
    /// @param config     Retention policy parameters.
    /// @return Paths selected for removal, sorted oldest-first.
    std::vector<std::filesystem::path> selectForRemoval(
        std::vector<std::filesystem::path> const& snapshots,
        RetentionConfig const& config) const;

    /// Scan a directory and collect all subdirectories whose name matches
    /// the timestamp pattern YYYYMMDD_HHMMSS.
    static std::vector<std::filesystem::path> scanSnapshots(
        std::filesystem::path const& directory);

    /// Parse a YYYYMMDD_HHMMSS filename into a time_point.
    /// Returns std::nullopt if the name doesn't match the pattern.
    static std::optional<std::chrono::system_clock::time_point> parseTimestamp(
        std::string const& name);
};

} // namespace backer

#include "scheduler/retention_policy.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <spdlog/spdlog.h>
#include <system_error>

namespace backer {

// ══════════════════════════════════════════════════════════════════════════════
// parseTimestamp
// ══════════════════════════════════════════════════════════════════════════════

std::optional<std::chrono::system_clock::time_point>
RetentionPolicy::parseTimestamp(std::string const& name) {
    // Expected format: YYYYMMDD_HHMMSS  (e.g. "20260712_143000")
    if (name.size() != 15 || name[8] != '_') {
        return std::nullopt;
    }

    std::tm tm{};

    auto safeStoi = [](std::string const& s) -> int {
        try { return std::stoi(s); }
        catch (...) { return -1; }
    };

    tm.tm_year = safeStoi(name.substr(0, 4)) - 1900;
    tm.tm_mon  = safeStoi(name.substr(4, 2)) - 1;
    tm.tm_mday = safeStoi(name.substr(6, 2));
    tm.tm_hour = safeStoi(name.substr(9, 2));
    tm.tm_min  = safeStoi(name.substr(11, 2));
    tm.tm_sec  = safeStoi(name.substr(13, 2));

    // Validate parsed values
    if (tm.tm_year < 0 || tm.tm_mon < 0 || tm.tm_mday <= 0 ||
        tm.tm_hour < 0 || tm.tm_min < 0 || tm.tm_sec < 0) {
        return std::nullopt;
    }

    // Normalize (mktime adjusts out-of-range values)
    std::time_t t = std::mktime(&tm);
    if (t == -1) return std::nullopt;

    return std::chrono::system_clock::from_time_t(t);
}

// ══════════════════════════════════════════════════════════════════════════════
// scanSnapshots
// ══════════════════════════════════════════════════════════════════════════════

std::vector<std::filesystem::path>
RetentionPolicy::scanSnapshots(std::filesystem::path const& directory) {
    std::vector<std::filesystem::path> result;

    std::error_code ec;
    for (auto const& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_directory(ec)) continue;
        auto name = entry.path().filename().string();
        if (parseTimestamp(name).has_value()) {
            result.push_back(entry.path());
        }
    }

    if (ec) {
        spdlog::warn("Error scanning snapshots in {}: {}",
                     directory.string(), ec.message());
    }

    // Sort by name (which sorts by timestamp since YYYYMMDD_HHMMSS is
    // lexicographically ordered).
    std::sort(result.begin(), result.end());

    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// selectForRemoval
// ══════════════════════════════════════════════════════════════════════════════

std::vector<std::filesystem::path>
RetentionPolicy::selectForRemoval(
    std::vector<std::filesystem::path> const& snapshots,
    RetentionConfig const& config) const
{
    if (snapshots.empty() || !config.enabled()) {
        return {};
    }

    // Build (path, timestamp) pairs sorted oldest-first (already sorted
    // by the caller / scanSnapshots, but be defensive).
    struct SnapshotInfo {
        std::filesystem::path path;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<SnapshotInfo> infos;
    infos.reserve(snapshots.size());

    for (auto const& p : snapshots) {
        auto ts = parseTimestamp(p.filename().string());
        if (!ts.has_value()) continue; // skip non-snapshot directories
        infos.push_back({p, *ts});
    }

    // Sort oldest-first (ascending timestamp)
    std::sort(infos.begin(), infos.end(),
              [](auto const& a, auto const& b) {
                  return a.timestamp < b.timestamp;
              });

    // Apply count-based retention: mark all but the N most recent.
    std::vector<bool> remove(infos.size(), false);
    if (config.maxSnapshots > 0 && infos.size() > static_cast<std::size_t>(config.maxSnapshots)) {
        std::size_t keep = static_cast<std::size_t>(config.maxSnapshots);
        for (std::size_t i = 0; i < infos.size() - keep; ++i) {
            remove[i] = true;
        }
    }

    // Apply age-based retention: mark snapshots older than N days.
    if (config.retentionDays > 0) {
        auto cutoff = std::chrono::system_clock::now()
                    - std::chrono::hours(24 * config.retentionDays);
        for (std::size_t i = 0; i < infos.size(); ++i) {
            if (infos[i].timestamp < cutoff) {
                remove[i] = true;
            }
        }
    }

    // Collect paths marked for removal.
    std::vector<std::filesystem::path> toRemove;
    for (std::size_t i = 0; i < infos.size(); ++i) {
        if (remove[i]) {
            toRemove.push_back(std::move(infos[i].path));
        }
    }

    return toRemove;
}

} // namespace backer

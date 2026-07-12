#pragma once

#include "core/error_code.h"
#include "core/expected.h"
#include "scheduler/backup_scheduler.h"

#include <filesystem>
#include <string>
#include <vector>

namespace backer {

/// Persists and restores scheduled backup jobs as a JSON file.
///
/// File format: a JSON object with a single "jobs" array, each element
/// describing one ScheduleJob.  Stored at a configurable path (default
/// `~/.config/backer/schedule.json`).
class ScheduleStore {
public:
    /// @param filePath  Path to the JSON file.  An empty string selects
    ///                  the default (`~/.config/backer/schedule.json`).
    explicit ScheduleStore(std::filesystem::path filePath = {});

    /// Save a list of jobs to the JSON file.
    Expected<void, ErrorCode> save(std::vector<ScheduleJob> const& jobs);

    /// Load jobs from the JSON file.  Returns an empty list if the file
    /// does not exist (first run).
    Expected<std::vector<ScheduleJob>, ErrorCode> load();

    /// Return the resolved file path.
    std::filesystem::path const& filePath() const { return filePath_; }

private:
    std::filesystem::path filePath_;

    static std::filesystem::path defaultPath();
};

} // namespace backer

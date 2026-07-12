#pragma once

#include "core/error_code.h"
#include "core/expected.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace backer {

/// Options for a scheduled backup job.
struct ScheduleBackupOptions {
    std::string packFormat;
    std::string compressAlgo;
    int compressLevel = 0;
    std::string encryptAlgo;
    std::string password;

    /// Number of recent snapshots to retain (0 = unlimited).
    int retainCount = 0;
    /// Number of days of snapshots to retain (0 = unlimited).
    int retainDays = 0;

    bool preserveMetadata = true;
    bool handleSpecial = true;
};

/// A single scheduled backup task.
struct ScheduleJob {
    std::string id;
    std::string name;
    std::string cronExpression;
    std::filesystem::path source;
    std::filesystem::path destination;
    ScheduleBackupOptions options;
    bool enabled = true;
    std::chrono::system_clock::time_point createdAt;
};

/// Job listing info, including the computed next fire time.
struct JobInfo {
    std::string id;
    std::string name;
    std::string cronExpression;
    std::filesystem::path source;
    std::filesystem::path destination;
    bool enabled;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point nextFireTime;
};

/// Cron-based backup scheduler.
///
/// Manages a list of scheduled backup jobs, each described by a cron
/// expression.  A blocking `run()` loop waits for the next job to fire,
/// calls the user-provided executor callback, then recalculates the
/// next trigger time.
///
/// Cross-platform (uses std::chrono + condition_variable rather than
/// Linux-specific timerfd/epoll).
class BackupScheduler {
public:
    /// Callback that executes one backup job.  Returns true on success.
    using BackupExecutor = std::function<bool(ScheduleJob const&)>;

    explicit BackupScheduler(BackupExecutor executor);
    ~BackupScheduler();

    BackupScheduler(BackupScheduler const&) = delete;
    BackupScheduler& operator=(BackupScheduler const&) = delete;

    /// Add a job and return its auto-generated ID.
    Expected<std::string, ErrorCode> addJob(ScheduleJob const& job);

    /// Remove a job by ID.
    Expected<void, ErrorCode> removeJob(std::string_view jobId);

    /// Enable or disable a job.
    Expected<void, ErrorCode> enableJob(std::string_view jobId, bool enabled);

    /// Return all registered jobs with their next fire time.
    std::vector<JobInfo> listJobs() const;

    /// Find a job by ID (nullptr if not found).
    ScheduleJob* findJob(std::string_view jobId);
    ScheduleJob const* findJob(std::string_view jobId) const;

    /// Run the scheduling loop (blocks until stop() is called).
    void run();

    /// Signal the loop to stop (thread-safe).
    void stop();

    bool isRunning() const { return running_.load(); }

    // ── Helpers (public for testing) ──────────────────────────────────

    /// Compute the next fire time for a cron expression.
    static std::chrono::system_clock::time_point calcNextFire(
        std::string const& cronExpr,
        std::chrono::system_clock::time_point from);

    /// Build a timestamped snapshot path below @p base.
    static std::filesystem::path makeSnapshotPath(
        std::filesystem::path const& base);

private:
    /// Return the earliest next-fire time across enabled jobs.
    /// Returns time_point::max() if no enabled jobs.
    std::chrono::system_clock::time_point earliestNext() const;

    /// Execute all jobs whose next-fire time is <= @p now.
    void fireDueJobs(std::chrono::system_clock::time_point now);

    BackupExecutor executor_;
    std::map<std::string, ScheduleJob> jobs_;
    std::map<std::string, std::chrono::system_clock::time_point> nextFireTimes_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};

} // namespace backer

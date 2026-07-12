#include "scheduler/backup_scheduler.h"

#include "core/error_code.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <spdlog/spdlog.h>

namespace backer {

// ══════════════════════════════════════════════════════════════════════════════
// ccronexpr integration
// ══════════════════════════════════════════════════════════════════════════════

// ccronexpr is a C library; its header has __cplusplus guards so it can be
// included directly from C++.  The implementation is compiled separately
// from ccronexpr.c (added to backer_core in CMakeLists.txt with
// CRON_USE_LOCAL_TIME=1 so cron_next returns local-time results).
#include "scheduler/ccronexpr.h"

namespace {

/// Generate a short unique job ID.
std::string generateJobId() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()).count() % 1000;
    std::ostringstream os;
    os << "job_" << t << "_" << ms;
    return os.str();
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════════
// calcNextFire
// ══════════════════════════════════════════════════════════════════════════════

std::chrono::system_clock::time_point
BackupScheduler::calcNextFire(
    std::string const& cronExpr,
    std::chrono::system_clock::time_point from)
{
    cron_expr expr;
    std::memset(&expr, 0, sizeof(expr));

    // Normalize: ccronexpr always expects 6 fields (including seconds).
    // If given a standard 5-field cron expression, prepend "0" for seconds.
    auto normalized = cronExpr;
    int fieldCount = 0;
    for (char c : cronExpr) { if (c == ' ') fieldCount++; }
    if (fieldCount == 4) { // 5 fields → prepend seconds=0
        normalized = "0 " + cronExpr;
    }

    char const* err = nullptr;
    cron_parse_expr(normalized.c_str(), &expr, &err);
    if (err != nullptr) {
        spdlog::error("Failed to parse cron expression '{}': {}",
                       cronExpr, err);
        // Return a sentinel far in the past so this job is effectively skipped
        return std::chrono::system_clock::time_point::min();
    }

    std::time_t next = cron_next(&expr, std::chrono::system_clock::to_time_t(from));
    if (next == -1) {
        spdlog::warn("Cron expression '{}' will never fire again", cronExpr);
        return std::chrono::system_clock::time_point::max();
    }

    return std::chrono::system_clock::from_time_t(next);
}

// ══════════════════════════════════════════════════════════════════════════════
// makeSnapshotPath
// ══════════════════════════════════════════════════════════════════════════════

std::filesystem::path
BackupScheduler::makeSnapshotPath(std::filesystem::path const& base) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if BACKER_PLATFORM_WINDOWS
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return base / os.str();
}

// ══════════════════════════════════════════════════════════════════════════════
// ctor / dtor
// ══════════════════════════════════════════════════════════════════════════════

BackupScheduler::BackupScheduler(BackupExecutor executor)
    : executor_(std::move(executor))
{
}

BackupScheduler::~BackupScheduler() {
    stop();
}

// ══════════════════════════════════════════════════════════════════════════════
// addJob
// ══════════════════════════════════════════════════════════════════════════════

Expected<std::string, ErrorCode>
BackupScheduler::addJob(ScheduleJob const& job) {
    auto id = job.id.empty() ? generateJobId() : job.id;

    std::lock_guard<std::mutex> lock(mutex_);

    if (jobs_.find(id) != jobs_.end()) {
        return ErrorCode::kUnknown; // ID collision
    }

    ScheduleJob stored = job;
    stored.id = id;

    // Compute the first fire time for the next-fire-time map
    if (stored.enabled) {
        nextFireTimes_[id] = calcNextFire(stored.cronExpression,
                                          std::chrono::system_clock::now());
    }

    jobs_.emplace(id, std::move(stored));
    cv_.notify_one(); // wake the loop so it can re-sort timers
    return id;
}

// ══════════════════════════════════════════════════════════════════════════════
// removeJob
// ══════════════════════════════════════════════════════════════════════════════

Expected<void, ErrorCode>
BackupScheduler::removeJob(std::string_view jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(std::string(jobId));
    if (it == jobs_.end()) {
        return ErrorCode::kPathNotFound;
    }
    jobs_.erase(it);
    nextFireTimes_.erase(std::string(jobId));
    cv_.notify_one();
    return {};
}

// ══════════════════════════════════════════════════════════════════════════════
// enableJob
// ══════════════════════════════════════════════════════════════════════════════

Expected<void, ErrorCode>
BackupScheduler::enableJob(std::string_view jobId, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(std::string(jobId));
    if (it == jobs_.end()) {
        return ErrorCode::kPathNotFound;
    }
    it->second.enabled = enabled;
    if (enabled) {
        nextFireTimes_[it->first] = calcNextFire(it->second.cronExpression,
                                                 std::chrono::system_clock::now());
    } else {
        nextFireTimes_.erase(it->first);
    }
    cv_.notify_one();
    return {};
}

// ══════════════════════════════════════════════════════════════════════════════
// findJob
// ══════════════════════════════════════════════════════════════════════════════

ScheduleJob* BackupScheduler::findJob(std::string_view jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(std::string(jobId));
    return it != jobs_.end() ? &it->second : nullptr;
}

ScheduleJob const* BackupScheduler::findJob(std::string_view jobId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(std::string(jobId));
    return it != jobs_.end() ? &it->second : nullptr;
}

// ══════════════════════════════════════════════════════════════════════════════
// listJobs
// ══════════════════════════════════════════════════════════════════════════════

std::vector<JobInfo> BackupScheduler::listJobs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<JobInfo> result;
    result.reserve(jobs_.size());

    for (auto const& [id, job] : jobs_) {
        JobInfo info;
        info.id             = job.id;
        info.name           = job.name;
        info.cronExpression = job.cronExpression;
        info.source         = job.source;
        info.destination    = job.destination;
        info.enabled        = job.enabled;
        info.createdAt      = job.createdAt;

        auto nft = nextFireTimes_.find(id);
        if (nft != nextFireTimes_.end()) {
            info.nextFireTime = nft->second;
        } else {
            info.nextFireTime = std::chrono::system_clock::time_point::max();
        }

        result.push_back(std::move(info));
    }
    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// earliestNext
// ══════════════════════════════════════════════════════════════════════════════

std::chrono::system_clock::time_point
BackupScheduler::earliestNext() const {
    auto earliest = std::chrono::system_clock::time_point::max();
    for (auto const& [id, nft] : nextFireTimes_) {
        if (nft < earliest) {
            earliest = nft;
        }
    }
    return earliest;
}

// ══════════════════════════════════════════════════════════════════════════════
// fireDueJobs
// ══════════════════════════════════════════════════════════════════════════════

void BackupScheduler::fireDueJobs(std::chrono::system_clock::time_point now) {
    std::vector<ScheduleJob> toFire;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, job] : jobs_) {
            if (!job.enabled) continue;

            auto nftIt = nextFireTimes_.find(id);
            if (nftIt == nextFireTimes_.end()) continue;
            if (nftIt->second > now) continue;

            toFire.push_back(job);
        }
    }

    // Execute outside the lock so long-running backups don't block management.
    for (auto const& job : toFire) {
        spdlog::info("[Scheduler] Firing job '{}' ({})", job.name, job.id);
        bool ok = executor_(job);

        if (ok) {
            spdlog::info("[Scheduler] Job '{}' completed successfully", job.id);
        } else {
            spdlog::error("[Scheduler] Job '{}' failed", job.id);
        }
    }

    // Recalculate next-fire times for the jobs we fired.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto const& job : toFire) {
            auto nftIt = nextFireTimes_.find(job.id);
            if (nftIt == nextFireTimes_.end()) continue;

            nftIt->second = calcNextFire(job.cronExpression, now);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// run (the main loop)
// ══════════════════════════════════════════════════════════════════════════════

void BackupScheduler::run() {
    if (running_.exchange(true)) {
        spdlog::warn("[Scheduler] Already running");
        return;
    }

    stopRequested_ = false;
    spdlog::info("[Scheduler] Event loop started");

    while (!stopRequested_) {
        auto now = std::chrono::system_clock::now();

        // Fire any jobs whose time has come
        fireDueJobs(now);

        // Wait until the earliest next fire time (or until notified)
        std::chrono::system_clock::time_point next;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            next = earliestNext();
        }

        if (next == std::chrono::system_clock::time_point::max()) {
            // No enabled jobs — wait indefinitely for a wake-up
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stopRequested_.load() || !jobs_.empty(); });
        } else {
            // Clamp to prevent extreme waits
            auto maxSleep = now + std::chrono::hours(24 * 365);
            if (next > maxSleep) next = maxSleep;

            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_until(lock, next, [this] { return stopRequested_.load(); });
        }
    }

    spdlog::info("[Scheduler] Event loop stopped");
    running_ = false;
}

// ══════════════════════════════════════════════════════════════════════════════
// stop
// ══════════════════════════════════════════════════════════════════════════════

void BackupScheduler::stop() {
    stopRequested_ = true;
    cv_.notify_all();
}

} // namespace backer

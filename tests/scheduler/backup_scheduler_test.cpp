#include "scheduler/backup_scheduler.h"
#include "scheduler/retention_policy.h"
#include "scheduler/schedule_store.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

namespace backer::testing {
namespace {

using namespace std::chrono_literals;

// ══════════════════════════════════════════════════════════════════════════════
// BackupScheduler tests
// ══════════════════════════════════════════════════════════════════════════════

TEST(SchedulerTest, AddAndListJobs) {
    std::atomic<int> fireCount{0};
    BackupScheduler scheduler([&](ScheduleJob const&) {
        fireCount++;
        return true;
    });

    ScheduleJob job;
    job.name           = "test-job";
    job.cronExpression = "0 2 * * *";
    job.source         = "/tmp/src";
    job.destination    = "/tmp/dst";
    job.enabled        = true;
    job.createdAt      = std::chrono::system_clock::now();

    auto id = scheduler.addJob(job);
    ASSERT_TRUE(id.has_value());
    EXPECT_FALSE(id.value().empty());

    auto jobs = scheduler.listJobs();
    ASSERT_EQ(1, jobs.size());
    EXPECT_EQ(job.name, jobs[0].name);
    EXPECT_TRUE(jobs[0].enabled);
}

TEST(SchedulerTest, RemoveJob) {
    BackupScheduler scheduler([](ScheduleJob const&) { return true; });

    ScheduleJob job;
    job.name           = "to-remove";
    job.cronExpression = "0 2 * * *";
    job.source         = "/tmp/src";
    job.destination    = "/tmp/dst";

    auto id = scheduler.addJob(job);
    ASSERT_TRUE(id.has_value());

    // Remove it
    auto result = scheduler.removeJob(id.value());
    EXPECT_TRUE(result.has_value());

    auto jobs = scheduler.listJobs();
    EXPECT_TRUE(jobs.empty());
}

TEST(SchedulerTest, RemoveNonExistent) {
    BackupScheduler scheduler([](ScheduleJob const&) { return true; });
    auto result = scheduler.removeJob("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST(SchedulerTest, EnableDisableJob) {
    BackupScheduler scheduler([](ScheduleJob const&) { return true; });

    ScheduleJob job;
    job.name           = "toggle";
    job.cronExpression = "0 2 * * *";
    job.source         = "/tmp/src";
    job.destination    = "/tmp/dst";

    auto id = scheduler.addJob(job);
    ASSERT_TRUE(id.has_value());

    // Disable
    EXPECT_TRUE(scheduler.enableJob(id.value(), false).has_value());
    auto jobs = scheduler.listJobs();
    ASSERT_EQ(1, jobs.size());
    EXPECT_FALSE(jobs[0].enabled);

    // Re-enable
    EXPECT_TRUE(scheduler.enableJob(id.value(), true).has_value());
    jobs = scheduler.listJobs();
    ASSERT_EQ(1, jobs.size());
    EXPECT_TRUE(jobs[0].enabled);
}

TEST(SchedulerTest, FindJob) {
    BackupScheduler scheduler([](ScheduleJob const&) { return true; });

    ScheduleJob job;
    job.name           = "findable";
    job.cronExpression = "0 2 * * *";
    job.source         = "/tmp/src";
    job.destination    = "/tmp/dst";

    auto id = scheduler.addJob(job);
    ASSERT_TRUE(id.has_value());

    auto* found = scheduler.findJob(id.value());
    ASSERT_NE(nullptr, found);
    EXPECT_EQ("findable", found->name);

    auto* notFound = scheduler.findJob("nope");
    EXPECT_EQ(nullptr, notFound);
}

TEST(SchedulerTest, StopWithoutRun) {
    // Should not crash
    BackupScheduler scheduler([](ScheduleJob const&) { return true; });
    scheduler.stop();
    EXPECT_FALSE(scheduler.isRunning());
}

TEST(SchedulerTest, RunAndStop) {
    BackupScheduler scheduler([](ScheduleJob const&) { return true; });

    ScheduleJob job;
    job.name           = "loop-test";
    job.cronExpression = "0 2 * * *";
    job.source         = "/tmp/src";
    job.destination    = "/tmp/dst";
    job.enabled        = true;

    scheduler.addJob(job);

    // Run in a background thread
    std::thread t([&] {
        scheduler.run();
    });

    // Give it a moment to start
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(scheduler.isRunning());

    // Stop it
    scheduler.stop();
    t.join();

    EXPECT_FALSE(scheduler.isRunning());
}

TEST(SchedulerTest, AddJobWithCustomId) {
    BackupScheduler scheduler([](ScheduleJob const&) { return true; });

    ScheduleJob job;
    job.id             = "custom-id-123";
    job.name           = "custom-id-job";
    job.cronExpression = "0 12 * * *";
    job.source         = "/tmp/src";
    job.destination    = "/tmp/dst";
    job.enabled        = true;

    auto id = scheduler.addJob(job);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ("custom-id-123", id.value());
}

// ══════════════════════════════════════════════════════════════════════════════
// ScheduleStore tests
// ══════════════════════════════════════════════════════════════════════════════

class ScheduleStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmpFile_ = std::filesystem::temp_directory_path() / "backer_schedule_test.json";
        std::filesystem::remove(tmpFile_);
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(tmpFile_, ec);
    }

    std::filesystem::path tmpFile_;
};

TEST_F(ScheduleStoreTest, SaveAndLoad) {
    ScheduleStore store(tmpFile_);

    ScheduleJob job;
    job.id             = "test_job_1";
    job.name           = "Test Job";
    job.cronExpression = "0 2 * * *";
    job.source         = "/home/user/data";
    job.destination    = "/backup/daily";
    job.enabled        = true;
    job.createdAt      = std::chrono::system_clock::now();
    job.options.compressAlgo = "zstd";
    job.options.retainCount  = 30;
    job.options.preserveMetadata = true;
    job.options.handleSpecial  = true;

    ASSERT_TRUE(store.save({job}).has_value());

    auto loaded = store.load();
    ASSERT_TRUE(loaded.has_value());
    auto const& jobs = loaded.value();
    ASSERT_EQ(1, jobs.size());

    auto const& lj = jobs[0];
    EXPECT_EQ(job.id,             lj.id);
    EXPECT_EQ(job.name,           lj.name);
    EXPECT_EQ(job.cronExpression, lj.cronExpression);
    EXPECT_EQ(job.source,         lj.source);
    EXPECT_EQ(job.destination,    lj.destination);
    EXPECT_EQ(job.enabled,        lj.enabled);
    EXPECT_EQ(job.options.compressAlgo, lj.options.compressAlgo);
    EXPECT_EQ(job.options.retainCount,  lj.options.retainCount);
}

TEST_F(ScheduleStoreTest, LoadNonExistent) {
    ScheduleStore store(tmpFile_);
    std::filesystem::remove(tmpFile_); // ensure it doesn't exist

    auto loaded = store.load();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded.value().empty());
}

TEST_F(ScheduleStoreTest, SaveMultipleAndLoad) {
    ScheduleStore store(tmpFile_);

    std::vector<ScheduleJob> jobs;
    for (int i = 0; i < 3; ++i) {
        ScheduleJob job;
        job.id             = "job_" + std::to_string(i);
        job.name           = "Job " + std::to_string(i);
        job.cronExpression = "0 " + std::to_string(i) + " * * *";
        job.source         = "/src/" + std::to_string(i);
        job.destination    = "/dst/" + std::to_string(i);
        job.enabled        = true;
        job.createdAt      = std::chrono::system_clock::now();
        jobs.push_back(std::move(job));
    }

    ASSERT_TRUE(store.save(jobs).has_value());

    auto loaded = store.load();
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(3, loaded.value().size());
}

// ══════════════════════════════════════════════════════════════════════════════
// RetentionPolicy — scanSnapshots
// ══════════════════════════════════════════════════════════════════════════════

class RetentionScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmpDir_ = std::filesystem::temp_directory_path() / "backer_retention_scan_test";
        std::error_code ec;
        std::filesystem::remove_all(tmpDir_, ec);
        std::filesystem::create_directories(tmpDir_);
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmpDir_, ec);
    }

    std::filesystem::path tmpDir_;
};

TEST_F(RetentionScanTest, ScansTimestampedDirs) {
    // Create a few timestamped directories
    std::filesystem::create_directory(tmpDir_ / "20260701_000000");
    std::filesystem::create_directory(tmpDir_ / "20260702_120000");
    std::filesystem::create_directory(tmpDir_ / "20260703_235959");

    // Create a non-timestamped file/dir
    std::filesystem::create_directory(tmpDir_ / "other_dir");
    {
        std::ofstream(tmpDir_ / "file.txt") << "hello";
    }

    auto snapshots = RetentionPolicy::scanSnapshots(tmpDir_);
    EXPECT_EQ(3, snapshots.size());
}

TEST_F(RetentionScanTest, EmptyDir) {
    auto snapshots = RetentionPolicy::scanSnapshots(tmpDir_);
    EXPECT_TRUE(snapshots.empty());
}

} // namespace
} // namespace backer::testing

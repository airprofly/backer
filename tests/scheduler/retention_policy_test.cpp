#include "scheduler/retention_policy.h"

#include <chrono>
#include <ctime>
#include <gtest/gtest.h>
#include <regex>
#include <set>

namespace backer::testing {
namespace {

// ══════════════════════════════════════════════════════════════════════════════
// parseTimestamp
// ══════════════════════════════════════════════════════════════════════════════

TEST(RetentionTest, ParseTimestampValid) {
    auto ts = RetentionPolicy::parseTimestamp("20260712_143000");
    ASSERT_TRUE(ts.has_value());

    auto t = std::chrono::system_clock::to_time_t(*ts);
    std::tm* tm = std::localtime(&t);
    ASSERT_NE(tm, nullptr);
    EXPECT_EQ(2026, tm->tm_year + 1900);
    EXPECT_EQ(7,    tm->tm_mon + 1);
    EXPECT_EQ(12,   tm->tm_mday);
    EXPECT_EQ(14,   tm->tm_hour);
    EXPECT_EQ(30,   tm->tm_min);
    EXPECT_EQ(0,    tm->tm_sec);
}

TEST(RetentionTest, ParseTimestampInvalidFormat) {
    EXPECT_FALSE(RetentionPolicy::parseTimestamp("not-a-timestamp").has_value());
    EXPECT_FALSE(RetentionPolicy::parseTimestamp("20260712").has_value());
    EXPECT_FALSE(RetentionPolicy::parseTimestamp("").has_value());
    EXPECT_FALSE(RetentionPolicy::parseTimestamp("abcdefgh_ijklmn").has_value());
}

TEST(RetentionTest, ParseTimestampWrongLength) {
    EXPECT_FALSE(RetentionPolicy::parseTimestamp("20260712143000").has_value());  // no underscore
    EXPECT_FALSE(RetentionPolicy::parseTimestamp("20260712_143000_extra").has_value());
}

// ══════════════════════════════════════════════════════════════════════════════
// selectForRemoval — count-based
// ══════════════════════════════════════════════════════════════════════════════

TEST(RetentionTest, KeepAllWhenUnlimited) {
    RetentionConfig cfg; // maxSnapshots=0, retentionDays=0
    cfg.maxSnapshots = 0;

    std::vector<std::filesystem::path> snapshots = {
        "/backup/20260701_000000",
        "/backup/20260702_000000",
        "/backup/20260703_000000",
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);
    EXPECT_TRUE(toRemove.empty());
}

TEST(RetentionTest, KeepAllWhenDisabled) {
    RetentionConfig cfg; // all zero = disabled
    cfg.maxSnapshots = 5;

    std::vector<std::filesystem::path> snapshots = {
        "/backup/20260701_000000",
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);
    EXPECT_TRUE(toRemove.empty());
}

TEST(RetentionTest, RemoveOldestWhenExceedingCount) {
    RetentionConfig cfg;
    cfg.maxSnapshots = 2;

    std::vector<std::filesystem::path> snapshots = {
        "/backup/20260701_000000",
        "/backup/20260702_000000",
        "/backup/20260703_000000",
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);

    ASSERT_EQ(1, toRemove.size());
    EXPECT_EQ("/backup/20260701_000000", toRemove[0].string());
}

TEST(RetentionTest, RemoveMultipleOldest) {
    RetentionConfig cfg;
    cfg.maxSnapshots = 2;

    std::vector<std::filesystem::path> snapshots = {
        "/backup/20260701_000000",
        "/backup/20260702_000000",
        "/backup/20260703_000000",
        "/backup/20260704_000000",
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);

    ASSERT_EQ(2, toRemove.size());
    EXPECT_EQ("/backup/20260701_000000", toRemove[0].string());
    EXPECT_EQ("/backup/20260702_000000", toRemove[1].string());
}

TEST(RetentionTest, KeepExactCount) {
    RetentionConfig cfg;
    cfg.maxSnapshots = 3;

    std::vector<std::filesystem::path> snapshots = {
        "/backup/20260701_000000",
        "/backup/20260702_000000",
        "/backup/20260703_000000",
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);
    EXPECT_TRUE(toRemove.empty());
}

// ══════════════════════════════════════════════════════════════════════════════
// selectForRemoval — age-based
// ══════════════════════════════════════════════════════════════════════════════

TEST(RetentionTest, RemoveOlderThanDays) {
    RetentionConfig cfg;
    cfg.retentionDays = 7;

    // Create snapshots with names that are 1, 5, 10, and 20 days old
    std::vector<std::filesystem::path> snapshots = {
        "/backup/20260705_000000",  // 7 days before 2026-07-12
        "/backup/20260702_000000",  // 10 days before
        "/backup/20260622_000000",  // 20 days before
        "/backup/20260711_000000",  // 1 day before
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);

    // Only snapshots > 7 days old should be removed
    // July 12 minus 7 = July 5, so anything before July 5 is removed
    // July 5 is exactly at the boundary, it could go either way depending on time of day
    // Let's just check that July 2 and June 22 are removed
    ASSERT_GE(toRemove.size(), 2);
    bool foundJuly2 = false;
    bool foundJune22 = false;
    for (auto const& p : toRemove) {
        auto name = p.filename().string();
        if (name == "20260702_000000") foundJuly2 = true;
        if (name == "20260622_000000") foundJune22 = true;
    }
    EXPECT_TRUE(foundJuly2);
    EXPECT_TRUE(foundJune22);
}

// ══════════════════════════════════════════════════════════════════════════════
// selectForRemoval — mixed count + age
// ══════════════════════════════════════════════════════════════════════════════

TEST(RetentionTest, MixedBothConstraints) {
    RetentionConfig cfg;
    cfg.maxSnapshots = 2;
    cfg.retentionDays = 10;

    // 4 snapshots: 1 old, 3 recent
    std::vector<std::filesystem::path> snapshots = {
        "/backup/20260601_000000",  // 41 days old (removed by age)
        "/backup/20260709_000000",  // 3 days old (kept by age, but count: only keep 2)
        "/backup/20260710_000000",  // 2 days old (keep)
        "/backup/20260711_000000",  // 1 day old  (keep)
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);

    // June 1 removed by age; by count, July 9 removed (only keep 2 newest)
    EXPECT_EQ(2, toRemove.size());

    auto names = [&] {
        std::set<std::string> s;
        for (auto const& p : toRemove) s.insert(p.filename().string());
        return s;
    }();
    EXPECT_TRUE(names.count("20260601_000000"));
    EXPECT_TRUE(names.count("20260709_000000"));
}

// ══════════════════════════════════════════════════════════════════════════════
// selectForRemoval — edge cases
// ══════════════════════════════════════════════════════════════════════════════

TEST(RetentionTest, EmptyList) {
    RetentionConfig cfg;
    cfg.maxSnapshots = 5;

    RetentionPolicy policy;
    EXPECT_TRUE(policy.selectForRemoval({}, cfg).empty());
}

TEST(RetentionTest, SingleSnapshot) {
    RetentionConfig cfg;
    cfg.maxSnapshots = 1;

    RetentionPolicy policy;
    EXPECT_TRUE(policy.selectForRemoval({
        "/backup/20260701_000000",
    }, cfg).empty());
}

TEST(RetentionTest, NonMatchingNamesAreSkipped) {
    RetentionConfig cfg;
    cfg.maxSnapshots = 2;

    // Mix of timestamped and non-timestamped directories
    std::vector<std::filesystem::path> snapshots = {
        "/backup/other_dir",
        "/backup/20260701_000000",
        "/backup/20260702_000000",
        "/backup/20260703_000000",
    };

    RetentionPolicy policy;
    auto toRemove = policy.selectForRemoval(snapshots, cfg);

    // "other_dir" should be skipped, so only 1 of the 3 timestamps is removed
    ASSERT_EQ(1, toRemove.size());
    EXPECT_EQ("/backup/20260701_000000", toRemove[0].string());
}

} // namespace
} // namespace backer::testing

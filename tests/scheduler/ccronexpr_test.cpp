#include "scheduler/backup_scheduler.h"

#include <chrono>
#include <ctime>
#include <gtest/gtest.h>

namespace backer::testing {
namespace {

using namespace std::chrono_literals;

/// Helper: create a time_point from a date (local time).
std::chrono::system_clock::time_point makeTime(int year, int mon, int day,
                                                int hour, int min, int sec)
{
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon  = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min  = min;
    tm.tm_sec  = sec;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// ══════════════════════════════════════════════════════════════════════════════
// calcNextFire
// ══════════════════════════════════════════════════════════════════════════════

TEST(CcronexprTest, DailyAt2am) {
    // "0 2 * * *" — every day at 02:00
    auto from = makeTime(2026, 7, 12, 10, 30, 0);
    auto next = BackupScheduler::calcNextFire("0 2 * * *", from);

    // Next day at 02:00 (July 13)
    auto expected = makeTime(2026, 7, 13, 2, 0, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, DailyAt2amBeforeMidnight) {
    auto from = makeTime(2026, 7, 12, 1, 0, 0);
    auto next = BackupScheduler::calcNextFire("0 2 * * *", from);

    // Same day at 02:00
    auto expected = makeTime(2026, 7, 12, 2, 0, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, EveryHour) {
    // "0 * * * *" — at minute 0 of every hour
    auto from = makeTime(2026, 7, 12, 10, 30, 0);
    auto next = BackupScheduler::calcNextFire("0 * * * *", from);

    auto expected = makeTime(2026, 7, 12, 11, 0, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, Every15Min) {
    // "*/15 * * * *" — every 15 minutes
    auto from = makeTime(2026, 7, 12, 10, 0, 0);
    auto next = BackupScheduler::calcNextFire("*/15 * * * *", from);

    auto expected = makeTime(2026, 7, 12, 10, 15, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, Every15MinFromOffset) {
    auto from = makeTime(2026, 7, 12, 10, 17, 0);
    auto next = BackupScheduler::calcNextFire("*/15 * * * *", from);

    auto expected = makeTime(2026, 7, 12, 10, 30, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, WeekdaysOnly) {
    // "0 9 * * 1-5" — weekdays at 09:00
    // July 12, 2026 is a Sunday → next should be Monday July 13
    auto from = makeTime(2026, 7, 12, 0, 0, 0);
    auto next = BackupScheduler::calcNextFire("0 9 * * 1-5", from);

    auto expected = makeTime(2026, 7, 13, 9, 0, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, WeeklyOnSunday) {
    // "0 0 * * 0" — every Sunday at midnight
    // July 12, 2026 is a Sunday → next should be next Sunday July 19
    auto from = makeTime(2026, 7, 12, 10, 0, 0);
    auto next = BackupScheduler::calcNextFire("0 0 * * 0", from);

    auto expected = makeTime(2026, 7, 19, 0, 0, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, RangeHours) {
    // "0 9-18 * * *" — every hour from 9 to 18
    auto from = makeTime(2026, 7, 12, 17, 30, 0);
    auto next = BackupScheduler::calcNextFire("0 9-18 * * *", from);

    auto expected = makeTime(2026, 7, 12, 18, 0, 0);
    EXPECT_EQ(expected, next);
}

TEST(CcronexprTest, InvalidExpression) {
    // Garbage expression — should return time_point::min() to signal error
    auto from = makeTime(2026, 7, 12, 10, 0, 0);
    auto next = BackupScheduler::calcNextFire("not-a-cron", from);
    EXPECT_EQ(std::chrono::system_clock::time_point::min(), next);
}

TEST(CcronexprTest, NeverFires) {
    // "0 0 30 2 *" — Feb 30 never exists
    auto from = makeTime(2026, 7, 12, 10, 0, 0);
    auto next = BackupScheduler::calcNextFire("0 0 30 2 *", from);
    EXPECT_EQ(std::chrono::system_clock::time_point::max(), next);
}

// ══════════════════════════════════════════════════════════════════════════════
// makeSnapshotPath
// ══════════════════════════════════════════════════════════════════════════════

TEST(SnapshotPathTest, ProducesTimestampedName) {
    auto path = BackupScheduler::makeSnapshotPath("/tmp/backups");

    // Use generic_string for cross-platform path checking (forward slashes everywhere)
    auto str = path.generic_string();
    EXPECT_TRUE(str.find("/tmp/backups/") == 0)
        << "Path should be under base: " << str;

    // Filename should be YYYYMMDD_HHMMSS (15 chars)
    auto filename = path.filename().string();
    EXPECT_EQ(15, filename.size());
    EXPECT_EQ('_', filename[8]);

    // All characters before '_' should be digits
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(filename[i])))
            << "Expected digit at position " << i;
    }
    // All characters after '_' should be digits
    for (int i = 9; i < 15; ++i) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(filename[i])))
            << "Expected digit at position " << i;
    }
}

} // namespace
} // namespace backer::testing

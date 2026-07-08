#include "core/types.h"
#include "filters/criteria_filter.h"
#include "filters/filter.h"

#include <gtest/gtest.h>
#include <filesystem>

namespace backer::test {
namespace {

FileEntry makeEntry(
    std::string const& relativePath,
    FileType type = FileType::kRegular,
    uint64_t size = 0,
    int64_t mtimeSec = 1000000,
    uint32_t ownerId = 1000,
    std::string const& symlinkTarget = "")
{
    FileEntry e;
    e.relativePath = std::filesystem::path(relativePath);
    e.type = type;
    e.size = size;
    e.metadata.ownerId = ownerId;
    e.metadata.groupId = 1000;
    e.metadata.modifyTimeSec = mtimeSec;
    e.metadata.modifyTimeNsec = 0;
    e.symlinkTarget = std::filesystem::path(symlinkTarget);
    return e;
}

} // anonymous namespace

// ── Path include ────────────────────────────────────────────────────────────

TEST(CriteriaFilterTest, IncludePathGlob)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().pathGlob = "src/*.cpp";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("src/main.cpp"),
        makeEntry("src/utils.h"),
        makeEntry("README.md"),
        makeEntry("src/sub/util.cpp"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "src/main.cpp");
}

TEST(CriteriaFilterTest, IncludePathGlobRecursive)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().pathGlob = "*.txt";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("a.txt"),
        makeEntry("sub/b.txt"),
        makeEntry("c.cpp"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "a.txt");
}

// ── Name include ────────────────────────────────────────────────────────────

TEST(CriteriaFilterTest, IncludeNameGlob)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.md";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("README.md"),
        makeEntry("src/CHANGES.md"),
        makeEntry("main.cpp"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].relativePath.string(), "README.md");
    EXPECT_EQ(result[1].relativePath.string(), "src/CHANGES.md");
}

// ── File type include ───────────────────────────────────────────────────────

TEST(CriteriaFilterTest, IncludeFileType)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().fileType = FileType::kDirectory;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("file.txt"),
        makeEntry("dir", FileType::kDirectory),
        makeEntry("subdir", FileType::kDirectory),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].relativePath.string(), "dir");
    EXPECT_EQ(result[1].relativePath.string(), "subdir");
}

// ── Time range include ──────────────────────────────────────────────────────

TEST(CriteriaFilterTest, IncludeTimeRange)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().timeRange = TimeRange{};
    criteria.back().timeRange->hasAfter = true;
    criteria.back().timeRange->afterSec = 2000000;
    criteria.back().timeRange->afterNsec = 0;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("old.txt", FileType::kRegular, 0, 1000000),
        makeEntry("new.txt", FileType::kRegular, 0, 3000000),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "new.txt");
}

TEST(CriteriaFilterTest, IncludeTimeRangeBefore)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().timeRange = TimeRange{};
    criteria.back().timeRange->hasBefore = true;
    criteria.back().timeRange->beforeSec = 2000000;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("old.txt", FileType::kRegular, 0, 1000000),
        makeEntry("new.txt", FileType::kRegular, 0, 3000000),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "old.txt");
}

// ── Size range include ──────────────────────────────────────────────────────

TEST(CriteriaFilterTest, IncludeSizeRange)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().sizeRange = SizeRange{};
    criteria.back().sizeRange->hasMin = true;
    criteria.back().sizeRange->minSize = 100;
    criteria.back().sizeRange->hasMax = true;
    criteria.back().sizeRange->maxSize = 1000;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("small.txt", FileType::kRegular, 50),
        makeEntry("medium.txt", FileType::kRegular, 500),
        makeEntry("large.txt", FileType::kRegular, 2000),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "medium.txt");
}

// ── Owner include ───────────────────────────────────────────────────────────

TEST(CriteriaFilterTest, IncludeOwner)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().ownerId = 1001;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("user1.txt", FileType::kRegular, 0, 0, 1000),
        makeEntry("user2.txt", FileType::kRegular, 0, 0, 1001),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "user2.txt");
}

// ── Exclude ─────────────────────────────────────────────────────────────────

TEST(CriteriaFilterTest, ExcludeType)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().fileType = FileType::kSymlink;
    criteria.back().exclude = true;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("file.txt"),
        makeEntry("link", FileType::kSymlink),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "file.txt");
}

TEST(CriteriaFilterTest, ExcludePathGlob)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().pathGlob = "tmp/*";
    criteria.back().exclude = true;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("important.txt"),
        makeEntry("tmp/cache.dat"),
        makeEntry("tmp/sub/other.dat"),
    };
    auto result = filter.apply(entries);

    // With FNM_PATHNAME semantics, * does not match /
    // so tmp/sub/other.dat passes through
    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].relativePath.string(), "important.txt");
    EXPECT_EQ(result[1].relativePath.string(), "tmp/sub/other.dat");
}

// ── Exclude priority ────────────────────────────────────────────────────────

TEST(CriteriaFilterTest, ExcludeOverridesInclude)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.cpp";

    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*test*";
    criteria.back().exclude = true;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("main.cpp"),
        makeEntry("test_main.cpp"),
        makeEntry("utils.h"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "main.cpp");
}

// ── Multiple include — OR within same dim, AND across dims ──────────────

TEST(CriteriaFilterTest, IncludeAcrossDimensionsAnd)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().fileType = FileType::kDirectory;

    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "build";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("build", FileType::kDirectory),  // matches both
        makeEntry("src", FileType::kDirectory),     // matches type only
        makeEntry("build", FileType::kRegular),     // matches name only
    };
    auto result = filter.apply(entries);

    // AND across dims: must be dir AND named "build"
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "build");
    EXPECT_EQ(result[0].type, FileType::kDirectory);
}

TEST(CriteriaFilterTest, IncludeSameDimensionOr)
{
    // Same attribute with multiple values — OR within the dimension
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().fileType = FileType::kDirectory;

    criteria.push_back(FilterCriteria{});
    criteria.back().fileType = FileType::kSymlink;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("file.txt"),
        makeEntry("dir", FileType::kDirectory),
        makeEntry("link", FileType::kSymlink),
        makeEntry("fifo", FileType::kFifo),
    };
    auto result = filter.apply(entries);

    // OR within type dim: dir OR symlink
    ASSERT_EQ(result.size(), 2U);
}

// ── AND within a single criterion ──────────────────────────────────────────

TEST(CriteriaFilterTest, SingleCriterionAndFields)
{
    std::vector<FilterCriteria> criteria;
    FilterCriteria c;
    c.fileType = FileType::kDirectory;
    c.nameGlob = "build";
    criteria.push_back(std::move(c));

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("build", FileType::kDirectory),  // matches both
        makeEntry("src", FileType::kDirectory),     // matches type only
        makeEntry("build", FileType::kRegular),     // matches name only
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "build");
    EXPECT_EQ(result[0].type, FileType::kDirectory);
}

// ── No include criteria = all pass ──────────────────────────────────────────

TEST(CriteriaFilterTest, NoIncludeCriteriaAllPass)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.log";
    criteria.back().exclude = true;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("file.txt"),
        makeEntry("debug.log"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "file.txt");
}

// ── Empty criteria passes all ───────────────────────────────────────────────

TEST(CriteriaFilterTest, EmptyCriteriaPassesAll)
{
    CriteriaFilter filter({});

    auto entries = std::vector<FileEntry>{
        makeEntry("a.txt"),
        makeEntry("b.log"),
    };
    auto result = filter.apply(entries);

    EXPECT_EQ(result.size(), 2U);
}

// ── Empty input ─────────────────────────────────────────────────────────────

TEST(CriteriaFilterTest, EmptyInput)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.cpp";

    CriteriaFilter filter(criteria);
    auto result = filter.apply({});

    EXPECT_TRUE(result.empty());
}

// ── All filtered out ────────────────────────────────────────────────────────

TEST(CriteriaFilterTest, AllFilteredOut)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "nothing*";

    CriteriaFilter filter(criteria);
    auto entries = std::vector<FileEntry>{
        makeEntry("a.txt"),
        makeEntry("b.log"),
    };
    auto result = filter.apply(entries);

    EXPECT_TRUE(result.empty());
}

// ══════════════════════════════════════════════════════════════════════════════
// NoopFilter
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, NoopFilterPassesAll)
{
    NoopFilter filter;
    auto entries = std::vector<FileEntry>{
        makeEntry("a.txt"),
        makeEntry("b.txt"),
    };
    auto result = filter.apply(entries);
    EXPECT_EQ(result.size(), 2U);
}

TEST(CriteriaFilterTest, NoopFilterEmptyInput)
{
    NoopFilter filter;
    auto result = filter.apply({});
    EXPECT_TRUE(result.empty());
}

// ══════════════════════════════════════════════════════════════════════════════
// Complex glob patterns
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, IncludeWildcardQuestionMark)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().pathGlob = "file?.txt";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("file1.txt"),
        makeEntry("file12.txt"),
        makeEntry("file.txt"),
        makeEntry("other.txt"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "file1.txt");
}

TEST(CriteriaFilterTest, IncludeBracketGlob)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "[ab]*.txt";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("a_file.txt"),
        makeEntry("b_file.txt"),
        makeEntry("c_file.txt"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].relativePath.string(), "a_file.txt");
    EXPECT_EQ(result[1].relativePath.string(), "b_file.txt");
}

// ══════════════════════════════════════════════════════════════════════════════
// Size boundary conditions
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, SizeExactlyAtMin)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().sizeRange = SizeRange{};
    criteria.back().sizeRange->hasMin = true;
    criteria.back().sizeRange->minSize = 100;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("exact.txt", FileType::kRegular, 100),
        makeEntry("small.txt", FileType::kRegular, 99),
        makeEntry("large.txt", FileType::kRegular, 101),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 2U);
}

TEST(CriteriaFilterTest, SizeExactlyAtMax)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().sizeRange = SizeRange{};
    criteria.back().sizeRange->hasMax = true;
    criteria.back().sizeRange->maxSize = 100;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("exact.txt", FileType::kRegular, 100),
        makeEntry("small.txt", FileType::kRegular, 99),
        makeEntry("large.txt", FileType::kRegular, 101),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 2U);
}

TEST(CriteriaFilterTest, SizeZeroBytes)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().sizeRange = SizeRange{};
    criteria.back().sizeRange->hasMin = true;
    criteria.back().sizeRange->minSize = 0;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("empty.txt", FileType::kRegular, 0),
        makeEntry("data.txt", FileType::kRegular, 100),
    };
    auto result = filter.apply(entries);
    EXPECT_EQ(result.size(), 2U);
}

// ══════════════════════════════════════════════════════════════════════════════
// Time boundary
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, TimeExactlyAtBoundary)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().timeRange = TimeRange{};
    criteria.back().timeRange->hasAfter = true;
    criteria.back().timeRange->afterSec = 1000;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("at.txt",     FileType::kRegular, 0, 1000),
        makeEntry("before.txt", FileType::kRegular, 0, 999),
        makeEntry("after.txt",  FileType::kRegular, 0, 1001),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 2U);
}

// ══════════════════════════════════════════════════════════════════════════════
// Multiple include — OR within same dim, AND across dims partial match
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, IncludeAcrossSomeMatch)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.txt";
    criteria.push_back(FilterCriteria{});
    criteria.back().pathGlob = "sub/*";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("sub/readme.txt"),   // matches both
        makeEntry("sub/notes.md"),     // matches path only
        makeEntry("root.txt"),         // matches name only
        makeEntry("other.log"),        // matches neither
    };
    auto result = filter.apply(entries);

    // AND across dims: must match name *.txt AND path sub/*
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "sub/readme.txt");
}

// ══════════════════════════════════════════════════════════════════════════════
// Path with special characters
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, IncludePathWithSpaces)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.txt";

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("my document.txt"),
        makeEntry("noextension"),
    };
    auto result = filter.apply(entries);
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "my document.txt");
}

// ══════════════════════════════════════════════════════════════════════════════
// Exclude multiple criteria — ANY match excludes
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, ExcludeMultipleCriteria)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.log";
    criteria.back().exclude = true;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "*.tmp";
    criteria.back().exclude = true;

    CriteriaFilter filter(criteria);

    auto entries = std::vector<FileEntry>{
        makeEntry("data.log"),
        makeEntry("cache.tmp"),
        makeEntry("main.txt"),
    };
    auto result = filter.apply(entries);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].relativePath.string(), "main.txt");
}

// ══════════════════════════════════════════════════════════════════════════════
// Large entry list — performance sanity
// ══════════════════════════════════════════════════════════════════════════════

TEST(CriteriaFilterTest, LargeEntryList)
{
    std::vector<FilterCriteria> criteria;
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "keep*";
    criteria.push_back(FilterCriteria{});
    criteria.back().nameGlob = "skip*";
    criteria.back().exclude = true;

    CriteriaFilter filter(criteria);

    std::vector<FileEntry> entries;
    entries.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        if (i < 500) {
            entries.push_back(makeEntry("keep_" + std::to_string(i) + ".txt"));
        } else {
            entries.push_back(makeEntry("skip_" + std::to_string(i) + ".txt"));
        }
    }

    auto result = filter.apply(entries);
    EXPECT_EQ(result.size(), 500U);
}

} // namespace backer::test

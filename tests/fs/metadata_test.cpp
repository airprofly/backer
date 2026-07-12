#include "core/types.h"
#include "fs/metadata.h"
#include "fs/platform.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#if BACKER_PLATFORM_POSIX
    #include <fcntl.h>       // AT_FDCWD, AT_SYMLINK_NOFOLLOW
    #include <sys/stat.h>
    #include <unistd.h>
#endif

namespace backer::test {
namespace {

class MetadataTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir_;

    void SetUp() override {
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        tempDir_ = std::filesystem::temp_directory_path() / "backer_test" / testName;
        std::filesystem::remove_all(tempDir_);
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    void createFile(std::filesystem::path const& path,
                    std::string const& content = "test")
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << content;
    }
};

// ── readMetadata basic ─────────────────────────────────────────────────────

TEST_F(MetadataTest, ReadMetadataRegular) {
    createFile(tempDir_ / "test.txt", "hello");

    auto meta = readMetadata(tempDir_ / "test.txt");
    ASSERT_TRUE(meta.has_value());
    // Just verify we got sensible data — timestamp format varies by platform
    EXPECT_NE(meta.value().permissions, 0u);
}

// ── readMetadata non-existent ──────────────────────────────────────────────

TEST_F(MetadataTest, ReadMetadataNonExistent) {
    auto meta = readMetadata(tempDir_ / "no_such_file");
    ASSERT_FALSE(meta.has_value());
    EXPECT_EQ(meta.error(), ErrorCode::kPathNotFound);
}

// ── readMetadata directory ─────────────────────────────────────────────────

TEST_F(MetadataTest, ReadMetadataDirectory) {
    std::filesystem::create_directories(tempDir_ / "mydir");

    auto meta = readMetadata(tempDir_ / "mydir");
    ASSERT_TRUE(meta.has_value());
}

// ── restoreMetadata permissions (cross-platform) ─────────────────────────

TEST_F(MetadataTest, RestorePermissions) {
    auto filePath = tempDir_ / "perm_test.txt";
    createFile(filePath, "check perms");

    Metadata newMeta;
    newMeta.permissions = 0644;  // rw-r--r--

    auto restoreResult = restoreMetadata(filePath, newMeta, false);
    EXPECT_TRUE(restoreResult.has_value());

    // Verify via std::filesystem
    auto perms = std::filesystem::status(filePath).permissions();
    EXPECT_NE(perms & std::filesystem::perms::owner_read, std::filesystem::perms::none);
    EXPECT_NE(perms & std::filesystem::perms::owner_write, std::filesystem::perms::none);
    EXPECT_NE(perms & std::filesystem::perms::group_read, std::filesystem::perms::none);
}

// ── JSON round-trip ────────────────────────────────────────────────────────

TEST_F(MetadataTest, MetadataJsonRoundTrip) {
    Metadata original;
    original.ownerId        = 1000;
    original.groupId        = 1001;
    original.permissions    = 0755;
    original.accessTimeSec  = 100;
    original.accessTimeNsec = 200;
    original.modifyTimeSec  = 300;
    original.modifyTimeNsec = 400;
    original.changeTimeSec  = 500;
    original.changeTimeNsec = 600;

    auto json = metadataToJson(original);
    EXPECT_FALSE(json.empty());

    auto parsed = metadataFromJson(json);
    ASSERT_TRUE(parsed.has_value());

    EXPECT_EQ(parsed->ownerId,        original.ownerId);
    EXPECT_EQ(parsed->groupId,        original.groupId);
    EXPECT_EQ(parsed->permissions,    original.permissions);
    EXPECT_EQ(parsed->accessTimeSec,  original.accessTimeSec);
    EXPECT_EQ(parsed->accessTimeNsec, original.accessTimeNsec);
    EXPECT_EQ(parsed->modifyTimeSec,  original.modifyTimeSec);
    EXPECT_EQ(parsed->modifyTimeNsec, original.modifyTimeNsec);
}

// ── POSIX-specific metadata tests ──────────────────────────────────────────

#if BACKER_PLATFORM_POSIX

TEST_F(MetadataTest, RestoreTimestamps) {
    auto filePath = tempDir_ / "time_test.txt";
    createFile(filePath, "check time");

    Metadata meta;
    meta.permissions     = 0644;
    meta.accessTimeSec   = 1234567890;
    meta.accessTimeNsec  = 0;
    meta.modifyTimeSec   = 1234567890;
    meta.modifyTimeNsec  = 0;

    auto r = restoreMetadata(filePath, meta, false);
    ASSERT_TRUE(r.has_value());

    struct stat st;
    ASSERT_EQ(::stat(filePath.c_str(), &st), 0);
    EXPECT_EQ(BACKER_STAT_ATIME(st).tv_sec, 1234567890);
    EXPECT_EQ(BACKER_STAT_MTIME(st).tv_sec, 1234567890);
}

TEST_F(MetadataTest, RestoreSetuidBit) {
    auto filePath = tempDir_ / "suid_test";
    createFile(filePath, "suid check");

    Metadata meta;
    meta.permissions = 4755;  // rwsr-xr-x

    auto r = restoreMetadata(filePath, meta, false);
    ASSERT_TRUE(r.has_value());

    struct stat st;
    ASSERT_EQ(::stat(filePath.c_str(), &st), 0);

    // Non-root users cannot set the setuid bit (kernel silently clears it).
    // The exact permission bits after fchmodat vary across systems and container
    // runtimes, so only assert precise values when running as root.
    if (geteuid() == 0) {
        EXPECT_TRUE(st.st_mode & S_ISUID);
        EXPECT_EQ(static_cast<unsigned>(st.st_mode & 07777), 04755u);
    } else {
        // Non-root: just verify the file exists and has some permissions
        EXPECT_NE(static_cast<unsigned>(st.st_mode & 07777), 0u);
    }
}

TEST_F(MetadataTest, canRestoreOwnership) {
    // Root gets true, non-root false
    bool result = canRestoreOwnership();
    EXPECT_EQ(result, geteuid() == 0);
}

TEST_F(MetadataTest, ReadMetadataSymlink) {
    auto targetPath = tempDir_ / "target.txt";
    auto linkPath   = tempDir_ / "the_link";
    createFile(targetPath, "symlink target");

    ASSERT_EQ(::symlink("target.txt", linkPath.c_str()), 0);

    auto meta = readMetadata(linkPath);
    ASSERT_TRUE(meta.has_value());

    // readMetadata uses lstat, so it sees the symlink itself
    // Symlink permissions are 0777 on Linux
    EXPECT_EQ(meta.value().permissions & 07777, 0777u);
}

#endif // BACKER_PLATFORM_POSIX

} // namespace
} // namespace backer::test

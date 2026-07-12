#include "core/types.h"
#include "fs/platform.h"
#include "fs/special_file.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#if BACKER_PLATFORM_POSIX
    #include <sys/stat.h>
    #include <unistd.h>
#endif

namespace backer::test {
namespace {

class SpecialFileTest : public ::testing::Test {
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

// ── detectFileType ─────────────────────────────────────────────────────────

TEST_F(SpecialFileTest, DetectRegularFile) {
    createFile(tempDir_ / "reg.txt");
    auto type = detectFileType(tempDir_ / "reg.txt");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type.value(), FileType::kRegular);
}

TEST_F(SpecialFileTest, DetectDirectory) {
    std::filesystem::create_directories(tempDir_ / "adir");
    auto type = detectFileType(tempDir_ / "adir");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type.value(), FileType::kDirectory);
}

TEST_F(SpecialFileTest, DetectNonExistent) {
    auto type = detectFileType(tempDir_ / "noexist");
    ASSERT_FALSE(type.has_value());
    EXPECT_EQ(type.error(), ErrorCode::kPathNotFound);
}

// ── createSymlink + readSymlink (POSIX-only; symlinks may not be
//    available on all Windows configurations) ───────────────────

#if BACKER_PLATFORM_POSIX

TEST_F(SpecialFileTest, SymlinkRoundTrip) {
    createFile(tempDir_ / "original.txt", "link target content");

    auto r = createSymlink(tempDir_ / "mylink", "original.txt");
    ASSERT_TRUE(r.has_value());

    auto target = readSymlink(tempDir_ / "mylink");
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target.value(), "original.txt");

    // Verify via detectFileType
    auto type = detectFileType(tempDir_ / "mylink");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type.value(), FileType::kSymlink);
}

TEST_F(SpecialFileTest, CreateSymlinkInNestedDir) {
    std::filesystem::create_directories(tempDir_ / "a" / "b");
    createFile(tempDir_ / "a" / "target.txt");

    auto r = createSymlink(tempDir_ / "a" / "b" / "link", "../target.txt");
    ASSERT_TRUE(r.has_value());

    auto target = readSymlink(tempDir_ / "a" / "b" / "link");
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target.value(), "../target.txt");
}

#else
// Non-POSIX: modern Windows (Developer Mode) supports symlinks;
// fallback environments may not. Adapt expectation to the runtime.
TEST_F(SpecialFileTest, SymlinkNotSupported) {
    auto r = createSymlink(tempDir_ / "link", "target");
    // If symlinks are not supported, verify we get an error.
    // If they are supported (Developer Mode on GitHub Actions), that is fine too.
    if (!r.has_value()) {
        EXPECT_EQ(r.error(), ErrorCode::kSpecialFileNotSupported);
    }
}
#endif // BACKER_PLATFORM_POSIX

// ── FIFO and Device tests (POSIX-only) ─────────────────────────────────────

#if BACKER_PLATFORM_POSIX

TEST_F(SpecialFileTest, DetectSymlink) {
    createFile(tempDir_ / "target");
    ASSERT_EQ(::symlink("target", (tempDir_ / "link").c_str()), 0);

    auto type = detectFileType(tempDir_ / "link");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type.value(), FileType::kSymlink);
}

TEST_F(SpecialFileTest, DetectFifo) {
    ASSERT_EQ(::mkfifo((tempDir_ / "myfifo").c_str(), 0644), 0);

    auto type = detectFileType(tempDir_ / "myfifo");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type.value(), FileType::kFifo);
}

TEST_F(SpecialFileTest, ReadSymlinkDangling) {
    // Create a dangling symlink manually
    ASSERT_EQ(::symlink("nowhere", (tempDir_ / "dangling").c_str()), 0);

    auto target = readSymlink(tempDir_ / "dangling");
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target.value(), "nowhere");
}

TEST_F(SpecialFileTest, CreateFifoAndDetect) {
    auto r = createFifo(tempDir_ / "pipe", 0644);
    ASSERT_TRUE(r.has_value());

    struct stat st;
    ASSERT_EQ(::lstat((tempDir_ / "pipe").c_str(), &st), 0);
    EXPECT_TRUE(S_ISFIFO(st.st_mode));

    // Verify type detection
    auto type = detectFileType(tempDir_ / "pipe");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type.value(), FileType::kFifo);
}

TEST_F(SpecialFileTest, CreateFifoWithPermissions) {
    auto r = createFifo(tempDir_ / "restricted_pipe", 0600);
    ASSERT_TRUE(r.has_value());

    struct stat st;
    ASSERT_EQ(::lstat((tempDir_ / "restricted_pipe").c_str(), &st), 0);
    EXPECT_TRUE(S_ISFIFO(st.st_mode));
    EXPECT_EQ(static_cast<unsigned>(st.st_mode & 07777), 0600u);
}

TEST_F(SpecialFileTest, CreateDeviceAsNonRoot) {
    // Non-root users cannot create device nodes.
    auto r = createDevice(tempDir_ / "null_clone",
                          FileType::kCharDevice, 1, 3, 0666);
    if (geteuid() == 0) {
        // Root: may succeed or fail depending on system
        SUCCEED();
    } else {
        // Non-root: expect permission denied
        ASSERT_FALSE(r.has_value());
    }
}

#endif // BACKER_PLATFORM_POSIX

// ── Unicode filename ───────────────────────────────────────────────────────

TEST_F(SpecialFileTest, DetectUnicodeFilename) {
    createFile(tempDir_ / "[中文].txt");
    auto type = detectFileType(tempDir_ / "[中文].txt");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(type.value(), FileType::kRegular);
}

} // namespace
} // namespace backer::test

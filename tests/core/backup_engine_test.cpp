#include "core/backup_engine.h"
#include "core/error_code.h"
#include "fs/fs_abstraction.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace backer::test {
namespace {

class BackupEngineTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir_;
    std::filesystem::path sourceDir_;
    std::filesystem::path backupDir_;

    void SetUp() override {
        // Create a unique temp directory for each test
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        tempDir_ = std::filesystem::temp_directory_path() / "backer_test" / testName;
        sourceDir_ = tempDir_ / "source";
        backupDir_ = tempDir_ / "backup";

        std::filesystem::remove_all(tempDir_);
        std::filesystem::create_directories(sourceDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    /// Create a file with the given content.
    void createFile(
        std::filesystem::path const& path,
        std::string const& content = "hello")
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << content;
    }

    /// Read a file's content as string.
    std::string readFile(std::filesystem::path const& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        return std::string(std::istreambuf_iterator<char>(file), {});
    }

    /// Create a small directory tree for testing.
    void createTestTree() {
        createFile(sourceDir_ / "file1.txt", "Hello, World!");
        createFile(sourceDir_ / "file2.txt", std::string(4096, 'B'));  // > 1 page
        createFile(sourceDir_ / "subdir" / "nested.txt", "Nested file");
        createFile(sourceDir_ / "subdir" / "deep" / "deep.txt", "Deep file");
        // Empty directory
        std::filesystem::create_directories(sourceDir_ / "emptydir");
    }
};

// ── Basic backup ─────────────────────────────────────────────────────────

TEST_F(BackupEngineTest, BackupBasic) {
    createTestTree();

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_EQ(result.stats.totalFiles, 4);
    EXPECT_EQ(result.stats.totalDirs, 3);  // subdir, subdir/deep, emptydir
}

// ── Backup content verification ──────────────────────────────────────────

TEST_F(BackupEngineTest, BackupContentMatches) {
    createTestTree();

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);
    ASSERT_TRUE(result.success);

    EXPECT_EQ(readFile(backupDir_ / "file1.txt"), "Hello, World!");
    EXPECT_EQ(readFile(backupDir_ / "subdir" / "nested.txt"), "Nested file");
    EXPECT_EQ(readFile(backupDir_ / "subdir" / "deep" / "deep.txt"), "Deep file");
}

// ── Backup empty directory ───────────────────────────────────────────────

TEST_F(BackupEngineTest, BackupWithEmptyDir) {
    std::filesystem::create_directories(sourceDir_ / "emptydir");

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(backupDir_ / "emptydir"));
    EXPECT_TRUE(std::filesystem::is_directory(backupDir_ / "emptydir"));
}

// ── Backup non-existent source ───────────────────────────────────────────

TEST_F(BackupEngineTest, BackupNonExistentSource) {
    auto badPath = tempDir_ / "nonexistent";

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(badPath, backupDir_);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, ErrorCode::kPathNotFound);
}

// ── Backup empty source ──────────────────────────────────────────────────

TEST_F(BackupEngineTest, BackupEmptyDirectory) {
    // sourceDir_ is already created but empty
    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.stats.totalFiles, 0);
    EXPECT_TRUE(std::filesystem::exists(backupDir_));
}

// ── Backup with binary content ───────────────────────────────────────────

TEST_F(BackupEngineTest, BackupBinaryFile) {
    std::vector<char> binaryData(65536 + 1);  // > 64 KiB buffer
    for (std::size_t i = 0; i < binaryData.size(); ++i) {
        binaryData[i] = static_cast<char>(i & 0xFF);
    }

    auto filePath = sourceDir_ / "binary.bin";
    {
        std::ofstream f(filePath, std::ios::binary);
        f.write(binaryData.data(), static_cast<std::streamsize>(binaryData.size()));
    }

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.stats.totalBytes, binaryData.size());

    // Verify content
    auto backedUp = readFile(backupDir_ / "binary.bin");
    ASSERT_EQ(backedUp.size(), binaryData.size());
    EXPECT_EQ(memcmp(backedUp.data(), binaryData.data(), binaryData.size()), 0);
}

// ── Backup with deep nesting ─────────────────────────────────────────────

TEST_F(BackupEngineTest, BackupDeepNestedDirectories) {
    // Create a deeply nested path
    auto deepPath = sourceDir_;
    for (int i = 0; i < 20; ++i) {
        deepPath /= "d" + std::to_string(i);
    }
    createFile(deepPath / "leaf.txt", "deep leaf");

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success);

    // Verify the deep path was recreated
    auto verifyPath = backupDir_;
    for (int i = 0; i < 20; ++i) {
        verifyPath /= "d" + std::to_string(i);
    }
    EXPECT_TRUE(std::filesystem::exists(verifyPath / "leaf.txt"));
    EXPECT_EQ(readFile(verifyPath / "leaf.txt"), "deep leaf");
}

// ── Backup with file name as source (not directory) ──────────────────────

TEST_F(BackupEngineTest, BackupFileAsSourceFails) {
    createFile(sourceDir_ / "afile.txt");

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_ / "afile.txt", backupDir_);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, ErrorCode::kPathNotFound);
}

// ── Backup with unicode and special filenames ─────────────────────────────

TEST_F(BackupEngineTest, BackupUnicodeFilenames) {
    createFile(sourceDir_ / "[unicode].txt", "unicode content");
    createFile(sourceDir_ / "file with spaces.txt", "spaces content");
    createFile(sourceDir_ / ".hidden", "hidden content");

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_EQ(result.stats.totalFiles, 3);

    EXPECT_EQ(readFile(backupDir_ / "[unicode].txt"), "unicode content");
    EXPECT_EQ(readFile(backupDir_ / "file with spaces.txt"), "spaces content");
    EXPECT_EQ(readFile(backupDir_ / ".hidden"), "hidden content");
}

// ── Backup idempotency ───────────────────────────────────────────────────

TEST_F(BackupEngineTest, BackupIdempotency) {
    createTestTree();

    auto fs1 = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine1(std::move(fs1));
    auto result1 = engine1.backup(sourceDir_, backupDir_);
    ASSERT_TRUE(result1.success) << result1.errorMessage;

    // Backup again to the same destination
    auto fs2 = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine2(std::move(fs2));
    auto result2 = engine2.backup(sourceDir_, backupDir_);
    ASSERT_TRUE(result2.success) << result2.errorMessage;

    EXPECT_EQ(result2.stats.totalFiles, result1.stats.totalFiles);
    EXPECT_EQ(result2.stats.totalDirs, result1.stats.totalDirs);
    EXPECT_EQ(result2.stats.totalBytes, result1.stats.totalBytes);

    // Verify content is still correct after second backup
    EXPECT_EQ(readFile(backupDir_ / "file1.txt"), "Hello, World!");
    EXPECT_EQ(readFile(backupDir_ / "subdir" / "nested.txt"), "Nested file");
}

// ── Backup large file (1 MiB) ────────────────────────────────────────────

TEST_F(BackupEngineTest, BackupLargeFile) {
    constexpr std::size_t kOneMiB = 1024 * 1024;
    std::vector<char> largeData(kOneMiB);
    for (std::size_t i = 0; i < kOneMiB; ++i) {
        largeData[i] = static_cast<char>((i * 13) & 0xFF);
    }

    auto filePath = sourceDir_ / "large.bin";
    {
        std::ofstream f(filePath, std::ios::binary);
        f.write(largeData.data(), static_cast<std::streamsize>(kOneMiB));
    }

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_EQ(result.stats.totalFiles, 1);
    EXPECT_EQ(result.stats.totalBytes, kOneMiB);

    // Verify byte-for-byte
    auto backedUp = readFile(backupDir_ / "large.bin");
    ASSERT_EQ(backedUp.size(), kOneMiB);
    EXPECT_EQ(memcmp(backedUp.data(), largeData.data(), kOneMiB), 0);
}

// ── Backup mixed content ─────────────────────────────────────────────────

TEST_F(BackupEngineTest, BackupMixedContent) {
    createFile(sourceDir_ / "regular.txt", "regular");
    createFile(sourceDir_ / "empty.txt", "");                        // empty file
    createFile(sourceDir_ / "sub1" / "a.txt", "a content");
    createFile(sourceDir_ / "sub2" / "b.txt", "b content");
    std::filesystem::create_directories(sourceDir_ / "emptydir1");
    std::filesystem::create_directories(sourceDir_ / "emptydir2" / "sub");

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_EQ(result.stats.totalFiles, 4);   // regular.txt, empty.txt, sub1/a.txt, sub2/b.txt
    EXPECT_EQ(result.stats.totalDirs, 5);    // sub1, sub2, emptydir1, emptydir2, emptydir2/sub

    // Verify empty file is preserved
    EXPECT_TRUE(std::filesystem::exists(backupDir_ / "empty.txt"));
    EXPECT_EQ(readFile(backupDir_ / "empty.txt"), "");
}

// ── Backup single file directory ─────────────────────────────────────────

TEST_F(BackupEngineTest, BackupSingleFile) {
    createFile(sourceDir_ / "only.txt", "just one");

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_EQ(result.stats.totalFiles, 1);
    EXPECT_EQ(result.stats.totalDirs, 0);
    EXPECT_EQ(readFile(backupDir_ / "only.txt"), "just one");
}

// ── Backup nested empty directories ──────────────────────────────────────

TEST_F(BackupEngineTest, BackupNestedEmptyDirs) {
    std::filesystem::create_directories(sourceDir_ / "a" / "b" / "c");
    std::filesystem::create_directories(sourceDir_ / "x" / "y");

    auto fs = std::make_unique<LocalFsAbstraction>();
    BackupEngine engine(std::move(fs));
    auto result = engine.backup(sourceDir_, backupDir_);

    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_EQ(result.stats.totalFiles, 0);
    EXPECT_TRUE(std::filesystem::is_directory(backupDir_ / "a" / "b" / "c"));
    EXPECT_TRUE(std::filesystem::is_directory(backupDir_ / "x" / "y"));
}

} // namespace
} // namespace backer::test

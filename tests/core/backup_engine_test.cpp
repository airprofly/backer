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

} // namespace
} // namespace backer::test

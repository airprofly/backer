#include "core/backup_engine.h"
#include "core/restore_engine.h"
#include "core/error_code.h"
#include "fs/fs_abstraction.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace backer::test {
namespace {

class RestoreEngineTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir_;
    std::filesystem::path sourceDir_;
    std::filesystem::path backupDir_;
    std::filesystem::path restoreDir_;

    void SetUp() override {
        auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        tempDir_ = std::filesystem::temp_directory_path() / "backer_test" / testName;
        sourceDir_ = tempDir_ / "source";
        backupDir_ = tempDir_ / "backup";
        restoreDir_ = tempDir_ / "restore";

        std::filesystem::remove_all(tempDir_);
        std::filesystem::create_directories(sourceDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir_);
    }

    void createFile(
        std::filesystem::path const& path,
        std::string const& content = "hello")
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << content;
    }

    std::string readFile(std::filesystem::path const& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        return std::string(std::istreambuf_iterator<char>(file), {});
    }

    void createTestTree() {
        createFile(sourceDir_ / "file1.txt", "Hello, World!");
        createFile(sourceDir_ / "file2.txt", std::string(4096, 'B'));
        createFile(sourceDir_ / "subdir" / "nested.txt", "Nested file");
        std::filesystem::create_directories(sourceDir_ / "emptydir");
    }

    /// Perform a backup that tests can then restore from.
    void doBackup() {
        auto fs = std::make_unique<LocalFsAbstraction>();
        BackupEngine engine(std::move(fs));
        auto result = engine.backup(sourceDir_, backupDir_);
        ASSERT_TRUE(result.success) << result.errorMessage;
    }
};

// ── Restore basic ────────────────────────────────────────────────────────

TEST_F(RestoreEngineTest, RestoreBasic) {
    createTestTree();
    doBackup();

    auto fs = std::make_unique<LocalFsAbstraction>();
    RestoreEngine engine(std::move(fs));
    auto result = engine.restore(backupDir_, restoreDir_);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.stats.totalFiles, 3);  // file1.txt, file2.txt, subdir/nested.txt
    EXPECT_EQ(result.stats.totalDirs, 2);   // subdir, emptydir
}

// ── Restore content verification ─────────────────────────────────────────

TEST_F(RestoreEngineTest, RestoreContentMatches) {
    createTestTree();
    doBackup();

    auto fs = std::make_unique<LocalFsAbstraction>();
    RestoreEngine engine(std::move(fs));
    auto result = engine.restore(backupDir_, restoreDir_);
    ASSERT_TRUE(result.success);

    EXPECT_EQ(readFile(restoreDir_ / "file1.txt"), "Hello, World!");
    EXPECT_EQ(readFile(restoreDir_ / "subdir" / "nested.txt"), "Nested file");
}

// ── Restore empty directory ──────────────────────────────────────────────

TEST_F(RestoreEngineTest, RestorePreservesEmptyDirs) {
    std::filesystem::create_directories(sourceDir_ / "emptydir");
    doBackup();

    auto fs = std::make_unique<LocalFsAbstraction>();
    RestoreEngine engine(std::move(fs));
    auto result = engine.restore(backupDir_, restoreDir_);
    ASSERT_TRUE(result.success);

    EXPECT_TRUE(std::filesystem::exists(restoreDir_ / "emptydir"));
    EXPECT_TRUE(std::filesystem::is_directory(restoreDir_ / "emptydir"));
}

// ── Restore non-existent source ──────────────────────────────────────────

TEST_F(RestoreEngineTest, RestoreNonExistentSource) {
    auto badPath = tempDir_ / "nonexistent_backup";

    auto fs = std::make_unique<LocalFsAbstraction>();
    RestoreEngine engine(std::move(fs));
    auto result = engine.restore(badPath, restoreDir_);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, ErrorCode::kPathNotFound);
}

// ── End-to-end: backup → remove source → restore → verify ───────────────

TEST_F(RestoreEngineTest, BackupRestoreEndToEnd) {
    // 1. Create source with diverse file types and directory structures
    createFile(sourceDir_ / "report.md", "# Annual Report\n\nContent here.");
    createFile(sourceDir_ / "src" / "main.cc", "int main() { return 0; }");
    createFile(sourceDir_ / "src" / "util.cc", "// utility");
    createFile(sourceDir_ / "include" / "util.h", "#pragma once");
    createFile(sourceDir_ / "docs" / "manual.txt", "User manual");
    std::filesystem::create_directories(sourceDir_ / "empty_data");

    // 2. Backup
    {
        auto fs = std::make_unique<LocalFsAbstraction>();
        BackupEngine engine(std::move(fs));
        auto backupResult = engine.backup(sourceDir_, backupDir_);
        ASSERT_TRUE(backupResult.success);
        EXPECT_EQ(backupResult.stats.totalFiles, 5);
    }

    // 3. Remove original source
    std::filesystem::remove_all(sourceDir_);
    ASSERT_FALSE(std::filesystem::exists(sourceDir_));

    // 4. Restore from backup
    {
        auto fs = std::make_unique<LocalFsAbstraction>();
        RestoreEngine engine(std::move(fs));
        auto restoreResult = engine.restore(backupDir_, restoreDir_);
        ASSERT_TRUE(restoreResult.success);
        EXPECT_EQ(restoreResult.stats.totalFiles, 5);
    }

    // 5. Verify content
    EXPECT_TRUE(std::filesystem::exists(restoreDir_));
    EXPECT_TRUE(std::filesystem::exists(restoreDir_ / "empty_data"));
    EXPECT_TRUE(std::filesystem::is_directory(restoreDir_ / "empty_data"));

    EXPECT_EQ(readFile(restoreDir_ / "report.md"), "# Annual Report\n\nContent here.");
    EXPECT_EQ(readFile(restoreDir_ / "src" / "main.cc"), "int main() { return 0; }");
    EXPECT_EQ(readFile(restoreDir_ / "src" / "util.cc"), "// utility");
    EXPECT_EQ(readFile(restoreDir_ / "include" / "util.h"), "#pragma once");
    EXPECT_EQ(readFile(restoreDir_ / "docs" / "manual.txt"), "User manual");
}

// ── End-to-end with deep nesting + hidden dirs + binary content ──────────

TEST_F(RestoreEngineTest, BackupRestoreComplexTree) {
    // Source structure exercising deep nesting, hidden dirs, and mixed content
    createFile(sourceDir_ / ".hidden" / ".secret.txt", "hidden file");
    createFile(sourceDir_ / "docs" / "readme.md", "# Title\n\nBody.\n");
    createFile(sourceDir_ / "a" / "b" / "c" / "d" / "e" / "f" / "g" / "h" / "leaf.txt", "deep leaf");
    createFile(sourceDir_ / "src" / "main.rs", "fn main() {}");
    std::filesystem::create_directories(sourceDir_ / "empty1");
    std::filesystem::create_directories(sourceDir_ / "empty2" / "sub");

    // Backup
    {
        auto fs = std::make_unique<LocalFsAbstraction>();
        BackupEngine engine(std::move(fs));
        auto result = engine.backup(sourceDir_, backupDir_);
        ASSERT_TRUE(result.success);
    }

    // Remove source
    std::filesystem::remove_all(sourceDir_);

    // Restore
    {
        auto fs = std::make_unique<LocalFsAbstraction>();
        RestoreEngine engine(std::move(fs));
        auto result = engine.restore(backupDir_, restoreDir_);
        ASSERT_TRUE(result.success);
    }

    // Verify content
    EXPECT_TRUE(std::filesystem::exists(restoreDir_ / ".hidden" / ".secret.txt"));
    EXPECT_EQ(readFile(restoreDir_ / ".hidden" / ".secret.txt"), "hidden file");
    EXPECT_EQ(readFile(restoreDir_ / "docs" / "readme.md"), "# Title\n\nBody.\n");
    EXPECT_EQ(readFile(restoreDir_ / "a" / "b" / "c" / "d" / "e" / "f" / "g" / "h" / "leaf.txt"), "deep leaf");
    EXPECT_EQ(readFile(restoreDir_ / "src" / "main.rs"), "fn main() {}");
    EXPECT_TRUE(std::filesystem::exists(restoreDir_ / "empty1"));
    EXPECT_TRUE(std::filesystem::exists(restoreDir_ / "empty2" / "sub"));

    // Verify backup → restore structural equality
    std::error_code ec;
    std::vector<std::filesystem::path> backupPaths, restorePaths;
    for (auto const& e : std::filesystem::recursive_directory_iterator(backupDir_, ec)) {
        backupPaths.push_back(e.path().lexically_relative(backupDir_));
    }
    for (auto const& e : std::filesystem::recursive_directory_iterator(restoreDir_, ec)) {
        restorePaths.push_back(e.path().lexically_relative(restoreDir_));
    }
    std::sort(backupPaths.begin(), backupPaths.end());
    std::sort(restorePaths.begin(), restorePaths.end());
    EXPECT_EQ(backupPaths, restorePaths);
}

// ── Restore with binary content ──────────────────────────────────────────

TEST_F(RestoreEngineTest, RestoreBinaryFile) {
    std::vector<char> binaryData(128 * 1024);  // 128 KiB
    for (std::size_t i = 0; i < binaryData.size(); ++i) {
        binaryData[i] = static_cast<char>((i * 7) & 0xFF);
    }

    auto filePath = sourceDir_ / "large.bin";
    {
        std::ofstream f(filePath, std::ios::binary);
        f.write(binaryData.data(), static_cast<std::streamsize>(binaryData.size()));
    }
    doBackup();

    // Restore
    auto fs = std::make_unique<LocalFsAbstraction>();
    RestoreEngine engine(std::move(fs));
    auto result = engine.restore(backupDir_, restoreDir_);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.stats.totalBytes, binaryData.size());

    // Verify
    auto restored = readFile(restoreDir_ / "large.bin");
    ASSERT_EQ(restored.size(), binaryData.size());
    EXPECT_EQ(memcmp(restored.data(), binaryData.data(), binaryData.size()), 0);
}

} // namespace
} // namespace backer::test

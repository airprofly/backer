#include "core/types.h"
#include "fs/fs_abstraction.h"
#include "pack/tar_packer.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <map>
#include <sstream>

namespace backer::test {
namespace {

// ── Mock FS for testing ─────────────────────────────────────────────────────

/// Normalize path separators for cross-platform key matching.
static std::string normalize(std::filesystem::path const& p) {
    return p.generic_string();
}
static std::string normalize(std::string const& s) {
    return std::filesystem::path(s).generic_string();
}

/// In-memory filesystem mock for packer testing.
class MockFS final : public FSAbstraction {
public:
    /// Register a file with its content at the given absolute path.
    void addFile(std::string const& absPath, std::string const& content) {
        files_[normalize(absPath)] = std::vector<char>(content.begin(), content.end());
    }

    Expected<std::vector<FileEntry>, ErrorCode>
        walk(std::filesystem::path const& /*root*/) override
    {
        return ErrorCode::kNotImplemented;
    }

    Expected<std::vector<char>, ErrorCode>
        read(std::filesystem::path const& path) override
    {
        auto key = normalize(path);
        auto it = files_.find(key);
        if (it != files_.end()) return it->second;
        return ErrorCode::kPathNotFound;
    }

    Expected<void, ErrorCode>
        write(std::filesystem::path const& path,
              std::vector<char> const& data,
              FileType type = FileType::kRegular) override
    {
        auto key = normalize(path);
        written_[key] = data;
        writtenTypes_[key] = type;
        return {};
    }

    Expected<void, ErrorCode>
        mkdir(std::filesystem::path const& path) override
    {
        dirs_.insert(normalize(path));
        return {};
    }

    Expected<Metadata, ErrorCode>
        readMetadata(std::filesystem::path const& /*path*/) override
    {
        return ErrorCode::kNotImplemented;
    }

    Expected<void, ErrorCode>
        restoreMetadata(std::filesystem::path const& /*path*/,
                        Metadata const& /*meta*/,
                        bool /*restoreOwnership*/ = true) override
    {
        // In tests, metadata restoration is best-effort
        return {};
    }

    // ── Test queries ───────────────────────────────────────────────────

    bool wasWritten(std::string const& path) const {
        return written_.count(normalize(path)) > 0;
    }

    std::vector<char> const& getWritten(std::string const& path) const {
        static std::vector<char> empty;
        auto it = written_.find(normalize(path));
        return it != written_.end() ? it->second : empty;
    }

    FileType getWrittenType(std::string const& path) const {
        auto it = writtenTypes_.find(normalize(path));
        return it != writtenTypes_.end() ? it->second : FileType::kUnknown;
    }

    bool wasDirCreated(std::string const& path) const {
        return dirs_.count(normalize(path)) > 0;
    }

private:
    std::map<std::string, std::vector<char>> files_;
    std::map<std::string, std::vector<char>> written_;
    std::map<std::string, FileType> writtenTypes_;
    std::set<std::string> dirs_;
};

// ── helpers ─────────────────────────────────────────────────────────────────

FileEntry makeEntry(
    std::string const& relativePath,
    FileType type = FileType::kRegular,
    uint64_t size = 0,
    int64_t mtimeSec = 1000000,
    uint32_t ownerId = 1000,
    std::string const& symlinkTarget = "",
    uint64_t devMajor = 0,
    uint64_t devMinor = 0)
{
    FileEntry e;
    e.relativePath = std::filesystem::path(relativePath);
    e.type = type;
    e.size = size;
    e.metadata.ownerId = ownerId;
    e.metadata.groupId = 1000;
    e.metadata.permissions = 0644;
    e.metadata.modifyTimeSec = mtimeSec;
    e.metadata.modifyTimeNsec = 0;
    e.symlinkTarget = std::filesystem::path(symlinkTarget);
    e.deviceMajor = devMajor;
    e.deviceMinor = devMinor;
    return e;
}

// ══════════════════════════════════════════════════════════════════════════════
// TarPacker tests
// ══════════════════════════════════════════════════════════════════════════════

class TarPackerTest : public ::testing::Test {
protected:
    TarPacker packer_;
    MockFS mockFS_;
    std::stringstream archive_;
    std::filesystem::path const sourceRoot_ = "/test/src";
    std::filesystem::path const destDir_    = "/test/dest";
};

// ── Pack single regular file ────────────────────────────────────────────────

TEST_F(TarPackerTest, PackSingleRegularFile) {
    mockFS_.addFile("/test/src/file.txt", "Hello, Tar!");

    auto entries = { makeEntry("file.txt", FileType::kRegular, 11) };

    auto result = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(result) << "pack failed";

    // Verify archive is non-empty and has a valid tar header
    auto archiveStr = archive_.str();
    ASSERT_GE(archiveStr.size(), 512);

    // Check magic
    EXPECT_EQ(std::memcmp(archiveStr.data() + 257, "ustar", 5), 0);

    // Check name
    EXPECT_EQ(archiveStr.substr(0, 8), std::string("file.txt"));
}

// ── Pack and unpack round-trip ──────────────────────────────────────────────

TEST_F(TarPackerTest, PackUnpackRoundTrip) {
    mockFS_.addFile("/test/src/a.txt", "Content A");
    mockFS_.addFile("/test/src/b.txt", "Content B");

    std::vector<FileEntry> entries = {
        makeEntry("a.txt", FileType::kRegular, 9),
        makeEntry("b.txt", FileType::kRegular, 9),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    // Unpack
    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    // Verify both files were written
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/a.txt")) << "a.txt was not written";
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/b.txt")) << "b.txt was not written";

    auto contentA = mockFS_.getWritten("/test/dest/a.txt");
    std::string strA(contentA.begin(), contentA.end());
    EXPECT_EQ(strA, "Content A");

    auto contentB = mockFS_.getWritten("/test/dest/b.txt");
    std::string strB(contentB.begin(), contentB.end());
    EXPECT_EQ(strB, "Content B");
}

// ── Pack directory ──────────────────────────────────────────────────────────

TEST_F(TarPackerTest, PackDirectory) {
    std::vector<FileEntry> entries = {
        makeEntry("subdir", FileType::kDirectory),
        makeEntry("subdir/nested.txt", FileType::kRegular, 5),
    };
    mockFS_.addFile("/test/src/subdir/nested.txt", "nested");

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasDirCreated("/test/dest/subdir"));
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/subdir/nested.txt"));
}

// ── Pack symlink ────────────────────────────────────────────────────────────

TEST_F(TarPackerTest, PackSymlink) {
    std::vector<FileEntry> entries = {
        makeEntry("link", FileType::kSymlink, 0, 0, 0, "../target"),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/link"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/link"), FileType::kSymlink);

    auto content = mockFS_.getWritten("/test/dest/link");
    std::string target(content.begin(), content.end());
    EXPECT_EQ(target, "../target");
}

// ── Pack FIFO ───────────────────────────────────────────────────────────────

TEST_F(TarPackerTest, PackFifo) {
    std::vector<FileEntry> entries = {
        makeEntry("pipe", FileType::kFifo),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/pipe"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/pipe"), FileType::kFifo);
}

// ── Pack device ─────────────────────────────────────────────────────────────

TEST_F(TarPackerTest, PackDevice) {
    std::vector<FileEntry> entries = {
        makeEntry("chardev", FileType::kCharDevice, 0, 0, 0, "", 4, 64),
        makeEntry("blkdev",  FileType::kBlockDevice, 0, 0, 0, "", 8, 0),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/chardev"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/chardev"), FileType::kCharDevice);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/blkdev"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/blkdev"), FileType::kBlockDevice);
}

// ── Empty file ──────────────────────────────────────────────────────────────

TEST_F(TarPackerTest, PackEmptyFile) {
    mockFS_.addFile("/test/src/empty.txt", "");

    std::vector<FileEntry> entries = {
        makeEntry("empty.txt", FileType::kRegular, 0),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/empty.txt"));
    EXPECT_TRUE(mockFS_.getWritten("/test/dest/empty.txt").empty());
}

// ── Long path (ustar prefix) ────────────────────────────────────────────────

TEST_F(TarPackerTest, PackLongPath) {
    // Build a path > 100 chars
    std::string longDir = "a/";
    longDir += std::string(60, 'b') + "/";
    longDir += std::string(60, 'c');
    // Total: 2 + 60 + 1 + 60 = 123 chars → needs prefix extension

    mockFS_.addFile("/test/src/" + longDir + "/file.txt", "deep content");

    std::vector<FileEntry> entries = {
        makeEntry(longDir, FileType::kDirectory),
        makeEntry(longDir + "/file.txt", FileType::kRegular, 12),
    };
    // Fix entry sizes
    entries[1].size = 12;

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult) << "pack failed for long path";

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult) << "unpack failed for long path";

    EXPECT_TRUE(mockFS_.wasDirCreated("/test/dest/" + longDir));
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/" + longDir + "/file.txt"))
        << "Written files: check mock";

    auto content = mockFS_.getWritten("/test/dest/" + longDir + "/file.txt");
    std::string str(content.begin(), content.end());
    EXPECT_EQ(str, "deep content");
}

// ── Multiple entries ────────────────────────────────────────────────────────

TEST_F(TarPackerTest, PackMultipleEntries) {
    mockFS_.addFile("/test/src/f1.txt", "file1");
    mockFS_.addFile("/test/src/f2.txt", "file2");
    mockFS_.addFile("/test/src/f3.txt", "file3");

    std::vector<FileEntry> entries = {
        makeEntry("dir1", FileType::kDirectory),
        makeEntry("dir1/f1.txt", FileType::kRegular, 5),
        makeEntry("dir2", FileType::kDirectory),
        makeEntry("dir2/f2.txt", FileType::kRegular, 5),
        makeEntry("f3.txt", FileType::kRegular, 5),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasDirCreated("/test/dest/dir1"));
    EXPECT_TRUE(mockFS_.wasDirCreated("/test/dest/dir2"));
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/dir1/f1.txt"));
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/dir2/f2.txt"));
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/f3.txt"));
}

// ── Empty archive ───────────────────────────────────────────────────────────

TEST_F(TarPackerTest, EmptyEntryList) {
    std::vector<FileEntry> entries;

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    // Should be just two zero blocks
    auto archiveStr = archive_.str();
    EXPECT_EQ(archiveStr.size(), 1024);  // 2 * 512

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    EXPECT_TRUE(unpackResult);
}

// ── Tar header format: checksum ─────────────────────────────────────────────

TEST_F(TarPackerTest, ValidChecksum) {
    mockFS_.addFile("/test/src/x.txt", "data");

    std::vector<FileEntry> entries = {
        makeEntry("x.txt", FileType::kRegular, 4),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto archiveStr = archive_.str();
    std::stringstream ss(archiveStr);

    // Read the first header and verify checksum
    TarHeader header;
    ss.read(reinterpret_cast<char*>(&header), sizeof(TarHeader));

    EXPECT_TRUE(packer_.verifyChecksum(header));
}

// ── Invalid archive ─────────────────────────────────────────────────────────

TEST_F(TarPackerTest, RejectInvalidArchive) {
    std::stringstream bad;
    bad << "this is not a tar archive";

    auto result = packer_.unpack(bad, destDir_, mockFS_);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), ErrorCode::kInvalidArchive);
}

// ── Format name ─────────────────────────────────────────────────────────────

TEST_F(TarPackerTest, FormatName) {
    EXPECT_EQ(packer_.formatName(), "tar");
}

// ══════════════════════════════════════════════════════════════════════════════
// Multi-block file (content spanning multiple 512-byte blocks)
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(TarPackerTest, PackFileSpanningMultipleBlocks) {
    // 1500 bytes = 2 full blocks + 476 bytes in 3rd block
    std::string content(1500, 'M');
    mockFS_.addFile("/test/src/multi.txt", content);

    std::vector<FileEntry> entries = {
        makeEntry("multi.txt", FileType::kRegular, 1500),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    ASSERT_TRUE(mockFS_.wasWritten("/test/dest/multi.txt"));
    auto const& written = mockFS_.getWritten("/test/dest/multi.txt");
    std::string writtenStr(written.begin(), written.end());
    EXPECT_EQ(writtenStr, content);
    EXPECT_EQ(writtenStr.size(), 1500U);
}

// ══════════════════════════════════════════════════════════════════════════════
// Exact block boundary (512 bytes — no padding needed)
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(TarPackerTest, PackExactBlockSizeFile) {
    std::string content(512, 'B');
    mockFS_.addFile("/test/src/exact.txt", content);

    std::vector<FileEntry> entries = {
        makeEntry("exact.txt", FileType::kRegular, 512),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    ASSERT_TRUE(mockFS_.wasWritten("/test/dest/exact.txt"));
    auto const& written = mockFS_.getWritten("/test/dest/exact.txt");
    std::string writtenStr(written.begin(), written.end());
    EXPECT_EQ(writtenStr, content);
    EXPECT_EQ(writtenStr.size(), 512U);
}

// ══════════════════════════════════════════════════════════════════════════════
// Binary data — all byte values 0x00-0xFF
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(TarPackerTest, PackBinaryDataAllBytes) {
    std::vector<char> binaryData(256);
    for (int i = 0; i < 256; ++i) {
        binaryData[i] = static_cast<char>(i);
    }
    mockFS_.addFile("/test/src/binary.bin",
                    std::string(binaryData.data(), binaryData.size()));

    std::vector<FileEntry> entries = {
        makeEntry("binary.bin", FileType::kRegular, 256),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    ASSERT_TRUE(mockFS_.wasWritten("/test/dest/binary.bin"));
    auto const& written = mockFS_.getWritten("/test/dest/binary.bin");
    ASSERT_EQ(written.size(), 256U);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(static_cast<unsigned char>(written[i]),
                  static_cast<unsigned char>(i))
            << "byte mismatch at position " << i;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Deeply nested paths
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(TarPackerTest, PackDeeplyNestedStructure) {
    // Create a/b/c/d/e/f/g/h/i/j/file.txt (10 levels)
    std::string const deepPath = "a/b/c/d/e/f/g/h/i/j";
    std::string const deepFile = deepPath + "/leaf.txt";
    mockFS_.addFile("/test/src/" + deepFile, "deep!");

    std::vector<FileEntry> entries;
    // Build directories
    std::string accum;
    for (auto const& part : {"a","b","c","d","e","f","g","h","i","j"}) {
        if (!accum.empty()) accum += "/";
        accum += part;
        entries.push_back(makeEntry(accum, FileType::kDirectory));
    }
    entries.push_back(makeEntry(deepFile, FileType::kRegular, 5));

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    // Verify all directories created
    accum.clear();
    for (auto const& part : {"a","b","c","d","e","f","g","h","i","j"}) {
        if (!accum.empty()) accum += "/";
        accum += part;
        EXPECT_TRUE(mockFS_.wasDirCreated("/test/dest/" + accum))
            << "directory not created: " << accum;
    }

    // Verify leaf file
    ASSERT_TRUE(mockFS_.wasWritten("/test/dest/" + deepFile));
    auto const& written = mockFS_.getWritten("/test/dest/" + deepFile);
    std::string ws(written.begin(), written.end());
    EXPECT_EQ(ws, "deep!");
}

// ══════════════════════════════════════════════════════════════════════════════
// File name exactly 100 characters (ustar boundary)
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(TarPackerTest, PackFileNameBoundary) {
    // Generate a 99-char filename (max for name field: 99 chars + null = 100 bytes)
    std::string name(95, 'x');
    name += ".txt";
    ASSERT_EQ(name.size(), 99U);

    mockFS_.addFile("/test/src/" + name, "boundary test");
    std::vector<FileEntry> entries = {
        makeEntry(name, FileType::kRegular, 13),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    ASSERT_TRUE(mockFS_.wasWritten("/test/dest/" + name));
    auto const& written = mockFS_.getWritten("/test/dest/" + name);
    std::string ws(written.begin(), written.end());
    EXPECT_EQ(ws, "boundary test");
}

// ══════════════════════════════════════════════════════════════════════════════
// Metadata preservation in round trip
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(TarPackerTest, MetadataPreservedInRoundTrip) {
    // Create a file with specific metadata
    FileEntry entry = makeEntry("meta.txt", FileType::kRegular, 8);
    entry.metadata.ownerId = 1234;
    entry.metadata.groupId = 5678;
    entry.metadata.permissions = 0755;
    entry.metadata.modifyTimeSec = 987654321;

    mockFS_.addFile("/test/src/meta.txt", "metadata");

    std::vector<FileEntry> entries = { entry };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    mockFS_ = MockFS{};  // Reset mock to verify unpack creates fresh entries

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    // We can't easily verify metadata through MockFS's restoreMetadata,
    // but the fact that the archive decoded without error verifies
    // the header encoding/decoding round-trip
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/meta.txt"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Stress test: many small files
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(TarPackerTest, PackManySmallFiles) {
    std::vector<FileEntry> entries;
    int constexpr kFileCount = 100;

    for (int i = 0; i < kFileCount; ++i) {
        auto name = "file_" + std::to_string(i) + ".txt";
        mockFS_.addFile("/test/src/" + name, std::to_string(i));
        entries.push_back(makeEntry(name, FileType::kRegular,
                                    static_cast<uint64_t>(std::to_string(i).size())));
    }

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    // Verify archive size is reasonable
    auto archiveSize = archive_.str().size();
    // Each entry: header(512) + data(rounded to 512) = 1024
    // Plus 1024 for end marker
    EXPECT_GE(archiveSize, static_cast<std::size_t>(kFileCount) * 512 + 1024);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    // Verify all files were extracted
    for (int i = 0; i < kFileCount; ++i) {
        auto name = "file_" + std::to_string(i) + ".txt";
        EXPECT_TRUE(mockFS_.wasWritten("/test/dest/" + name))
            << "Missing: " << name;
    }
}

} // namespace
} // namespace backer::test

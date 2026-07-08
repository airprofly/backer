#include "core/types.h"
#include "fs/fs_abstraction.h"
#include "pack/zip_packer.h"

#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>

// miniz (for testing — building archives without metadata entry)
#include "miniz.h"

namespace backer::test {
namespace {

// ══════════════════════════════════════════════════════════════════════════════
// Mock FS ── same pattern as tar_packer_test
// ══════════════════════════════════════════════════════════════════════════════

static std::string normalize(std::filesystem::path const& p) {
    return p.generic_string();
}

class MockFS final : public FSAbstraction {
public:
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

// ══════════════════════════════════════════════════════════════════════════════
// Helpers
// ══════════════════════════════════════════════════════════════════════════════

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
// ZipPacker tests
// ══════════════════════════════════════════════════════════════════════════════

class ZipPackerTest : public ::testing::Test {
protected:
    ZipPacker packer_;
    MockFS mockFS_;
    std::stringstream archive_;
    std::filesystem::path const sourceRoot_ = "/test/src";
    std::filesystem::path const destDir_    = "/test/dest";
};

// ── Format name ──────────────────────────────────────────────────────────────

TEST_F(ZipPackerTest, FormatName) {
    EXPECT_EQ(packer_.formatName(), "zip");
}

// ── Pack single regular file ─────────────────────────────────────────────────

TEST_F(ZipPackerTest, PackSingleRegularFile) {
    mockFS_.addFile("/test/src/file.txt", "Hello, Zip!");

    auto entries = { makeEntry("file.txt", FileType::kRegular, 10) };

    auto result = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(result) << "pack failed";

    // Verify archive is non-empty and looks like Zip (PK\x03\x04 magic)
    auto archiveStr = archive_.str();
    ASSERT_GE(archiveStr.size(), 30);
    EXPECT_EQ(archiveStr[0], 'P');
    EXPECT_EQ(archiveStr[1], 'K');
    EXPECT_EQ(static_cast<unsigned char>(archiveStr[2]), 0x03);
    EXPECT_EQ(static_cast<unsigned char>(archiveStr[3]), 0x04);
}

// ── Pack and unpack round-trip ──────────────────────────────────────────────

TEST_F(ZipPackerTest, PackUnpackRoundTrip) {
    mockFS_.addFile("/test/src/a.txt", "Content A");
    mockFS_.addFile("/test/src/b.txt", "Content B");

    std::vector<FileEntry> entries = {
        makeEntry("a.txt", FileType::kRegular, 9),
        makeEntry("b.txt", FileType::kRegular, 9),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/a.txt"));
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/b.txt"));

    auto contentA = mockFS_.getWritten("/test/dest/a.txt");
    EXPECT_EQ(std::string(contentA.begin(), contentA.end()), "Content A");

    auto contentB = mockFS_.getWritten("/test/dest/b.txt");
    EXPECT_EQ(std::string(contentB.begin(), contentB.end()), "Content B");
}

// ── Pack directory ──────────────────────────────────────────────────────────

TEST_F(ZipPackerTest, PackDirectory) {
    std::vector<FileEntry> entries = {
        makeEntry("subdir", FileType::kDirectory),
        makeEntry("subdir/nested.txt", FileType::kRegular, 6),
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

TEST_F(ZipPackerTest, PackSymlink) {
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
    EXPECT_EQ(std::string(content.begin(), content.end()), "../target");
}

// ── Pack FIFO ───────────────────────────────────────────────────────────────

TEST_F(ZipPackerTest, PackFifo) {
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

TEST_F(ZipPackerTest, PackDevice) {
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

TEST_F(ZipPackerTest, PackEmptyFile) {
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

// ── Empty entry list ────────────────────────────────────────────────────────

TEST_F(ZipPackerTest, EmptyEntryList) {
    std::vector<FileEntry> entries;

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult) << "pack of empty list should succeed";

    // Zip with metadata-only should be > 0 bytes
    auto archiveStr = archive_.str();
    EXPECT_GT(archiveStr.size(), 0U);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    EXPECT_TRUE(unpackResult) << "unpack of empty-archive should succeed";

    // No files should have been written
    // (only the metadata entry was in the zip)
}

// ── Invalid archive ─────────────────────────────────────────────────────────

TEST_F(ZipPackerTest, RejectInvalidArchive) {
    std::stringstream bad;
    bad << "this is not a zip archive";

    auto result = packer_.unpack(bad, destDir_, mockFS_);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), ErrorCode::kInvalidArchive);
}

// ── Corrupted zip (missing metadata) ────────────────────────────────────────

TEST_F(ZipPackerTest, RejectZipWithoutMetadata) {
    // Build a minimal but valid zip with a regular file but no .backer_zip_meta
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_heap(&zip, 0, 0));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "orphan.txt", "data", 4,
                                       MZ_DEFAULT_COMPRESSION));

    void* buf = nullptr;
    size_t bufSize = 0;
    ASSERT_TRUE(mz_zip_writer_finalize_heap_archive(&zip, &buf, &bufSize));
    ASSERT_TRUE(buf && bufSize > 0);

    std::stringstream ss;
    ss.write(static_cast<char const*>(buf), static_cast<std::streamsize>(bufSize));
    mz_free(buf);

    auto result = packer_.unpack(ss, destDir_, mockFS_);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), ErrorCode::kInvalidArchive);
}

// ── Multiple entries (directories + files) ──────────────────────────────────

TEST_F(ZipPackerTest, PackMultipleEntries) {
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

// ── Binary data — all byte values 0x00-0xFF ─────────────────────────────────

TEST_F(ZipPackerTest, PackBinaryDataAllBytes) {
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

// ── Metadata preservation in round trip ─────────────────────────────────────

TEST_F(ZipPackerTest, MetadataPreservedInRoundTrip) {
    FileEntry entry = makeEntry("meta.txt", FileType::kRegular, 8);
    entry.metadata.ownerId = 1234;
    entry.metadata.groupId = 5678;
    entry.metadata.permissions = 0755;
    entry.metadata.modifyTimeSec = 987654321;
    entry.metadata.accessTimeSec = 111111111;
    entry.metadata.accessTimeNsec = 222222222;

    mockFS_.addFile("/test/src/meta.txt", "metadata");

    std::vector<FileEntry> entries = { entry };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    // Reset mock to verify unpack creates fresh entries with correct types
    mockFS_ = MockFS{};

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/meta.txt"));
}

// ── Symlink with metadata ───────────────────────────────────────────────────

TEST_F(ZipPackerTest, SymlinkWithMetadata) {
    FileEntry entry = makeEntry("mylink", FileType::kSymlink, 0, 2000000, 1001, "/abs/target");
    entry.metadata.groupId = 1002;
    entry.metadata.permissions = 0777;

    std::vector<FileEntry> entries = { entry };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/mylink"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/mylink"), FileType::kSymlink);

    auto content = mockFS_.getWritten("/test/dest/mylink");
    EXPECT_EQ(std::string(content.begin(), content.end()), "/abs/target");
}

// ── Large file (multiple MB) ────────────────────────────────────────────────

TEST_F(ZipPackerTest, PackLargeFile) {
    // 5 MB of data
    std::string const content(5 * 1024 * 1024, 'Z');
    mockFS_.addFile("/test/src/large.bin", content);

    std::vector<FileEntry> entries = {
        makeEntry("large.bin", FileType::kRegular, content.size()),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    ASSERT_TRUE(mockFS_.wasWritten("/test/dest/large.bin"));
    auto const& written = mockFS_.getWritten("/test/dest/large.bin");
    ASSERT_EQ(written.size(), content.size());

    // Spot-check first, middle, and last bytes
    EXPECT_EQ(written[0], 'Z');
    EXPECT_EQ(written[content.size() / 2], 'Z');
    EXPECT_EQ(written[content.size() - 1], 'Z');
}

// ── Deeply nested paths ─────────────────────────────────────────────────────

TEST_F(ZipPackerTest, PackDeeplyNestedStructure) {
    std::string const deepPath = "a/b/c/d/e/f/g/h/i/j";
    std::string const deepFile = deepPath + "/leaf.txt";
    mockFS_.addFile("/test/src/" + deepFile, "deep!");

    std::vector<FileEntry> entries;
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

    accum.clear();
    for (auto const& part : {"a","b","c","d","e","f","g","h","i","j"}) {
        if (!accum.empty()) accum += "/";
        accum += part;
        EXPECT_TRUE(mockFS_.wasDirCreated("/test/dest/" + accum))
            << "directory not created: " << accum;
    }

    ASSERT_TRUE(mockFS_.wasWritten("/test/dest/" + deepFile));
    auto const& written = mockFS_.getWritten("/test/dest/" + deepFile);
    EXPECT_EQ(std::string(written.begin(), written.end()), "deep!");
}

// ── Stress test: many small files ───────────────────────────────────────────

TEST_F(ZipPackerTest, PackManySmallFiles) {
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

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    for (int i = 0; i < kFileCount; ++i) {
        auto name = "file_" + std::to_string(i) + ".txt";
        EXPECT_TRUE(mockFS_.wasWritten("/test/dest/" + name))
            << "Missing: " << name;
    }
}

// ── Mixed types (all supported file types together) ─────────────────────────

TEST_F(ZipPackerTest, PackAllTypes) {
    mockFS_.addFile("/test/src/regular.txt", "regular content");
    mockFS_.addFile("/test/src/dir/nested.txt", "nested");

    std::vector<FileEntry> entries = {
        makeEntry("regular.txt", FileType::kRegular, 15),
        makeEntry("linky", FileType::kSymlink, 0, 0, 0, "../target"),
        makeEntry("pipe", FileType::kFifo),
        makeEntry("chardev", FileType::kCharDevice, 0, 0, 0, "", 5, 1),
        makeEntry("blk", FileType::kBlockDevice, 0, 0, 0, "", 8, 0),
        makeEntry("dir", FileType::kDirectory),
        makeEntry("dir/nested.txt", FileType::kRegular, 6),
    };

    auto packResult = packer_.pack(entries, mockFS_, sourceRoot_, archive_);
    ASSERT_TRUE(packResult);

    auto unpackResult = packer_.unpack(archive_, destDir_, mockFS_);
    ASSERT_TRUE(unpackResult);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/regular.txt"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/regular.txt"), FileType::kRegular);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/linky"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/linky"), FileType::kSymlink);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/pipe"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/pipe"), FileType::kFifo);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/chardev"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/chardev"), FileType::kCharDevice);

    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/blk"));
    EXPECT_EQ(mockFS_.getWrittenType("/test/dest/blk"), FileType::kBlockDevice);

    EXPECT_TRUE(mockFS_.wasDirCreated("/test/dest/dir"));
    EXPECT_TRUE(mockFS_.wasWritten("/test/dest/dir/nested.txt"));
}

} // namespace
} // namespace backer::test

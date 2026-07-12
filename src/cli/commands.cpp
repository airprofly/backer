#include "cli/commands.h"
#include "compress/build_compressor.h"
#include "core/backup_engine.h"
#include "core/restore_engine.h"
#include "core/types.h"
#include "crypto/build_encryptor.h"
#include "filters/criteria_filter.h"
#include "filters/filter.h"
#include "fs/fs_abstraction.h"
#include "fs/platform.h"
#include "pack/packer.h"
#include "pack/tar_packer.h"
#include "pack/zip_packer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <spdlog/spdlog.h>

#if BACKER_PLATFORM_POSIX
    #include <pwd.h>
    #include <termios.h>
    #include <unistd.h>
#endif

namespace backer::cli {
namespace {

// ══════════════════════════════════════════════════════════════════════════════
// Helper: parse file type string → FileType
// ══════════════════════════════════════════════════════════════════════════════

FileType parseFileType(std::string const& typeStr) {
    if (typeStr == "file"    || typeStr == "regular" || typeStr == "f") return FileType::kRegular;
    if (typeStr == "dir"     || typeStr == "directory" || typeStr == "d") return FileType::kDirectory;
    if (typeStr == "symlink" || typeStr == "link" || typeStr == "l")     return FileType::kSymlink;
    if (typeStr == "fifo"    || typeStr == "pipe" || typeStr == "p")     return FileType::kFifo;
    if (typeStr == "block"   || typeStr == "blockdev" || typeStr == "b") return FileType::kBlockDevice;
    if (typeStr == "char"    || typeStr == "chardev" || typeStr == "c")  return FileType::kCharDevice;
    if (typeStr == "socket"  || typeStr == "sock" || typeStr == "s")     return FileType::kSocket;
    return FileType::kUnknown;
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: resolve username → UID (POSIX only)
// ══════════════════════════════════════════════════════════════════════════════

bool resolveUser(std::string const& userName, uint32_t& uid) {
#if BACKER_PLATFORM_POSIX
    struct passwd pwd;
    struct passwd* result = nullptr;
    char buf[4096];
    int rc = getpwnam_r(userName.c_str(), &pwd, buf, sizeof(buf), &result);
    if (rc == 0 && result != nullptr) {
        uid = static_cast<uint32_t>(result->pw_uid);
        return true;
    }
    spdlog::warn("Cannot resolve user '{}': {}", userName,
                 rc == 0 ? "not found" : std::strerror(rc));
    return false;
#else
    (void)userName;
    (void)uid;
    spdlog::warn("User filtering not supported on this platform");
    return false;
#endif
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: construct Filter from BackupOptions
// ══════════════════════════════════════════════════════════════════════════════

std::unique_ptr<Filter> buildFilter(BackupOptions const& options) {
    std::vector<FilterCriteria> criteria;

    // ── Path includes ─────────────────────────────────────────────────
    for (auto const& pattern : options.includePaths) {
        FilterCriteria c;
        c.pathGlob = pattern;
        criteria.push_back(std::move(c));
    }

    // ── Path excludes ─────────────────────────────────────────────────
    for (auto const& pattern : options.excludePaths) {
        FilterCriteria c;
        c.pathGlob = pattern;
        c.exclude = true;
        criteria.push_back(std::move(c));
    }

    // ── Type includes ─────────────────────────────────────────────────
    for (auto const& typeStr : options.includeTypes) {
        auto ft = parseFileType(typeStr);
        if (ft != FileType::kUnknown) {
            FilterCriteria c;
            c.fileType = ft;
            criteria.push_back(std::move(c));
        } else {
            spdlog::warn("Unknown file type '{}' in --include-type", typeStr);
        }
    }

    // ── Type excludes ─────────────────────────────────────────────────
    for (auto const& typeStr : options.excludeTypes) {
        auto ft = parseFileType(typeStr);
        if (ft != FileType::kUnknown) {
            FilterCriteria c;
            c.fileType = ft;
            c.exclude = true;
            criteria.push_back(std::move(c));
        } else {
            spdlog::warn("Unknown file type '{}' in --exclude-type", typeStr);
        }
    }

    // ── Name includes ─────────────────────────────────────────────────
    for (auto const& pattern : options.includeNames) {
        FilterCriteria c;
        c.nameGlob = pattern;
        criteria.push_back(std::move(c));
    }

    // ── Name excludes ─────────────────────────────────────────────────
    for (auto const& pattern : options.excludeNames) {
        FilterCriteria c;
        c.nameGlob = pattern;
        c.exclude = true;
        criteria.push_back(std::move(c));
    }

    // ── Mtime before/after ────────────────────────────────────────────
    if (!options.mtimeBefore.empty() || !options.mtimeAfter.empty()) {
        FilterCriteria c;
        TimeRange tr;
        if (!options.mtimeBefore.empty()) {
            tr.hasBefore = true;
            tr.beforeSec = std::atol(options.mtimeBefore.c_str());
        }
        if (!options.mtimeAfter.empty()) {
            tr.hasAfter = true;
            tr.afterSec = std::atol(options.mtimeAfter.c_str());
        }
        c.timeRange = tr;
        criteria.push_back(std::move(c));
    }

    // ── Size min/max ──────────────────────────────────────────────────
    if (options.hasSizeMin || options.hasSizeMax) {
        FilterCriteria c;
        SizeRange sr;
        if (options.hasSizeMin) {
            sr.hasMin = true;
            sr.minSize = options.sizeMin;
        }
        if (options.hasSizeMax) {
            sr.hasMax = true;
            sr.maxSize = options.sizeMax;
        }
        c.sizeRange = sr;
        criteria.push_back(std::move(c));
    }

    // ── Owner ─────────────────────────────────────────────────────────
    if (!options.owner.empty()) {
        uint32_t uid = 0;
        if (resolveUser(options.owner, uid)) {
            FilterCriteria c;
            c.ownerId = uid;
            criteria.push_back(std::move(c));
        }
    }

    if (criteria.empty()) {
        return std::make_unique<NoopFilter>();
    }

    return std::make_unique<CriteriaFilter>(std::move(criteria));
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: construct Packer from options
// ══════════════════════════════════════════════════════════════════════════════

std::unique_ptr<Packer> buildPacker(std::string const& format) {
    if (format == "tar") {
        return std::make_unique<TarPacker>();
    }
    if (format == "zip") {
        return std::make_unique<ZipPacker>();
    }
    return nullptr;
}

// ══════════════════════════════════════════════════════════════════════════════
// Helper: read + compress/decompress + write
// ══════════════════════════════════════════════════════════════════════════════

/// Read @p inFile, transform via @p method, write to @p outFile.
/// Uses file_size()+single-read for efficiency (vs istreambuf_iterator).
bool transformFile(std::filesystem::path const& inFile,
                   std::filesystem::path const& outFile,
                   Compressor& comp,
                   Expected<void, ErrorCode> (Compressor::*method)(
                       backer::span<char const>, std::vector<char>&),
                   std::string_view action)
{
    std::ifstream in(inFile, std::ios::binary);
    if (!in) return false;

    std::error_code ec;
    auto fsize = std::filesystem::file_size(inFile, ec);
    if (ec) return false;

    std::vector<char> inData(fsize ? fsize : 1);
    in.read(inData.data(), static_cast<std::streamsize>(inData.size()));
    if (!in && !in.eof()) return false;
    inData.resize(static_cast<std::size_t>(in.gcount()));

    std::vector<char> outData;
    auto r = (comp.*method)(backer::span<char const>(inData.data(), inData.size()), outData);
    if (!r.has_value()) {
        spdlog::error("{} failed for {}", action, inFile.string());
        return false;
    }

    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(outData.data(), static_cast<std::streamsize>(outData.size()));
    return static_cast<bool>(out);
}

// ══════════════════════════════════════════════════════════════════════════════
// Password helpers
// ══════════════════════════════════════════════════════════════════════════════

/// Prompt the user for a password without echoing to terminal.
/// @param confirm  If true, prompt twice and confirm match.
/// @return The password, or std::nullopt if input fails.
std::optional<std::string> promptPassword(bool confirm) {
#if BACKER_PLATFORM_POSIX
    auto readPass = [](char const* prompt_str) -> std::optional<std::string> {
        std::string pwd;
        std::cerr << prompt_str << std::flush;

        struct termios old{}, newt{};
        if (tcgetattr(STDIN_FILENO, &old) != 0) return std::nullopt;
        newt = old;
        newt.c_lflag &= static_cast<tcflag_t>(~ECHO);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) return std::nullopt;

        std::getline(std::cin, pwd);
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
        std::cerr << "\n";
        return pwd;
    };

    auto pwd = readPass("Enter encryption password: ");
    if (!pwd) return std::nullopt;

    if (confirm) {
        auto confirmPwd = readPass("Confirm encryption password: ");
        if (!confirmPwd) return std::nullopt;
        if (*pwd != *confirmPwd) {
            std::cerr << "✗ Passwords do not match\n";
            return std::nullopt;
        }
    }

    return pwd;
#else
    // Fallback: just read without hiding
    auto pwd = []() -> std::optional<std::string> {
        std::string pwd;
        std::cerr << "Enter encryption password: " << std::flush;
        std::getline(std::cin, pwd);
        return pwd;
    }();
    if (!pwd) return std::nullopt;

    if (confirm) {
        auto confirmPwd = []() -> std::optional<std::string> {
            std::string pwd;
            std::cerr << "Confirm encryption password: " << std::flush;
            std::getline(std::cin, pwd);
            return pwd;
        }();
        if (!confirmPwd) return std::nullopt;
        if (*pwd != *confirmPwd) {
            std::cerr << "✗ Passwords do not match\n";
            return std::nullopt;
        }
    }
    return pwd;
#endif
}

// ══════════════════════════════════════════════════════════════════════════════
// Encrypt / Decrypt file helpers
// ══════════════════════════════════════════════════════════════════════════════

/// Read @p inFile, encrypt with @p enc, write to @p outFile.
static bool encryptFile(std::filesystem::path const& inFile,
                         std::filesystem::path const& outFile,
                         Encryptor& enc,
                         std::string_view password)
{
    std::ifstream in(inFile, std::ios::binary);
    if (!in) {
        spdlog::error("Cannot open input file: {}", inFile.string());
        return false;
    }

    std::error_code ec;
    auto fsize = std::filesystem::file_size(inFile, ec);
    if (ec) {
        spdlog::error("Cannot get file size: {}", inFile.string());
        return false;
    }

    std::vector<char> inData(fsize ? fsize : 1);
    in.read(inData.data(), static_cast<std::streamsize>(inData.size()));
    if (!in && !in.eof()) {
        spdlog::error("Failed to read: {}", inFile.string());
        return false;
    }
    inData.resize(static_cast<std::size_t>(in.gcount()));

    std::vector<char> outData;
    auto r = enc.encrypt(
        backer::span<char const>(inData.data(), inData.size()), outData, password);
    if (!r.has_value()) {
        spdlog::error("Encryption failed for {}", inFile.string());
        return false;
    }

    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("Cannot open output file: {}", outFile.string());
        return false;
    }
    out.write(outData.data(), static_cast<std::streamsize>(outData.size()));
    return static_cast<bool>(out);
}

/// Read @p inFile, decrypt with @p enc, write to @p outFile.
static bool decryptFile(std::filesystem::path const& inFile,
                         std::filesystem::path const& outFile,
                         Encryptor& enc,
                         std::string_view password)
{
    std::ifstream in(inFile, std::ios::binary);
    if (!in) {
        spdlog::error("Cannot open input file: {}", inFile.string());
        return false;
    }

    std::error_code ec;
    auto fsize = std::filesystem::file_size(inFile, ec);
    if (ec) {
        spdlog::error("Cannot get file size: {}", inFile.string());
        return false;
    }

    std::vector<char> inData(fsize ? fsize : 1);
    in.read(inData.data(), static_cast<std::streamsize>(inData.size()));
    if (!in && !in.eof()) {
        spdlog::error("Failed to read: {}", inFile.string());
        return false;
    }
    inData.resize(static_cast<std::size_t>(in.gcount()));

    std::vector<char> outData;
    auto r = enc.decrypt(
        backer::span<char const>(inData.data(), inData.size()), outData, password);
    if (!r.has_value()) {
        spdlog::error("Decryption failed for {} — wrong password or corrupted data",
                      inFile.string());
        return false;
    }

    std::ofstream out(outFile, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("Cannot open output file: {}", outFile.string());
        return false;
    }
    out.write(outData.data(), static_cast<std::streamsize>(outData.size()));
    return static_cast<bool>(out);
}

/// Resolve the encryption password: use the provided value, or prompt the user
/// interactively. During backup (backup=true) the user is asked to confirm.
static std::optional<std::string> resolvePassword(std::string const& provided,
                                                   bool confirm) {
    if (!provided.empty()) return provided;
    return promptPassword(confirm);
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════════
// handleBackup
// ══════════════════════════════════════════════════════════════════════════════

int handleBackup(
    std::filesystem::path const& source,
    std::filesystem::path const& destination,
    BackupOptions const& options)
{
    spdlog::info("Starting backup: {} → {}", source.string(), destination.string());

    if (!options.preserveMetadata) {
        spdlog::warn("Metadata preservation is DISABLED");
    }
    if (!options.handleSpecial) {
        spdlog::warn("Special file handling is DISABLED");
    }

    auto fs = std::make_unique<LocalFsAbstraction>();

    // Build filter
    auto filter = buildFilter(options);

    // Build packer
    auto packer = buildPacker(options.packFormat);
    if (packer) {
        spdlog::info("Packing format: {}", packer->formatName());
    }

    // ── Auto-append file extension for archive mode ────────────────
    // e.g. --pack tar → destination + ".tar", --pack zip → destination + ".zip"
    // Per docs/plans/05-packing.md naming convention.
    auto actualDest = destination;
    if (packer) {
        auto ext = std::string(".") + std::string(packer->formatName());
        auto destStr = destination.string();
        // Avoid double-adding extension if user already typed it
        if (destStr.size() < ext.size() ||
            destStr.compare(destStr.size() - ext.size(), ext.size(), ext) != 0) {
            actualDest = destination;
            actualDest += ext;
        }
        spdlog::info("Output archive: {}", actualDest.string());
    }

    BackupEngine engine(std::move(fs));
    auto result = engine.backup(source, actualDest, filter.get(), packer.get());

    if (!result.success) {
        std::cerr
            << "✗ Backup failed: " << result.errorMessage << "\n";
        return 1;
    }

    // ── Optional post-compression of the archive ───────────────────────
    auto currentDest = actualDest;
    if (!options.compressAlgo.empty()) {
        auto comp = buildCompressor(options.compressAlgo, options.compressLevel);
        if (!comp) {
            std::cerr << "✗ Unknown compression algorithm: " << options.compressAlgo << "\n";
            return 1;
        }
        auto compressedDest = currentDest;
        compressedDest += comp->suffix();
        spdlog::info("Compressing with {} → {}", options.compressAlgo, compressedDest.string());
        if (!transformFile(currentDest, compressedDest, *comp, &Compressor::compress, "Compression")) {
            std::cerr << "✗ Compression failed\n";
            return 1;
        }
        std::filesystem::remove(currentDest);
        currentDest = compressedDest;
    }

    // ── Optional post-encryption ───────────────────────────────────────
    auto finalDest = currentDest;
    if (!options.encryptAlgo.empty()) {
        auto enc = buildEncryptor(options.encryptAlgo);
        if (!enc) {
            std::cerr << "✗ Unknown encryption algorithm: " << options.encryptAlgo << "\n";
            return 1;
        }

        // Resolve password: use provided or prompt interactively
        auto password = resolvePassword(options.password, /*confirm=*/true);
        if (!password.has_value()) {
            std::cerr << "✗ No password provided for encryption\n";
            return 1;
        }

        finalDest += enc->suffix();  // ".enc"
        spdlog::info("Encrypting with {} → {}", enc->name(), finalDest.string());
        if (!encryptFile(currentDest, finalDest, *enc, *password)) {
            std::cerr << "✗ Encryption failed\n";
            return 1;
        }
        std::filesystem::remove(currentDest);
    }

    auto const& s = result.stats;
    std::cout
        << "✓ Backup completed successfully\n"
        << "  Archive: " << finalDest.string() << "\n"
        << "  Files:   " << s.totalFiles << "\n"
        << "  Dirs:    " << s.totalDirs << "\n"
        << "  Size:    " << s.totalBytes << " bytes\n"
        << "  Skipped: " << s.skipped << "\n"
        << "  Time:    " << s.elapsed.count() << " ms\n";
    return 0;
}

// ══════════════════════════════════════════════════════════════════════════════
// handleRestore
// ══════════════════════════════════════════════════════════════════════════════

int handleRestore(
    std::filesystem::path const& source,
    std::filesystem::path const& destination,
    RestoreOptions const& options)
{
    spdlog::info("Starting restore: {} → {}", source.string(), destination.string());

    if (!options.preserveMetadata) {
        spdlog::warn("Metadata preservation is DISABLED");
    }
    if (!options.handleSpecial) {
        spdlog::warn("Special file handling is DISABLED");
    }

    auto fs = std::make_unique<LocalFsAbstraction>();

    // ── Optional pre-decryption of the source archive ────────────────────
    auto currentSource = source;
    if (!options.decryptAlgo.empty()) {
        auto enc = buildEncryptor(options.decryptAlgo);
        if (!enc) {
            std::cerr << "✗ Unknown decryption algorithm: " << options.decryptAlgo << "\n";
            return 1;
        }

        auto password = resolvePassword(options.password, /*confirm=*/false);
        if (!password.has_value()) {
            std::cerr << "✗ No password provided for decryption\n";
            return 1;
        }

        auto decryptedPath = source;
        decryptedPath += ".decrypted";
        spdlog::info("Decrypting with {} → {}",
                     enc->name(), decryptedPath.string());
        if (!decryptFile(source, decryptedPath, *enc, *password)) {
            std::cerr << "✗ Decryption failed — wrong password or corrupted data\n";
            return 1;
        }
        currentSource = decryptedPath;
    }

    // ── Optional pre-decompression of the source archive ───────────────
    auto actualSource = currentSource;
    if (!options.decompressAlgo.empty()) {
        auto comp = buildCompressor(options.decompressAlgo, 0);
        if (!comp) {
            std::cerr << "✗ Unknown decompression algorithm: " << options.decompressAlgo << "\n";
            return 1;
        }
        // Decompress to a temp file in the same directory as source
        auto tempPath = currentSource;
        tempPath += ".decompressed";
        spdlog::info("Decompressing {} ({}) → {}",
                     currentSource.string(), options.decompressAlgo, tempPath.string());
        if (!transformFile(currentSource, tempPath, *comp, &Compressor::decompress, "Decompression")) {
            std::cerr << "✗ Decompression failed\n";
            return 1;
        }
        actualSource = tempPath;
    }

    // Build packer for archive mode
    auto packer = buildPacker(options.packFormat);
    if (packer) {
        spdlog::info("Unpacking format: {}", packer->formatName());
    }

    RestoreEngine engine(std::move(fs));
    auto result = engine.restore(actualSource, destination, packer.get());

    // Clean up temp files
    if (actualSource != currentSource) {
        std::filesystem::remove(actualSource);  // decompressed temp
    }
    if (currentSource != source) {
        std::filesystem::remove(currentSource);  // decrypted temp
    }

    if (result.success) {
        auto const& s = result.stats;
        std::cout
            << "✓ Restore completed successfully\n"
            << "  Files:   " << s.totalFiles << "\n"
            << "  Dirs:    " << s.totalDirs << "\n"
            << "  Size:    " << s.totalBytes << " bytes\n"
            << "  Skipped: " << s.skipped << "\n"
            << "  Time:    " << s.elapsed.count() << " ms\n";
        return 0;
    }

    std::cerr
        << "✗ Restore failed: " << result.errorMessage << "\n";
    return 1;
}

} // namespace backer::cli

#include "pack/zip_packer.h"
#include "fs/fs_abstraction.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <spdlog/spdlog.h>

// miniz library (fetched via CMake FetchContent)
#include "miniz.h"

namespace backer {
namespace {

// ══════════════════════════════════════════════════════════════════════════════
// JSON helpers (minimal — no external dependency)
// ══════════════════════════════════════════════════════════════════════════════

std::string jsonEscape(std::string const& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            default:
                // Control characters that should never appear in POSIX paths
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    r += buf;
                } else {
                    r += c;
                }
        }
    }
    return r;
}

std::string jsonUnescape(std::string const& s) {
    std::string r;
    r.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[++i]) {
                case '"':  r += '"';  break;
                case '\\': r += '\\'; break;
                case '/':  r += '/';  break;
                case 'n':  r += '\n'; break;
                case 'r':  r += '\r'; break;
                case 't':  r += '\t'; break;
                case 'u': {
                    // \uXXXX — parse 4 hex digits
                    if (i + 4 < s.size()) {
                        unsigned cp = 0;
                        for (int j = 0; j < 4; ++j) {
                            char h = s[++i];
                            cp <<= 4;
                            if (h >= '0' && h <= '9')       cp |= (h - '0');
                            else if (h >= 'a' && h <= 'f')  cp |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F')  cp |= (h - 'A' + 10);
                        }
                        r += static_cast<char>(cp);
                    }
                    break;
                }
                default: r += s[i]; break;  // unknown escape: keep as-is
            }
        } else {
            r += s[i];
        }
    }
    return r;
}

} // anonymous namespace

std::string ZipPacker::buildMetaJson(std::vector<FileEntry> const& files) {
    std::string json = R"({"v":1,"m":{)";
    bool first = true;
    for (auto const& f : files) {
        if (!first) json += ',';
        first = false;

        // Key: escaped relative path
        json += '"' + jsonEscape(f.relativePath.generic_string()) + "\":{";
        json += "\"t\":"  + std::to_string(static_cast<int>(f.type)) + ',';
        json += "\"u\":"  + std::to_string(f.metadata.ownerId) + ',';
        json += "\"g\":"  + std::to_string(f.metadata.groupId) + ',';
        json += "\"p\":"  + std::to_string(f.metadata.permissions) + ',';
        json += "\"as\":" + std::to_string(f.metadata.accessTimeSec) + ',';
        json += "\"an\":" + std::to_string(f.metadata.accessTimeNsec) + ',';
        json += "\"ms\":" + std::to_string(f.metadata.modifyTimeSec) + ',';
        json += "\"mn\":" + std::to_string(f.metadata.modifyTimeNsec) + ',';
        json += "\"l\":\""  + jsonEscape(f.symlinkTarget.generic_string()) + "\",";
        json += "\"dm\":" + std::to_string(f.deviceMajor) + ',';
        json += "\"dn\":" + std::to_string(f.deviceMinor) + '}';
    }
    json += "}}";
    return json;
}

namespace {

/// Skip whitespace, return position after the last whitespace char.
std::size_t skipWhitespace(std::string const& s, std::size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'
           || s[pos] == '\n' || s[pos] == '\r')) ++pos;
    return pos;
}

/// Parse a JSON string at @p pos in @p s.
/// Returns the unescaped string and the position after the closing quote.
struct StringResult { std::string value; std::size_t endPos; };
std::optional<StringResult> parseJsonString(std::string const& s, std::size_t pos) {
    if (pos >= s.size() || s[pos] != '"') return std::nullopt;
    ++pos;  // skip opening quote
    std::string val;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\') {
            // Collect raw escape sequence for jsonUnescape
            if (pos + 1 < s.size()) {
                val += s[pos++];  // backslash
                val += s[pos++];  // escape char
            }
        } else {
            val += s[pos++];
        }
    }
    if (pos >= s.size()) return std::nullopt;
    return StringResult{jsonUnescape(val), pos + 1};  // skip closing quote
}

} // anonymous namespace

std::map<std::string, std::string> ZipPacker::parseMetaEntries(
    std::string const& json)
{
    // Expected format: {"v":1,"m":{"path1":{...},"path2":{...}}}
    std::map<std::string, std::string> result;

    // Find "m":{ or "m": {
    auto mPos = json.find("\"m\"");
    if (mPos == std::string::npos) return result;
    mPos = json.find('{', mPos + 2);
    if (mPos == std::string::npos) return result;
    ++mPos;  // skip '{'

    // Parse each "path":{...} pair
    std::size_t const endMap = json.rfind('}');
    if (endMap == std::string::npos) return result;

    std::size_t pos = mPos;
    while (pos < endMap) {
        pos = skipWhitespace(json, pos);
        if (pos >= endMap || json[pos] == '}') break;

        // Parse key (path string)
        auto keyResult = parseJsonString(json, pos);
        if (!keyResult) break;
        std::string path = keyResult->value;
        pos = skipWhitespace(json, keyResult->endPos);

        // Expect ':'
        if (pos >= json.size() || json[pos] != ':') break;
        ++pos;
        pos = skipWhitespace(json, pos);

        // Parse value object {...}
        if (pos >= json.size() || json[pos] != '{') break;
        int braceDepth = 1;
        std::size_t objStart = pos;
        ++pos;
        while (pos < json.size() && braceDepth > 0) {
            if (json[pos] == '{') ++braceDepth;
            else if (json[pos] == '}') --braceDepth;
            // Skip string contents to avoid counting braces inside strings
            else if (json[pos] == '"') {
                ++pos;
                while (pos < json.size() && json[pos] != '"') {
                    if (json[pos] == '\\') ++pos;  // skip escaped char
                    ++pos;
                }
            }
            if (braceDepth > 0) ++pos;
        }
        if (braceDepth != 0) break;

        result[path] = json.substr(objStart, pos - objStart + 1);
        ++pos;  // skip '}'

        // Skip comma separator
        pos = skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') ++pos;
    }

    return result;
}

std::optional<int64_t> ZipPacker::extractField(
    std::string const& entryJson, std::string const& key)
{
    // Find "key": or "key": (with optional spaces)
    std::string search = '"' + key + '"';
    auto pos = entryJson.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos += search.size();
    pos = skipWhitespace(entryJson, pos);
    if (pos >= entryJson.size() || entryJson[pos] != ':') return std::nullopt;
    ++pos;
    pos = skipWhitespace(entryJson, pos);
    if (pos >= entryJson.size()) return std::nullopt;

    // Parse number (may be negative)
    bool neg = false;
    if (entryJson[pos] == '-') { neg = true; ++pos; }
    if (pos >= entryJson.size() || entryJson[pos] < '0' || entryJson[pos] > '9') {
        return std::nullopt;
    }
    int64_t val = 0;
    while (pos < entryJson.size() && entryJson[pos] >= '0' && entryJson[pos] <= '9') {
        val = val * 10 + (entryJson[pos++] - '0');
    }
    return neg ? -val : val;
}

std::string ZipPacker::extractStrField(
    std::string const& entryJson, std::string const& key)
{
    std::string search = '"' + key + '"';
    auto pos = entryJson.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    pos = skipWhitespace(entryJson, pos);
    if (pos >= entryJson.size() || entryJson[pos] != ':') return {};
    ++pos;
    pos = skipWhitespace(entryJson, pos);

    // Parse JSON string value
    auto sr = parseJsonString(entryJson, pos);
    if (!sr) return {};
    return sr->value;
}

auto ZipPacker::parseEntryMeta(std::string const& jsonBody,
                                std::string const& /*path*/)
    -> Expected<EntryMeta, ErrorCode>
{
    EntryMeta em;

    auto t = extractField(jsonBody, "t");
    if (!t) return ErrorCode::kInvalidArchive;
    em.type = static_cast<FileType>(*t);

    auto uid = extractField(jsonBody, "u");
    auto gid = extractField(jsonBody, "g");
    auto mode = extractField(jsonBody, "p");
    auto ats  = extractField(jsonBody, "as");
    auto atns = extractField(jsonBody, "an");
    auto mts  = extractField(jsonBody, "ms");
    auto mtns = extractField(jsonBody, "mn");
    auto dm   = extractField(jsonBody, "dm");
    auto dn   = extractField(jsonBody, "dn");

    em.metadata.ownerId        = static_cast<uint32_t>(uid.value_or(0));
    em.metadata.groupId        = static_cast<uint32_t>(gid.value_or(0));
    em.metadata.permissions    = static_cast<uint32_t>(mode.value_or(0));
    em.metadata.accessTimeSec  = ats.value_or(0);
    em.metadata.accessTimeNsec = atns.value_or(0);
    em.metadata.modifyTimeSec  = mts.value_or(0);
    em.metadata.modifyTimeNsec = mtns.value_or(0);
    em.devMajor = static_cast<uint64_t>(dm.value_or(0));
    em.devMinor = static_cast<uint64_t>(dn.value_or(0));

    // Symlink target
    em.symlinkTarget = extractStrField(jsonBody, "l");

    return em;
}

// ══════════════════════════════════════════════════════════════════════════════
// Pack
// ══════════════════════════════════════════════════════════════════════════════

Expected<void, ErrorCode> ZipPacker::pack(
    std::vector<FileEntry> const& files,
    FSAbstraction& fs,
    std::filesystem::path const& sourceRoot,
    std::ostream& output)
{
    // 1. Build metadata JSON
    auto metaJson = buildMetaJson(files);

    // 2. Initialize miniz heap writer
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        spdlog::error("ZipPacker: failed to initialize heap writer");
        return ErrorCode::kCompressionFailed;
    }

    // 3. Write metadata entry first (internal, no meaningful timestamp)
    if (!mz_zip_writer_add_mem_ex_v2(&zip, ".backer_zip_meta",
                                      metaJson.data(), metaJson.size(),
                                      nullptr, 0, MZ_DEFAULT_COMPRESSION,
                                      0, 0, nullptr,
                                      nullptr, 0, nullptr, 0))
    {
        mz_zip_writer_end(&zip);
        spdlog::error("ZipPacker: failed to add metadata entry");
        return ErrorCode::kCompressionFailed;
    }
    for (auto const& entry : files) {
        auto pathStr = entry.relativePath.generic_string();
        MZ_TIME_T entryTime = static_cast<MZ_TIME_T>(entry.metadata.modifyTimeSec);

        switch (entry.type) {

        case FileType::kRegular: {
            auto absPath = sourceRoot / entry.relativePath;
            auto readResult = fs.read(absPath);
            if (!readResult) {
                // Unreadable file — add zero-byte entry
                if (!mz_zip_writer_add_mem_ex_v2(&zip, pathStr.c_str(),
                                                  nullptr, 0,
                                                  nullptr, 0, MZ_DEFAULT_COMPRESSION,
                                                  0, 0, &entryTime,
                                                  nullptr, 0, nullptr, 0))
                {
                    mz_zip_writer_end(&zip);
                    return ErrorCode::kCompressionFailed;
                }
                spdlog::warn("ZipPacker: unreadable file {}, skipping content: {}",
                             absPath.string(), toString(readResult.error()));
                continue;
            }

            auto const& content = readResult.value();
            if (!mz_zip_writer_add_mem_ex_v2(&zip, pathStr.c_str(),
                                              content.data(), content.size(),
                                              nullptr, 0, MZ_DEFAULT_COMPRESSION,
                                              0, 0, &entryTime,
                                              nullptr, 0, nullptr, 0))
            {
                mz_zip_writer_end(&zip);
                spdlog::error("ZipPacker: failed to add entry: {}", pathStr);
                return ErrorCode::kCompressionFailed;
            }
            break;
        }

        case FileType::kDirectory: {
            // Directory entry: name ends with '/', empty content
            std::string dirName = pathStr + '/';
            if (!mz_zip_writer_add_mem_ex_v2(&zip, dirName.c_str(),
                                              nullptr, 0,
                                              nullptr, 0, MZ_DEFAULT_COMPRESSION,
                                              0, 0, &entryTime,
                                              nullptr, 0, nullptr, 0))
            {
                mz_zip_writer_end(&zip);
                return ErrorCode::kCompressionFailed;
            }
            break;
        }

        case FileType::kSymlink: {
            // Store symlink target as file content
            auto targetStr = entry.symlinkTarget.generic_string();
            if (!mz_zip_writer_add_mem_ex_v2(&zip, pathStr.c_str(),
                                              targetStr.data(), targetStr.size(),
                                              nullptr, 0, MZ_DEFAULT_COMPRESSION,
                                              0, 0, &entryTime,
                                              nullptr, 0, nullptr, 0))
            {
                mz_zip_writer_end(&zip);
                return ErrorCode::kCompressionFailed;
            }
            break;
        }

        case FileType::kFifo:
        case FileType::kBlockDevice:
        case FileType::kCharDevice:
        case FileType::kSocket: {
            // Zero-byte placeholder
            if (!mz_zip_writer_add_mem_ex_v2(&zip, pathStr.c_str(),
                                              nullptr, 0,
                                              nullptr, 0, MZ_DEFAULT_COMPRESSION,
                                              0, 0, &entryTime,
                                              nullptr, 0, nullptr, 0))
            {
                mz_zip_writer_end(&zip);
                return ErrorCode::kCompressionFailed;
            }
            break;
        }

        default:
            spdlog::debug("ZipPacker: skipping unsupported entry type {}",
                          static_cast<int>(entry.type));
            break;
        }
    }

    // 5. Finalize and extract the heap buffer
    void* buf = nullptr;
    std::size_t bufSize = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &buf, &bufSize)) {
        spdlog::error("ZipPacker: failed to finalize archive");
        return ErrorCode::kCompressionFailed;
    }

    if (buf && bufSize > 0) {
        output.write(static_cast<char const*>(buf),
                     static_cast<std::streamsize>(bufSize));
        mz_free(buf);
    }

    return {};
}

// ══════════════════════════════════════════════════════════════════════════════
// Unpack
// ══════════════════════════════════════════════════════════════════════════════

Expected<void, ErrorCode> ZipPacker::unpack(
    std::istream& input,
    std::filesystem::path const& dest,
    FSAbstraction& fs)
{
    // 1. Read entire archive into memory
    input.seekg(0, std::ios::end);
    auto const fileSize = static_cast<std::size_t>(input.tellg());
    input.seekg(0, std::ios::beg);

    std::vector<char> archiveData(fileSize);
    if (fileSize > 0) {
        input.read(archiveData.data(),
                   static_cast<std::streamsize>(fileSize));
        if (static_cast<std::size_t>(input.gcount()) != fileSize) {
            spdlog::error("ZipPacker: failed to read archive");
            return ErrorCode::kReadFailed;
        }
    }

    // 2. Initialize miniz memory reader
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_mem(&zip, archiveData.data(), archiveData.size(), 0)) {
        spdlog::error("ZipPacker: invalid or corrupted zip archive");
        return ErrorCode::kInvalidArchive;
    }

    auto const numFiles = mz_zip_reader_get_num_files(&zip);

    // 3. First pass: find and parse metadata entry
    int metaIndex = -1;
    for (int i = 0; i < numFiles; ++i) {
        char name[256];
        mz_zip_reader_get_filename(&zip, i, name, sizeof(name));
        if (std::strcmp(name, ".backer_zip_meta") == 0) {
            metaIndex = i;
            break;
        }
    }

    if (metaIndex < 0) {
        mz_zip_reader_end(&zip);
        spdlog::error("ZipPacker: archive missing .backer_zip_meta entry");
        return ErrorCode::kInvalidArchive;
    }

    // Extract metadata
    std::size_t metaSize = 0;
    auto* metaRaw = static_cast<char*>(
        mz_zip_reader_extract_to_heap(&zip, metaIndex, &metaSize, 0));
    if (!metaRaw || metaSize == 0) {
        mz_zip_reader_end(&zip);
        spdlog::error("ZipPacker: failed to extract metadata");
        return ErrorCode::kInvalidArchive;
    }
    std::string metaJson(metaRaw, metaSize);
    mz_free(metaRaw);

    auto entriesMeta = parseMetaEntries(metaJson);

    // 4. Second pass: extract all entries (skip metadata)
    //    Restore directory metadata deferred after all content written
    std::vector<std::pair<std::filesystem::path, Metadata>> dirsToRestore;

    for (int i = 0; i < numFiles; ++i) {
        if (i == metaIndex) continue;

        char nameBuf[512];
        mz_zip_reader_get_filename(&zip, i, nameBuf, sizeof(nameBuf));
        std::string name(nameBuf);

        // Strip trailing '/' for directories
        bool isDir = (!name.empty() && name.back() == '/');
        std::string relPath = isDir ? name.substr(0, name.size() - 1) : name;
        auto destPath = dest / relPath;

        // Look up metadata
        EntryMeta em;
        auto it = entriesMeta.find(relPath);
        if (it != entriesMeta.end()) {
            auto parsed = parseEntryMeta(it->second, relPath);
            if (!parsed) {
                spdlog::warn("ZipPacker: invalid metadata for '{}', skipping", relPath);
                continue;
            }
            em = parsed.value();
        } else {
            // Fallback: treat as regular file with minimal metadata
            em.type = isDir ? FileType::kDirectory : FileType::kRegular;
            em.metadata.modifyTimeSec = 0;
        }

        switch (em.type) {

        case FileType::kRegular: {
            // Ensure parent directory exists
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) {
                    spdlog::warn("ZipPacker: mkdir parent failed: {}", parent.string());
                }
            }

            // Extract file content to heap
            std::size_t extractedSize = 0;
            auto* data = static_cast<char*>(
                mz_zip_reader_extract_to_heap(&zip, i, &extractedSize, 0));
            if (!data && extractedSize == 0) {
                // Empty file (extract_to_heap returns NULL for zero-byte files)
                std::vector<char> empty;
                auto r = fs.write(destPath, empty, FileType::kRegular);
                if (!r) {
                    spdlog::warn("ZipPacker: write failed for {}", destPath.string());
                }
            } else if (data) {
                std::vector<char> content(data, data + extractedSize);
                mz_free(data);
                auto r = fs.write(destPath, content, FileType::kRegular);
                if (!r) {
                    spdlog::warn("ZipPacker: write failed for {}", destPath.string());
                }
            }

            // Restore metadata
            auto mr = fs.restoreMetadata(destPath, em.metadata, false);
            if (!mr) {
                spdlog::warn("ZipPacker: metadata restore partially failed for {}",
                             destPath.string());
            }
            break;
        }

        case FileType::kDirectory: {
            auto r = fs.mkdir(destPath);
            if (!r) {
                spdlog::warn("ZipPacker: mkdir failed for {}", destPath.string());
            }
            // Defer metadata until all children are written
            dirsToRestore.emplace_back(destPath, em.metadata);
            break;
        }

        case FileType::kSymlink: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) {
                    spdlog::warn("ZipPacker: mkdir parent failed: {}", parent.string());
                }
            }

            std::vector<char> symData(em.symlinkTarget.begin(),
                                      em.symlinkTarget.end());
            auto r = fs.write(destPath, symData, FileType::kSymlink);
            if (!r) {
                spdlog::warn("ZipPacker: symlink failed for {}", destPath.string());
            }

            auto mr = fs.restoreMetadata(destPath, em.metadata, false);
            if (!mr) {
                spdlog::warn("ZipPacker: metadata restore partially failed for {}",
                             destPath.string());
            }
            break;
        }

        case FileType::kFifo: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) {
                    spdlog::warn("ZipPacker: mkdir parent failed: {}", parent.string());
                }
            }

            std::vector<char> modeData(sizeof(uint32_t));
            *reinterpret_cast<uint32_t*>(modeData.data()) =
                em.metadata.permissions;
            auto r = fs.write(destPath, modeData, FileType::kFifo);
            if (!r) {
                spdlog::warn("ZipPacker: FIFO creation failed for {}",
                             destPath.string());
            }

            auto mr = fs.restoreMetadata(destPath, em.metadata, false);
            if (!mr) {
                spdlog::warn("ZipPacker: metadata restore partially failed for {}",
                             destPath.string());
            }
            break;
        }

        case FileType::kBlockDevice:
        case FileType::kCharDevice: {
            auto parent = destPath.parent_path();
            if (!parent.empty()) {
                auto r = fs.mkdir(parent);
                if (!r) {
                    spdlog::warn("ZipPacker: mkdir parent failed: {}", parent.string());
                }
            }

            std::vector<char> devData(sizeof(uint32_t));
            *reinterpret_cast<uint32_t*>(devData.data()) =
                em.metadata.permissions;
            auto r = fs.write(destPath, devData, em.type);
            if (!r) {
                spdlog::warn("ZipPacker: device creation failed for {}",
                             destPath.string());
            }

            auto mr = fs.restoreMetadata(destPath, em.metadata, false);
            if (!mr) {
                spdlog::warn("ZipPacker: metadata restore partially failed for {}",
                             destPath.string());
            }
            break;
        }

        case FileType::kSocket:
        default:
            spdlog::debug("ZipPacker: skipping unsupported entry type {}",
                          static_cast<int>(em.type));
            break;
        }
    }

    mz_zip_reader_end(&zip);

    // 5. Restore directory metadata: deepest first so child writes
    //    don't overwrite parent mtime.
    std::sort(dirsToRestore.begin(), dirsToRestore.end(),
        [](auto const& a, auto const& b) {
            auto depthA = std::distance(a.first.begin(), a.first.end());
            auto depthB = std::distance(b.first.begin(), b.first.end());
            return depthA > depthB;
        });

    for (auto const& [dirPath, meta] : dirsToRestore) {
        auto mr = fs.restoreMetadata(dirPath, meta, false);
        if (!mr) {
            spdlog::warn("ZipPacker: metadata restore partially failed for {}",
                         dirPath.string());
        }
    }

    return {};
}

} // namespace backer

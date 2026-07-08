#include "filters/criteria_filter.h"

#include "fs/platform.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <regex>
#include <string>

#if BACKER_PLATFORM_POSIX
    #include <fnmatch.h>
#endif

namespace backer {

// ── glob matching helpers ───────────────────────────────────────────────────

namespace {

/// Simple glob-to-regex conversion for platforms without fnmatch.
/// Handles *, ?, and escapes regex special characters.
/// When @p pathMode is true, '*' does not match '/' (FNM_PATHNAME semantics).
std::string globToRegex(std::string const& pattern, bool pathMode) {
    std::string out;
    out.reserve(pattern.size() + 4);
    out += '^';
    for (char c : pattern) {
        switch (c) {
            case '*': out += pathMode ? "[^/]*" : ".*"; break;
            case '?': out += pathMode ? "[^/]" : ".";  break;
            case '.': out += "\\.";  break;
            case '+': out += "\\+";  break;
            case '\\': out += "\\\\"; break;
            case '^': out += "\\^";  break;
            case '$': out += "\\$";  break;
            case '|': out += "\\|";  break;
            case '(': out += "\\(";  break;
            case ')': out += "\\)";  break;
            // [ and ] are glob character classes — pass through as regex char classes
            case '[': case ']':
                out += c;
                break;
            case '{': out += "\\{";  break;
            case '}': out += "\\}";  break;
            default:  out += c;      break;
        }
    }
    out += '$';
    return out;
}

/// Returns true if @p str matches the glob @p pattern.
/// If @p pathMode is true, '*' does not match '/' (fnmatch FNM_PATHNAME semantics).
bool matchGlob(std::string const& str, std::string const& pattern, bool pathMode) {
#if BACKER_PLATFORM_POSIX
    int flags = pathMode ? FNM_PATHNAME : 0;
    return fnmatch(pattern.c_str(), str.c_str(), flags) == 0;
#else
    try {
        std::regex re(globToRegex(pattern, pathMode));
        return std::regex_match(str, re);
    } catch (std::regex_error const& e) {
        spdlog::warn("matchGlob: regex error for pattern '{}': {}", pattern, e.what());
        return false;
    }
#endif
}

} // anonymous namespace

// ── CriteriaFilter ──────────────────────────────────────────────────────────

CriteriaFilter::CriteriaFilter(std::vector<FilterCriteria> criteria)
    : criteria_(std::move(criteria))
{
    std::size_t includeCount = 0;
    std::size_t excludeCount = 0;
    for (auto const& c : criteria_) {
        if (c.exclude) ++excludeCount;
        else ++includeCount;
    }
    spdlog::debug("CriteriaFilter: {} include + {} exclude criteria",
                  includeCount, excludeCount);
}

bool CriteriaFilter::matches(
    FilterCriteria const& criterion,
    FileEntry const& entry) const
{
    // Path glob matching (against relative path, with FNM_PATHNAME semantics)
    if (criterion.pathGlob.has_value()) {
        auto pathStr = entry.relativePath.generic_string();
        if (!matchGlob(pathStr, criterion.pathGlob.value(), true)) {
            return false;
        }
    }

    // File type matching
    if (criterion.fileType.has_value()) {
        if (entry.type != criterion.fileType.value()) {
            return false;
        }
    }

    // Name glob matching (against filename only)
    if (criterion.nameGlob.has_value()) {
        auto nameStr = entry.relativePath.filename().generic_string();
        if (!matchGlob(nameStr, criterion.nameGlob.value(), false)) {
            return false;
        }
    }

    // Time range matching
    if (criterion.timeRange.has_value()) {
        auto const& tr = criterion.timeRange.value();
        int64_t mtimeSec = entry.metadata.modifyTimeSec;

        if (tr.hasAfter) {
            // Modified at or after this time → pass
            if (mtimeSec < tr.afterSec) return false;
            if (mtimeSec == tr.afterSec) {
                int64_t mtimeNsec = entry.metadata.modifyTimeNsec;
                if (mtimeNsec < tr.afterNsec) return false;
            }
        }
        if (tr.hasBefore) {
            // Modified before this time → pass
            if (mtimeSec > tr.beforeSec) return false;
            if (mtimeSec == tr.beforeSec) {
                int64_t mtimeNsec = entry.metadata.modifyTimeNsec;
                if (mtimeNsec >= tr.beforeNsec) return false;
            }
        }
    }

    // Size range matching
    if (criterion.sizeRange.has_value()) {
        auto const& sr = criterion.sizeRange.value();
        if (sr.hasMin && entry.size < sr.minSize) return false;
        if (sr.hasMax && entry.size > sr.maxSize) return false;
    }

    // Owner matching
    if (criterion.ownerId.has_value()) {
        if (entry.metadata.ownerId != criterion.ownerId.value()) {
            return false;
        }
    }

    return true;
}

std::vector<FileEntry> CriteriaFilter::apply(
    std::vector<FileEntry> const& files)
{
    if (criteria_.empty()) {
        spdlog::debug("CriteriaFilter: no criteria, passing all {} entries", files.size());
        return files;
    }

    std::vector<FileEntry> result;
    result.reserve(files.size());

    // ── Determine which dimensions have include criteria ──────────────────────
    //
    // Include logic: entries pass if they satisfy ALL active dimensions.
    // Within each dimension, matching ANY criterion satisfies that dimension (OR).
    // Across dimensions, ALL must be satisfied (AND).
    struct IncludeDims {
        bool hasPathGlob   = false;
        bool hasFileType   = false;
        bool hasNameGlob   = false;
        bool hasTimeRange  = false;
        bool hasSizeRange  = false;
        bool hasOwnerId    = false;
        bool any() const { return hasPathGlob || hasFileType || hasNameGlob
                                  || hasTimeRange || hasSizeRange || hasOwnerId; }
    };
    IncludeDims dims;
    for (auto const& c : criteria_) {
        if (c.exclude) continue;
        if (c.pathGlob)   dims.hasPathGlob   = true;
        if (c.fileType)   dims.hasFileType   = true;
        if (c.nameGlob)   dims.hasNameGlob   = true;
        if (c.timeRange)  dims.hasTimeRange  = true;
        if (c.sizeRange)  dims.hasSizeRange  = true;
        if (c.ownerId)    dims.hasOwnerId    = true;
    }

    for (auto const& entry : files) {
        // ── Include phase ─────────────────────────────────────────────────────
        if (dims.any()) {
            bool passesAll = true;

            // Path: entry matches at least one pathGlob criterion
            if (passesAll && dims.hasPathGlob) {
                bool ok = false;
                auto pathStr = entry.relativePath.generic_string();
                for (auto const& c : criteria_) {
                    if (!c.exclude && c.pathGlob
                        && matchGlob(pathStr, c.pathGlob.value(), true)) {
                        ok = true; break;
                    }
                }
                if (!ok) passesAll = false;
            }

            // File type: entry matches at least one fileType criterion
            if (passesAll && dims.hasFileType) {
                bool ok = false;
                for (auto const& c : criteria_) {
                    if (!c.exclude && c.fileType
                        && entry.type == c.fileType.value()) {
                        ok = true; break;
                    }
                }
                if (!ok) passesAll = false;
            }

            // Name: entry matches at least one nameGlob criterion
            if (passesAll && dims.hasNameGlob) {
                bool ok = false;
                auto nameStr = entry.relativePath.filename().generic_string();
                for (auto const& c : criteria_) {
                    if (!c.exclude && c.nameGlob
                        && matchGlob(nameStr, c.nameGlob.value(), false)) {
                        ok = true; break;
                    }
                }
                if (!ok) passesAll = false;
            }

            // Time range: entry matches at least one timeRange criterion
            if (passesAll && dims.hasTimeRange) {
                bool ok = false;
                int64_t mtimeSec = entry.metadata.modifyTimeSec;
                int64_t mtimeNsec = entry.metadata.modifyTimeNsec;
                for (auto const& c : criteria_) {
                    if (c.exclude || !c.timeRange) continue;
                    auto const& tr = c.timeRange.value();
                    bool match = true;
                    if (tr.hasAfter) {
                        if (mtimeSec < tr.afterSec) match = false;
                        else if (mtimeSec == tr.afterSec && mtimeNsec < tr.afterNsec) match = false;
                    }
                    if (match && tr.hasBefore) {
                        if (mtimeSec > tr.beforeSec) match = false;
                        else if (mtimeSec == tr.beforeSec && mtimeNsec >= tr.beforeNsec) match = false;
                    }
                    if (match) { ok = true; break; }
                }
                if (!ok) passesAll = false;
            }

            // Size range: entry matches at least one sizeRange criterion
            if (passesAll && dims.hasSizeRange) {
                bool ok = false;
                for (auto const& c : criteria_) {
                    if (c.exclude || !c.sizeRange) continue;
                    auto const& sr = c.sizeRange.value();
                    bool match = true;
                    if (sr.hasMin && entry.size < sr.minSize) match = false;
                    if (sr.hasMax && entry.size > sr.maxSize) match = false;
                    if (match) { ok = true; break; }
                }
                if (!ok) passesAll = false;
            }

            // Owner: entry matches at least one ownerId criterion
            if (passesAll && dims.hasOwnerId) {
                bool ok = false;
                for (auto const& c : criteria_) {
                    if (!c.exclude && c.ownerId
                        && entry.metadata.ownerId == c.ownerId.value()) {
                        ok = true; break;
                    }
                }
                if (!ok) passesAll = false;
            }

            if (!passesAll) continue;
        }

        // Apply all exclude criteria (ANY match removes)
        bool excluded = false;
        for (auto const& c : criteria_) {
            if (c.exclude) {
                if (matches(c, entry)) {
                    excluded = true;
                    break;
                }
            }
        }

        if (!excluded) {
            result.push_back(entry);
        }
    }

    result.shrink_to_fit();
    spdlog::info("CriteriaFilter: {} → {} entries after filtering",
                 files.size(), result.size());
    return result;
}

} // namespace backer

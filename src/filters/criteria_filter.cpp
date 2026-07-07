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

    for (auto const& entry : files) {
        bool passesInclude = true;

        // Apply all include criteria (AND logic)
        for (auto const& c : criteria_) {
            if (!c.exclude) {
                if (!matches(c, entry)) {
                    passesInclude = false;
                    break;
                }
            }
        }

        if (!passesInclude) {
            continue;
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

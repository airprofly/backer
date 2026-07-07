#pragma once

#include "filters/filter.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace backer {

/// Modification time range for filtering.
struct TimeRange {
    int64_t beforeSec = 0;   ///< Exclude files modified at or after this time (exclusive upper bound).
    int64_t beforeNsec = 0;
    int64_t afterSec  = 0;   ///< Exclude files modified before this time (inclusive lower bound).
    int64_t afterNsec = 0;
    bool hasBefore = false;
    bool hasAfter  = false;
};

/// File size range for filtering.
struct SizeRange {
    uint64_t minSize = 0;
    uint64_t maxSize = 0;
    bool hasMin = false;
    bool hasMax = false;
};

/// A single filter criterion.
///
/// Each optional field represents one dimension of filtering.
/// For include criteria (exclude=false), ALL non-empty fields must match (AND logic).
/// For exclude criteria (exclude=true), ANY match causes removal.
struct FilterCriteria {
    std::optional<std::string> pathGlob;   ///< Glob pattern on relative path (e.g. "src/*.cpp")
    std::optional<FileType>    fileType;    ///< File type to match (e.g. FileType::kSymlink)
    std::optional<std::string> nameGlob;    ///< Glob pattern on filename only (e.g. "*.txt")
    std::optional<TimeRange>   timeRange;   ///< Modification time range
    std::optional<SizeRange>   sizeRange;   ///< File size range
    std::optional<uint32_t>    ownerId;     ///< Owner UID (resolved from username by caller)
    bool                       exclude = false;  ///< true = remove matches; false = keep only matches
};

/// Filter implementation based on a list of FilterCriteria.
///
/// Processing order:
///   1. Include phase — entries must match ALL include criteria (if any exist).
///      If no include criteria exist, all entries pass the include phase.
///   2. Exclude phase — entries matching ANY exclude criterion are removed.
class CriteriaFilter final : public Filter {
public:
    explicit CriteriaFilter(std::vector<FilterCriteria> criteria);

    std::vector<FileEntry> apply(
        std::vector<FileEntry> const& files) override;

private:
    /// Test whether a single entry matches a single criterion (all fields AND).
    bool matches(FilterCriteria const& criterion, FileEntry const& entry) const;

    std::vector<FilterCriteria> criteria_;
};

} // namespace backer

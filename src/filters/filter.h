#pragma once

#include "core/types.h"

#include <vector>

namespace backer {

/// Abstract interface for file entry filters.
///
/// Applied after directory walk and before storage, allowing
/// inclusion/exclusion of entries based on various criteria.
class Filter {
public:
    virtual ~Filter() = default;

    /// Apply this filter to the given list of file entries.
    /// Returns a subset (or the full set) of entries that pass the filter.
    virtual std::vector<FileEntry> apply(
        std::vector<FileEntry> const& files) = 0;
};

/// No-op filter that passes all entries through unchanged.
class NoopFilter final : public Filter {
public:
    std::vector<FileEntry> apply(
        std::vector<FileEntry> const& files) override
    {
        return files;
    }
};

} // namespace backer

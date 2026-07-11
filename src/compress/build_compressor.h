#pragma once

#include "compress/compressor.h"

#include <memory>
#include <string_view>

namespace backer {

/// Create a compressor instance by algorithm name.
/// Returns nullptr if @p name is not recognised.
/// @p level 0 means "use the algorithm's default level".
std::unique_ptr<Compressor> buildCompressor(std::string_view name, int level = 0);

} // namespace backer

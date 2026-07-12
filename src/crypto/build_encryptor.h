#pragma once

#include "crypto/encryptor.h"

#include <memory>
#include <string_view>

namespace backer {

/// Create an Encryptor instance by algorithm name.
/// Returns nullptr if @p name is not recognised.
std::unique_ptr<Encryptor> buildEncryptor(std::string_view name);

} // namespace backer

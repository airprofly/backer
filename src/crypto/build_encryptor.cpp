#include "crypto/build_encryptor.h"
#include "crypto/openssl_encryptor.h"

namespace backer {

std::unique_ptr<Encryptor> buildEncryptor(std::string_view name) {
    if (name == "aes256"   || name == "aes-256-gcm") {
        return std::make_unique<OpenSslEncryptor>("aes256");
    }
    if (name == "sm4"      || name == "sm4-cbc") {
        return std::make_unique<OpenSslEncryptor>("sm4");
    }
    return nullptr;
}

} // namespace backer

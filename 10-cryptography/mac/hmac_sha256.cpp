#include "hmac_sha256.hpp"
#include "../hashing/sha256.hpp"

namespace crypto {

void hmacSha256(
    const uint8_t* key, size_t keyLength,
    const uint8_t* message, size_t messageLength,
    uint8_t* macOut
) {
    uint8_t keyBlock[SHA256_BLOCK_SIZE] = {0};

    if (keyLength > SHA256_BLOCK_SIZE) {
        // RFC 2104: a key longer than the hash's block size is
        // shortened by hashing it first.
        sha256(key, keyLength, keyBlock);
        // The rest of keyBlock beyond the 32-byte digest stays
        // zero-padded, matching RFC 2104's "then pad with zeros."
    } else {
        for (size_t i = 0; i < keyLength; ++i) {
            keyBlock[i] = key[i];
        }
    }

    uint8_t innerPad[SHA256_BLOCK_SIZE];
    uint8_t outerPad[SHA256_BLOCK_SIZE];
    for (size_t i = 0; i < SHA256_BLOCK_SIZE; ++i) {
        innerPad[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x36);
        outerPad[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x5C);
    }

    // inner = SHA256(innerPad || message)
    Sha256 innerHasher;
    innerHasher.update(innerPad, SHA256_BLOCK_SIZE);
    innerHasher.update(message, messageLength);
    uint8_t innerDigest[SHA256_DIGEST_SIZE];
    innerHasher.finish(innerDigest);

    // HMAC = SHA256(outerPad || inner)
    Sha256 outerHasher;
    outerHasher.update(outerPad, SHA256_BLOCK_SIZE);
    outerHasher.update(innerDigest, SHA256_DIGEST_SIZE);
    outerHasher.finish(macOut);
}

bool constantTimeEquals(
    const uint8_t* a, size_t aLength,
    const uint8_t* b, size_t bLength
) {
    if (aLength != bLength) {
        return false;
    }

    uint8_t diff = 0;
    for (size_t i = 0; i < aLength; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

}  // namespace crypto

#include "auth.hpp"

#include "../../10-cryptography/mac/hmac_sha256.hpp"

namespace dist {

std::vector<uint8_t> authenticateMessage(
    const std::vector<uint8_t>& message,
    const uint8_t* key, size_t keyLength
) {
    uint8_t tag[crypto::HMAC_SHA256_OUTPUT_SIZE];
    crypto::hmacSha256(key, keyLength, message.data(), message.size(), tag);

    std::vector<uint8_t> result = message;
    result.insert(result.end(), tag, tag + crypto::HMAC_SHA256_OUTPUT_SIZE);
    return result;
}

bool verifyAndExtractMessage(
    const std::vector<uint8_t>& authenticated,
    const uint8_t* key, size_t keyLength,
    std::vector<uint8_t>& messageOut
) {
    if (authenticated.size() < crypto::HMAC_SHA256_OUTPUT_SIZE) {
        return false;
    }

    size_t messageLength = authenticated.size() - crypto::HMAC_SHA256_OUTPUT_SIZE;
    const uint8_t* messageBytes = authenticated.data();
    const uint8_t* claimedTag = authenticated.data() + messageLength;

    uint8_t expectedTag[crypto::HMAC_SHA256_OUTPUT_SIZE];
    crypto::hmacSha256(key, keyLength, messageBytes, messageLength, expectedTag);

    if (!crypto::constantTimeEquals(
            claimedTag, crypto::HMAC_SHA256_OUTPUT_SIZE,
            expectedTag, crypto::HMAC_SHA256_OUTPUT_SIZE)) {
        return false;
    }

    messageOut.assign(messageBytes, messageBytes + messageLength);
    return true;
}

}  // namespace dist

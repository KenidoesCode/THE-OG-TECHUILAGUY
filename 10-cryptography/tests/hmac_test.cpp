// Known-answer tests for HMAC-SHA256, verified against RFC 4231's own
// published test vectors — the same discipline applied to SHA-256
// itself in ADR 0006 (and the same lesson learned there: verify
// hand-transcribed hex constants by length and cross-reference before
// concluding the implementation is wrong).

#include "../mac/hmac_sha256.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

std::string toHex(const uint8_t* bytes, size_t length) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        out += digits[(bytes[i] >> 4) & 0xF];
        out += digits[bytes[i] & 0xF];
    }
    return out;
}

void checkHmac(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& message,
    const std::string& expectedHex,
    const std::string& description
) {
    uint8_t mac[crypto::HMAC_SHA256_OUTPUT_SIZE];
    crypto::hmacSha256(key.data(), key.size(), message.data(), message.size(), mac);

    std::string actualHex = toHex(mac, crypto::HMAC_SHA256_OUTPUT_SIZE);

    if (actualHex == expectedHex) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description
                  << " — expected " << expectedHex
                  << ", got " << actualHex << "\n";
        failures++;
    }
}

std::vector<uint8_t> repeat(uint8_t byte, size_t count) {
    return std::vector<uint8_t>(count, byte);
}

std::vector<uint8_t> fromString(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

}  // namespace

void testRfc4231TestCase1() {
    checkHmac(
        repeat(0x0b, 20),
        fromString("Hi There"),
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
        "HMAC-SHA256: RFC 4231 test case 1 (20-byte key, short message)"
    );
}

void testRfc4231TestCase2() {
    checkHmac(
        fromString("Jefe"),
        fromString("what do ya want for nothing?"),
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        "HMAC-SHA256: RFC 4231 test case 2 (short ASCII key \"Jefe\")"
    );
}

void testRfc4231TestCase3() {
    checkHmac(
        repeat(0xaa, 20),
        repeat(0xdd, 50),
        "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
        "HMAC-SHA256: RFC 4231 test case 3 (binary key and message)"
    );
}

void testRfc4231TestCase6KeyLongerThanBlockSize() {
    checkHmac(
        repeat(0xaa, 131),  // longer than SHA-256's 64-byte block size
        fromString("Test Using Larger Than Block-Size Key - Hash Key First"),
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
        "HMAC-SHA256: RFC 4231 test case 6 (key longer than the block "
        "size, exercising the 'hash the key first' branch of RFC 2104)"
    );
}

void testDifferentKeysProduceDifferentMacs() {
    std::vector<uint8_t> message = fromString("same message");
    uint8_t mac1[crypto::HMAC_SHA256_OUTPUT_SIZE];
    uint8_t mac2[crypto::HMAC_SHA256_OUTPUT_SIZE];

    std::vector<uint8_t> key1 = fromString("key one");
    std::vector<uint8_t> key2 = fromString("key two");

    crypto::hmacSha256(key1.data(), key1.size(), message.data(), message.size(), mac1);
    crypto::hmacSha256(key2.data(), key2.size(), message.data(), message.size(), mac2);

    bool different = !crypto::constantTimeEquals(
        mac1, crypto::HMAC_SHA256_OUTPUT_SIZE, mac2, crypto::HMAC_SHA256_OUTPUT_SIZE
    );

    if (different) {
        std::cout << "[PASS] HMAC-SHA256: the same message under two "
                     "different keys produces two different MACs\n";
    } else {
        std::cout << "[FAIL] HMAC-SHA256: two different keys produced "
                     "the identical MAC for the same message\n";
        failures++;
    }
}

void testConstantTimeEqualsCorrectness() {
    uint8_t a[4] = {1, 2, 3, 4};
    uint8_t b[4] = {1, 2, 3, 4};
    uint8_t c[4] = {1, 2, 3, 5};
    uint8_t d[3] = {1, 2, 3};

    bool eq = crypto::constantTimeEquals(a, 4, b, 4);
    bool neq = crypto::constantTimeEquals(a, 4, c, 4);
    bool lengthMismatch = crypto::constantTimeEquals(a, 4, d, 3);

    if (eq && !neq && !lengthMismatch) {
        std::cout << "[PASS] constantTimeEquals: correctly reports equal, "
                     "unequal, and length-mismatched buffers\n";
    } else {
        std::cout << "[FAIL] constantTimeEquals: incorrect result for "
                     "at least one of equal/unequal/length-mismatched cases\n";
        failures++;
    }
}

int main() {
    testRfc4231TestCase1();
    testRfc4231TestCase2();
    testRfc4231TestCase3();
    testRfc4231TestCase6KeyLongerThanBlockSize();
    testDifferentKeysProduceDifferentMacs();
    testConstantTimeEqualsCorrectness();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

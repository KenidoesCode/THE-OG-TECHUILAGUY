// Known-answer tests (KATs) for the SHA-256 implementation
// (hashing/sha256.cpp), verified against FIPS 180-4's own published
// test vectors — the only way to honestly claim "this is SHA-256" is
// to match the standard's exact byte-for-byte output, not merely "a
// similarly-structured hash." Hosted, hardware-independent — the same
// implementation runs unchanged inside 22-os.

#include "../hashing/sha256.hpp"

#include <cstring>
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

void checkDigest(
    const std::string& message,
    const std::string& expectedHex,
    const std::string& description
) {
    uint8_t digest[crypto::SHA256_DIGEST_SIZE];
    crypto::sha256(
        reinterpret_cast<const uint8_t*>(message.data()),
        message.size(),
        digest
    );

    std::string actualHex = toHex(digest, crypto::SHA256_DIGEST_SIZE);

    if (actualHex == expectedHex) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description
                  << " — expected " << expectedHex
                  << ", got " << actualHex << "\n";
        failures++;
    }
}

}  // namespace

void testFips1804EmptyString() {
    checkDigest(
        "",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA-256: FIPS 180-4 known-answer test — empty string"
    );
}

void testFips1804SingleBlockMessage() {
    checkDigest(
        "abc",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256: FIPS 180-4 known-answer test — \"abc\" (single block)"
    );
}

void testFips1804TwoBlockMessage() {
    checkDigest(
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "SHA-256: FIPS 180-4 known-answer test — 56-byte message "
        "spanning two blocks (exercises padding into a fresh block)"
    );
}

void testFips1804OneMillionRepeatedA() {
    std::string message(1000000, 'a');
    checkDigest(
        message,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        "SHA-256: FIPS 180-4 known-answer test — one million repeated "
        "'a' characters (exercises many-block streaming via update())"
    );
}

void testIncrementalUpdateMatchesOneShot() {
    // Feeding the same message through several small update() calls
    // must produce the identical digest to one call with the whole
    // buffer — the real invariant any streaming hash API must satisfy,
    // not just "the one-shot wrapper happens to work."
    std::string message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    uint8_t oneShot[crypto::SHA256_DIGEST_SIZE];
    crypto::sha256(
        reinterpret_cast<const uint8_t*>(message.data()), message.size(), oneShot
    );

    crypto::Sha256 hasher;
    for (size_t i = 0; i < message.size(); ++i) {
        hasher.update(reinterpret_cast<const uint8_t*>(&message[i]), 1);
    }
    uint8_t incremental[crypto::SHA256_DIGEST_SIZE];
    hasher.finish(incremental);

    bool same = std::memcmp(oneShot, incremental, crypto::SHA256_DIGEST_SIZE) == 0;
    if (same) {
        std::cout << "[PASS] SHA-256: byte-at-a-time incremental update() "
                     "produces the identical digest to a single one-shot call\n";
    } else {
        std::cout << "[FAIL] SHA-256: byte-at-a-time incremental update() "
                     "produced a DIFFERENT digest than a one-shot call\n";
        failures++;
    }
}

void testResetAllowsReuse() {
    crypto::Sha256 hasher;
    uint8_t first[crypto::SHA256_DIGEST_SIZE];
    hasher.update(reinterpret_cast<const uint8_t*>("abc"), 3);
    hasher.finish(first);

    hasher.reset();
    uint8_t second[crypto::SHA256_DIGEST_SIZE];
    hasher.update(reinterpret_cast<const uint8_t*>("abc"), 3);
    hasher.finish(second);

    bool same = std::memcmp(first, second, crypto::SHA256_DIGEST_SIZE) == 0;
    if (same) {
        std::cout << "[PASS] SHA-256: reset() genuinely restores initial "
                     "state — hashing the same input twice gives the same digest\n";
    } else {
        std::cout << "[FAIL] SHA-256: reset() did not fully restore state "
                     "— hashing the same input twice gave different digests\n";
        failures++;
    }
}

void testDifferentInputsProduceDifferentDigests() {
    uint8_t digestA[crypto::SHA256_DIGEST_SIZE];
    uint8_t digestB[crypto::SHA256_DIGEST_SIZE];
    crypto::sha256(reinterpret_cast<const uint8_t*>("abc"), 3, digestA);
    crypto::sha256(reinterpret_cast<const uint8_t*>("abd"), 3, digestB);

    bool different = std::memcmp(digestA, digestB, crypto::SHA256_DIGEST_SIZE) != 0;
    if (different) {
        std::cout << "[PASS] SHA-256: a single-bit-different input produces "
                     "a completely different digest (basic avalanche sanity check)\n";
    } else {
        std::cout << "[FAIL] SHA-256: two different inputs produced the same digest\n";
        failures++;
    }
}

int main() {
    testFips1804EmptyString();
    testFips1804SingleBlockMessage();
    testFips1804TwoBlockMessage();
    testFips1804OneMillionRepeatedA();
    testIncrementalUpdateMatchesOneShot();
    testResetAllowsReuse();
    testDifferentInputsProduceDifferentDigests();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

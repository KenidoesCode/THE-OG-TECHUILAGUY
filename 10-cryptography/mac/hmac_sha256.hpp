#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// HMAC-SHA256 (RFC 2104's construction, instantiated with the
// SHA-256 from ../hashing/sha256.hpp — a real integration within this
// same layer, reusing the already-verified primitive rather than a
// second hash implementation). Verified against RFC 4231's own
// published test vectors (see docs/ADR/0011-hmac.md).

namespace crypto {

constexpr size_t HMAC_SHA256_OUTPUT_SIZE = 32;

// Computes HMAC-SHA256(key, message) and writes exactly
// HMAC_SHA256_OUTPUT_SIZE bytes to `macOut`. `key` may be any length
// (RFC 2104: a key longer than the hash's block size is itself hashed
// down first; a key shorter than the block size is zero-padded) —
// both cases are exercised by RFC 4231's own test vectors.
void hmacSha256(
    const uint8_t* key, size_t keyLength,
    const uint8_t* message, size_t messageLength,
    uint8_t* macOut
);

// Constant-time comparison of two equal-length byte buffers — the
// correct way to compare a computed MAC against an expected one.
// Using `==`/memcmp for this purpose leaks timing information about
// how many leading bytes matched, which is a real, exploitable
// side channel for MAC verification (a "timing attack" against
// message authentication); this function always inspects every byte
// regardless of where a mismatch occurs. Returns false immediately
// (without a constant-time comparison, since there is nothing
// meaningful to compare) if the two lengths differ.
bool constantTimeEquals(
    const uint8_t* a, size_t aLength,
    const uint8_t* b, size_t bLength
);

}  // namespace crypto

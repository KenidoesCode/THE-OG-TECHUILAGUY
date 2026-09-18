#pragma once

#include <stdint.h>
#include <stddef.h>

// A real SHA-256 implementation (FIPS 180-4) — no allocation, no OS
// dependency, hardware/host-independent by construction so it can run
// identically inside 22-os (freestanding) or a normal hosted test.
// This is a from-scratch implementation for this project, not a
// vendored copy of an existing library, verified against the
// standard's own published test vectors (see
// docs/ADR/0006-cryptography-hashing.md) — an unavoidable requirement
// for any hash claiming to *be* SHA-256 rather than merely resembling
// it, since implementing against a real standard means matching its
// exact output byte-for-byte, not merely "a similar strength hash."

namespace crypto {

constexpr size_t SHA256_DIGEST_SIZE = 32;
constexpr size_t SHA256_BLOCK_SIZE = 64;

class Sha256 {
public:
    Sha256();

    void reset();
    void update(const uint8_t* data, size_t length);

    // Finalizes and writes exactly SHA256_DIGEST_SIZE bytes to
    // `digestOut`. The object must not be used again without calling
    // reset() first — finalization is destructive to the internal
    // state (the padding it appends is not undone), matching every
    // other real SHA-256 implementation's one-shot finalize contract.
    void finish(uint8_t* digestOut);

private:
    uint32_t state[8];
    uint8_t buffer[SHA256_BLOCK_SIZE];
    uint32_t bufferLength;
    uint64_t totalLength;  // in bytes, across every update() call

    void processBlock(const uint8_t* block);
};

// One-shot convenience wrapper: hashes `data` (length bytes) and
// writes the digest to `digestOut`.
void sha256(const uint8_t* data, size_t length, uint8_t* digestOut);

}  // namespace crypto

# Layer 10 — Cryptography & PQC

**Status: FOUNDATION.** A real, from-scratch SHA-256 (FIPS 180-4) and
HMAC-SHA256 (RFC 2104) implementation, each verified against the
standard's own published test vectors. Everything else in this layer's
PRD scope (AEAD, PKI, ML-KEM, ML-DSA, SLH-DSA, hybrid classical+PQC
protocols) is not yet implemented — see
[`docs/ADR/0006-cryptography-hashing.md`](../docs/ADR/0006-cryptography-hashing.md)
and [`docs/ADR/0011-hmac.md`](../docs/ADR/0011-hmac.md).

## Implemented and tested

- `hashing/sha256.hpp`/`.cpp`: SHA-256, matching FIPS 180-4 exactly —
  a streaming API (`update()`/`finish()`, safe to call in a loop for
  arbitrarily large input) plus a one-shot convenience wrapper. No
  allocation, no OS dependency — runs identically hosted or inside
  `22-os`.
- `mac/hmac_sha256.hpp`/`.cpp`: HMAC-SHA256 (RFC 2104), built by
  reusing the SHA-256 implementation directly, plus a
  `constantTimeEquals` helper for safely comparing a computed MAC
  against an expected one (an ordinary `==`/`memcmp` leaks a timing
  side channel for MAC verification).
- 7 hosted unit assertions for SHA-256 (`tests/sha256_test.cpp`/
  `sha256_test.sh`): the standard's own known-answer test vectors
  (empty string, `"abc"`, a 56-byte message that spans two blocks, and
  one million repeated `'a'` characters that exercise many-block
  streaming), plus incremental-vs-one-shot equivalence, `reset()`
  correctness, and a basic avalanche sanity check.
- 6 hosted unit assertions for HMAC-SHA256 (`tests/hmac_test.cpp`/
  `hmac_test.sh`): RFC 4231 test cases 1, 2, 3, and 6 (the last
  specifically exercising the key-longer-than-block-size branch of
  RFC 2104), different keys producing different MACs, and
  `constantTimeEquals` correctness.

## Not yet implemented

- AEAD, PKI, digital signatures, key exchange (classical or PQC)
- ML-KEM, ML-DSA, SLH-DSA
- a KDF (e.g. HKDF, which would itself build on this layer's HMAC)
- crypto agility / hybrid classical+PQC protocol negotiation
- any integration with networking, storage, or space-systems
  telemetry/telecommand (all currently PLANNED elsewhere in the PRD)

## Building and testing

```sh
bash tests/sha256_test.sh   # hosted known-answer tests, no hardware needed
bash tests/hmac_test.sh     # hosted known-answer tests, no hardware needed
```

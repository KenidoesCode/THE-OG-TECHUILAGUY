# Layer 10 — Cryptography & PQC

**Status: FOUNDATION.** A real, from-scratch SHA-256 (FIPS 180-4)
implementation, verified against the standard's own published test
vectors. Everything else in this layer's PRD scope (AEAD, PKI,
ML-KEM, ML-DSA, SLH-DSA, hybrid classical+PQC protocols) is not yet
implemented — see [`docs/ADR/0006-cryptography-hashing.md`](../docs/ADR/0006-cryptography-hashing.md).

## Implemented and tested

- `hashing/sha256.hpp`/`.cpp`: SHA-256, matching FIPS 180-4 exactly —
  a streaming API (`update()`/`finish()`, safe to call in a loop for
  arbitrarily large input) plus a one-shot convenience wrapper. No
  allocation, no OS dependency — runs identically hosted or inside
  `22-os`.
- 7 hosted unit assertions (`tests/sha256_test.cpp`/`sha256_test.sh`):
  the standard's own known-answer test vectors (empty string, `"abc"`,
  a 56-byte message that spans two blocks, and one million repeated
  `'a'` characters that exercise many-block streaming), plus
  incremental-vs-one-shot equivalence, `reset()` correctness, and a
  basic avalanche sanity check.

## Not yet implemented

- AEAD, PKI, digital signatures, key exchange (classical or PQC)
- ML-KEM, ML-DSA, SLH-DSA
- HMAC or any other MAC construction
- crypto agility / hybrid classical+PQC protocol negotiation
- any integration with networking, storage, or space-systems
  telemetry/telecommand (all currently PLANNED elsewhere in the PRD)

## Building and testing

```sh
bash tests/sha256_test.sh   # hosted known-answer tests, no hardware needed
```

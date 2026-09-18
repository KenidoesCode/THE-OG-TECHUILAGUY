# ADR 0006: Cryptographic hashing foundation (SHA-256)

**Status:** Accepted. This is the first, narrow slice of Layer 10
(Cryptography & PQC) — see "What this is not" for the substantial
remaining scope.

## Context

Layer 10 (FR list under §7.7) calls for AEAD, PKI, ML-KEM, ML-DSA,
SLH-DSA, and hybrid classical+PQC protocols — a very large scope with
no existing code to build from. A cryptographic hash function is the
most foundational primitive nearly everything else in this layer (and
much of the rest of the PRD — content-addressed storage in OGGit,
integrity checks in OGRegistry, key derivation, MACs) will eventually
depend on, and it is small enough to implement, verify, and document
completely and honestly in one pass. This ADR records that first
slice.

## Decision

`10-cryptography/hashing/sha256.cpp` is a from-scratch SHA-256
implementation (FIPS 180-4) for this project — not a vendored copy of
an existing library. Implementing "against the standard" for a hash
function means something very specific and checkable: the output must
match the standard's own defined algorithm byte-for-byte on every
input, verified here against FIPS 180-4's own published test vectors
(the empty string, `"abc"`, a 56-byte two-block message, and one
million repeated `'a'` characters — chosen specifically because they
exercise the single-block case, the two-block padding boundary case,
and long streaming input respectively).

The implementation is a streaming API (`update()` may be called
repeatedly with arbitrary-sized chunks; `finish()` performs the
standard's padding — a `0x80` byte, zero bytes, then the original
message's bit-length as a big-endian 64-bit field — and produces the
32-byte digest) plus a one-shot `sha256()` convenience wrapper. No
allocation, no OS dependency, so the identical code can run inside
`22-os`'s freestanding kernel or a normal hosted test unchanged.

## A real bug this caught (not by inspection)

Three of the four known-answer tests failed on the first run. The
natural assumption — a bug in `finish()`'s padding logic — turned out
to be wrong: debugging traced every failure to the *test file's own*
"expected" hex-string literals being one character short (a manual
transcription mistake when typing 64-character hex constants by
hand), not the implementation itself. This was confirmed methodically:
`wc -c` on each literal showed 63 characters instead of the required
64, and the actual computed digests matched well-known, independently
verifiable SHA-256 values for those exact inputs. This is recorded
here deliberately — a hash implementation's tests are only as trustworthy
as the reference values they're checked against, and "the test failed"
is not automatically "the implementation is wrong."

## What this is not

- **Not a MAC, KDF, or AEAD.** SHA-256 alone provides none of these;
  HMAC (or another real MAC construction) is a natural, still-unbuilt
  next step that would reuse this hash.
- **Not PQC.** ML-KEM, ML-DSA, and SLH-DSA are unrelated algorithm
  families (lattice-based KEM, lattice-based signatures, hash-based
  signatures respectively) and share no code with this hash.
- **Not integrated anywhere yet.** Nothing in OGGit, storage, or
  networking calls this code yet — it exists as a standalone,
  independently verified primitive, ready to be built on.
- **Not constant-time / side-channel-hardened.** This implementation
  has not been audited or hardened against timing/cache side-channel
  attacks; it should not be treated as production-grade cryptographic
  code without that additional work, exactly the "research
  implementation vs. production-grade" distinction the PRD itself
  calls for.

## Tested invariants

`10-cryptography/tests/sha256_test.cpp` (7 hosted assertions): all
four FIPS 180-4 known-answer vectors match exactly; feeding a message
through `update()` one byte at a time produces the identical digest to
a single one-shot call (the real invariant any streaming hash API must
satisfy); `reset()` genuinely restores the initial state (hashing the
same input twice after a reset gives the same digest); and a
single-character-different input produces a completely different
digest (a basic avalanche sanity check, not a substitute for the KATs
above).

## Consequences

Every claim about cryptography elsewhere in this repository must
describe this layer using the scope recorded here: a verified SHA-256
implementation, nothing else. This ADR is the single source of truth
for that distinction until a future ADR (covering HMAC, AEAD, or a PQC
primitive) extends it.

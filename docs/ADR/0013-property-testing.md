# ADR 0013: Property-based testing infrastructure (v1)

**Status:** Accepted. Starts Layer 11 (Formal Verification &
Reliability) as a real cross-layer verification tool applied to
already-shipped code, not a standalone demo.

## Context

Every subsystem built so far (networking parsers, RPC serialization,
the WAL, HMAC, the ELF loader) has been tested with hand-written
example-based tests: specific known-good inputs, specific known-bad
inputs. That style is necessary but incomplete — it only checks the
cases a human thought to write. Layer 11 calls for property testing,
fuzzing, and invariant checking; the right first piece of
infrastructure is a genuine property-based test generator, deliberately
built to be pointed at *existing* subsystems rather than tested only
against itself, since a verification framework with nothing real to
verify would just be another demo.

## Decision

`11-verification/property_testing.hpp` is a small, header-only
QuickCheck-style framework: `Random` is a seeded, deterministic PRNG
(xorshift64\*, the same well-tested generator already used by
`07-distributed-systems`' `SimulatedNetwork` — reused, not reinvented,
for the identical reason: reproducibility from a seed matters more
than cryptographic-quality randomness for a test generator) with
convenience methods for generating random bytes, byte blobs, and
printable strings of random length. `forAll<T>()` runs a property
function against many independently generated inputs and reports a
`[PASS]`/`[FAIL]` result in the same convention every other test suite
in this repository already uses; on failure, it reports the exact
trial index and seed, so — because generation is fully deterministic
from the seed — the identical failing case is exactly reproducible by
rerunning with that seed, the entire point of seeded generation over
unseeded randomness for a test suite.

`11-verification/tests/property_tests.cpp` applies this to five real,
already-shipped subsystems spanning four other layers plus the OS:

- **Serialization round-trip** (Layer 7): `dist::Encoder`/`Decoder`
  correctly round-trips an arbitrary random byte blob and string, for
  500 random inputs.
- **HMAC determinism** (Layer 10): `hmacSha256(key, message)` produces
  the identical result across repeated calls with the identical
  inputs, for 300 random (key, message) pairs.
- **WAL exactness** (Layer 8): `WriteAheadLog::recoverRecords()`
  returns exactly the sequence of records that was appended — no more,
  no fewer, in order — for 100 random append sequences of random
  length and content, each run through a genuine temporary file on
  disk.
- **ELF loader fuzz robustness** (the OS, `22-os/elf/`):
  `elf_validate_and_plan()` never crashes (segfault, out-of-bounds
  read, infinite loop) on arbitrary random bytes, for 2000 random
  buffers. Since a random buffer is astronomically unlikely to satisfy
  every one of the loader's structural checks, this mostly exercises
  the actual fuzz-testing question for an untrusted-input parser: does
  it survive garbage without crashing, not whether it happens to
  accept it.
- **Network parser fuzz robustness** (Layer 6): every one of
  `parseEthernetHeader`/`parseArpPacket`/`parseIpv4Header`/
  `parseIcmpEcho`/`parseUdpHeader` never crashes on arbitrary random
  bytes, for 2000 random buffers — the identical untrusted-input
  discipline applied to a completely different parser family,
  confirming the approach generalizes rather than being specific to
  one subsystem's quirks.

## What this is not

- **Not a shrinker.** A real QuickCheck-style framework, on finding a
  failing case, automatically searches for a *smaller* failing input
  to report (e.g. a 3-byte buffer instead of the original 180-byte
  one) — this v1 reports the exact seed and trial index (fully
  reproducible) but not a minimized counterexample. Shrinking is a
  meaningful, still-unbuilt improvement.
- **Not a model checker, symbolic execution engine, or theorem
  prover.** This checks a property against many concrete random
  inputs, which can find counterexamples but can never prove a
  property holds for *all* possible inputs the way formal
  verification tools aim to. Both remain PLANNED, separate efforts.
- **Not integrated into every existing test suite's default run.**
  Each subsystem's own hosted test script (`elf_test.sh`,
  `protocols_test.sh`, etc.) still runs its own hand-written example-
  based tests; this property suite is a separate, additional layer of
  coverage over the same code, not a replacement for those tests.
- **Not exhaustive fuzzing.** 2000 uniformly-random trials per parser
  is a real, meaningful robustness check, but it is not coverage-
  guided fuzzing (e.g. libFuzzer/AFL-style, which uses code coverage
  feedback to steer generation toward unexplored paths) — a
  substantially more thorough (and more complex to set up) technique
  that remains a natural future addition.

## Tested invariants

Every property listed under "Decision" above passed on the first real
run against the actual shipped implementations — a genuine result, not
a retrofit after finding and fixing a bug (unlike several earlier
milestones in this project's history, e.g. the Raft addressing bug in
ADR 0009 or the checksum two-zeros bug in ADR 0005, both caught by
their own test suites). That the very first property run passed for
all five subsystems is itself evidence the earlier hand-written
example-based tests for those subsystems already covered their real
edge cases reasonably well — a property-testing framework's value
includes exactly this kind of independent confirmation, not only
finding new bugs.

## Consequences

Every claim about formal verification/property testing elsewhere in
this repository must describe this layer using the scope recorded
here: a real, working, seeded property-test generator applied to five
genuine subsystems across four other layers, not a model checker, not
a shrinking QuickCheck, not coverage-guided fuzzing. This ADR is the
single source of truth for that distinction until a future ADR
(covering shrinking, model checking, or coverage-guided fuzzing)
extends it.

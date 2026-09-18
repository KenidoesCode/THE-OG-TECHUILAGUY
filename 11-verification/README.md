# Layer 11 — Formal Verification & Reliability

**Status: FOUNDATION.** A real, seeded, property-based testing
framework, applied to five genuine subsystems across four other layers
plus the OS — not a standalone demo. No model checking, no
shrinking QuickCheck, no coverage-guided fuzzing yet. See
[`docs/ADR/0013-property-testing.md`](../docs/ADR/0013-property-testing.md).

## Implemented and tested

- `property_testing.hpp`: a header-only, seeded PRNG (`Random`) plus
  `forAll<T>()`, which runs a property function against many
  independently generated random inputs and reports the exact seed
  and trial index of any failure — fully reproducible by rerunning
  with the same seed.
- `tests/property_tests.cpp` applies it to five real, already-shipped
  subsystems: `dist::Encoder`/`Decoder` round-trip correctness (Layer
  7), HMAC-SHA256 determinism (Layer 10), write-ahead log
  append/recover exactness against real disk I/O (Layer 8), and
  fuzz-style crash-robustness of both the ELF loader (`22-os/elf/`)
  and every network protocol parser (Layer 6) against thousands of
  random buffers each.

## Not yet implemented

- a shrinker (minimizing a failing case to the smallest reproducing
  input, rather than just reporting the seed/trial index)
- model checking, symbolic execution, theorem-proving experiments
- coverage-guided fuzzing (this uses uniform random generation, not
  code-coverage feedback to steer generation)
- deterministic state-machine testing, chaos testing, reliability
  metrics as their own dedicated infrastructure (beyond what
  `07-distributed-systems`' `SimulatedNetwork` already provides for
  Raft specifically)

## Building and testing

```sh
bash tests/property_tests.sh   # hosted, applies property tests to real code in other layers
```

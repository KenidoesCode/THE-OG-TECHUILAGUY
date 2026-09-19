# Layer 15 — Quantum

**Status: FOUNDATION.** A real classical state-vector simulator
(complex amplitudes, exact gate application, probabilistic
measurement) — not physical quantum hardware, and never claimed to be.
See [`docs/ADR/0015-quantum-simulator.md`](../docs/ADR/0015-quantum-simulator.md).

## Implemented and tested

- `simulator/qsim.hpp`/`.cpp`: an n-qubit `QuantumState` (2^n complex
  amplitudes), single-qubit X/Z/H gates, two-qubit CNOT, and
  measurement (collapse) driven by an externally supplied uniform
  random value — no built-in RNG, so measurement is exactly
  reproducible given the same random draws.
- 17 hosted unit/property assertions (`tests/qsim_test.cpp`/
  `qsim_test.sh`): exact-amplitude checks for X/Z/H/CNOT and the
  canonical Bell state; measurement-correlation checks over 1000
  (Bell) and 500 (3-qubit GHZ) trials confirming the qubits' outcomes
  are genuinely correlated, not just that the amplitudes look right;
  measurement collapse verified directly; and a property test (real
  integration with `11-verification/property_testing.hpp`) confirming
  total probability stays exactly 1.0 across 300 random gate
  sequences.

## Not yet implemented

- a reusable algorithm library (Deutsch-Jozsa, Grover, QFT, a Shor
  research prototype) — Bell/GHZ states are constructed directly in
  the tests, not as named, reusable circuits
- a circuit representation, circuit optimizer, or gate decomposition
- noise models / decoherence — every gate is applied exactly (unitary)
- error-correction research (stabilizer codes, surface codes, etc.)
- any connection to real quantum hardware (this is classical
  simulation only)
- integration with cryptography/optimization/scientific computing (the
  mission names these as eventual connections; none exist yet)

## Building and testing

```sh
bash tests/qsim_test.sh   # hosted tests, no hardware needed
```

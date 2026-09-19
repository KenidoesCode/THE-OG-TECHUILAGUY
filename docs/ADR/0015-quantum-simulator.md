# ADR 0015: Quantum state-vector simulator (v1)

**Status:** Accepted. Starts Layer 15 (Quantum) as a real classical
simulation of quantum mechanics — not physical quantum hardware, and
explicitly never claimed to be.

## Context

Layer 15 calls for quantum gates, circuits, a simulator, measurement,
and known algorithms (Bell states, GHZ, Deutsch-Jozsa, Grover, QFT).
A state-vector simulator is the correct, standard starting point: it
represents an n-qubit register's complete quantum state as 2^n complex
amplitudes and applies gates as exact linear-algebra operations on
that vector — the same technique real quantum-computing frameworks
(Qiskit's `Statevector`, Cirq's simulator, etc.) use for their own
classical simulation backends, not an invented shortcut.

## Decision

`15-quantum/simulator/qsim.cpp`'s `QuantumState` holds `2^n` complex
amplitudes (`std::complex<double>`) for an n-qubit register, starting
at `|0...0>`. `applyX`/`applyZ`/`applyH` apply the standard single-
qubit Pauli-X, Pauli-Z, and Hadamard matrices via
`applySingleQubitGate`'s generic 2x2-matrix-on-a-bit-pair application
(iterating basis-state pairs that differ only in the target qubit);
`applyCNOT` swaps amplitude pairs where the control qubit is 1,
implementing the standard controlled-NOT. `measure()` takes an
external uniform random value in `[0, 1)` — the simulator has no
built-in randomness source, so measurement is exactly reproducible
given the same external random draws, reusing
`11-verification/property_testing.hpp`'s `Random` directly in the
tests rather than introducing a second RNG for no reason — samples an
outcome by cumulative probability (`|amplitude|^2` per basis state)
and collapses the state vector to that outcome.

Correctness is checked two independent ways, both of which a
simulator could get wrong without the other catching it: **exact
amplitude values** (Bell state `H(0)` then `CNOT(0,1)` must produce
*precisely* `(|00> + |11>)/sqrt(2)`, checked to floating-point
tolerance) and **measurement-correlation behavior over many trials**
(a Bell state must *never* measure to `01`/`10` across 1000 samples,
and a 3-qubit GHZ state must never measure to a mixed outcome across
500 samples) — a simulator with a subtly wrong CNOT implementation
could still happen to preserve total probability (passing a norm
check) while producing the wrong correlations, so both checks matter
independently.

A genuine cross-layer integration: `propertyTotalProbabilityAlwaysOne`
uses `11-verification/property_testing.hpp`'s `forAll` to check that
total measurement probability stays exactly 1.0 after 300 random
sequences of up to 20 random gates each on a 3-qubit register — the
first real application of that framework outside the five subsystems
ADR 0013 originally applied it to.

## What this is not

- **Not physical quantum hardware, and not a claim of one.** This is
  classical simulation; its cost is exponential in qubit count (2^n
  amplitudes), the fundamental reason real quantum hardware is
  pursued at all. Nothing here is validated against, or claims access
  to, real quantum hardware.
- **No noise model.** Every gate is applied exactly (unitary, no
  decoherence, no gate error) — real hardware validation and noise
  modeling are both unimplemented.
- **No algorithm library yet.** Bell and GHZ states are constructed
  directly in the tests as the simulator's own correctness check;
  Deutsch-Jozsa, Grover, QFT, and a Shor research prototype (all named
  in the mission) are not implemented as reusable circuits yet.
- **No circuit representation/compiler.** Gates are applied by direct
  function calls in sequence; there is no separate circuit data
  structure, no circuit optimizer, and no gate decomposition.
- **No error-correction research** (surface codes, stabilizer
  formalism, etc.).
- **Practical qubit-count limit.** Memory is `16 * 2^n` bytes for the
  amplitude vector alone; this is fine for the small registers (1-3
  qubits) exercised by the tests, but nothing here addresses the
  practical ceiling a real state-vector simulator eventually hits.

## Tested invariants

`15-quantum/tests/qsim_test.cpp` (17 hosted assertions): the initial
state is exactly `|0...0>`; X flips deterministically and is its own
inverse; H produces the exact `1/sqrt(2)` equal-superposition
amplitudes; Z leaves `|0>`'s amplitude unchanged and negates `|1>`'s
exactly; CNOT flips the target exactly when the control is 1 and
leaves it alone otherwise; a Bell state's amplitudes match the
canonical `(|00> + |11>)/sqrt(2)` exactly, with `|01>`/`|10>` at
exactly zero; a Bell state never measures to a mismatched outcome
across 1000 trials, with both real outcomes occurring roughly equally
often; a 3-qubit GHZ state never measures to a mixed outcome across
500 trials; measurement genuinely collapses the state vector (the
observed outcome's amplitude becomes exactly 1, everything else
exactly 0); and — the property-test integration with Layer 11 — total
probability stays exactly 1.0 across 300 random gate sequences.

## Consequences

Every claim about quantum computing elsewhere in this repository must
describe this capability using the scope recorded here: a real,
tested, classical state-vector simulator for small qubit registers —
not physical hardware, not a noise-aware simulator, not an algorithm
library, not error correction. This ADR is the single source of truth
for that distinction until a future ADR (covering named algorithms,
noise modeling, or a circuit representation) extends it.

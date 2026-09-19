#pragma once

#include <complex>
#include <cstdint>
#include <vector>

// A real quantum state-vector simulator: complex amplitudes for every
// one of 2^n basis states of an n-qubit register, gate application by
// direct amplitude manipulation (no approximation), and probabilistic
// measurement collapse. This is classical simulation of quantum
// mechanics — it runs on ordinary hardware and its cost is exponential
// in qubit count, real physical quantum hardware is not simulated or
// claimed. See docs/ADR/0015-quantum-simulator.md for the exact scope
// (state-vector simulation only, no noise model, no real hardware
// backend, qubit count limited by 2^n memory in practice).

namespace quantum {

using Complex = std::complex<double>;

class QuantumState {
public:
    // All qubits start in |0...0>.
    explicit QuantumState(uint32_t qubitCount);

    uint32_t qubitCount() const { return numQubits; }

    // Single-qubit gates, applied to `target` (0-indexed).
    void applyX(uint32_t target);
    void applyZ(uint32_t target);
    void applyH(uint32_t target);

    // Two-qubit controlled-NOT: flips `target` iff `control` is |1>.
    void applyCNOT(uint32_t control, uint32_t target);

    // The raw amplitude of basis state `index` (0 <= index < 2^n, bit
    // i of index is qubit i's value) — exposed for exact numerical
    // testing (KAT-style checks against known state vectors), not
    // something a real quantum computer could ever read out directly
    // (measuring a real qubit collapses it; there is no way to read
    // an amplitude off real hardware without full state tomography
    // over many repeated experiments — this simulator can do it
    // because it IS the classical data structure being simulated).
    Complex amplitude(uint32_t basisIndex) const;

    // Sum of |amplitude|^2 over every basis state — must always equal
    // 1.0 (within floating-point tolerance) for a physically valid
    // state; exposed so callers/tests can verify this invariant
    // directly rather than trusting gate application to preserve it.
    double totalProbability() const;

    // Measures the entire register in the computational basis,
    // collapsing the state vector to the observed outcome (every
    // amplitude except the observed basis state's becomes exactly
    // zero, and that one is renormalized to magnitude 1) and
    // returning the observed basis-state index. `rng` must be a
    // uniform random source in [0, 1) — the simulator has no built-in
    // randomness source of its own, so measurement is exactly
    // reproducible given the same sequence of external random draws
    // (see 11-verification/property_testing.hpp's Random, reused
    // directly by this layer's tests).
    uint32_t measure(double uniformRandom01);

private:
    uint32_t numQubits;
    std::vector<Complex> amplitudes;  // size 2^numQubits

    void applySingleQubitGate(
        uint32_t target, Complex m00, Complex m01, Complex m10, Complex m11
    );
};

}  // namespace quantum

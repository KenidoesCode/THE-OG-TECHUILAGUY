// Real assertion-based and property-based tests for the quantum
// state-vector simulator (simulator/qsim.cpp). Known quantum states
// (Bell, GHZ) are checked both by exact amplitude values and by their
// measurement-correlation behavior over many trials — the two
// independent ways a simulator's correctness can actually be checked
// without real quantum hardware. Also integrates directly with
// 11-verification/property_testing.hpp (a real cross-layer
// connection): the "total probability is always 1" invariant is
// checked as a property over random gate sequences, not just a few
// hand-picked examples.

#include "../simulator/qsim.hpp"
#include "../../11-verification/property_testing.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

bool approxEqual(quantum::Complex a, quantum::Complex b, double tolerance = 1e-9) {
    return std::abs(a - b) < tolerance;
}

}  // namespace

void testInitialStateIsAllZero() {
    quantum::QuantumState state(2);

    check(approxEqual(state.amplitude(0b00), quantum::Complex(1.0, 0.0)),
          "qsim: a freshly constructed 2-qubit state has amplitude 1 on |00>");
    check(approxEqual(state.amplitude(0b01), quantum::Complex(0.0, 0.0)) &&
          approxEqual(state.amplitude(0b10), quantum::Complex(0.0, 0.0)) &&
          approxEqual(state.amplitude(0b11), quantum::Complex(0.0, 0.0)),
          "qsim: every other basis state starts at amplitude 0");
}

void testXGateFlipsDeterministically() {
    quantum::QuantumState state(1);
    state.applyX(0);

    check(approxEqual(state.amplitude(0), quantum::Complex(0.0, 0.0)) &&
          approxEqual(state.amplitude(1), quantum::Complex(1.0, 0.0)),
          "qsim: X on |0> deterministically produces |1> (exact amplitudes)");

    state.applyX(0);
    check(approxEqual(state.amplitude(0), quantum::Complex(1.0, 0.0)),
          "qsim: applying X twice returns to |0> (X is its own inverse)");
}

void testHGateCreatesEqualSuperposition() {
    quantum::QuantumState state(1);
    state.applyH(0);

    double expected = 1.0 / std::sqrt(2.0);
    check(approxEqual(state.amplitude(0), quantum::Complex(expected, 0.0)) &&
          approxEqual(state.amplitude(1), quantum::Complex(expected, 0.0)),
          "qsim: H on |0> produces the exact expected equal-superposition "
          "amplitudes (1/sqrt(2) on both |0> and |1>)");
}

void testZGateAppliesPhaseOnlyToOneState() {
    quantum::QuantumState state(1);
    state.applyH(0);  // (|0> + |1>) / sqrt(2)
    state.applyZ(0);  // (|0> - |1>) / sqrt(2)

    double expected = 1.0 / std::sqrt(2.0);
    check(approxEqual(state.amplitude(0), quantum::Complex(expected, 0.0)) &&
          approxEqual(state.amplitude(1), quantum::Complex(-expected, 0.0)),
          "qsim: Z leaves |0>'s amplitude unchanged and negates |1>'s — "
          "the exact expected phase-flip behavior");
}

void testCNOTFlipsTargetOnlyWhenControlIsOne() {
    // |10> --CNOT(0,1)--> |11> (control=qubit0=1, so target=qubit1 flips)
    quantum::QuantumState state(2);
    state.applyX(0);  // control qubit -> 1, state is now |10> in (q1 q0) bit order...
    // basis index bit i = qubit i, so applyX(0) sets bit 0: index becomes 0b01.
    state.applyCNOT(0, 1);

    check(approxEqual(state.amplitude(0b11), quantum::Complex(1.0, 0.0)),
          "qsim: CNOT flips the target qubit exactly when the control qubit is 1");

    quantum::QuantumState state2(2);  // control qubit0 stays 0
    state2.applyCNOT(0, 1);
    check(approxEqual(state2.amplitude(0b00), quantum::Complex(1.0, 0.0)),
          "qsim: CNOT leaves the target qubit unchanged when the control qubit is 0");
}

void testBellStateExactAmplitudes() {
    // The canonical Bell state (|00> + |11>) / sqrt(2): H on qubit 0,
    // then CNOT(0, 1).
    quantum::QuantumState state(2);
    state.applyH(0);
    state.applyCNOT(0, 1);

    double expected = 1.0 / std::sqrt(2.0);
    check(approxEqual(state.amplitude(0b00), quantum::Complex(expected, 0.0)) &&
          approxEqual(state.amplitude(0b11), quantum::Complex(expected, 0.0)) &&
          approxEqual(state.amplitude(0b01), quantum::Complex(0.0, 0.0)) &&
          approxEqual(state.amplitude(0b10), quantum::Complex(0.0, 0.0)),
          "qsim: H(0) then CNOT(0,1) produces the exact canonical Bell "
          "state amplitudes: (|00> + |11>) / sqrt(2), with |01>/|10> "
          "at exactly zero amplitude");
}

void testBellStateMeasurementCorrelation() {
    // A Bell state must NEVER measure to |01> or |10> — its two
    // qubits are perfectly correlated. Checked over many trials with
    // a seeded, reproducible random source (reusing
    // 11-verification's Random rather than std::rand), not just the
    // exact-amplitude check above, since a correct amplitude
    // structure and correct measurement-sampling behavior are two
    // independently-checkable claims about the simulator.
    verify::Random rng(0xB311);

    int zeroZeroCount = 0;
    int oneOneCount = 0;
    int mismatchCount = 0;
    const int trials = 1000;

    for (int i = 0; i < trials; ++i) {
        quantum::QuantumState state(2);
        state.applyH(0);
        state.applyCNOT(0, 1);

        double r = static_cast<double>(rng.nextU64() >> 11) * (1.0 / 9007199254740992.0);
        uint32_t outcome = state.measure(r);

        if (outcome == 0b00) ++zeroZeroCount;
        else if (outcome == 0b11) ++oneOneCount;
        else ++mismatchCount;
    }

    check(mismatchCount == 0,
          "qsim: a Bell state never measures to a mismatched outcome "
          "(01 or 10) across 1000 trials — the qubits are perfectly correlated");
    check(zeroZeroCount > 400 && oneOneCount > 400,
          "qsim: a Bell state's two possible outcomes (00, 11) both "
          "occur roughly equally often across 1000 trials (a basic "
          "sanity check on the measurement sampling distribution, not "
          "a precise statistical test)");
}

void testGHZStateThreeQubitCorrelation() {
    // The GHZ state for 3 qubits: (|000> + |111>) / sqrt(2) — H on
    // qubit 0, then CNOT(0,1), then CNOT(0,2). Every measurement must
    // yield either 000 or 111, never a mixed outcome.
    verify::Random rng(0x64485A);

    bool allCorrelated = true;
    int allZeroCount = 0;
    int allOneCount = 0;
    const int trials = 500;

    for (int i = 0; i < trials; ++i) {
        quantum::QuantumState state(3);
        state.applyH(0);
        state.applyCNOT(0, 1);
        state.applyCNOT(0, 2);

        double r = static_cast<double>(rng.nextU64() >> 11) * (1.0 / 9007199254740992.0);
        uint32_t outcome = state.measure(r);

        if (outcome == 0b000) ++allZeroCount;
        else if (outcome == 0b111) ++allOneCount;
        else allCorrelated = false;
    }

    check(allCorrelated,
          "qsim: a 3-qubit GHZ state (H + two CNOTs) only ever measures "
          "to all-zeros or all-ones across 500 trials, never a mixed outcome");
    check(allZeroCount > 150 && allOneCount > 150,
          "qsim: both GHZ outcomes occur a reasonable number of times "
          "(basic sampling sanity check)");
}

void testMeasurementCollapsesState() {
    quantum::QuantumState state(1);
    state.applyH(0);

    // Force collapse to |1> by supplying a random value past the
    // |0>-outcome's cumulative probability (0.5).
    uint32_t outcome = state.measure(0.9);

    check(outcome == 1, "qsim: measure() with a random draw past the "
                         "|0> cumulative probability collapses to |1>");
    check(approxEqual(state.amplitude(1), quantum::Complex(1.0, 0.0)) &&
          approxEqual(state.amplitude(0), quantum::Complex(0.0, 0.0)),
          "qsim: after measurement, the state vector is fully collapsed "
          "— the observed outcome has amplitude 1, everything else 0");
}

// --- Property test (real integration with Layer 11) ---

void propertyTotalProbabilityAlwaysOne() {
    bool ok = verify::forAll<std::vector<int>>(
        "property: total measurement probability (sum of |amplitude|^2 "
        "over all basis states) is always 1.0 after any random sequence "
        "of gates, for 300 random gate sequences on a 3-qubit register",
        /*seed=*/7007, /*trialCount=*/300,
        [](verify::Random& rng) {
            // Each int encodes one random gate application: 0=X,
            // 1=Z, 2=H on a random qubit, or 3=CNOT on a random
            // (control, target) pair — encoded compactly as
            // gateType*100 + qubitIndices so the property function
            // can decode and apply it deterministically from the
            // same generated sequence.
            std::vector<int> ops;
            uint32_t opCount = rng.nextInRange(0, 20);
            for (uint32_t i = 0; i < opCount; ++i) {
                uint32_t gateType = rng.nextInRange(0, 3);
                uint32_t q1 = rng.nextInRange(0, 2);
                uint32_t q2 = rng.nextInRange(0, 2);
                ops.push_back(static_cast<int>(gateType * 100 + q1 * 10 + q2));
            }
            return ops;
        },
        [](const std::vector<int>& ops) {
            quantum::QuantumState state(3);
            for (int encoded : ops) {
                uint32_t gateType = static_cast<uint32_t>(encoded) / 100;
                uint32_t q1 = (static_cast<uint32_t>(encoded) / 10) % 10;
                uint32_t q2 = static_cast<uint32_t>(encoded) % 10;

                switch (gateType) {
                    case 0: state.applyX(q1); break;
                    case 1: state.applyZ(q1); break;
                    case 2: state.applyH(q1); break;
                    case 3: if (q1 != q2) state.applyCNOT(q1, q2); break;
                }
            }

            return std::abs(state.totalProbability() - 1.0) < 1e-9;
        }
    );
    if (!ok) failures++;
}

int main() {
    testInitialStateIsAllZero();
    testXGateFlipsDeterministically();
    testHGateCreatesEqualSuperposition();
    testZGateAppliesPhaseOnlyToOneState();
    testCNOTFlipsTargetOnlyWhenControlIsOne();
    testBellStateExactAmplitudes();
    testBellStateMeasurementCorrelation();
    testGHZStateThreeQubitCorrelation();
    testMeasurementCollapsesState();
    propertyTotalProbabilityAlwaysOne();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

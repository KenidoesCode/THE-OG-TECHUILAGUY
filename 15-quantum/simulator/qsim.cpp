#include "qsim.hpp"

#include <cmath>

namespace quantum {

QuantumState::QuantumState(uint32_t qubitCount)
    : numQubits(qubitCount), amplitudes(size_t(1) << qubitCount, Complex(0.0, 0.0)) {
    amplitudes[0] = Complex(1.0, 0.0);  // |0...0>
}

Complex QuantumState::amplitude(uint32_t basisIndex) const {
    return amplitudes[basisIndex];
}

double QuantumState::totalProbability() const {
    double sum = 0.0;
    for (const auto& amp : amplitudes) {
        sum += std::norm(amp);  // |amp|^2
    }
    return sum;
}

void QuantumState::applySingleQubitGate(
    uint32_t target, Complex m00, Complex m01, Complex m10, Complex m11
) {
    uint32_t bit = 1u << target;

    for (uint32_t i = 0; i < amplitudes.size(); ++i) {
        if ((i & bit) != 0) {
            continue;  // handle each |...0...>/|...1...> pair once, from the 0-side
        }

        uint32_t j = i | bit;  // the paired basis state with `target` set to 1

        Complex a0 = amplitudes[i];
        Complex a1 = amplitudes[j];

        amplitudes[i] = m00 * a0 + m01 * a1;
        amplitudes[j] = m10 * a0 + m11 * a1;
    }
}

void QuantumState::applyX(uint32_t target) {
    applySingleQubitGate(
        target,
        Complex(0, 0), Complex(1, 0),
        Complex(1, 0), Complex(0, 0)
    );
}

void QuantumState::applyZ(uint32_t target) {
    applySingleQubitGate(
        target,
        Complex(1, 0), Complex(0, 0),
        Complex(0, 0), Complex(-1, 0)
    );
}

void QuantumState::applyH(uint32_t target) {
    double s = 1.0 / std::sqrt(2.0);
    applySingleQubitGate(
        target,
        Complex(s, 0), Complex(s, 0),
        Complex(s, 0), Complex(-s, 0)
    );
}

void QuantumState::applyCNOT(uint32_t control, uint32_t target) {
    uint32_t controlBit = 1u << control;
    uint32_t targetBit = 1u << target;

    for (uint32_t i = 0; i < amplitudes.size(); ++i) {
        // Only act when control=1 and target=0, swapping with the
        // control=1,target=1 partner — each such pair handled once
        // from the target=0 side, exactly mirroring
        // applySingleQubitGate's i/j pairing approach but gated on
        // the control bit.
        if ((i & controlBit) != 0 && (i & targetBit) == 0) {
            uint32_t j = i | targetBit;
            std::swap(amplitudes[i], amplitudes[j]);
        }
    }
}

uint32_t QuantumState::measure(double uniformRandom01) {
    double cumulative = 0.0;
    uint32_t chosen = static_cast<uint32_t>(amplitudes.size()) - 1;

    for (uint32_t i = 0; i < amplitudes.size(); ++i) {
        cumulative += std::norm(amplitudes[i]);
        if (uniformRandom01 < cumulative) {
            chosen = i;
            break;
        }
    }

    for (uint32_t i = 0; i < amplitudes.size(); ++i) {
        amplitudes[i] = (i == chosen) ? Complex(1.0, 0.0) : Complex(0.0, 0.0);
    }

    return chosen;
}

}  // namespace quantum

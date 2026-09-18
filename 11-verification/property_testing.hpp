#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// A small, real property-based testing framework (QuickCheck-style):
// generate many random inputs from a seeded, reproducible source and
// check that a property holds for all of them, reporting the exact
// seed that produced any failing case so it can be reproduced. This
// is Layer 11 (Formal Verification)'s first piece of real
// infrastructure, and it exists specifically to be pointed at other
// layers' existing code (see tests/ in this directory) rather than
// tested in isolation — a property-testing framework with nothing to
// test would not be verification infrastructure, just a demo.
//
// This is not a model checker, not a symbolic execution engine, and
// not a theorem prover — see docs/ADR/0013-property-testing.md for
// the exact scope.

namespace verify {

// A minimal, deterministic PRNG (xorshift64*) — the same well-tested
// choice already used by 07-distributed-systems' SimulatedNetwork,
// reused here rather than reinvented, for the identical reason:
// reproducibility from a seed matters far more than
// cryptographic-quality randomness for a test generator.
class Random {
public:
    explicit Random(uint64_t seed) : state(seed == 0 ? 0x2545F4914F6CDD1DULL : seed) {}

    uint64_t nextU64() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    }

    uint32_t nextU32() { return static_cast<uint32_t>(nextU64()); }
    uint8_t nextU8() { return static_cast<uint8_t>(nextU64()); }

    // Uniform in [minValue, maxValue], inclusive.
    uint32_t nextInRange(uint32_t minValue, uint32_t maxValue) {
        uint32_t range = maxValue - minValue + 1;
        return minValue + static_cast<uint32_t>(nextU64() % range);
    }

    std::vector<uint8_t> nextBytes(uint32_t minLength, uint32_t maxLength) {
        uint32_t length = nextInRange(minLength, maxLength);
        std::vector<uint8_t> result;
        result.reserve(length);
        for (uint32_t i = 0; i < length; ++i) result.push_back(nextU8());
        return result;
    }

    std::string nextPrintableString(uint32_t minLength, uint32_t maxLength) {
        uint32_t length = nextInRange(minLength, maxLength);
        std::string result;
        result.reserve(length);
        for (uint32_t i = 0; i < length; ++i) {
            result += static_cast<char>(nextInRange(0x20, 0x7E));  // printable ASCII
        }
        return result;
    }

private:
    uint64_t state;
};

// Runs `property(generatedInput)` for `trialCount` independently
// generated inputs (each produced by `generate(rng)`), reporting a
// PASS/FAIL summary in the same [PASS]/[FAIL] convention every other
// test suite in this repository uses. On the first failure, prints
// the exact trial index and master seed that produced it — since
// generation is fully deterministic from the seed, re-running with the
// identical seed reproduces the identical failing input, the whole
// point of seeded generation over unseeded randomness for a test
// suite.
template <typename T>
bool forAll(
    const std::string& description,
    uint64_t seed,
    int trialCount,
    std::function<T(Random&)> generate,
    std::function<bool(const T&)> property
) {
    Random rng(seed);

    for (int trial = 0; trial < trialCount; ++trial) {
        T input = generate(rng);

        if (!property(input)) {
            std::cout << "[FAIL] " << description
                      << " (counterexample found at trial " << trial
                      << " of " << trialCount << ", seed=" << seed
                      << " — rerun with this exact seed to reproduce)\n";
            return false;
        }
    }

    std::cout << "[PASS] " << description
              << " (" << trialCount << " random trials, seed=" << seed << ")\n";
    return true;
}

}  // namespace verify

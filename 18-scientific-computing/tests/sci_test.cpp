// Real assertion-based tests for Layer 18's linear algebra (Vec3) and
// numerical ODE integration (RK4), verified against known analytical
// solutions — the only honest way to check a numerical method's
// correctness, rather than just "it runs."

#include "../linalg/vec3.hpp"
#include "../ode/rk4.hpp"

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

bool approx(double a, double b, double tolerance) {
    return std::abs(a - b) < tolerance;
}

}  // namespace

void testVec3BasicAlgebra() {
    sci::Vec3 a(1, 2, 3);
    sci::Vec3 b(4, 5, 6);

    sci::Vec3 sum = a + b;
    check(sum.x == 5 && sum.y == 7 && sum.z == 9, "vec3: addition is componentwise");

    double dot = a.dot(b);
    check(dot == 32.0, "vec3: dot product matches the known value (1*4+2*5+3*6=32)");

    sci::Vec3 cross = a.cross(b);
    check(cross.x == -3 && cross.y == 6 && cross.z == -3,
          "vec3: cross product matches the known value for (1,2,3)x(4,5,6)");
}

void testVec3NormAndNormalize() {
    sci::Vec3 v(3, 4, 0);
    check(approx(v.norm(), 5.0, 1e-12), "vec3: norm of (3,4,0) is exactly 5 (3-4-5 triangle)");

    sci::Vec3 n = v.normalized();
    check(approx(n.norm(), 1.0, 1e-12), "vec3: a normalized vector has unit norm");
}

void testVec3ZeroVectorNormalizesToZero() {
    sci::Vec3 zero(0, 0, 0);
    sci::Vec3 n = zero.normalized();
    check(n.x == 0 && n.y == 0 && n.z == 0,
          "vec3: normalizing the zero vector returns zero rather than dividing by zero (NaN)");
}

void testRk4ExponentialDecay() {
    // dy/dt = -y, y(0) = 1 — exact solution y(t) = e^(-t).
    sci::Derivative<1> f = [](double, const sci::State<1>& y) -> sci::State<1> {
        return {-y[0]};
    };

    sci::State<1> initial = {1.0};
    sci::State<1> result = sci::rk4Integrate<1>(f, 0.0, initial, 0.01, 200);  // t = 2.0

    double exact = std::exp(-2.0);
    check(approx(result[0], exact, 1e-6),
          "rk4: exponential decay (dy/dt=-y) matches the exact solution "
          "e^-2 to within 1e-6 after 200 steps of size 0.01");
}

void testRk4SimpleHarmonicOscillator() {
    // d^2x/dt^2 = -x, i.e. state = (x, v), dx/dt = v, dv/dt = -x.
    // Exact solution for x(0)=1, v(0)=0: x(t) = cos(t).
    sci::Derivative<2> f = [](double, const sci::State<2>& s) -> sci::State<2> {
        return {s[1], -s[0]};
    };

    sci::State<2> initial = {1.0, 0.0};
    double t = 2.0 * M_PI;  // exactly one full period
    sci::State<2> result = sci::rk4Integrate<2>(f, 0.0, initial, 0.001, 6283);  // ~2*pi

    check(approx(result[0], std::cos(t), 1e-3) && approx(result[1], -std::sin(t), 1e-3),
          "rk4: a simple harmonic oscillator (x''=-x) returns close to "
          "its exact analytical value after integrating one full period");
}

void testRk4EnergyConservationOverManyPeriods() {
    // For the harmonic oscillator, total energy E = 0.5*(v^2 + x^2)
    // is exactly conserved by the true dynamics. RK4 is not
    // symplectic, so energy drifts slightly over long integrations —
    // this test checks the drift stays small over 10 periods with a
    // reasonably fine step, not that it's exactly zero.
    sci::Derivative<2> f = [](double, const sci::State<2>& s) -> sci::State<2> {
        return {s[1], -s[0]};
    };

    sci::State<2> state = {1.0, 0.0};
    double initialEnergy = 0.5 * (state[1] * state[1] + state[0] * state[0]);

    double dt = 0.001;
    uint32_t stepsPerPeriod = static_cast<uint32_t>(2.0 * M_PI / dt);

    for (int period = 0; period < 10; ++period) {
        state = sci::rk4Integrate<2>(f, 0.0, state, dt, stepsPerPeriod);
    }

    double finalEnergy = 0.5 * (state[1] * state[1] + state[0] * state[0]);

    check(std::abs(finalEnergy - initialEnergy) < 0.01,
          "rk4: total energy of a harmonic oscillator drifts by less "
          "than 1% over 10 full periods (RK4 is not exactly "
          "energy-conserving, but the drift stays small at this step size)");
}

int main() {
    testVec3BasicAlgebra();
    testVec3NormAndNormalize();
    testVec3ZeroVectorNormalizesToZero();
    testRk4ExponentialDecay();
    testRk4SimpleHarmonicOscillator();
    testRk4EnergyConservationOverManyPeriods();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

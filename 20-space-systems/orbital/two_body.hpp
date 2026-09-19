#pragma once

#include "../../18-scientific-computing/linalg/vec3.hpp"
#include "../../18-scientific-computing/ode/rk4.hpp"

// Two-body orbital propagation — a real dependency chain, Layer 18
// (Scientific Computing)'s Vec3/RK4 used directly by Layer 20 (Space
// Systems), not reimplemented here. Propagates a spacecraft's
// position/velocity under Newtonian two-body gravity
// (d^2r/dt^2 = -mu * r / |r|^3) by numerical integration — this is
// the same underlying physics Kepler's analytical orbit equations
// describe in closed form, but propagated numerically rather than
// solved analytically, which is what lets later work (perturbations,
// drag, thrust) add additional force terms to the same integrator
// rather than needing a fundamentally different approach. See
// docs/ADR/0016-scientific-computing-linalg.md for accuracy scope
// (fixed-step RK4, no adaptive step control, two-body only — no
// perturbations, no drag, no third-body effects yet).

namespace space {

// Earth's standard gravitational parameter (GM), in km^3/s^2 — a
// real, standard physical constant (IAU/WGS84-consistent value), not
// an invented placeholder.
inline constexpr double EARTH_MU_KM3_S2 = 398600.4418;

struct OrbitalState {
    sci::Vec3 position;  // km, in an inertial frame
    sci::Vec3 velocity;  // km/s
};

// Propagates `initial` forward by `durationSeconds` under pure
// two-body gravity with the given gravitational parameter `mu`
// (defaults to Earth's), using fixed-step RK4 with step size `dt`
// seconds (smaller = more accurate, more computation — see the tests
// for a demonstrated accuracy/step-size relationship).
OrbitalState propagateTwoBody(
    const OrbitalState& initial,
    double durationSeconds,
    double dt = 1.0,
    double mu = EARTH_MU_KM3_S2
);

// The period of a circular orbit at radius `radiusKm`, per Kepler's
// third law: T = 2*pi*sqrt(r^3 / mu). Used by the tests to verify the
// numerical propagator against this closed-form analytical result —
// the standard way to validate a numerical orbit propagator: check it
// against the cases where an exact answer is independently known.
double circularOrbitPeriodSeconds(double radiusKm, double mu = EARTH_MU_KM3_S2);

// Specific orbital energy: v^2/2 - mu/r. Constant along any
// unperturbed two-body orbit (a real physical invariant, the
// vis-viva relationship's energy form) — used by tests to check the
// propagator conserves it to within numerical-integration tolerance,
// the same kind of physical-invariant check
// 18-scientific-computing/tests/sci_test.cpp already applies to the
// harmonic oscillator's energy.
double specificOrbitalEnergy(const OrbitalState& state, double mu = EARTH_MU_KM3_S2);

}  // namespace space

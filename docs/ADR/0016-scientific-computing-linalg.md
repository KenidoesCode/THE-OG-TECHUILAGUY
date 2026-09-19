# ADR 0016: Scientific computing numerics + two-body orbital mechanics

**Status:** Accepted. Starts Layer 18 (Scientific Computing) and Layer
20 (Space Systems) together, deliberately, as a real dependency chain
rather than two isolated additions — the mission's own chain example
("orbital simulator → spacecraft → ...") begins exactly here.

## Context

Layer 20 (Space Systems) needs orbital mechanics before anything else
in that layer (a digital twin, flight software, ADCS) is meaningful.
Orbital mechanics needs vector algebra and a numerical ODE integrator
first — building those as Layer 18 (Scientific Computing) primitives,
then building Layer 20's propagator *on top of* them rather than
duplicating vector math and an integrator inside the space-systems
code, is the genuine cross-layer dependency the mission calls for.

## Decision

`18-scientific-computing/linalg/vec3.hpp` is minimal 3D vector algebra
(add/subtract/scale/dot/cross/norm/normalize) — deliberately not a
general linear-algebra library (no arbitrary-size matrices, no linear
solvers) since nothing built so far needs more than 3-vectors.
`18-scientific-computing/ode/rk4.hpp` is a generic, textbook classical
4th-order Runge-Kutta integrator over a fixed-size `std::array<double, N>`
state — the standard method (not an invented shortcut), with the
usual O(h^4) global error characterization, verified against two
problems with known exact solutions (exponential decay, a simple
harmonic oscillator) rather than merely "it runs."

`20-space-systems/orbital/two_body.cpp` propagates a 6-element state
(position + velocity) under Newtonian two-body gravity
(`d²r/dt² = -mu·r/|r|³`) by calling `sci::rk4Integrate<6>` directly —
the real dependency chain. `EARTH_MU_KM3_S2` is Earth's actual standard
gravitational parameter (a real physical constant, not a placeholder).
`circularOrbitPeriodSeconds` implements Kepler's third law in closed
form, used both as a standalone utility and as the independent
ground truth the numerical propagator is checked against.

### A real accuracy bug caught by its own test, not shipped silently

The first version of `propagateTwoBody` computed its step count as
`duration / dt` truncated to an integer, discarding whatever fraction
of a step didn't divide evenly — for an orbital period (almost never
an exact multiple of the chosen step size), this silently left up to
one full step's worth of time unintegrated. At orbital velocities
(several km/s for a LEO orbit), that translated into several
kilometers of spurious position error — caught immediately by
`testCircularOrbitReturnsToStartAfterOnePeriod` failing a check that a
circular orbit returns to within 1 km of its start after exactly one
period. Fixed by taking a final partial-duration RK4 step covering
exactly the leftover remainder time, so `propagateTwoBody(state,
duration, ...)` now genuinely integrates for `duration` seconds, not
`floor(duration/dt)*dt`.

## What this is not

- **Not a general linear-algebra library.** No matrices, no linear
  system solvers, no eigenvalue computation — `Vec3` only.
- **Not an adaptive-step or implicit ODE solver.** Fixed step size
  only; a stiff system or one needing very different accuracy at
  different points in the trajectory would need a different method.
- **Two-body only.** No perturbations (J2 oblateness, third-body
  effects, solar radiation pressure), no atmospheric drag, no thrust
  — all explicitly named later Space Systems requirements, none
  implemented yet. This propagator is the foundation those would add
  additional force terms to inside the same `d^2r/dt^2 = ...` right-
  hand side, not a replacement for that future work.
- **No coordinate-frame transforms, no orbital-element conversions**
  (Cartesian state ↔ classical orbital elements) — state is always
  Cartesian position/velocity in a single inertial frame.
- **No spacecraft model, flight computer, ADCS, EPS, thermal,
  communications, telemetry/telecommand, digital twin, or mission
  simulator** — this ADR is strictly the orbital-mechanics
  foundation those would be built on.

## Tested invariants

`18-scientific-computing/tests/sci_test.cpp` (9 hosted assertions):
`Vec3` addition/dot/cross against hand-computed known values, norm of
a 3-4-5 triangle, safe zero-vector normalization; RK4 against the
exact exponential-decay and harmonic-oscillator solutions; and energy
drift over 10 oscillator periods staying under 1%.

`20-space-systems/tests/orbital_test.cpp` (7 hosted assertions):
`circularOrbitPeriodSeconds` matches the real, independently-known
geostationary period (~86164 s at ~42164 km) to within 0.1%; a
circular LEO orbit returns within 1 km/1 m·s⁻¹ of its start after
exactly one Kepler period; a circular orbit's radius never deviates by
more than 1 km at any of 20 checkpoints across a full period; specific
orbital energy is conserved to within 0.01% for a genuinely eccentric
(non-circular, confirmed by its radius actually varying) elliptical
orbit; and a finer integration step measurably reduces position error
versus a coarser one, confirming RK4's accuracy characterization holds
in practice for this specific propagator.

## Consequences

Every claim about scientific computing or orbital mechanics elsewhere
in this repository must describe these layers using the scope
recorded here: Vec3 + RK4 (not a general numerics library) and
two-body Keplerian propagation only (not perturbations, drag, a
spacecraft model, or a mission simulator). This ADR is the single
source of truth for that distinction until a future ADR (covering
perturbations, a spacecraft model, or additional numerical methods)
extends it.

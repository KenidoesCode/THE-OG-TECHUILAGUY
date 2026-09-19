# Layer 18 — Scientific Computing

**Status: FOUNDATION.** Minimal 3D vector algebra and a real, verified
RK4 ODE integrator — directly used by Layer 20's orbital mechanics
(`20-space-systems/orbital/`), a genuine cross-layer dependency. See
[`docs/ADR/0016-scientific-computing-linalg.md`](../docs/ADR/0016-scientific-computing-linalg.md).

## Implemented and tested

- `linalg/vec3.hpp`: 3D vector algebra (add/subtract/scale/dot/cross/
  norm/normalize), safe against dividing by zero when normalizing the
  zero vector.
- `ode/rk4.hpp`: a generic, textbook classical 4th-order Runge-Kutta
  integrator over a fixed-size state vector, verified against two
  problems with known exact analytical solutions.
- 9 hosted unit assertions (`tests/sci_test.cpp`/`sci_test.sh`): Vec3
  algebra against hand-computed values; RK4 against exact exponential-
  decay and harmonic-oscillator solutions; energy-drift bound over 10
  oscillator periods.

## Not yet implemented

- a general linear-algebra library (matrices, linear solvers,
  eigenvalues) — `Vec3` only
- adaptive-step or implicit ODE methods (fixed step size only)
- optimization, statistics, PDE tools, parallel computation,
  visualization, reproducibility infrastructure (all named in the PRD,
  none started)

## Building and testing

```sh
bash tests/sci_test.sh   # hosted tests, no dependencies
```

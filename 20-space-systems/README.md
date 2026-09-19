# Layer 20 — Space Systems

**Status: FOUNDATION.** Real two-body orbital propagation, built
directly on Layer 18's vector algebra and RK4 integrator — a genuine
dependency chain. No spacecraft model, flight software, ADCS, EPS,
thermal, communications, telemetry/telecommand, digital twin, or
mission simulator yet. See
[`docs/ADR/0016-scientific-computing-linalg.md`](../docs/ADR/0016-scientific-computing-linalg.md).

## Implemented and tested

- `orbital/two_body.hpp`/`.cpp`: propagates a spacecraft's position/
  velocity under Newtonian two-body gravity by numerical integration
  (calling `18-scientific-computing`'s RK4 directly), plus
  `circularOrbitPeriodSeconds` (Kepler's third law, closed form) and
  `specificOrbitalEnergy` (a real conserved physical quantity used to
  validate the propagator).
- 7 hosted unit assertions (`tests/orbital_test.cpp`/`orbital_test.sh`):
  the period formula matches the real, independently-known
  geostationary period; a circular orbit returns to within 1 km/1 m·s⁻¹
  of its start after exactly one period and never deviates by more
  than 1 km at any of 20 checkpoints along the way; specific orbital
  energy is conserved to within 0.01% for a genuinely eccentric orbit;
  and a finer integration step measurably reduces error versus a
  coarser one.

## Not yet implemented

- perturbations (J2 oblateness, third-body effects, solar radiation
  pressure), atmospheric drag, thrust — the propagator's two-body-only
  right-hand side is the foundation these would extend, not built yet
- coordinate-frame transforms, Cartesian ↔ classical orbital element
  conversions
- a spacecraft model, flight computer/flight software, ADCS, EPS,
  thermal, structures, communications
- telemetry, telecommand, ground station simulation, mission control
- a digital twin or full mission simulator
- hardware-in-the-loop
- PQC-authenticated telemetry/telecommand integration (Layer 10/20
  connection named in the mission, not built)

## Building and testing

```sh
bash tests/orbital_test.sh   # hosted tests, real dependency on 18-scientific-computing
```

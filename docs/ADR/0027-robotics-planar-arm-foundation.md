# ADR 0027: Robotics — planar-arm kinematics + proportional joint control foundation

**Status:** Accepted. This is the FOUNDATION vertical slice of PRD
Layer 16 (Robotics & autonomy). It builds real forward kinematics for
a 2-link planar robot arm and a real (if minimal) proportional joint
controller, simulated over discrete time steps and verified against a
closed-form analytic solution — not merely "some numbers changed."

## Context

Robotics had no code anywhere in the repository. Per the project's own
rule, the smallest real implementation that establishes the actual
architecture is: a real kinematic model (joint angles → end-effector
position), and a real controller that can move a joint toward a
target — the two building blocks essentially everything else in
robotics (inverse kinematics, trajectory planning, sensors, SLAM)
builds on top of. Both are built directly on
`18-scientific-computing/linalg/vec3.hpp`'s existing `Vec3` — a
genuine cross-layer integration, not a parallel vector type.

## Forward kinematics

`16-robotics/kinematics/planar_arm.hpp/cpp`: `PlanarArm2Link { link1Length,
link2Length, joint1Angle, joint2Angle }` (angles in radians, measured
from the positive X axis, standard planar-manipulator convention).
`forwardKinematics(arm)` computes the real 2D trigonometric chain (the
first joint's position, then the end-effector position relative to
it), returning a `sci::Vec3` with `z = 0` (the arm is planar):

```
joint1Position = (L1·cos(θ1), L1·sin(θ1), 0)
endEffector    = joint1Position + (L2·cos(θ1+θ2), L2·sin(θ1+θ2), 0)
```

This is the standard closed-form 2-link planar forward-kinematics
equation — not an approximation, not an iterative solver (that's what
*inverse* kinematics would need, and isn't attempted here).

## Proportional joint control

`16-robotics/control/joint_controller.hpp/cpp`: `PController { gain }`.
`step(controller, currentAngle, targetAngle, dt)` returns the next
angle via the standard discrete-time proportional-control update:

```
error = targetAngle - currentAngle
nextAngle = currentAngle + gain * error * dt
```

This is a real, if simple, linear first-order control law — not a PID
controller (no integral or derivative term), and not guaranteed stable
for an arbitrary `gain * dt` (a caller must keep `gain * dt < 1` for
this discrete update to converge rather than oscillate/diverge, the
same real numerical-stability constraint any explicit first-order
integrator has — see `18-scientific-computing`'s own RK4 vs. explicit-
Euler distinction for the analogous numerical-methods concern one
layer over).

## Determinism and verification

Because the proportional-control update above is linear, the error
after `n` steps of fixed `dt` has an exact closed form:
`error(n) = error(0) · (1 - gain·dt)^n`. The test suite verifies the
simulated controller's error against this closed-form prediction
directly — an independent analytic check of the simulation loop,
the same "don't just trust the implementation, verify against a
second, independently-derived source of truth" discipline this
project already applies to autodiff (ADR 0024's numerical-gradient
cross-check) and orbital mechanics (`20-space-systems`'s energy-
conservation check).

## What this does not support

- **No inverse kinematics.** Nothing here solves "what joint angles
  reach this target end-effector position" — only the forward
  direction (angles → position) exists.
- **No 3D kinematics, no arbitrary link count, no revolute/prismatic
  joint mix.** Exactly one kinematic chain shape: two revolute joints,
  planar (Z always 0).
- **No dynamics** (mass, inertia, torque, gravity) — this is a pure
  kinematic + control-signal model; nothing here simulates *forces*,
  only *positions/angles*.
- **No sensors** (encoders, IMUs, cameras) and no sensor noise/
  estimation (no Kalman filter, no SLAM).
- **No PID** — proportional-only control, no integral or derivative
  term, and no anti-windup or stability analysis beyond the documented
  `gain·dt < 1` constraint.
- **No collision detection, no path/motion planning, no multi-arm/
  swarm coordination.**
- **No visualization/integration with the Graphics or ECS layers yet**
  — an obvious next step (a robot arm is a natural `ecs::MeshComponent`
  hierarchy driven by this kinematics), not attempted in this slice.

## Tested invariants

`16-robotics/tests/robotics_test.cpp`: forward kinematics at
zero-angle configuration places the end-effector at exactly
`(L1+L2, 0, 0)`; a 90-degree first joint with a straight second joint
places it at `(0, L1+L2, 0)`; a general two-angle configuration matches
independently hand-computed trigonometry; different link lengths scale
the result correctly; a single proportional-control step matches the
hand-derived update exactly; the controller's error after `n` fixed-
`dt` steps matches the independently-derived closed-form
`error(0)·(1-gain·dt)^n` prediction (not just "got closer to target");
a full simulated control loop (repeatedly computing forward kinematics
from a joint angle being driven toward a target by the controller)
converges the end-effector's simulated position to within a documented
tolerance of the true target configuration's forward-kinematics
position; and the entire simulation is deterministic (identical inputs
produce identical trajectories across repeated runs).

## Consequences

THE OG TECHUILAGUY now has a real, tested robotics foundation: forward
kinematics for a genuine (if simple) kinematic chain, and a real
closed-loop control simulation verified against an independent
analytic solution — a real "robot model → controller → simulated
movement" loop, at exactly this scope. The next real gaps, in roughly
increasing order: inverse kinematics for the same 2-link arm (a
natural, bounded next slice), then a visualization integration with
the ECS/Graphics layer (ADR 0026), then dynamics/sensors/SLAM — none
of which exist yet.

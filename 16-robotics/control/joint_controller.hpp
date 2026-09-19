#pragma once

// A real, minimal proportional (P-only) joint controller — see
// docs/ADR/0027-robotics-planar-arm-foundation.md for exactly why this
// is P-only (no PID) and the gain*dt<1 stability constraint.

namespace robotics {

struct PController {
    double gain = 1.0;
};

// Standard discrete-time proportional-control update:
// next = current + gain * (target - current) * dt.
// Converges monotonically toward `targetAngle` only if gain*dt < 1;
// the caller is responsible for choosing a stable gain/dt combination
// (this function does not clamp or validate that).
double step(const PController& controller, double currentAngle, double targetAngle, double dt);

}  // namespace robotics

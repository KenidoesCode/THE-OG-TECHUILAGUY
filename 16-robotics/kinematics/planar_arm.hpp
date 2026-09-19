#pragma once

#include "../../18-scientific-computing/linalg/vec3.hpp"

// Forward kinematics for a 2-link planar (Z=0) robot arm — the
// standard closed-form equation, not an iterative solver. See
// docs/ADR/0027-robotics-planar-arm-foundation.md.

namespace robotics {

struct PlanarArm2Link {
    double link1Length = 1.0;
    double link2Length = 1.0;
    double joint1Angle = 0.0;  // radians, from the positive X axis
    double joint2Angle = 0.0;  // radians, relative to link 1's direction
};

// Real trigonometric forward kinematics: joint1's position, then the
// end-effector position relative to it. Always returns z=0 (the arm
// is planar).
sci::Vec3 forwardKinematics(const PlanarArm2Link& arm);

// The intermediate joint1 position alone (useful for visualization —
// not currently consumed by anything, but a real, independently
// meaningful quantity, not an internal-only helper).
sci::Vec3 joint1Position(const PlanarArm2Link& arm);

}  // namespace robotics

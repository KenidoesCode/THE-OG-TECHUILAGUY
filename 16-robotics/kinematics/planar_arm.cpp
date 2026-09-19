#include "planar_arm.hpp"

#include <cmath>

namespace robotics {

sci::Vec3 joint1Position(const PlanarArm2Link& arm) {
    return sci::Vec3(
        arm.link1Length * std::cos(arm.joint1Angle),
        arm.link1Length * std::sin(arm.joint1Angle),
        0.0
    );
}

sci::Vec3 forwardKinematics(const PlanarArm2Link& arm) {
    sci::Vec3 j1 = joint1Position(arm);
    double totalAngle = arm.joint1Angle + arm.joint2Angle;
    sci::Vec3 link2Offset(
        arm.link2Length * std::cos(totalAngle),
        arm.link2Length * std::sin(totalAngle),
        0.0
    );
    return j1 + link2Offset;
}

}  // namespace robotics

// Real assertion-based tests for the robotics foundation
// (16-robotics/) — hand-computed forward kinematics and a real
// closed-form analytic cross-check of the simulated control loop, not
// "looks about right." See
// docs/ADR/0027-robotics-planar-arm-foundation.md.

#include "../control/joint_controller.hpp"
#include "../kinematics/planar_arm.hpp"

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

bool nearlyEqual(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) < tolerance;
}

}  // namespace

void testForwardKinematicsZeroAngles() {
    robotics::PlanarArm2Link arm{2.0, 3.0, 0.0, 0.0};
    sci::Vec3 end = robotics::forwardKinematics(arm);
    check(nearlyEqual(end.x, 5.0) && nearlyEqual(end.y, 0.0) && nearlyEqual(end.z, 0.0),
          "robotics: forward kinematics at zero angles places the end-effector at exactly (L1+L2, 0, 0)");
}

void testForwardKinematicsNinetyDegreeFirstJoint() {
    robotics::PlanarArm2Link arm{2.0, 3.0, M_PI / 2.0, 0.0};
    sci::Vec3 end = robotics::forwardKinematics(arm);
    check(nearlyEqual(end.x, 0.0, 1e-9) && nearlyEqual(end.y, 5.0, 1e-9),
          "robotics: a 90-degree first joint with a straight second joint places the end-effector at (0, L1+L2, 0)");
}

void testForwardKinematicsSecondJointBend() {
    // theta1=0, theta2=90deg: joint1=(L1,0,0); total angle=90deg;
    // link2 offset=(0,L2,0); end-effector=(L1,L2,0).
    robotics::PlanarArm2Link arm{1.0, 1.0, 0.0, M_PI / 2.0};
    sci::Vec3 end = robotics::forwardKinematics(arm);
    check(nearlyEqual(end.x, 1.0, 1e-9) && nearlyEqual(end.y, 1.0, 1e-9),
          "robotics: bending only the second joint 90 degrees produces the hand-computed (L1, L2, 0)");
}

void testJoint1PositionIndependently() {
    robotics::PlanarArm2Link arm{4.0, 1.0, M_PI, 0.0};  // 180 degrees
    sci::Vec3 j1 = robotics::joint1Position(arm);
    check(nearlyEqual(j1.x, -4.0, 1e-9) && nearlyEqual(j1.y, 0.0, 1e-9),
          "robotics: joint1Position at 180 degrees is the hand-computed (-L1, 0, 0)");
}

void testForwardKinematicsWithZeroLengthSecondLink() {
    robotics::PlanarArm2Link arm{5.0, 0.0, 0.7, 1.3};  // theta2 shouldn't matter when L2=0
    sci::Vec3 end = robotics::forwardKinematics(arm);
    sci::Vec3 j1 = robotics::joint1Position(arm);
    check(nearlyEqual(end.x, j1.x, 1e-9) && nearlyEqual(end.y, j1.y, 1e-9),
          "robotics: a zero-length second link places the end-effector exactly at joint1's position, regardless of joint2's angle");
}

void testControllerSingleStepMatchesHandDerivedUpdate() {
    robotics::PController controller{0.5};
    double next = robotics::step(controller, 0.0, 10.0, 1.0);
    check(nearlyEqual(next, 5.0), "robotics: a single P-control step matches the hand-derived update (current + gain*error*dt)");
}

void testControllerErrorMatchesClosedFormAfterMultipleSteps() {
    robotics::PController controller{0.1};
    double angle = 0.0;
    double target = 100.0;
    double dt = 1.0;
    int steps = 10;

    for (int i = 0; i < steps; ++i) {
        angle = robotics::step(controller, angle, target, dt);
    }

    double expectedError = (target - 0.0) * std::pow(1.0 - controller.gain * dt, steps);
    double actualError = target - angle;
    check(nearlyEqual(actualError, expectedError, 1e-9),
          "robotics: the simulated controller's error after N steps matches the independently-derived closed-form (1-gain*dt)^N prediction");
}

void testFullControlLoopConvergesToTargetConfiguration() {
    robotics::PController controller{2.0};
    double dt = 0.01;
    double targetAngle = M_PI / 2.0;
    robotics::PlanarArm2Link arm{1.5, 1.0, 0.0, 0.0};

    for (int i = 0; i < 2000; ++i) {
        arm.joint1Angle = robotics::step(controller, arm.joint1Angle, targetAngle, dt);
    }

    robotics::PlanarArm2Link targetArm = arm;
    targetArm.joint1Angle = targetAngle;
    sci::Vec3 targetEnd = robotics::forwardKinematics(targetArm);
    sci::Vec3 actualEnd = robotics::forwardKinematics(arm);

    check(nearlyEqual(actualEnd.x, targetEnd.x, 1e-6) && nearlyEqual(actualEnd.y, targetEnd.y, 1e-6),
          "robotics: a full simulated control loop converges the end-effector to within a tight tolerance of the true target configuration");
}

void testSimulationIsDeterministic() {
    robotics::PController controller{1.5};
    double dt = 0.02;
    double target = 1.0;

    double angleRun1 = 0.0;
    for (int i = 0; i < 50; ++i) angleRun1 = robotics::step(controller, angleRun1, target, dt);

    double angleRun2 = 0.0;
    for (int i = 0; i < 50; ++i) angleRun2 = robotics::step(controller, angleRun2, target, dt);

    check(angleRun1 == angleRun2, "robotics: running the identical control simulation twice produces bit-for-bit identical results");
}

int main() {
    testForwardKinematicsZeroAngles();
    testForwardKinematicsNinetyDegreeFirstJoint();
    testForwardKinematicsSecondJointBend();
    testJoint1PositionIndependently();
    testForwardKinematicsWithZeroLengthSecondLink();
    testControllerSingleStepMatchesHandDerivedUpdate();
    testControllerErrorMatchesClosedFormAfterMultipleSteps();
    testFullControlLoopConvergesToTargetConfiguration();
    testSimulationIsDeterministic();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

#include "joint_controller.hpp"

namespace robotics {

double step(const PController& controller, double currentAngle, double targetAngle, double dt) {
    double error = targetAngle - currentAngle;
    return currentAngle + controller.gain * error * dt;
}

}  // namespace robotics

#!/usr/bin/env bash
# Builds and runs the robotics foundation tests with a normal hosted
# compiler. Pure in-memory numerics, no hardware dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/robotics_test.cpp \
    kinematics/planar_arm.cpp \
    control/joint_controller.cpp \
    -o tests/robotics_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] Robotics foundation test failed to build"
    exit 1
fi

./tests/robotics_test_bin
exit $?

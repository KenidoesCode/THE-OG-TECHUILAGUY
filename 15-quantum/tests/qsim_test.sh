#!/usr/bin/env bash
# Builds and runs the quantum state-vector simulator tests with a
# normal hosted compiler. Real integration with
# 11-verification/property_testing.hpp.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/qsim_test.cpp \
    simulator/qsim.cpp \
    -o tests/qsim_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] quantum simulator test failed to build"
    exit 1
fi

./tests/qsim_test_bin
exit $?

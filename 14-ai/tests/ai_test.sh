#!/usr/bin/env bash
# Builds and runs the AI Tensor + scalar-autodiff foundation tests
# with a normal hosted compiler. Pure in-memory numerics — no OS or
# hardware dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/ai_test.cpp \
    tensor/tensor.cpp \
    autodiff/value.cpp \
    training/linear_regression.cpp \
    -o tests/ai_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] AI tensor/autodiff test failed to build"
    exit 1
fi

./tests/ai_test_bin
exit $?

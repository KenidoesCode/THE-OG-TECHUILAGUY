#!/usr/bin/env bash
# Builds and runs Layer 18 (Scientific Computing) tests with a normal
# hosted compiler. Header-only linalg/ode libraries, no build deps.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/sci_test.cpp \
    -o tests/sci_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] scientific computing test failed to build"
    exit 1
fi

./tests/sci_test_bin
exit $?

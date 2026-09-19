#!/usr/bin/env bash
# Builds and runs Layer 20's two-body orbital propagation tests with a
# normal hosted compiler. Real dependency on 18-scientific-computing's
# Vec3/RK4 (header-only, no separate build needed for those).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/orbital_test.cpp \
    orbital/two_body.cpp \
    -o tests/orbital_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] orbital mechanics test failed to build"
    exit 1
fi

./tests/orbital_test_bin
exit $?

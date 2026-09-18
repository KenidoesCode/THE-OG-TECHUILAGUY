#!/usr/bin/env bash
# Builds and runs the SHA-256 known-answer tests with a normal hosted
# compiler. The implementation has no hardware/OS dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/sha256_test.cpp \
    hashing/sha256.cpp \
    -o tests/sha256_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] SHA-256 test failed to build"
    exit 1
fi

./tests/sha256_test_bin
exit $?

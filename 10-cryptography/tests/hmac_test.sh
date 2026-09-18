#!/usr/bin/env bash
# Builds and runs the HMAC-SHA256 known-answer tests with a normal
# hosted compiler. No hardware/OS dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/hmac_test.cpp \
    mac/hmac_sha256.cpp \
    hashing/sha256.cpp \
    -o tests/hmac_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] HMAC test failed to build"
    exit 1
fi

./tests/hmac_test_bin
exit $?

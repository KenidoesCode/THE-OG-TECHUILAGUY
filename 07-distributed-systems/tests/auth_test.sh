#!/usr/bin/env bash
# Builds and runs the authenticated-RPC-envelope tests with a normal
# hosted compiler. Real integration with 10-cryptography's HMAC-SHA256.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/auth_test.cpp \
    rpc/auth.cpp \
    rpc/rpc.cpp \
    rpc/serialization.cpp \
    ../10-cryptography/mac/hmac_sha256.cpp \
    ../10-cryptography/hashing/sha256.cpp \
    -o tests/auth_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] auth test failed to build"
    exit 1
fi

./tests/auth_test_bin
exit $?

#!/usr/bin/env bash
# Builds and runs the RPC + deterministic network simulation tests
# with a normal hosted compiler. No hardware/OS dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/rpc_test.cpp \
    rpc/rpc.cpp \
    rpc/serialization.cpp \
    -o tests/rpc_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] RPC test failed to build"
    exit 1
fi

./tests/rpc_test_bin
exit $?

#!/usr/bin/env bash
# Builds and runs the Raft leader-election tests with a normal hosted
# compiler. Fully deterministic (seeded), no hardware dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/raft_test.cpp \
    raft/raft.cpp \
    rpc/rpc.cpp \
    rpc/serialization.cpp \
    -o tests/raft_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] Raft test failed to build"
    exit 1
fi

./tests/raft_test_bin
exit $?

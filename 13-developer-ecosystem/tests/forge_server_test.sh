#!/usr/bin/env bash
# Builds and runs OGForge's server-foundation tests with a normal
# hosted compiler. Real disk I/O against real OGGit repositories in a
# temporary directory, real SHA-256, real serialization — not mocks.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/forge_server_test.cpp \
    forge/forge_server.cpp \
    oggit/object_store.cpp \
    oggit/refs.cpp \
    ../10-cryptography/hashing/sha256.cpp \
    ../07-distributed-systems/rpc/serialization.cpp \
    -o tests/forge_server_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] OGForge server test failed to build"
    exit 1
fi

./tests/forge_server_test_bin
exit $?

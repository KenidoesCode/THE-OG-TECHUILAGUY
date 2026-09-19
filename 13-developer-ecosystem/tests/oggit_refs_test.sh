#!/usr/bin/env bash
# Builds and runs OGGit refs/HEAD/history tests with a normal hosted
# compiler. Real disk I/O against a temporary directory.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/oggit_refs_test.cpp \
    oggit/refs.cpp \
    oggit/object_store.cpp \
    ../10-cryptography/hashing/sha256.cpp \
    -o tests/oggit_refs_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] OGGit refs test failed to build"
    exit 1
fi

./tests/oggit_refs_test_bin
exit $?

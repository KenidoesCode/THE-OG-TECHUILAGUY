#!/usr/bin/env bash
# Builds and runs OGGit's diff tests with a normal hosted compiler.
# Real disk I/O against a temporary directory, real ObjectStore/Index/
# Tree machinery, and real SHA-256 from 10-cryptography/ (not a mock).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/oggit_diff_test.cpp \
    oggit/diff.cpp \
    oggit/index.cpp \
    oggit/object_store.cpp \
    ../10-cryptography/hashing/sha256.cpp \
    -o tests/oggit_diff_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] OGGit diff test failed to build"
    exit 1
fi

./tests/oggit_diff_test_bin
exit $?

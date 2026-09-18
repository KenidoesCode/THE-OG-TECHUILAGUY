#!/usr/bin/env bash
# Builds and runs OGGit's object store tests with a normal hosted
# compiler. Real disk I/O against a temporary directory, real SHA-256
# from 10-cryptography/ (not a mock).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/oggit_object_store_test.cpp \
    oggit/object_store.cpp \
    ../10-cryptography/hashing/sha256.cpp \
    -o tests/oggit_object_store_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] OGGit object store test failed to build"
    exit 1
fi

./tests/oggit_object_store_test_bin
exit $?

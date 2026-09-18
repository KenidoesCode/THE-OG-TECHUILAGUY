#!/usr/bin/env bash
# Builds and runs Layer 8 (Storage) tests with a normal hosted
# compiler. Real file I/O against a temporary directory, real
# integration with 07-distributed-systems' serialization codec.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/storage_test.cpp \
    kv/kv_store.cpp \
    wal/wal.cpp \
    wal/crc32.cpp \
    ../07-distributed-systems/rpc/serialization.cpp \
    -o tests/storage_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] storage test failed to build"
    exit 1
fi

./tests/storage_test_bin
exit $?

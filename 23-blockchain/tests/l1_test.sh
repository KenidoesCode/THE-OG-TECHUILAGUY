#!/usr/bin/env bash
# Builds and runs the Techuilaguy L1 blockchain tests with a normal
# hosted compiler. Real SHA-256, real serialization, and real
# WAL-backed KV persistence against a temporary file (not mocks).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/l1_test.cpp \
    l1/types.cpp \
    l1/transaction.cpp \
    l1/ledger.cpp \
    l1/block.cpp \
    l1/chain.cpp \
    ../10-cryptography/hashing/sha256.cpp \
    ../07-distributed-systems/rpc/serialization.cpp \
    ../08-storage/kv/kv_store.cpp \
    ../08-storage/wal/wal.cpp \
    ../08-storage/wal/crc32.cpp \
    -o tests/l1_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] Techuilaguy L1 test failed to build"
    exit 1
fi

./tests/l1_test_bin
exit $?

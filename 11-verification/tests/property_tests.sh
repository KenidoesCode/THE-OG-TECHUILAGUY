#!/usr/bin/env bash
# Builds and runs the property-based tests applying
# 11-verification/property_testing.hpp to real existing subsystems
# across the repository (networking, distributed systems, storage,
# cryptography, and the OS's ELF loader). Hosted, no hardware needed.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/property_tests.cpp \
    ../06-networking/protocols/protocols.cpp \
    ../07-distributed-systems/rpc/serialization.cpp \
    ../08-storage/wal/wal.cpp \
    ../08-storage/wal/crc32.cpp \
    ../10-cryptography/mac/hmac_sha256.cpp \
    ../10-cryptography/hashing/sha256.cpp \
    ../22-os/elf/elf.cpp \
    -o tests/property_tests_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] property tests failed to build"
    exit 1
fi

./tests/property_tests_bin
exit $?

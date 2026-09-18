#!/usr/bin/env bash
# Builds and runs the ELF32/i386 validation-and-planning unit tests
# with a normal hosted compiler. elf.cpp's actual logic (header/
# program-header parsing, bounds/overflow checks, segment-overlap and
# entry-point validation) has no hardware dependency and doesn't need
# the freestanding cross-compile toolchain or a boot cycle to verify.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR/.."

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/elf_test.cpp \
    elf/elf.cpp \
    -o tests/elf_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] elf test failed to build"
    exit 1
fi

./tests/elf_test_bin
exit $?

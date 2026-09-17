#!/usr/bin/env bash
# Builds and runs the keyboard scancode-translation unit test with a
# normal hosted compiler — this logic has no hardware I/O, so unlike
# the rest of the OS it doesn't need the freestanding cross-compile
# toolchain, an emulator, or a boot cycle to verify.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR/.."

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/keyboard_translation_test.cpp \
    drivers/keyboard_translation.cpp \
    -o tests/keyboard_translation_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] keyboard translation test failed to build"
    exit 1
fi

./tests/keyboard_translation_test_bin
exit $?

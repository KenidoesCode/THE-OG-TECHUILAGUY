#!/usr/bin/env bash
# Builds and runs the OGLang frontend/middle-end unit tests.
# Discovered and executed automatically by og-build.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR/.."

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/unit_tests.cpp \
    lexer/lexer.cpp \
    parser/parser.cpp \
    types/type_checker.cpp \
    ir/lower.cpp \
    analysis/liveness.cpp \
    analysis/interference.cpp \
    codegen/register_allocator.cpp \
    codegen/x86_64.cpp \
    -I. \
    -o tests/unit_tests_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] unit test suite failed to build"
    exit 1
fi

./tests/unit_tests_bin
exit $?

#!/usr/bin/env bash
# Builds and runs the kernel heap allocator unit tests with a normal
# hosted compiler. heap.cpp's actual allocation logic (block
# splitting, coalescing, first-fit search) has no hardware dependency,
# but heap.cpp does depend on memory_alloc_page()/memory_free_page(),
# whose real implementation (memory/memory.cpp) hands back raw
# physical addresses that are only dereferenceable inside the kernel's
# own identity-mapped address space — so this test provides its own
# fake page allocator backed by real host heap memory (see
# tests/heap_test.cpp) rather than linking the real one.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR/.."

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/heap_test.cpp \
    heap/heap.cpp \
    -o tests/heap_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] heap test failed to build"
    exit 1
fi

./tests/heap_test_bin
exit $?

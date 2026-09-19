#!/usr/bin/env bash
# Builds and runs the deterministic software-rasterizer tests with a
# normal hosted compiler. Pure in-memory numerics/pixel buffers — no
# GPU, window system, or OS dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/graphics_test.cpp \
    math/mat4.cpp \
    render/framebuffer.cpp \
    render/rasterizer.cpp \
    scene/scene.cpp \
    -o tests/graphics_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] Graphics rasterizer test failed to build"
    exit 1
fi

./tests/graphics_test_bin
exit $?

#!/usr/bin/env bash
# Builds and runs the ECS / game-engine foundation tests with a normal
# hosted compiler. Real integration with the graphics rasterizer/scene
# renderer (ADR 0025) — no GPU/window dependency.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/ecs_test.cpp \
    ecs/systems.cpp \
    math/mat4.cpp \
    render/framebuffer.cpp \
    render/rasterizer.cpp \
    scene/scene.cpp \
    -o tests/ecs_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] ECS/game-engine test failed to build"
    exit 1
fi

./tests/ecs_test_bin
exit $?

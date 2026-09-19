#!/usr/bin/env bash
# Builds and runs Techuilaguy NetLab's simulation engine tests with a
# normal hosted compiler. Real Ethernet frame codecs from
# 06-networking/protocols/ (not a mock/reimplementation).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/netlab_test.cpp \
    netlab/packet.cpp \
    netlab/topology.cpp \
    ../06-networking/protocols/protocols.cpp \
    -o tests/netlab_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] Techuilaguy NetLab test failed to build"
    exit 1
fi

./tests/netlab_test_bin
exit $?

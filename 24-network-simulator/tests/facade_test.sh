#!/usr/bin/env bash
# Builds and runs NetLab's browser-foundation facade tests with a
# normal hosted compiler. Real Ethernet/ARP/IPv4/ICMP codecs from
# 06-networking/protocols/ (not a mock/reimplementation). Note: this
# is a NATIVE C++ test — no WebAssembly/browser toolchain is used or
# required (none is available in this environment; see
# docs/ADR/0030-netlab-browser-foundation-facade.md).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/facade_test.cpp \
    netlab/facade.cpp \
    netlab/packet_inspector.cpp \
    netlab/simulation_session.cpp \
    netlab/mission.cpp \
    netlab/ip.cpp \
    netlab/packet.cpp \
    netlab/topology.cpp \
    ../06-networking/protocols/protocols.cpp \
    -o tests/facade_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] NetLab facade test failed to build"
    exit 1
fi

./tests/facade_test_bin
exit $?

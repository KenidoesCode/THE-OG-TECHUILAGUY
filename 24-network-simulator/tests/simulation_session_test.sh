#!/usr/bin/env bash
# Builds and runs NetLab's timeline-session tests with a normal hosted
# compiler. Real Ethernet/ARP/IPv4/ICMP codecs from
# 06-networking/protocols/ (not a mock/reimplementation).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/simulation_session_test.cpp \
    netlab/simulation_session.cpp \
    netlab/mission.cpp \
    netlab/ip.cpp \
    netlab/packet.cpp \
    netlab/topology.cpp \
    ../06-networking/protocols/protocols.cpp \
    -o tests/simulation_session_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] NetLab simulation-session test failed to build"
    exit 1
fi

./tests/simulation_session_test_bin
exit $?

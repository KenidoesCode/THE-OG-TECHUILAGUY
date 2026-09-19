#!/usr/bin/env bash
# Builds and runs NetLab's IPv4/ARP/routing + first-mission tests with
# a normal hosted compiler. Real Ethernet/ARP/IPv4/ICMP codecs from
# 06-networking/protocols/ (not a mock/reimplementation).

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/ip_routing_test.cpp \
    netlab/mission.cpp \
    netlab/ip.cpp \
    netlab/packet.cpp \
    netlab/topology.cpp \
    ../06-networking/protocols/protocols.cpp \
    -o tests/ip_routing_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] NetLab IP/routing/mission test failed to build"
    exit 1
fi

./tests/ip_routing_test_bin
exit $?

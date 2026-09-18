#!/usr/bin/env bash
# Builds and runs the Layer 6 (Networking) protocol codec unit tests
# with a normal hosted compiler. This logic (Ethernet/ARP/IPv4/ICMP/UDP
# parsing, serialization, and checksums) has no hardware dependency —
# there is no NIC driver anywhere in this repository yet — so it's
# tested the same way 22-os/tests/elf_test.sh and heap_test.sh test
# their own hardware-independent logic.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

g++ -std=c++20 -Wall -Wextra -O2 \
    tests/protocols_test.cpp \
    protocols/protocols.cpp \
    -o tests/protocols_test_bin

if [ $? -ne 0 ]; then
    echo "[FAIL] networking protocol test failed to build"
    exit 1
fi

./tests/protocols_test_bin
exit $?

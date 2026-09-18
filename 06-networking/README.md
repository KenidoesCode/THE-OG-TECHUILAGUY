# Layer 6 — Networking

**Status: FOUNDATION.** Real, standards-based wire-format codecs for
Ethernet, ARP, IPv4, ICMP, and UDP — parsing, serialization, and
checksum computation/verification against the actual RFCs (826, 791,
792, 768). Not a network stack: there is no network interface card
driver anywhere in this repository yet, so nothing here transmits or
receives a real frame. See
[`docs/ADR/0005-networking-protocol-layer.md`](../docs/ADR/0005-networking-protocol-layer.md)
for the full design and exact scope.

## Implemented and tested

- `protocols/protocols.hpp`/`.cpp`: Ethernet header parse/serialize;
  ARP request/reply parse/serialize (Ethernet+IPv4 only); IPv4 header
  parse/serialize with RFC 1071 Internet checksum computation and
  verification (options unsupported — `IHL` must be exactly 5); ICMP
  echo request/reply parse/serialize with checksum verification; UDP
  header parse/serialize with the IPv4 pseudo-header checksum,
  including RFC 768's "checksum of zero means none computed" rule.
  Every parse function treats its input as untrusted: buffer lengths
  are checked before any field read, and a packet-claimed length is
  always validated against the real buffer size rather than trusted.
- 40 hosted unit assertions (`tests/protocols_test.cpp`/
  `protocols_test.sh`): a full round-trip (serialize → parse → field
  equality) for every protocol, plus every realistic rejection case
  (truncated buffers, undersized output buffers, wrong ARP hardware
  type/address lengths/operation code, wrong IPv4 version/IHL/
  totalLength, a corrupted checksum for IPv4/ICMP/UDP, UDP's
  pseudo-header genuinely participating in the checksum).

## Not yet implemented

- a network interface card driver (nothing here integrates with real
  hardware or QEMU's emulated NICs yet)
- TCP (a stateful protocol substantially larger than a header codec —
  deliberately deferred rather than attempted partially)
- IPv4 fragmentation/reassembly, options, or routing
- a sockets API or interface abstraction
- ARP cache / routing table
- TLS, QUIC, HTTP (FR-NET-2 — these sit atop TCP)
- the gamified network simulator (FR-NET-3)

## Building and testing

```sh
bash tests/protocols_test.sh   # hosted unit tests, no hardware/emulator needed
```

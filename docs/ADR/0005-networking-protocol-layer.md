# ADR 0005: Networking protocol codec layer (v1)

**Status:** Accepted. This is a foundation layer, not a network stack —
see "What this is not" below for exactly what remains before this
becomes an interoperable, wired-up network stack.

## Context

Layer 6 (Networking) of the PRD (FR-NET-1) called for Ethernet/ARP/
IPv4/ICMP/UDP/TCP with packet-level tests, fuzzing, and failure
injection. There was no networking code anywhere in the repository and
no network interface card driver in `22-os`. Building a full stack
(real driver, routing, sockets, TCP) with no hardware/driver
foundation first would produce exactly the kind of untestable,
unintegrated stub this project's own rules forbid. This ADR records
the layer built instead: the wire-format codecs every later piece
(a driver, a socket API, TCP itself) will need regardless, built and
proven correct in complete isolation from any hardware dependency.

## Decision

`06-networking/protocols/` implements real parsing, serialization, and
checksum computation/verification for Ethernet (IEEE 802.3), ARP
(RFC 826), IPv4 (RFC 791, header only), ICMP echo (RFC 792), and UDP
(RFC 768) — against the actual standards, not an invented replacement
protocol, per this project's own instruction to implement against real
standards where they exist.

Every function is pure arithmetic over a caller-supplied byte buffer:
no allocation, no OS dependency, no hardware access. This mirrors
`22-os/elf/elf.cpp` and `22-os/heap/heap.cpp`'s own separation of
"logic that can be exhaustively hosted-tested" from "integration that
needs real hardware/an emulator" — and for networking specifically,
there is no NIC driver to integrate with yet at all, so this layer is
necessarily the entire deliverable for now.

### Security posture

Every parse function treats its input as untrusted: buffer lengths are
checked before any field is read, a packet-claimed length (IPv4's
`totalLength`, UDP's `length`) is always validated against the real
buffer size rather than trusted on its own, and checksums are
independently verified rather than assumed. IPv4 rejects any header
claiming options (`IHL != 5`) outright rather than computing an offset
from an untrusted field and skipping past data this parser never
validates.

### The one's-complement two-zeros bug (and the fix)

`internetChecksum`'s carry-folding produces a 16-bit result that can
legitimately land on either `0x0000` or `0xFFFF` — the two
representations of zero inherent to one's-complement arithmetic (a
sum is only defined modulo `0xFFFF`, not `0x10000`). A first version of
the IPv4/ICMP/UDP checksum-verification code checked only for
`== 0x0000` and consequently rejected perfectly valid, correctly
checksummed packets — caught immediately by the hosted round-trip
test failing on a freshly serialized (not corrupted) header. Every
verification call site now accepts both representations.

## What this is not

- **Not a network stack.** No NIC driver exists anywhere in this
  repository — nothing here transmits or receives a real frame. This
  is the wire-format layer a driver would eventually sit underneath.
- **No TCP yet.** TCP's state machine (connection establishment,
  sequence/ack tracking, retransmission, congestion control) is a
  substantially larger undertaking than a stateless header codec and
  is explicitly deferred, not attempted partially.
- **No IPv4 options, fragmentation/reassembly, or routing.** `IHL != 5`
  is rejected outright; there is no logic anywhere here that
  fragments, reassembles, or routes a packet.
- **No sockets API, no interface abstraction, no ARP cache/routing
  table.** These are the next integration layer once a driver exists
  to actually receive traffic.
- **No TLS/QUIC/HTTP** (FR-NET-2) — those sit atop TCP, which doesn't
  exist here yet.

## Tested invariants

`06-networking/tests/protocols_test.cpp` (40 assertions, hosted,
`tests/protocols_test.sh`): for each protocol, a positive round-trip
(serialize then parse, confirming every field survives) plus every
realistic rejection case — truncated buffers for all five protocols;
Ethernet/ARP serialize functions refusing an undersized output buffer
rather than partially writing; ARP rejecting a non-Ethernet hardware
type, wrong address lengths, and an unrecognized operation code; IPv4
rejecting a wrong version, a nonzero-options IHL, a `totalLength`
exceeding the real buffer, and a corrupted (mismatched) checksum; ICMP
detecting a corrupted payload via checksum verification; UDP
confirming its pseudo-header genuinely participates in the checksum
(the identical datagram fails verification against a different
destination IP), accepting a zero checksum as "none computed" per
RFC 768, and rejecting a length field exceeding the real buffer.

## Consequences

Every claim about OGLang/Techuilaguy's networking capability elsewhere
in this repository must describe it using the scope recorded here: a
tested wire-format codec layer for Ethernet/ARP/IPv4/ICMP/UDP, not a
network stack, not TCP, not anything wired to real hardware. This ADR
is the single source of truth for that distinction until a future ADR
(covering a NIC driver, TCP, or sockets) supersedes or extends it.

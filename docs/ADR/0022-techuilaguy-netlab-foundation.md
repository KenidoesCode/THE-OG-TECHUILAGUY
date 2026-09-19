# ADR 0022: Techuilaguy NetLab — simulation engine foundation

**Status:** Accepted. This is the FOUNDATION vertical slice of a new
domain: an original (not Cisco-derived in implementation, UI,
terminology, or assets) educational network simulator, named
**Techuilaguy NetLab**. It establishes a real topology + Ethernet-
frame-delivery simulation engine with genuine (simplified) L2 switch
learning/flooding behavior. Everything past that — IP/ARP/DHCP/DNS
protocol behavior riding on top of these real frames, a visual
topology editor, missions/grading/XP — is explicitly future work.

## Context

The project's own `06-networking/` layer already has real, RFC-
verified wire-format codecs (Ethernet/ARP/IPv4/ICMP/UDP) but
deliberately stops at "parses/builds the bytes a real driver would
hand to hardware" — nothing transmits anything, because there is no
NIC driver. NetLab's job is different and complementary: not to drive
real hardware, but to **simulate** a network of nodes and links well
enough that those same real wire-format codecs can be exercised
end-to-end (a host builds a real Ethernet frame, a simulated switch
forwards it, another host parses it back) without needing any physical
or virtual NIC at all.

## Scope of this slice

1. A `Node` (`Host` or `Switch`) and an undirected `Link` between two
   nodes — a `Topology` is a graph of these.
2. Real Ethernet frames (`netlab::Packet`, built directly on
   `06-networking/protocols/protocols.hpp`'s `serializeEthernetHeader`/
   `parseEthernetHeader` — a genuine cross-layer integration, not a
   simulator-specific reimplementation of frame formats).
3. `Topology::sendFrame(originHostId, packet)`: a real (simplified) L2
   delivery simulation — a `Switch` node **learns** which neighbor a
   source MAC arrived from and, once learned, forwards a
   frame addressed to that MAC only to that one neighbor instead of
   flooding every port, exactly the real behavior a learning bridge
   provides (RFC 802.1D's substance, not its full spanning-tree
   machinery — see "What this does not support"). A `Host` node never
   forwards; it either accepts a frame addressed to its own MAC (or
   the broadcast MAC) or ignores it.
4. A minimal end-to-end demo: Host A → Switch → Host B, sending a real
   simulated frame and observing it delivered — the exact "create
   nodes → connect links → configure addresses → send packet → observe
   packet" slice named by the project's own network-simulator
   direction.

## Design

`Topology::sendFrame` is a real BFS over the graph starting at every
neighbor of the originating host, carrying `(arrivedFrom, current)` at
each step:

- At a **Host**, the frame is recorded as received if the frame's
  destination MAC matches that host's own MAC, or is the broadcast
  address (`FF:FF:FF:FF:FF:FF`) — hosts never forward further.
- At a **Switch**, the frame's source MAC is learned against
  `arrivedFrom` (the neighbor the frame just came from, from this
  switch's point of view) in a per-switch MAC table. The frame is then
  forwarded: to the single learned neighbor if the destination MAC has
  a table entry (and that entry isn't the same neighbor the frame just
  arrived from, a defensive fallback to flooding in that otherwise-odd
  case), or to every other neighbor (flooding) if the destination MAC
  is unknown or is the broadcast address.
- A `visited` node set prevents infinite loops. **This is a real
  simplification, not a broadcast-storm simulation**: a topology with
  a physical cycle (no spanning-tree protocol exists here) will simply
  have the *first* BFS arrival at any given node win, silently
  dropping the frame along whichever path arrives later — this
  simulator supports only cycle-free (tree-shaped) topologies
  correctly; see "What this does not support."

## What this does not support

- **No spanning-tree protocol.** Cyclic topologies are not correctly
  simulated (see above) — this is an explicit, tested limitation
  (`testCyclicTopologyDoesNotInfiniteLoop` proves the simulator
  terminates and doesn't crash, not that it produces "correct"
  multi-path delivery, which real STP would need to define anyway).
- **No IP/ARP/DHCP/DNS/TCP/UDP behavior yet.** This slice moves real
  raw Ethernet frames; nothing here parses or reacts to an ARP request,
  assigns an IP via DHCP, or resolves a name via DNS. Those are all
  future slices layered on top of this same frame-delivery engine,
  using `06-networking`'s existing ARP/IPv4/ICMP/UDP codecs the same
  way this slice already uses its Ethernet codec.
- **No routers/Layer 3 forwarding, no VLANs, no NAT, no firewalls.**
  Only `Host` and `Switch` (pure Layer 2) node kinds exist.
- **No latency, packet loss, or bandwidth simulation.** Delivery is
  instantaneous and lossless in this slice — `07-distributed-systems`'s
  existing `SimulatedNetwork` already has real seeded fault-injection
  (drop/duplicate/delay/partition) machinery that a future slice should
  integrate with rather than reimplement.
- **No visual topology editor, no packet capture UI, no CLI terminals
  per node, no missions/grading/XP/skill tree.** This ADR is the
  simulation engine only; everything visual or pedagogical is later,
  separate work.
- **No multiplayer, no persistence/save-load of a topology.**

## Tested invariants

`24-network-simulator/tests/netlab_test.cpp`: a real Ethernet frame
built via `netlab::Packet` round-trips through `parseEthernetHeader`
exactly; a minimal Host–Switch–Host topology delivers a frame from A to
B and NOT to an uninvolved third host on the same switch (proving
unicast delivery isn't accidentally broadcast); a switch with no prior
learning correctly floods an unknown-destination frame to every other
port; after one frame from A, the switch has learned A's port, so a
reply from B addressed to A is forwarded only to A's port and does NOT
reach a third host C also connected to the same switch (the real
learning behavior, not naive flooding every time); a broadcast-
destination frame is delivered to every host on the topology; a frame
addressed to a MAC nobody on the topology owns is delivered to nobody
(silently, not an error — real Ethernet has no "destination
unreachable" signal at Layer 2 either); and a topology containing a
cycle does not infinite-loop or crash `sendFrame` (see "What this does
not support" for exactly what guarantee that is and isn't).

## Consequences

THE OG TECHUILAGUY now has a real, tested, from-scratch (Cisco
Packet-Tracer-*inspired-category*, not derived) network simulation
engine capable of genuinely moving real Ethernet frames through a
learning-switch topology. The next real gaps, in roughly increasing
order: layering `06-networking`'s existing ARP codec on top (so hosts
can actually resolve MAC addresses instead of test code hardcoding
them), then IPv4/ICMP for host-to-host ping across the same engine,
then a visual topology representation, then the pedagogical layer
(missions, grading, XP) the project's education/gamification direction
calls for. None of those exist yet.

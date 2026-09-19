# ADR 0028: NetLab IPv4 + ARP + basic routing, packet timeline, first mission

**Status:** Accepted. NetLab is now a first-class product frontier
(per explicit project direction), not an isolated subsystem. This ADR
is the first complete educational vertical slice on top of ADR 0022's
L2 foundation: real IPv4 addressing, real ARP resolution, a basic
router doing real single-hop L3 forwarding, an inspectable packet
timeline, and the first gamified mission ("Connect Two Networks") —
all built by reusing `06-networking`'s existing protocol codecs, never
duplicating them.

## Context

ADR 0022 gave NetLab real Ethernet frames and a learning L2 switch,
explicitly scoped as "no IP/ARP/DHCP/DNS/TCP behavior yet." The
project's own next objective for NetLab names exactly that gap: a
student should be able to build `PC1 → Switch → Router → Switch → PC2`,
configure addresses, send a packet, and watch it move through real
Ethernet → ARP → IPv4 → routing → Ethernet → destination — with the
sequence of events inspectable afterward, not just a pass/fail
result.

## Architecture: simulation core independent of any UI

Everything in this ADR is a pure, synchronous, deterministic C++
library call (`netlab::sendIcmpEcho(topology, ...)` returns a fully-
computed `PingResult` — there is no event loop, no threading, no
timers, and no I/O). This is a deliberate architectural choice, not an
oversight: a core with no OS/UI dependency is what could eventually be
compiled to WebAssembly for a browser front end (not attempted in this
ADR — see "What this does not support"). The existing `Topology`/
`Packet` types from ADR 0022 are extended, not replaced, and every
frame this ADR constructs is a real frame built through
`06-networking/protocols/protocols.hpp`'s existing
`serializeArpPacket`/`serializeIpv4Header`/`serializeIcmpEcho` — no
protocol byte-layout logic is reimplemented here.

## Node model extensions

`Node` (`node.hpp`) gains: `ipAddress`/`subnetMask`/`defaultGatewayIp`
(meaningful for `Host` nodes) and `routerInterfaces` (meaningful for
the new `NodeKind::Router`) — each a `RouterInterface { neighborNodeId,
mac, ipAddress, subnetMask }`, one per link a router is configured on.
A router interface is identified by which neighbor (link) it faces,
matching how a real router's physical/logical interfaces correspond to
physical links.

## Router L2 behavior: a router is an L2 boundary

`Topology::sendFrame`'s per-node-kind handling gains a `Router` case:
unlike a `Switch` (which floods), a router **never forwards an L2
frame to another link** — it behaves like a `Host` at the L2 layer
(accepting a frame only if the destination MAC matches the specific
interface facing where the frame arrived, or is broadcast) and then
stops. This is the real, correct behavior: a router is a Layer-2
broadcast-domain boundary — an ARP broadcast on one side never reaches
devices on the other side, which is exactly why routing (not just
switching) is needed to cross between subnets. `DeliveryResult` gains
`receivedByRouterInterfaces` (which router, which interface index)
alongside the existing `receivedByHosts`.

`Topology::sendFrameFromInterface(routerId, egressNeighborId, packet)`
is the router-as-*originator* counterpart — used when a router itself
needs to transmit (an ARP request on the egress subnet, or the
forwarded IP packet): unlike `sendFrame` (which floods to *every*
neighbor of the origin, correct for a `Host` with one link), this
starts the delivery BFS from exactly one specified neighbor, so a
router forwarding onto subnet B never also re-transmits back onto
subnet A.

## ARP resolution

`netlab::resolveArp` (internal to `ip.cpp`) builds a real ARP request
(`net::ArpPacket{Request, ...}`, `serializeArpPacket`, wrapped in a
broadcast Ethernet frame via `buildEthernetFrame`) and delivers it via
`Topology::sendFrame`/`sendFrameFromInterface`. It inspects the
`DeliveryResult` for a host or router interface whose configured IP
matches the target, then sends a real unicast ARP reply back the same
way. Both frames are recorded as real `SimulationEvent`s with the
actual frame bytes attached — a caller (a future UI) can independently
re-parse either frame with the exact same `parseEthernetHeader`/
`parseArpPacket` functions `06-networking`'s own test suite uses.
Failure (no device on the reachable segment owns the target IP) is a
real, reported outcome (`PingResult::failureReason`), not a crash or a
silent wrong answer.

## IPv4 + ICMP echo simulation and single-hop routing

`netlab::sendIcmpEcho(topology, fromHostId, destIp)`:

1. Determines whether `destIp` is on the sender's own subnet
   (`destIp & subnetMask == ipAddress & subnetMask`); if not, the ARP
   target becomes the sender's configured `defaultGatewayIp` instead of
   `destIp` directly — the standard host routing decision.
2. ARP-resolves that next-hop IP's MAC (see above).
3. Builds a real ICMP echo request (`serializeIcmpEcho`) inside a real
   IPv4 header (`serializeIpv4Header`, protocol `IPV4_PROTO_ICMP`,
   correct `totalLength`) inside a real Ethernet frame addressed to the
   resolved next-hop MAC, and delivers it.
4. If it lands on a **router interface** (not the final destination
   host), the router performs real single-hop forwarding: it checks
   its *other* interfaces for one whose subnet contains `destIp`; if
   found, it ARP-resolves `destIp` on that egress interface, re-frames
   the *same* IP packet (the IP header/payload are untouched — only
   the Ethernet framing changes, which is exactly what real IP routing
   does) with the router's egress MAC as source, and delivers it via
   `sendFrameFromInterface`. If no interface's subnet contains
   `destIp`, this is a real, reported "no route" failure.
5. Every step (ARP broadcast, ARP reply, IP packet sent, router
   forwarding decision, final delivery or failure) is appended to
   `PingResult::timeline` as a `SimulationEvent { description,
   frameBytes }` — the actual packet timeline / step-through/inspection
   mechanism this ADR's objective calls for. Because every step is
   pure computation over the given `Topology` state, calling
   `sendIcmpEcho` again with unchanged topology state always reproduces
   the identical timeline — the "deterministic replay" property is a
   direct consequence of the architecture, verified directly by a test
   that runs the identical simulation twice and diffs the timelines.

## First mission: "Connect Two Networks"

`netlab::evaluateConnectTwoNetworksMission(topology, hostAId, hostBId)`
(`mission.hpp/cpp`): a real, data-driven objective/validation/failure/
completion evaluator, not a static pass/fail stub:

- **Objective**: "Configure two hosts on different subnets and a
  router between them so `hostA` can successfully ping `hostB`."
- **Validation**: both hosts must exist and have IPv4 configured, must
  be on genuinely different subnets (otherwise the mission's own point
  — routing — isn't being exercised), and a real `sendIcmpEcho` call
  from `hostA` to `hostB`'s address must actually succeed.
- **Failure states**: each real failure mode (missing IP config,
  hosts on the same subnet, ARP resolution failure, no route found at
  the router) is reported with its own specific, distinguishable
  reason string — a student debugging a broken mission attempt gets a
  real diagnostic, not a generic "failed."
- **Completion**: `MissionResult{success: true, reason: "..."}` only
  when the actual simulated ping delivers end to end.

## What this does not support

- **No browser UI, no WebAssembly build yet.** The core is designed to
  be WASM-compatible (no OS/threading/timer dependency) but is not
  actually compiled to WASM in this ADR — that remains explicit future
  work.
- **No multi-hop routing.** Exactly one router hop is modeled; a
  topology needing two or more routers to reach the destination is out
  of scope (the router's forwarding logic only checks its own directly-
  connected interfaces' subnets, not a learned/configured routing
  table with next-hop entries for indirect networks).
- **No DHCP, no DNS, no dynamic ARP-cache expiry/timing** — every ARP
  resolution in this slice re-broadcasts fresh each call; there is no
  persistent ARP cache carried between `sendIcmpEcho` calls.
- **No TCP/UDP application traffic** — only ICMP echo (ping) is
  modeled, since it's the smallest complete protocol that exercises
  the full ARP→IP→routing→IP→ARP chain end to end.
- **No packet loss/latency/bandwidth simulation** — inherited directly
  from ADR 0022's own scope; delivery here is still instantaneous and
  lossless.
- **No visual topology editor, no interactive step/pause/rewind UI.**
  `PingResult::timeline` is the data a future UI would step through;
  no UI exists yet.
- **No additional missions beyond "Connect Two Networks."**

## Tested invariants

`24-network-simulator/tests/ip_routing_test.cpp`: a ping between two
hosts on the same subnet (no router involved) succeeds via direct ARP
resolution and delivery; the full `PC1 → Switch → Router → Switch →
PC2` topology (the project's own named example) successfully pings end
to end, with the timeline showing real ARP-request/reply and IP-packet
events on both sides of the router; ARP resolution failure (pinging an
IP nobody on the reachable segment owns) is reported with a specific
failure reason rather than crashing or reporting false success; a
router with no interface covering the destination subnet reports a
real "no route" failure; running the identical successful simulation
twice produces byte-for-byte identical timelines (the deterministic-
replay property); and the "Connect Two Networks" mission both succeeds
on a correctly-configured topology and fails with the correct, specific
reason for each of: hosts on the same subnet, a missing/misconfigured
gateway, and no router present at all.

## Consequences

NetLab now has a real, tested IPv4+ARP+single-hop-routing simulation
built entirely on `06-networking`'s existing protocol codecs, plus the
first inspectable packet timeline and the first gamified mission — the
actual "Ethernet → ARP → IPv4 → routing → Ethernet → destination"
learning experience this frontier's objective named. The next real
gaps, in the order the project's own priority list gives them: richer
timeline/replay presentation (still core-side, before any UI),
multi-hop routing, a second and third mission, and only then a
browser/WebAssembly front end and additional protocols (DHCP, DNS,
TCP) — none of which exist yet.

# ADR 0029: NetLab packet inspector + timeline session (Phase 1 core)

**Status:** Accepted. This is Phase 1 of the NetLab product build
directive's recommended order ("Timeline UI, packet inspector, step/
pause/replay") — the **UI-independent core** those capabilities need,
not the browser rendering itself. Per architectural rule 1 ("simulation
core remains UI-independent") and rule 12 ("keep the core portable so
native + WASM can share it"), this ADR deliberately does not introduce
any web/UI technology (no HTML/JS/browser toolchain exists anywhere in
this repository yet, and Phase 5 — the actual WASM/browser build — is
explicitly later). What it builds is the real, testable data and
control layer a future UI (browser, native, or even a CLI) would call
into: decoding a captured frame's real header fields, and stepping
through a simulation's recorded timeline forward, backward, and to an
arbitrary point.

## Context

ADR 0028 already produces a `PingResult::timeline` of `SimulationEvent`
objects, each carrying real frame bytes. Two things are still missing
before any UI (of any kind) could present that timeline usefully: (1)
a way to turn a raw frame's bytes into human-readable protocol field
values ("packet inspector / header decoding / explanations"), and (2)
a way to move through a recorded timeline under caller control (step
forward, step backward/rewind, jump to a point) rather than only ever
seeing the whole thing at once. Both are pure data/logic — no reason
either needs a UI to exist and be fully tested.

## Packet inspector

`netlab::decodeFrame(frameBytes)` (`packet_inspector.hpp/cpp`) parses a
real captured frame using **only** `06-networking`'s existing parse
functions (`parseEthernetHeader`, then dispatching on `etherType` to
`parseArpPacket` or `parseIpv4Header` + `parseIcmpEcho`) — architectural
rule 4 ("reuse existing protocol implementations") applied directly;
no header layout is reimplemented here. It returns a `DecodedFrame`:// a
one-line human-readable `summary` (e.g. `"ARP Request: who has
10.0.2.1? tell 10.0.1.10"` or `"IPv4 ICMP Echo Request: 10.0.1.10 ->
10.0.2.10, ttl=64"`) plus two flat lists of `DecodedField{name, value}`
pairs (Ethernet-layer fields, then the dispatched protocol's fields) —
exactly the shape a future UI's "header decoding" panel would render
field-by-field, and exactly what a future rule-based or AI-grounded
"explanation" layer (Phase 9) would read from rather than re-deriving.
A frame that fails to parse at any stage returns
`DecodedFrame{parsedSuccessfully: false}` with an empty summary/field
list — never a crash, never fabricated field values.

## Timeline session (step / pause / rewind / replay)

`netlab::SimulationSession` (`simulation_session.hpp/cpp`) wraps an
already-computed, immutable `std::vector<SimulationEvent>` (e.g. from
`PingResult::timeline`) with a cursor representing how many events have
been "revealed" so far (`0` = nothing shown yet, `eventCount()` = the
full timeline has played through):

- `stepForward()` reveals the next event and returns a pointer to it
  (or `nullptr`, moving nothing, if already at the end).
- `stepBackward()` hides the most recently revealed event and returns
  a pointer to whatever is now the current (last-revealed) event, or
  `nullptr` if already at the start — this is real rewind, not just
  "start over."
- `reset()` returns to the start; `jumpTo(index)` moves directly to an
  arbitrary point (clamped to `[0, eventCount()]`).
- `eventsUpTo(index)` returns the events revealed as of a given cursor
  position — the "replay so far" a UI's timeline view would render.

Because `SimulationSession` only ever indexes into an already-computed,
immutable vector, stepping is trivially deterministic: replaying the
identical step sequence against two `SimulationSession`s built from two
independent (but identical-input) simulation runs always yields
identical events at every cursor position — directly tested, tying
ADR 0028's own timeline-determinism guarantee together with the
stepping mechanism on top of it.

## What this does not support

- **No browser UI, no rendering, no WASM build.** Nothing here is
  drawn, laid out, or served over HTTP — this is the data/control layer
  a UI would call, not the UI. Phases 2 and 5 (visual topology editor,
  WASM/browser build) are explicitly separate, unstarted work.
- **No live/streaming simulation** — `SimulationSession` operates over
  an already-fully-computed timeline (the simulation already ran to
  completion, e.g. via `sendIcmpEcho`); it does not pause a simulation
  *while it is running* or drive the simulation itself.
- **No "explanation" text beyond the one-line summary.** A richer,
  pedagogical "why did this happen" narrative (Phase 9's AI-tutor
  direction) is explicitly future work; `decodeFrame`'s summary is a
  factual one-line description of the packet, not a lesson.
- **No decoding for protocols beyond Ethernet/ARP/IPv4/ICMP** —
  exactly what ADR 0028's simulation currently produces. UDP/TCP/DHCP/
  DNS decoding (Phase 4) will extend this the same way, not replace it.
- **No timeline editing/branching** — a session only ever moves through
  the one fixed, already-recorded sequence of events it was built
  from; there is no "fork the simulation from this point" (a real,
  larger feature: "simulation comparison" from the product vision,
  explicitly not attempted here).

## Tested invariants

`24-network-simulator/tests/packet_inspector_test.cpp` and
`simulation_session_test.cpp`: `decodeFrame` correctly identifies and
extracts fields from a real ARP request frame, a real ARP reply frame,
and a real IPv4+ICMP-echo frame (each built the same way ADR 0028's
own `sendIcmpEcho`/ARP resolution builds them — not hand-crafted test-
only shapes), including the exact sender/target IPs and TTL; a
malformed/truncated frame is reported as `parsedSuccessfully = false`
rather than crashing or fabricating values; `SimulationSession` steps
forward through a real multi-event timeline (captured from an actual
`sendIcmpEcho` call across the router) in the exact recorded order;
stepping backward from the end returns to the same intermediate events
in reverse; stepping forward past the end and backward past the start
both return `nullptr` without moving the cursor further; `jumpTo`/
`reset` land on the exact expected cursor position; and decoding an
event captured live from a real `sendIcmpEcho` run reproduces the exact
destination IP that call actually used — a direct proof that the
inspector's output genuinely reflects the real simulated packet, not a
separately-maintained or fabricated representation (architectural rule
3: "every visual packet must correspond to a real simulated packet").

## Consequences

NetLab's core now has everything a future UI needs to present "step
through the packet timeline, inspect any packet's real header fields"
without that UI needing to reimplement any protocol parsing or replay
logic itself — genuinely portable to a native debug tool, a CLI, or
(Phase 5) a WASM-compiled browser front end unchanged. The next real
gap in this specific direction, per the product build directive's own
phase order, is Phase 2 (the actual visual topology editor and live
packet visualization) — not attempted here, and explicitly requiring
its own ADR when it begins, per architectural rule 7.

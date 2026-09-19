# ADR 0030: NetLab browser-foundation facade (pre-WASM API consolidation)

**Status:** Accepted. This is a real step toward Phase A ("Browser
Foundation") of the NetLab product build directive — **not** a
completed browser demo, and **not** a WebAssembly build. Both of those
remain explicitly unbuilt in this ADR; see "What this does not
support" for exactly why and what's actually blocking them.

## Context: a real, disclosed toolchain gap

Phase A's target architecture is:

```
C++ NetLab Core → WebAssembly → Browser UI
```

Producing that requires an Emscripten toolchain (`emcc`) to compile the
existing C++ core to WASM. **This session's execution environment does
not have Emscripten installed** (`which emcc` finds nothing; `node`/
`npm` are present, but a JS runtime alone cannot compile C++ to WASM).
Per this project's own standing rule against fake completeness, this
ADR does not claim a WASM build exists, does not fabricate build output,
and does not claim browser verification that never happened. The
Emscripten toolchain requirement is recorded here as the actual,
specific blocker for the remaining Phase A work (the WASM compile step
and the browser shell itself), not glossed over.

## What this ADR builds instead: a real, useful prerequisite

Before any WASM binding can be written, the C++ core needs **one
stable, coherent API surface** a binding layer (Emscripten Embind, or a
plain C ABI) would wrap — today, a caller (this session's own test
files) has to separately construct a `Topology`, call `sendIcmpEcho`,
build a `SimulationSession` from its `PingResult::timeline`, and call
`decodeFrame` on individual events' bytes. That's fine for C++ tests;
it's the wrong shape for a UI binding, which wants one object with a
small number of clearly-named methods.

`netlab::NetLabFacade` (`24-network-simulator/netlab/facade.hpp/cpp`)
consolidates exactly that, without introducing any new networking
logic — it is a thin composition of ADR 0022/0028/0029's existing,
already-tested pieces:

- `NetLabFacade::buildReferenceScenario()` builds the project's own
  canonical example — `PC1 → Switch → Router → Switch → PC2`, with the
  same addressing ADR 0028's own tests already use — as a ready-to-run
  facade instance. This is the concrete "first browser demo must
  reproduce the existing verified scenario" requirement, satisfied at
  the API-boundary level before any UI exists to call it.
- `runPing(fromHostId, destIp)` runs the real `sendIcmpEcho` and stores
  its `PingResult`, immediately building a `SimulationSession` over the
  resulting timeline — one call replaces what a caller previously had
  to do in two steps.
- `stepForward`/`stepBackward`/`reset`/`jumpTo`/`eventCount`/
  `hasCurrentEvent`/`currentEventDescription`/`currentEventDecoded`
  delegate directly to the existing `SimulationSession`/`decodeFrame` —
  no new stepping or decoding logic, just a single object exposing them
  together.
- `runConnectTwoNetworksMission(hostAId, hostBId)` delegates directly
  to ADR 0028's `evaluateConnectTwoNetworksMission`.
- `topology()` exposes the underlying `Topology&` directly, so a future
  editor UI can still add/configure nodes through the same real API a
  native caller would use — the facade adds a convenience layer, it
  does not hide or restrict the existing capability.

This is deliberately **not** a new abstraction layer with its own
logic — every method is a one-line-or-few delegation to something
already real and already tested. That is the point: the facade's own
correctness is almost entirely inherited from what it wraps, and the
facade-level tests exist to prove the *composition* is wired correctly
(e.g. that `runPing` really does hand its result to a fresh, correctly-
ordered `SimulationSession`), not to re-prove ARP/IP/routing
correctness a second time.

## What this does not support

- **No WebAssembly build.** `emcc` is not available in this
  environment; nothing here has been compiled to WASM or run in a
  browser or a JS engine. This is the single largest remaining gap
  toward Phase A.
- **No browser application shell, no topology renderer, no packet
  visualization, no HTML/CSS/JS of any kind.** Nothing in this
  repository's web-facing surface exists yet — there is no `npm`
  project, no bundler config, no front-end framework choice made.
- **No Embind/C-ABI bindings.** `NetLabFacade` is designed to be
  *bindable* (plain methods, simple parameter/return types — strings,
  `uint32_t`, `bool`, `size_t` — nothing exotic that would complicate a
  binding), but no actual binding code has been written, since there is
  no toolchain in this environment to compile and test it against.
- **No JSON/serialization protocol.** A real browser binding will
  eventually need some wire format between JS and the (eventually)
  WASM-compiled core; this ADR does not invent one, since doing so
  without a working WASM build to validate it against would itself be
  speculative, untested design — exactly what this project's rules
  warn against.
- **No new networking behavior of any kind.** Every capability exposed
  through `NetLabFacade` already existed and was already tested in ADR
  0022/0028/0029; this ADR adds zero new protocol logic.

## Tested invariants

`24-network-simulator/tests/facade_test.cpp`: `buildReferenceScenario()`
produces a topology whose `PC1`→`PC2` ping succeeds through the real
router (the exact same underlying behavior ADR 0028's own test already
verifies, now reachable through the facade's single entry point);
stepping the facade's session forward reproduces the identical event
sequence (by description and frame bytes) that directly building a
`SimulationSession` from the same `PingResult::timeline` would produce
— proving the facade doesn't silently alter or reorder anything;
`currentEventDecoded()` at the step containing the real ICMP echo event
correctly decodes the real destination IP and TTL (the same rule-3
"no fabricated visual state" proof ADR 0029 already established, now
verified through the facade); and `runConnectTwoNetworksMission`
through the facade succeeds on the reference scenario, matching direct
evaluation.

## Consequences

The C++ side of Phase A now has the one clean, coherent, already-
proven-correct-by-composition API surface a future WASM binding would
wrap — a real, verifiable step, not a placeholder. The actual remaining
Phase A work — installing/using an Emscripten toolchain, compiling
`NetLabFacade` to WASM, writing Embind bindings, and building an actual
browser application shell with a topology renderer and packet
visualization — is unstarted and is honestly reported as such, not
implied to exist. That toolchain/environment gap, not an architectural
one, is the concrete next blocker for Phase A.

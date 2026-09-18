# ADR 0008: Distributed systems RPC + deterministic simulation (v1)

**Status:** Accepted. This is the first slice of Layer 7 (Distributed
Systems) — membership, leader election, Raft, and replicated state
machines (FR-DIST-1) all still need to be built on top of it.

## Context

FR-DIST-1 requires "RPC + membership + leader election + Raft with
**deterministic simulation** and fault injection (partitions, node
loss, message loss/reorder/duplication, recovery)." There is no real
network transport anywhere in this repository (Layer 6 has protocol
codecs but no NIC driver — see `06-networking/README.md`), so a
distributed-systems layer built on a real socket transport would be
blocked on work that doesn't exist yet. The PRD's own phrasing —
*deterministic simulation* — points at the right first slice instead:
build the RPC and fault-injection model as a deterministic, in-process
simulator now, which is both immediately useful (Raft and friends can
be built and tested against it without any real network) and never
made obsolete by a later real transport (a real transport would
implement the same `send`/receive shape underneath the same
`RpcClient`/`RpcServer` API).

## Decision

`07-distributed-systems/rpc/serialization.hpp` is a small, bounds-checked
binary encoder/decoder (big-endian, length-prefixed strings/blobs) —
deliberately schema-free (a caller encodes/decodes fields in a fixed
known order) rather than a self-describing schema system, since no
consumer needs cross-version compatibility yet and a real schema/
versioning layer is a substantial independent design (see "What this
is not").

`rpc.hpp`'s `RpcRequest`/`RpcResponse` are the wire messages
(id/method/payload, and id/ok/payload-or-error respectively), and
`SimulatedNetwork` is a deterministic in-process fault-injection
simulator: every random decision (drop, duplicate, extra delivery
delay) is drawn from one seeded PRNG (xorshift64\*, not
cryptographically secure — determinism and speed are what a
fault-injection simulator needs, not unpredictability) owned by the
network instance. Running the identical sequence of `send()`/`tick()`
calls against two `SimulatedNetwork`s built with the same seed produces
byte-identical fault decisions and delivery order every time — this is
what "deterministic simulation" means in practice: not that faults
don't happen, but that any specific failure is exactly reproducible
from its seed, which is what makes debugging a distributed-systems bug
tractable instead of a heisenbug hunt. `partition()`/`healPartition()`
model a hard network partition (unconditional drop between two nodes,
independent of the random drop probability) that can later be healed.

`RpcServer` dispatches an incoming request to a registered named
handler, returning a real error response (never a crash or a silently
dropped request) for an unrecognized method. `RpcClient::call()` sends
a request, then drives the simulation forward tick-by-tick — pumping
the target server's inbox through its `RpcServer` each tick — until a
response matching its own request id arrives or a caller-supplied tick
budget (the "timeout," expressed in simulated ticks rather than
wall-clock time, since this is a step simulation) is exhausted. A
response bearing a different id (a stale duplicate from an earlier
call, or a message intended for someone else) is discarded rather than
mistaken for the answer — the concrete reason request ids exist.

## What this is not

- **Not a real network transport.** Everything happens in one process,
  in memory; there is no socket, no real bytes-on-a-wire, and nothing
  here talks to `06-networking/protocols`' Ethernet/IPv4/UDP codecs
  yet. A real transport would need its own implementation providing
  the same conceptual send/receive shape.
- **Not a self-describing schema/versioning system.** Fields are
  encoded/decoded in a fixed order known to both sides at compile
  time; there is no field-tagging, no forward/backward-compatibility
  story, and no schema evolution mechanism. This is an explicit,
  deferred design question, not an oversight.
- **No request deduplication.** `SimulatedNetwork`'s duplication fault
  can deliver the same request bytes to a server twice; `RpcServer`
  executes its handler once per delivered request with no dedup cache,
  so a duplicated request *is* executed twice server-side (the client
  still resolves correctly by request id — see the "duplication" test
  — but a handler with side effects would run twice). A real RPC
  system typically needs an idempotency/dedup layer for this; it's
  recorded here as a known, deliberate v1 gap rather than silently
  assumed away.
- **No membership, leader election, or Raft yet.** Those are the next
  layer built on top of this RPC foundation, not part of this ADR.
- **Single-call reordering, not concurrent-call reordering.** The
  reordering fault-injection test exercises multiple *sequential*
  calls (each fully resolves before the next begins) under delay/
  reorder; it does not test several calls *in flight
  simultaneously* racing each other, which a more advanced
  (asynchronous, non-blocking) client would be needed to drive.

## Tested invariants

`07-distributed-systems/tests/rpc_test.cpp` (35 hosted assertions):
serialization round trips and bounds-checked rejection of truncated/
oversized/trailing-garbage input; RPC request/response message framing
round trips and rejects malformed messages; a full request/response
call succeeds with no fault injection; an unknown method produces a
real error response naming the method, not a crash; several
independent calls on the same client/server pair each resolve to their
own correct response; a 100% message-drop rate causes a timeout rather
than a hang; a partitioned link blocks delivery unconditionally and
the identical call succeeds immediately once healed; 100% message
duplication does not break the client's ability to match its own
response by request id; delay-induced reordering across sequential
calls still resolves each one correctly; and — the determinism claim
itself, not merely asserted — two simulations built from the identical
seed and fault configuration produce a byte-for-byte identical sequence
of call outcomes across 8 calls, while a different seed produces a
genuinely different pattern (confirming the seed actually drives the
outcome rather than the test being trivially deterministic regardless
of seed).

## Consequences

Every claim about distributed systems elsewhere in this repository
must describe this layer using the scope recorded here: a deterministic
in-process RPC simulation with fault injection, not a real network
transport, not membership/leader-election/Raft, not a schema/
versioning system, not request deduplication. This ADR is the single
source of truth for that distinction until a future ADR (covering
membership, Raft, or a real transport) extends it.

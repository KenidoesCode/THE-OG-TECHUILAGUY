# Layer 7 — Distributed Systems

**Status: FOUNDATION.** RPC message framing plus a deterministic,
seeded, in-process network simulator with real fault injection
(message drop, duplication, reordering/delay, and hard partitions),
and Raft leader election built on top of it. No real network
transport, membership protocol, or Raft log replication yet. See
[`docs/ADR/0008-distributed-rpc.md`](../docs/ADR/0008-distributed-rpc.md)
and [`docs/ADR/0009-raft-leader-election.md`](../docs/ADR/0009-raft-leader-election.md).

## Implemented and tested

- `rpc/serialization.hpp`/`.cpp`: a small, bounds-checked binary
  encoder/decoder (big-endian, length-prefixed strings/blobs) used for
  RPC message framing.
- `rpc/rpc.hpp`/`.cpp`: `RpcRequest`/`RpcResponse` wire messages;
  `SimulatedNetwork` — a deterministic, seeded fault-injection
  simulator (drop/duplicate/delay probabilities, plus hard
  `partition()`/`healPartition()`) where the identical sequence of
  calls against the same seed always produces byte-identical outcomes;
  `RpcServer` (named method handlers, real error responses for unknown
  methods) and `RpcClient` (tick-budget-based "timeout," matches
  responses to their own request by id, discarding stale/foreign
  ones).
- 35 hosted unit assertions (`tests/rpc_test.cpp`/`rpc_test.sh`):
  serialization round trips and malformed-input rejection; RPC message
  framing round trips and rejection; a full no-fault call; unknown-
  method error handling; multiple independent calls each resolving
  correctly; message-drop-induced timeout; partition blocking and
  healing; duplication not breaking client-side matching; reordering
  across sequential calls; and a genuine determinism proof (same seed
  → identical 8-call outcome sequence; different seed → different
  sequence).

- `raft/raft.hpp`/`.cpp`: Raft leader election — randomized election
  timeouts (independently seeded per node), `RequestVote`/`Heartbeat`
  RPCs, term-based safety (a higher observed term always steps a node
  down to Follower), and a `RaftCluster` harness driving every node
  forward together tick by tick.
- 10 hosted unit assertions (`tests/raft_test.cpp`/`raft_test.sh`):
  3/5-node clusters converge to exactly one leader; a single-node
  cluster becomes its own leader immediately; the core safety property
  (no two simultaneous leaders in the same term) is checked at *every*
  tick of a 300-tick run, not just the end; isolating an established
  leader triggers a new, higher-term election among the survivors;
  identical seeds produce identical elected leaders/terms; and a
  cluster still converges under a 20% message drop rate.

## Not yet implemented

- a real network transport (everything runs in one process, in
  memory; nothing here talks to `06-networking/protocols` yet)
- a self-describing schema/versioning system (fields are encoded/
  decoded in a fixed, compile-time-known order)
- request deduplication (a duplicated request *is* executed twice
  server-side; the client still resolves correctly by request id, but
  a handler with side effects would run twice — a known, documented
  v1 gap)
- membership, heartbeats-as-failure-detector beyond Raft's own,
  Raft log replication, committed entries, replicated state machines
- concurrent in-flight calls racing each other (the reordering test
  covers sequential calls under delay, not simultaneous ones)

## Building and testing

```sh
bash tests/rpc_test.sh    # hosted tests, fully deterministic (seeded), no hardware/network needed
bash tests/raft_test.sh   # hosted Raft leader-election tests, fully deterministic
```

# Layer 7 — Distributed Systems

**Status: FOUNDATION.** RPC message framing plus a deterministic,
seeded, in-process network simulator with real fault injection
(message drop, duplication, reordering/delay, and hard partitions). No
real network transport, membership protocol, leader election, or Raft
yet. See [`docs/ADR/0008-distributed-rpc.md`](../docs/ADR/0008-distributed-rpc.md).

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

## Not yet implemented

- a real network transport (everything runs in one process, in
  memory; nothing here talks to `06-networking/protocols` yet)
- a self-describing schema/versioning system (fields are encoded/
  decoded in a fixed, compile-time-known order)
- request deduplication (a duplicated request *is* executed twice
  server-side; the client still resolves correctly by request id, but
  a handler with side effects would run twice — a known, documented
  v1 gap)
- membership, heartbeats, failure detection, leader election, Raft,
  replicated state machines
- concurrent in-flight calls racing each other (the reordering test
  covers sequential calls under delay, not simultaneous ones)

## Building and testing

```sh
bash tests/rpc_test.sh   # hosted tests, fully deterministic (seeded), no hardware/network needed
```

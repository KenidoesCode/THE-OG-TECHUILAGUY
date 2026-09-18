# ADR 0009: Raft leader election (v1)

**Status:** Accepted. Leader election only — log replication, committed
entries, and state-machine application (the rest of Raft, and the rest
of FR-DIST-1) are not part of this ADR.

## Context

FR-DIST-1 lists Raft as part of Layer 7's required deterministic-
simulation scope, after RPC (ADR 0008). Raft's full design (leader
election + log replication + safety proofs around commitment) is
large; leader election is the self-contained first third — it has its
own well-known safety property (at most one leader per term) and
liveness property (randomized timeouts eventually break a split vote),
both independently testable without needing a log at all.

## Decision

`07-distributed-systems/raft/raft.cpp` implements Raft leader election
(Ongaro & Ousterhout) directly on top of ADR 0008's `RpcServer`/
`SimulatedNetwork`: each `RaftNode` tracks a role (Follower/Candidate/
Leader), a current term, and who it voted for this term; a randomized
election timeout (its own independently-seeded PRNG, so a cluster's
nodes don't all roll identical "random" timeouts and deadlock in
lockstep) triggers a new election when no valid heartbeat or vote
grant has reset it in time. `RequestVote` and `Heartbeat` are real RPC
methods registered on each node's own `RpcServer`; a `RaftCluster`
owns every node plus one shared `SimulatedNetwork` and drives them all
forward together, one simulated tick at a time.

### A real concurrency/addressing bug, caught by the tests it was built to satisfy

The very first implementation had every node share one `SimulatedNetwork`
inbox address for two different purposes: requests arriving at its
`RpcServer`, and responses arriving to calls *it* had made as a
client. Every Raft node is both at once. The bug: `pumpServer()`
(called for every node, every tick, to service its `RpcServer`) drained
that shared inbox indiscriminately — including a node's own pending
`RequestVote` responses, which it then tried to parse as if they were
*requests*, failed, and silently discarded as malformed input. The
result: a candidate's vote responses were stolen and dropped by its
own server-pump logic before its `processIncomingResponses()` ever saw
them, so no candidate could ever count a majority and no cluster
larger than one node could ever elect a leader.

This was caught immediately, before the test suite was declared done:
the very first 3/5-node test run segfaulted (a downstream test
indexing into an empty "current leader" list after the cluster failed
to converge within its tick budget — a direct symptom of the real
bug, not a separate one), traced by isolating a minimal single- and
multi-node reproduction, tracing per-tick role/term state directly,
and observing that followers correctly updated their term (proof they
processed the `RequestVote` request and voted) while the candidate
never advanced past `Candidate` (proof its response was never counted).
The fix: `rpc.hpp`'s new `responsePort(NodeId)` gives every node a
second, logically distinct address for inbound responses — the same
distinction any real RPC system draws between a service's listening
address and a caller's reply address — and `pumpServer()`/`RpcClient::call()`/
`RaftNode::processIncomingResponses()` were updated to address/read
from the correct one. This is recorded here in detail because it's
exactly the class of "two logically distinct message flows sharing one
physical channel" bug that recurs in real distributed systems code and
is worth naming precisely, not glossing over as "fixed a bug."

## What this is not

- **Not full Raft.** No log, no `AppendEntries` carrying real entries,
  no commit index, no state machine, no log matching/consistency
  checks, no snapshotting. Only the leader-election subprotocol.
- **Not a real network transport** — inherits ADR 0008's scope exactly
  (in-process, in-memory simulation only).
- **No pre-vote extension** or other later Raft refinements (e.g. the
  "pre-vote" phase some real implementations add to reduce disruptive
  elections from a partitioned-then-rejoining node) — this is the
  original 2014 paper's basic election protocol.
- **Simplified request ids.** `startElection()`/`sendHeartbeats()`
  compute a request id from `(term, nodeId)` rather than a strictly
  monotonic per-node counter; this is sufficient because vote-counting
  keys off the response's carried *term*, not its request id, but a
  future log-replication layer needing true request/response
  correlation (e.g. matching a specific `AppendEntries` retry) would
  need a proper id scheme.

## Tested invariants

`07-distributed-systems/tests/raft_test.cpp` (10 hosted assertions):
3-node and 5-node clusters converge to exactly one leader within a
fixed tick budget with no fault injection; a single-node cluster
becomes its own leader as soon as its own election timeout fires
(no peers to wait on); **the core safety property — no two nodes ever
simultaneously believe they are leader for the same term — is checked
at every tick of a 300-tick run**, not merely at the end, since a
transient dual-leader bug could otherwise resolve itself before an
end-of-run check ever saw it; isolating an established leader (via a
hard partition from every other node) triggers a new election among
the survivors whose term is strictly greater than the old leader's;
two clusters built from identical network and timeout seeds elect the
identical leader in the identical term (a genuine determinism proof,
not merely an assumption), while a different timeout seed produces a
different outcome in the tested configuration; and a cluster still
converges on a leader within a generous tick budget despite a 20%
message drop rate (the liveness property randomized timeouts are
supposed to guarantee).

## Consequences

Every claim about Raft/consensus elsewhere in this repository must
describe this layer using the scope recorded here: leader election
only, verified for both safety (checked continuously, not just at the
end) and liveness (under message loss), built on ADR 0008's
deterministic simulation — not full Raft, not log replication, not a
real transport. This ADR is the single source of truth for that
distinction until a future ADR (covering log replication) extends it.

# ADR 0012: Authenticated RPC message envelopes

**Status:** Accepted. A cross-layer integration (Layer 10 crypto into
Layer 7 distributed systems), not a change to RPC's default behavior
— `RpcClient`/`RpcServer` do not call this yet.

## Context

The mission for this phase of the project explicitly calls out
crypto → distributed systems as one of the cross-layer integrations to
pursue, and ADR 0008 already noted that `SimulatedNetwork`'s fault
injection only drops/duplicates/delays *genuine* messages — it never
mutates content or injects forged ones, a strictly weaker threat model
than a real adversarial network. Authenticating RPC messages with the
HMAC-SHA256 built in ADR 0011 closes part of that gap: it lets an
endpoint detect a message tampered with, or forged, by anyone who
doesn't know a shared key.

## Decision

`07-distributed-systems/rpc/auth.cpp` wraps arbitrary message bytes
(in practice, `serializeRequest()`/`serializeResponse()`'s output)
with a trailing 32-byte HMAC-SHA256 tag computed over them using a
shared symmetric key, calling `10-cryptography/mac/hmac_sha256.hpp`
directly — a real function call crossing the layer boundary, not two
layers merely coexisting in the same repository.
`verifyAndExtractMessage()` recomputes the tag and compares it with
`crypto::constantTimeEquals` (never an ordinary `==`), returning
`false` uniformly for "too short to contain a tag," "wrong key," and
"tampered content" — deliberately not distinguishing between these
failure modes in the return value, since which one occurred is itself
information an attacker could exploit as an oracle.

## What this is not

- **Not wired into `RpcClient`/`RpcServer`'s default send/receive
  path.** Adding authentication to the default path would need a key-
  distribution story (how do two nodes agree on a shared key in the
  first place?) that doesn't exist yet, and would change the behavior
  of every existing RPC/Raft test that currently sends unauthenticated
  messages. This ADR provides the building block and proves it
  composes correctly with the existing wire format
  (`testEndToEndAuthenticatedRpcRequestRoundTrip`), not yet the default.
- **Not a full secure-channel/handshake protocol.** No key exchange, no
  session establishment, no protection against replay (an attacker
  who captured a valid authenticated message earlier can still resend
  the identical bytes later — nothing here binds a message to a
  specific point in time or a specific connection). A real secure RPC
  channel would need a handshake (see the still-unimplemented ML-KEM/
  hybrid classical+PQC key exchange named elsewhere in the PRD) and
  likely per-message freshness (a nonce or sequence number).
- **Symmetric-key only.** There's no signature scheme here (no
  asymmetric authentication where a verifier doesn't need to hold the
  same secret a signer does) — that's ML-DSA/SLH-DSA's eventual role,
  not HMAC's.

## Tested invariants

`07-distributed-systems/tests/auth_test.cpp` (10 hosted assertions):
authenticate-then-verify round trips correctly and recovers the exact
original message; verification fails with the wrong key; a single
tampered byte in the message body is detected; a tampered tag (message
left intact) is also detected; input too short to even contain a tag
is rejected outright rather than treated as a message with a missing/
zero tag; a full realistic pipeline (build a real `RpcRequest`,
serialize it, authenticate the serialized bytes, verify+extract, parse
it back) round-trips correctly end to end; and a syntactically valid
but unauthenticated (forged, with fabricated tag bytes) request is
rejected — proving that knowing the wire format alone (which is not
itself secret) is not sufficient to forge a message without the shared
key.

## Consequences

Every claim about RPC/message security elsewhere in this repository
must describe this capability using the scope recorded here: an
available, tested, composable authentication primitive for RPC
messages, not the default behavior of `RpcClient`/`RpcServer`, not a
full secure channel, not replay-protected, not asymmetric. This ADR is
the single source of truth for that distinction until a future ADR
(covering key exchange, replay protection, or wiring this into the
default RPC path) extends it.

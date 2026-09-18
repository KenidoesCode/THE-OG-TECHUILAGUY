#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Authenticated message envelopes for RPC, integrating
// 10-cryptography's HMAC-SHA256 directly into Layer 7 — a genuine
// cross-layer connection (crypto -> distributed systems), not two
// layers sitting side by side. See docs/ADR/0012-authenticated-rpc.md
// for the exact scope and threat model.
//
// This wraps arbitrary message bytes (in practice, the output of
// serializeRequest()/serializeResponse() from rpc.hpp) with an
// HMAC-SHA256 tag computed over them using a shared symmetric key —
// letting an endpoint detect a message that was tampered with or
// forged by anyone who doesn't know the key, a genuinely stronger
// threat model than SimulatedNetwork's own fault injection currently
// models (which only drops/duplicates/delays *genuine* messages, it
// never mutates their content or injects new ones).

namespace dist {

// Appends a 32-byte HMAC-SHA256 tag (computed over `message` with
// `key`) to the end of `message` and returns the combined bytes.
std::vector<uint8_t> authenticateMessage(
    const std::vector<uint8_t>& message,
    const uint8_t* key, size_t keyLength
);

// Splits the trailing 32-byte tag off `authenticated`, recomputes the
// HMAC over everything before it using `key`, and compares the two
// with a constant-time comparison (never an ordinary == on the raw
// tag bytes — see 10-cryptography/mac/hmac_sha256.hpp's
// constantTimeEquals for why). Returns false (leaving `messageOut`
// untouched) if `authenticated` is too short to even contain a tag, or
// if the recomputed tag doesn't match — the caller cannot distinguish
// "wrong key" from "tampered message" from this return value alone,
// which is the correct, safe behavior (leaking which one occurred
// would itself be an oracle an attacker could exploit).
bool verifyAndExtractMessage(
    const std::vector<uint8_t>& authenticated,
    const uint8_t* key, size_t keyLength,
    std::vector<uint8_t>& messageOut
);

}  // namespace dist

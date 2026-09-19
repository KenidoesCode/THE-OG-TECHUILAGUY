#pragma once

#include "types.hpp"

#include <cstdint>
#include <vector>

// A plain balance-transfer transaction and its canonical serialization
// — see docs/ADR/0021-techuilaguy-blockchain-l1.md. Not signed (no
// asymmetric-key integration yet); `from` is simply asserted, not
// authenticated.

namespace l1 {

struct Transaction {
    Address from;
    Address to;
    uint64_t amount = 0;
    uint64_t nonce = 0;
};

// Fixed field order, built on 07-distributed-systems/rpc/serialization.hpp's
// Encoder — deterministic: identical field values always produce
// identical bytes.
std::vector<uint8_t> serializeTransaction(const Transaction& tx);

// Returns false (leaving `out` unspecified) on any malformed/truncated
// input — never reads past the buffer, never partially parses.
bool parseTransaction(const std::vector<uint8_t>& bytes, Transaction& out);

// SHA-256(serializeTransaction(tx)) — a content-addressed transaction
// id, the same principle ADR 0007 already established for OGGit's
// objects.
Hash32 transactionHash(const Transaction& tx);

}  // namespace l1

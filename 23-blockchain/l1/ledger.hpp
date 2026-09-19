#pragma once

#include "transaction.hpp"
#include "types.hpp"

#include <map>
#include <utility>
#include <vector>

// The real state-transition function — see
// docs/ADR/0021-techuilaguy-blockchain-l1.md's "Deterministic state
// transition" section for the exact rules.

namespace l1 {

struct AccountState {
    uint64_t balance = 0;
    uint64_t nonce = 0;
};

enum class TxResult {
    Ok,
    InsufficientBalance,
    InvalidNonce,
};

class Ledger {
public:
    // A never-seen address reads as {balance: 0, nonce: 0} — a normal,
    // valid state, not an error.
    AccountState get(const Address& addr) const;

    // Validates and, only on success, atomically applies `tx`:
    // rejects (mutating nothing) on a nonce mismatch or insufficient
    // balance; otherwise debits the sender, advances its nonce by
    // exactly 1, and credits the recipient (creating it implicitly at
    // {0, 0} first if it didn't already exist).
    TxResult apply(const Transaction& tx);

    // SHA-256 over every account's serialized (address, balance,
    // nonce), sorted by address — a whole-state hash, not a Merkle
    // tree (see the ADR's "What this does not support").
    Hash32 computeStateRoot() const;

    // Every account, sorted by address — used for whole-state
    // persistence (chain.hpp) and for tests.
    std::vector<std::pair<Address, AccountState>> snapshot() const;

    // Replaces the entire ledger with `entries` — used to restore a
    // persisted snapshot (chain.hpp), not part of normal transaction
    // processing.
    void loadSnapshot(const std::vector<std::pair<Address, AccountState>>& entries);

private:
    std::map<Address, AccountState> accounts;
};

}  // namespace l1

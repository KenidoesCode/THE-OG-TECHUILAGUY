#pragma once

#include "../../08-storage/kv/kv_store.hpp"
#include "block.hpp"
#include "ledger.hpp"

#include <string>
#include <vector>

// A single-node, whole-state-snapshot Techuilaguy L1 chain — see
// docs/ADR/0021-techuilaguy-blockchain-l1.md for the full design,
// including why persistence is a whole-ledger snapshot per block
// rather than incremental (a deliberate, documented scalability
// trade-off for this vertical slice).

namespace l1 {

class Chain {
public:
    // Opens (or creates, with a genesis block) a chain backed by a
    // real storage::KVStore (08-storage/kv/kv_store.hpp, itself
    // WAL-backed) at `storagePath`. If `storagePath` already holds a
    // chain, its height, tip, and full account state are loaded
    // immediately — no historical replay is needed, since the
    // persisted snapshot already *is* current state.
    explicit Chain(const std::string& storagePath);

    // Validates `tx` against the in-memory ledger and, only on
    // success, applies it immediately and queues it as pending for
    // the next produceBlock() call. A rejected transaction mutates
    // nothing and is never queued. Not yet durably persisted on its
    // own — see the ADR's "mempool" limitation.
    TxResult submitTransaction(const Transaction& tx);

    // Snapshots every currently-pending transaction into a new block
    // (allowed to be empty), persists it, persists the resulting
    // whole-ledger state snapshot, and clears the pending queue.
    Block produceBlock();

    uint64_t height() const { return currentHeight; }
    Hash32 tipHash() const { return currentTip; }
    size_t pendingCount() const { return pending.size(); }

    AccountState getAccount(const Address& addr) const;

    // Reads a specific historical block back from storage. Returns
    // false if `atHeight` doesn't exist.
    bool getBlock(uint64_t atHeight, Block& out) const;

private:
    storage::KVStore kv;
    Ledger ledger;
    std::vector<Transaction> pending;
    uint64_t currentHeight = 0;
    Hash32 currentTip = ZERO_HASH32;

    void initializeGenesisOrLoad();
    void persistBlock(const Block& block, const Hash32& hash);
    void persistSnapshotAndTip();
};

}  // namespace l1

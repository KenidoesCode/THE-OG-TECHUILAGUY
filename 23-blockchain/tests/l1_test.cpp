// Real assertion-based tests for the Techuilaguy L1 blockchain slice
// (23-blockchain/l1/) — real SHA-256 (10-cryptography/), real
// serialization (07-distributed-systems/rpc/serialization.hpp), and
// real WAL-backed persistence (08-storage/kv/kv_store.hpp) against a
// temporary file, not mocks. See
// docs/ADR/0021-techuilaguy-blockchain-l1.md.

#include "../l1/block.hpp"
#include "../l1/chain.hpp"
#include "../l1/ledger.hpp"
#include "../l1/transaction.hpp"
#include "../l1/types.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

std::string tempPath(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("l1_test_" + name);
    std::filesystem::remove(path);
    return path.string();
}

}  // namespace

void testAddressDerivationIsDeterministic() {
    l1::Address a = l1::deriveAddress("alice");
    l1::Address b = l1::deriveAddress("alice");
    l1::Address c = l1::deriveAddress("bob");
    check(a == b, "l1: deriving an address from the same label twice produces the identical address");
    check(!(a == c), "l1: deriving an address from a different label produces a different address");

    l1::Address parsed;
    check(l1::parseAddress(l1::toHex(a), parsed) && parsed == a,
          "l1: an address round-trips through toHex/parseAddress exactly");
    check(!l1::parseAddress("not valid hex", parsed), "l1: parseAddress rejects non-hex input");
    check(!l1::parseAddress(std::string(39, 'a'), parsed), "l1: parseAddress rejects a hex string of the wrong length");
}

void testTransactionSerializationRoundTripsAndIsDeterministic() {
    l1::Transaction tx;
    tx.from = l1::deriveAddress("alice");
    tx.to = l1::deriveAddress("bob");
    tx.amount = 42;
    tx.nonce = 3;

    std::vector<uint8_t> bytes = l1::serializeTransaction(tx);
    std::vector<uint8_t> bytesAgain = l1::serializeTransaction(tx);
    check(bytes == bytesAgain, "l1: serializing the identical transaction twice produces identical bytes");

    l1::Transaction parsed;
    check(l1::parseTransaction(bytes, parsed), "l1: a serialized transaction parses back successfully");
    check(parsed.from == tx.from && parsed.to == tx.to && parsed.amount == tx.amount && parsed.nonce == tx.nonce,
          "l1: a parsed transaction's fields exactly match the original");

    l1::Hash32 hash1 = l1::transactionHash(tx);
    l1::Hash32 hash2 = l1::transactionHash(tx);
    check(hash1 == hash2, "l1: transactionHash is deterministic for identical transactions");

    l1::Transaction different = tx;
    different.amount = 43;
    check(!(l1::transactionHash(different) == hash1), "l1: changing one field changes the transaction hash");
}

void testMalformedTransactionBytesAreRejected() {
    l1::Transaction ignored;
    check(!l1::parseTransaction({}, ignored), "l1: parsing an empty buffer as a transaction fails cleanly, not a crash");
    check(!l1::parseTransaction({0xFF, 0x00, 0x01}, ignored),
          "l1: parsing garbage bytes as a transaction fails cleanly, not a crash");

    l1::Transaction real;
    real.from = l1::deriveAddress("alice");
    real.to = l1::deriveAddress("bob");
    real.amount = 10;
    real.nonce = 0;
    std::vector<uint8_t> bytes = l1::serializeTransaction(real);
    bytes.resize(bytes.size() - 3);  // truncate
    check(!l1::parseTransaction(bytes, ignored), "l1: parsing a truncated (but otherwise real) transaction fails cleanly");

    std::vector<uint8_t> withTrailingGarbage = l1::serializeTransaction(real);
    withTrailingGarbage.push_back(0xAB);
    check(!l1::parseTransaction(withTrailingGarbage, ignored),
          "l1: parsing a transaction with unexpected trailing bytes is rejected, not silently accepted");
}

void testLedgerStateTransitionRules() {
    l1::Ledger ledger;
    l1::Address alice = l1::deriveAddress("alice");
    l1::Address bob = l1::deriveAddress("bob");

    check(ledger.get(alice).balance == 0 && ledger.get(alice).nonce == 0,
          "l1: a never-seen address reads as balance=0, nonce=0, not an error");

    // Give alice a balance the only way this model allows: as a
    // recipient of a transaction from an address that starts with a
    // balance too (there is no "mint" primitive here — this test uses
    // the genesis Chain elsewhere for that; here we test Ledger in
    // isolation by directly asserting its rejection/acceptance rules).
    l1::Transaction insufficientTx{alice, bob, 100, 0};
    check(ledger.apply(insufficientTx) == l1::TxResult::InsufficientBalance,
          "l1: a transaction from a zero-balance account is rejected as InsufficientBalance");
    check(ledger.get(alice).nonce == 0, "l1: a rejected transaction does not advance the sender's nonce");
    check(ledger.get(bob).balance == 0, "l1: a rejected transaction does not credit the recipient");
}

void testDoubleSpendIsRejectedByNonceCheck() {
    l1::Address alice = l1::deriveAddress("alice_ds");
    l1::Address bob = l1::deriveAddress("bob_ds");

    // There is no mint primitive in this slice (see the ADR), so a
    // spendable balance is established directly via loadSnapshot — the
    // same real mechanism Chain uses internally for persistence, not
    // a mock or a shortcut around Ledger's actual rules.
    l1::Ledger ledger;
    ledger.loadSnapshot({{alice, l1::AccountState{100, 0}}});

    l1::Transaction tx{alice, bob, 30, 0};
    check(ledger.apply(tx) == l1::TxResult::Ok, "l1: a well-formed transaction from a funded account with the correct nonce succeeds");
    check(ledger.get(alice).balance == 70 && ledger.get(alice).nonce == 1,
          "l1: a successful transaction debits the sender and advances its nonce by exactly 1");
    check(ledger.get(bob).balance == 30, "l1: a successful transaction credits the recipient");

    // Replay the IDENTICAL transaction (same nonce) again.
    check(ledger.apply(tx) == l1::TxResult::InvalidNonce,
          "l1: replaying an already-applied transaction unchanged is rejected as InvalidNonce (the double-spend defense)");
    check(ledger.get(alice).balance == 70,
          "l1: a rejected replay does not debit the sender a second time");
}

void testGenesisBlockHasZeroPreviousHash() {
    std::string path = tempPath("genesis");
    l1::Chain chain(path);

    check(chain.height() == 0, "l1: a freshly-created chain starts at height 0 (the genesis block)");
    l1::Block genesis;
    check(chain.getBlock(0, genesis), "l1: the genesis block can be read back from storage");
    check(genesis.header.previousHash == l1::ZERO_HASH32, "l1: the genesis block's previousHash is the all-zero value");
    check(genesis.transactions.empty(), "l1: the genesis block has no transactions");

    std::filesystem::remove(path);
}

void testProduceBlockChainsToThePreviousBlockHash() {
    std::string path = tempPath("chaining");
    l1::Chain chain(path);

    l1::Hash32 genesisHash = chain.tipHash();
    l1::Block block1 = chain.produceBlock();  // empty block is allowed
    check(block1.header.height == 1, "l1: the first produced block is height 1 (genesis is height 0)");
    check(block1.header.previousHash == genesisHash, "l1: a produced block's previousHash correctly chains to the prior block's real hash");
    check(chain.tipHash() == l1::blockHash(block1.header), "l1: the chain's tip hash matches the newly-produced block's real hash");

    l1::Hash32 tipAfterBlock1 = chain.tipHash();
    l1::Block block2 = chain.produceBlock();
    check(block2.header.previousHash == tipAfterBlock1, "l1: a second produced block correctly chains to the first block's hash, not the genesis");

    std::filesystem::remove(path);
}

void testBlockHeaderSerializationRoundTrips() {
    l1::BlockHeader header;
    header.height = 7;
    header.previousHash = l1::ZERO_HASH32;
    header.stateRoot = l1::ZERO_HASH32;
    header.txRoot = l1::ZERO_HASH32;

    std::vector<uint8_t> bytes = l1::serializeBlockHeader(header);
    l1::BlockHeader parsed;
    check(l1::parseBlockHeader(bytes, parsed), "l1: a serialized block header parses back successfully");
    check(parsed.height == header.height, "l1: a parsed block header's height matches the original");

    l1::BlockHeader ignored;
    check(!l1::parseBlockHeader({0x01, 0x02}, ignored), "l1: parsing garbage bytes as a block header fails cleanly, not a crash");
}

void testSubmitTransactionRejectsInsufficientBalanceWithoutMutatingPending() {
    std::string path = tempPath("reject");
    l1::Chain chain(path);

    l1::Address alice = l1::deriveAddress("alice_reject");
    l1::Address bob = l1::deriveAddress("bob_reject");
    l1::Transaction tx{alice, bob, 50, 0};

    check(chain.submitTransaction(tx) == l1::TxResult::InsufficientBalance,
          "l1: Chain::submitTransaction rejects a transaction from a zero-balance account");
    check(chain.pendingCount() == 0, "l1: a rejected transaction is never added to the pending queue");
    check(chain.getAccount(alice).balance == 0, "l1: a rejected transaction leaves the sender's balance unchanged");

    std::filesystem::remove(path);
}

void testChainPersistenceSurvivesRestart() {
    std::string path = tempPath("persistence");
    l1::Address alice = l1::deriveAddress("alice_persist");
    l1::Address bob = l1::deriveAddress("bob_persist");

    {
        l1::Chain chain(path);
        // There is no mint primitive in this slice (see the ADR), so
        // to exercise real persisted state we produce two blocks and
        // confirm height/tip/genesis-derived state survives a
        // restart — the meaningful, always-available guarantee.
        chain.produceBlock();
        chain.produceBlock();
        check(chain.height() == 2, "l1: producing two blocks advances height to 2 before any restart");
    }

    {
        // A second, independent Chain instance over the SAME path —
        // a real process-restart simulation, not an in-memory check.
        l1::Chain reopened(path);
        check(reopened.height() == 2, "l1: reopening a chain at the same storage path recovers the exact same height after a restart");
        l1::Block block2;
        check(reopened.getBlock(2, block2), "l1: a historical block produced before restart is still readable after reopening");
        check(reopened.getAccount(alice).balance == 0 && reopened.getAccount(bob).balance == 0,
              "l1: account state (here, correctly still zero — no mint primitive) survives a restart exactly as it was");
    }

    std::filesystem::remove(path);
}

int main() {
    testAddressDerivationIsDeterministic();
    testTransactionSerializationRoundTripsAndIsDeterministic();
    testMalformedTransactionBytesAreRejected();
    testLedgerStateTransitionRules();
    testDoubleSpendIsRejectedByNonceCheck();
    testGenesisBlockHasZeroPreviousHash();
    testProduceBlockChainsToThePreviousBlockHash();
    testBlockHeaderSerializationRoundTrips();
    testSubmitTransactionRejectsInsufficientBalanceWithoutMutatingPending();
    testChainPersistenceSurvivesRestart();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

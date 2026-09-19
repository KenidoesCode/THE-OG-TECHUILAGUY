#pragma once

#include "transaction.hpp"
#include "types.hpp"

#include <vector>

// Block structure — see docs/ADR/0021-techuilaguy-blockchain-l1.md.
// stateRoot/txRoot are whole-content hashes, not Merkle roots (see the
// ADR's "What this does not support").

namespace l1 {

struct BlockHeader {
    uint64_t height = 0;
    Hash32 previousHash = ZERO_HASH32;
    Hash32 stateRoot = ZERO_HASH32;
    Hash32 txRoot = ZERO_HASH32;
};

struct Block {
    BlockHeader header;
    std::vector<Transaction> transactions;
};

std::vector<uint8_t> serializeBlockHeader(const BlockHeader& header);
bool parseBlockHeader(const std::vector<uint8_t>& bytes, BlockHeader& out);

std::vector<uint8_t> serializeBlock(const Block& block);
bool parseBlock(const std::vector<uint8_t>& bytes, Block& out);

// SHA-256(serializeBlockHeader(header)) — a block's real identity;
// chains to the previous block via header.previousHash.
Hash32 blockHash(const BlockHeader& header);

// SHA-256 over every transaction's serialized bytes, concatenated in
// block order — a whole-content hash, not a Merkle root.
Hash32 computeTxRoot(const std::vector<Transaction>& transactions);

}  // namespace l1

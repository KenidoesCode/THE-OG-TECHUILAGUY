#include "block.hpp"

#include "../../07-distributed-systems/rpc/serialization.hpp"
#include "../../10-cryptography/hashing/sha256.hpp"

namespace l1 {

namespace {

std::vector<uint8_t> hash32ToVector(const Hash32& h) {
    return std::vector<uint8_t>(h.begin(), h.end());
}

bool vectorToHash32(const std::vector<uint8_t>& v, Hash32& out) {
    if (v.size() != out.size()) return false;
    for (size_t i = 0; i < out.size(); ++i) out[i] = v[i];
    return true;
}

}  // namespace

std::vector<uint8_t> serializeBlockHeader(const BlockHeader& header) {
    dist::Encoder encoder;
    encoder.writeU64(header.height);
    encoder.writeBytes(hash32ToVector(header.previousHash));
    encoder.writeBytes(hash32ToVector(header.stateRoot));
    encoder.writeBytes(hash32ToVector(header.txRoot));
    return encoder.data();
}

bool parseBlockHeader(const std::vector<uint8_t>& bytes, BlockHeader& out) {
    dist::Decoder decoder(bytes);
    uint64_t height = 0;
    std::vector<uint8_t> prevBytes, stateBytes, txBytes;

    if (!decoder.readU64(height)) return false;
    if (!decoder.readBytes(prevBytes)) return false;
    if (!decoder.readBytes(stateBytes)) return false;
    if (!decoder.readBytes(txBytes)) return false;
    if (!decoder.atEnd()) return false;

    BlockHeader result;
    result.height = height;
    if (!vectorToHash32(prevBytes, result.previousHash)) return false;
    if (!vectorToHash32(stateBytes, result.stateRoot)) return false;
    if (!vectorToHash32(txBytes, result.txRoot)) return false;

    out = result;
    return true;
}

std::vector<uint8_t> serializeBlock(const Block& block) {
    dist::Encoder encoder;
    std::vector<uint8_t> headerBytes = serializeBlockHeader(block.header);
    encoder.writeBytes(headerBytes);
    encoder.writeU32(static_cast<uint32_t>(block.transactions.size()));
    for (const auto& tx : block.transactions) {
        encoder.writeBytes(serializeTransaction(tx));
    }
    return encoder.data();
}

bool parseBlock(const std::vector<uint8_t>& bytes, Block& out) {
    dist::Decoder decoder(bytes);
    std::vector<uint8_t> headerBytes;
    if (!decoder.readBytes(headerBytes)) return false;

    BlockHeader header;
    if (!parseBlockHeader(headerBytes, header)) return false;

    uint32_t txCount = 0;
    if (!decoder.readU32(txCount)) return false;

    std::vector<Transaction> transactions;
    transactions.reserve(txCount);
    for (uint32_t i = 0; i < txCount; ++i) {
        std::vector<uint8_t> txBytes;
        if (!decoder.readBytes(txBytes)) return false;
        Transaction tx;
        if (!parseTransaction(txBytes, tx)) return false;
        transactions.push_back(tx);
    }
    if (!decoder.atEnd()) return false;

    out.header = header;
    out.transactions = std::move(transactions);
    return true;
}

Hash32 blockHash(const BlockHeader& header) {
    std::vector<uint8_t> serialized = serializeBlockHeader(header);
    uint8_t digest[32];
    crypto::sha256(serialized.data(), serialized.size(), digest);
    Hash32 hash;
    for (size_t i = 0; i < hash.size(); ++i) hash[i] = digest[i];
    return hash;
}

Hash32 computeTxRoot(const std::vector<Transaction>& transactions) {
    crypto::Sha256 hasher;
    for (const auto& tx : transactions) {
        std::vector<uint8_t> serialized = serializeTransaction(tx);
        hasher.update(serialized.data(), serialized.size());
    }
    uint8_t digest[32];
    hasher.finish(digest);
    Hash32 hash;
    for (size_t i = 0; i < hash.size(); ++i) hash[i] = digest[i];
    return hash;
}

}  // namespace l1

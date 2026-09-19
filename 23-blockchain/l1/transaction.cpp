#include "transaction.hpp"

#include "../../07-distributed-systems/rpc/serialization.hpp"
#include "../../10-cryptography/hashing/sha256.hpp"

namespace l1 {

namespace {

std::vector<uint8_t> addressToVector(const Address& addr) {
    return std::vector<uint8_t>(addr.begin(), addr.end());
}

bool vectorToAddress(const std::vector<uint8_t>& v, Address& out) {
    if (v.size() != out.size()) return false;
    for (size_t i = 0; i < out.size(); ++i) out[i] = v[i];
    return true;
}

}  // namespace

std::vector<uint8_t> serializeTransaction(const Transaction& tx) {
    dist::Encoder encoder;
    encoder.writeBytes(addressToVector(tx.from));
    encoder.writeBytes(addressToVector(tx.to));
    encoder.writeU64(tx.amount);
    encoder.writeU64(tx.nonce);
    return encoder.data();
}

bool parseTransaction(const std::vector<uint8_t>& bytes, Transaction& out) {
    dist::Decoder decoder(bytes);
    std::vector<uint8_t> fromBytes;
    std::vector<uint8_t> toBytes;
    uint64_t amount = 0;
    uint64_t nonce = 0;

    if (!decoder.readBytes(fromBytes)) return false;
    if (!decoder.readBytes(toBytes)) return false;
    if (!decoder.readU64(amount)) return false;
    if (!decoder.readU64(nonce)) return false;
    if (!decoder.atEnd()) return false;

    Transaction result;
    if (!vectorToAddress(fromBytes, result.from)) return false;
    if (!vectorToAddress(toBytes, result.to)) return false;
    result.amount = amount;
    result.nonce = nonce;

    out = result;
    return true;
}

Hash32 transactionHash(const Transaction& tx) {
    std::vector<uint8_t> serialized = serializeTransaction(tx);
    uint8_t digest[32];
    crypto::sha256(serialized.data(), serialized.size(), digest);
    Hash32 hash;
    for (size_t i = 0; i < hash.size(); ++i) hash[i] = digest[i];
    return hash;
}

}  // namespace l1

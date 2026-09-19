#include "chain.hpp"

#include "../../07-distributed-systems/rpc/serialization.hpp"

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

std::vector<uint8_t> hash32ToVector(const Hash32& h) {
    return std::vector<uint8_t>(h.begin(), h.end());
}

bool vectorToHash32(const std::vector<uint8_t>& v, Hash32& out) {
    if (v.size() != out.size()) return false;
    for (size_t i = 0; i < out.size(); ++i) out[i] = v[i];
    return true;
}

std::string blockKey(uint64_t height) {
    return "block:" + std::to_string(height);
}

}  // namespace

Chain::Chain(const std::string& storagePath) : kv(storagePath) {
    initializeGenesisOrLoad();
}

void Chain::initializeGenesisOrLoad() {
    std::vector<uint8_t> heightBytes;
    if (kv.get("height", heightBytes)) {
        dist::Decoder heightDecoder(heightBytes);
        uint64_t loadedHeight = 0;
        heightDecoder.readU64(loadedHeight);
        currentHeight = loadedHeight;

        std::vector<uint8_t> tipBytes;
        if (kv.get("tip", tipBytes)) {
            vectorToHash32(tipBytes, currentTip);
        }

        std::vector<uint8_t> snapshotBytes;
        if (kv.get("state_snapshot", snapshotBytes)) {
            dist::Decoder snapDecoder(snapshotBytes);
            uint32_t count = 0;
            std::vector<std::pair<Address, AccountState>> entries;
            if (snapDecoder.readU32(count)) {
                entries.reserve(count);
                for (uint32_t i = 0; i < count; ++i) {
                    std::vector<uint8_t> addrBytes;
                    uint64_t balance = 0;
                    uint64_t nonce = 0;
                    if (!snapDecoder.readBytes(addrBytes)) break;
                    if (!snapDecoder.readU64(balance)) break;
                    if (!snapDecoder.readU64(nonce)) break;
                    Address addr;
                    if (!vectorToAddress(addrBytes, addr)) continue;
                    entries.emplace_back(addr, AccountState{balance, nonce});
                }
            }
            ledger.loadSnapshot(entries);
        }
        return;
    }

    // Fresh chain: build and persist the genesis block.
    BlockHeader header;
    header.height = 0;
    header.previousHash = ZERO_HASH32;
    header.stateRoot = ledger.computeStateRoot();
    header.txRoot = computeTxRoot({});
    Block genesis{header, {}};
    Hash32 hash = blockHash(header);

    persistBlock(genesis, hash);
    currentHeight = 0;
    currentTip = hash;
    persistSnapshotAndTip();
}

void Chain::persistBlock(const Block& block, const Hash32& hash) {
    (void)hash;  // the header (and therefore the hash) is recoverable from the block itself
    kv.put(blockKey(block.header.height), serializeBlock(block));
}

void Chain::persistSnapshotAndTip() {
    dist::Encoder heightEncoder;
    heightEncoder.writeU64(currentHeight);
    kv.put("height", heightEncoder.data());

    kv.put("tip", hash32ToVector(currentTip));

    dist::Encoder snapshotEncoder;
    std::vector<std::pair<Address, AccountState>> entries = ledger.snapshot();
    snapshotEncoder.writeU32(static_cast<uint32_t>(entries.size()));
    for (const auto& [addr, state] : entries) {
        snapshotEncoder.writeBytes(addressToVector(addr));
        snapshotEncoder.writeU64(state.balance);
        snapshotEncoder.writeU64(state.nonce);
    }
    kv.put("state_snapshot", snapshotEncoder.data());
}

TxResult Chain::submitTransaction(const Transaction& tx) {
    TxResult result = ledger.apply(tx);
    if (result == TxResult::Ok) {
        pending.push_back(tx);
    }
    return result;
}

Block Chain::produceBlock() {
    uint64_t newHeight = currentHeight + 1;
    BlockHeader header;
    header.height = newHeight;
    header.previousHash = currentTip;
    header.stateRoot = ledger.computeStateRoot();
    header.txRoot = computeTxRoot(pending);

    Block block{header, pending};
    Hash32 hash = blockHash(header);

    persistBlock(block, hash);
    currentHeight = newHeight;
    currentTip = hash;
    persistSnapshotAndTip();
    pending.clear();

    return block;
}

AccountState Chain::getAccount(const Address& addr) const {
    return ledger.get(addr);
}

bool Chain::getBlock(uint64_t atHeight, Block& out) const {
    std::vector<uint8_t> bytes;
    if (!kv.get(blockKey(atHeight), bytes)) return false;
    return parseBlock(bytes, out);
}

}  // namespace l1

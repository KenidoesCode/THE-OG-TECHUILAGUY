#include "kv_store.hpp"

#include "../../07-distributed-systems/rpc/serialization.hpp"

namespace storage {

namespace {

constexpr uint8_t OP_PUT = 0;
constexpr uint8_t OP_DELETE = 1;

std::vector<uint8_t> encodePut(const std::string& key, const std::vector<uint8_t>& value) {
    dist::Encoder enc;
    enc.writeU8(OP_PUT);
    enc.writeString(key);
    enc.writeBytes(value);
    return enc.data();
}

std::vector<uint8_t> encodeDelete(const std::string& key) {
    dist::Encoder enc;
    enc.writeU8(OP_DELETE);
    enc.writeString(key);
    return enc.data();
}

}  // namespace

KVStore::KVStore(const std::string& walPath) : wal(walPath) {
    replay();
}

void KVStore::replay() {
    for (auto& record : wal.recoverRecords()) {
        applyRecord(record);
    }
}

void KVStore::applyRecord(const std::vector<uint8_t>& record) {
    dist::Decoder dec(record);

    uint8_t opType;
    if (!dec.readU8(opType)) return;

    std::string key;
    if (!dec.readString(key)) return;

    if (opType == OP_PUT) {
        std::vector<uint8_t> value;
        if (!dec.readBytes(value)) return;
        if (!dec.atEnd()) return;
        data[key] = std::move(value);
    } else if (opType == OP_DELETE) {
        if (!dec.atEnd()) return;
        data.erase(key);
    }
    // An unrecognized opType (which should never happen for a record
    // that passed the WAL's own crc32 check, since only this code
    // ever produces records) is silently ignored rather than
    // crashing — defensive, since a record having a valid checksum
    // proves it wasn't corrupted in transit, not that it was ever
    // written by *this* version of applyRecord's encoding.
}

bool KVStore::put(const std::string& key, const std::vector<uint8_t>& value) {
    if (!wal.append(encodePut(key, value))) {
        return false;
    }
    data[key] = value;
    return true;
}

bool KVStore::remove(const std::string& key) {
    if (!wal.append(encodeDelete(key))) {
        return false;
    }
    data.erase(key);
    return true;
}

bool KVStore::get(const std::string& key, std::vector<uint8_t>& valueOut) const {
    auto it = data.find(key);
    if (it == data.end()) return false;
    valueOut = it->second;
    return true;
}

bool KVStore::contains(const std::string& key) const {
    return data.find(key) != data.end();
}

size_t KVStore::size() const {
    return data.size();
}

}  // namespace storage

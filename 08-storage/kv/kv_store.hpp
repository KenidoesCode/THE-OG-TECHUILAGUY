#pragma once

#include "../wal/wal.hpp"

#include <string>
#include <unordered_map>
#include <vector>

// A durable key/value store: every mutation is appended to a
// write-ahead log (../wal/wal.hpp) before it's applied to the
// in-memory map, and the map is rebuilt by replaying the log on
// construction — so a KVStore reopened after a clean shutdown, an
// unclean shutdown, or a partial/corrupted trailing write all recover
// to a well-defined state (see docs/ADR/0010-storage-wal.md for
// exactly which one). Record encoding reuses
// 07-distributed-systems/rpc/serialization.hpp's Encoder/Decoder
// directly — a real cross-layer integration (distributed systems'
// wire-format codec doubling as storage's on-disk record format)
// rather than a third bespoke serializer.

namespace storage {

class KVStore {
public:
    // Opens (or creates) the WAL at `walPath` and replays every
    // recoverable record to rebuild this store's state before
    // returning — by the time the constructor returns, get() already
    // reflects everything durably committed by a prior run.
    explicit KVStore(const std::string& walPath);

    // Appends a Put record to the WAL (durable before this call
    // returns) and then applies it to the in-memory map. Returns
    // false only if the WAL append itself failed.
    bool put(const std::string& key, const std::vector<uint8_t>& value);

    // Appends a Delete record to the WAL and removes the key from the
    // in-memory map (a no-op removal — deleting an absent key — is
    // still logged and still returns true, matching real KV store
    // semantics where deletion is idempotent).
    bool remove(const std::string& key);

    bool get(const std::string& key, std::vector<uint8_t>& valueOut) const;
    bool contains(const std::string& key) const;
    size_t size() const;

private:
    WriteAheadLog wal;
    std::unordered_map<std::string, std::vector<uint8_t>> data;

    void replay();
    void applyRecord(const std::vector<uint8_t>& record);
};

}  // namespace storage

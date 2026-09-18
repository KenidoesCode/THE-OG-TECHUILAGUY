#pragma once

#include <cstdint>
#include <string>
#include <vector>

// A real write-ahead log: an append-only sequence of
// [length][crc32][payload] records on a real file, with crash-safe
// recovery — the foundational durability primitive the KV store
// (../kv/kv_store.hpp) is built on. See
// docs/ADR/0010-storage-wal.md for the exact recovery semantics and
// what "crash-safe" does and does not mean here.

namespace storage {

class WriteAheadLog {
public:
    // Opens (creating if necessary) the log file at `path` in
    // append mode — existing content, including any records left by a
    // prior run, is preserved, not truncated.
    explicit WriteAheadLog(std::string path);

    // Appends one record (opaque bytes — the KV store above this
    // layer decides what they mean) to the log, flushing before
    // returning so the record is durable on disk the moment this call
    // succeeds. Returns false only if the underlying file write
    // itself failed (e.g. the file could not be opened at all); it
    // never partially writes a record that recoverRecords() would
    // later see as valid.
    bool append(const std::vector<uint8_t>& payload);

    // Reads every record from the beginning of the file up to the
    // first sign of corruption or an incomplete trailing record — the
    // expected shape of a file left behind by a crash partway through
    // a write. Recovery never throws, never crashes, and never trusts
    // a length field into reading past a corrupted or missing crc32:
    // it stops at the first bad record and returns everything valid
    // that came before it, discarding that record and anything after
    // it (there is no way to know whether a later record "recovered
    // by luck" past a corrupted one actually represents a committed
    // write or leftover garbage, so this deliberately does not try).
    std::vector<std::vector<uint8_t>> recoverRecords() const;

    // Truncates the log to zero length. Not used by KVStore in v1
    // (which never compacts — see docs/ADR/0010-storage-wal.md),
    // provided for direct testing and future use.
    void clear();

    const std::string& filePath() const { return path; }

private:
    std::string path;
};

}  // namespace storage

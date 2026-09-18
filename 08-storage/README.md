# Layer 8 — Storage

**Status: FOUNDATION.** A real write-ahead log (CRC-32-checked,
crash-tested) and a durable KV store built on it. No block-device
abstraction, transactions, MVCC, indexes, query engine, compaction, or
replication yet. See
[`docs/ADR/0010-storage-wal.md`](../docs/ADR/0010-storage-wal.md).

## Implemented and tested

- `wal/crc32.hpp`/`.cpp`: CRC-32 (IEEE 802.3), verified against the
  standard's own published check value.
- `wal/wal.hpp`/`.cpp`: an append-only, crc32-checked write-ahead log
  on a real file. Recovery stops cleanly at the first torn or
  corrupted record rather than trusting or skipping past it.
- `kv/kv_store.hpp`/`.cpp`: a durable key/value store — every mutation
  is WAL-logged before being applied in memory, and state is rebuilt
  by replaying the log on open. Record encoding reuses
  `07-distributed-systems/rpc/serialization.hpp`'s `Encoder`/`Decoder`
  directly (a real cross-layer integration).
- 23 hosted unit assertions (`tests/storage_test.cpp`/
  `storage_test.sh`), including genuine crash/corruption simulation:
  a torn trailing record (a length field written with nothing
  following it) and a bit-flipped record checksum, both produced by
  directly modifying bytes on disk, both correctly truncating recovery
  at exactly the right point with no crash and no data loss before
  that point.

## Not yet implemented

- a block-device / sector-page abstraction (this operates directly on
  a host file via `fstream`, not through a block-level interface)
- compaction (the WAL grows forever; `KVStore` never snapshots+resets it)
- transactions, concurrency control, MVCC
- indexes and a query engine (lookup is a single exact-match hash map)
- replication (nothing here talks to `07-distributed-systems`' Raft
  yet, despite the serialization-layer reuse)
- integration with `22-os`'s VFS or `13-developer-ecosystem`'s OGGit

## Building and testing

```sh
bash tests/storage_test.sh   # hosted tests, real file I/O + real crash simulation
```

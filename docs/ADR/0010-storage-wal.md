# ADR 0010: Storage — write-ahead log + KV store (v1)

**Status:** Accepted. This is the first slice of Layer 8 (Storage):
transactions, MVCC, indexes, a query engine, and replication all still
need to be built on top of it.

## Context

Layer 8 requires a full progression from block storage through a
query engine. A write-ahead log is the right foundation to start from:
it is the specific mechanism that makes crash recovery possible at
every later layer (a KV store, a transaction log, a filesystem's own
journal), and its correctness is fully verifiable in isolation — by
simulating a crash directly (truncating or corrupting a file on disk)
and checking recovery behaves exactly as specified, not just "doesn't
throw."

## Decision

`08-storage/wal/crc32.cpp` implements CRC-32 (the IEEE 802.3/Ethernet/
gzip/PNG variant) — a real, standard checksum, verified against the
standard's own published check value (CRC-32 of the nine ASCII bytes
`"123456789"` must equal `0xCBF43926`), used to detect a corrupted or
torn WAL record.

`08-storage/wal/wal.cpp`'s `WriteAheadLog` is an append-only sequence
of `[length: u32][crc32: u32][payload]` records on a real file.
`append()` flushes before returning, so a record is durable the moment
the call succeeds. `recoverRecords()` reads from the start and stops
at the **first** sign of trouble — an incomplete length or crc32 field
(a write torn mid-header), a payload shorter than its claimed length
(torn mid-payload), or a payload whose crc32 doesn't match (corruption)
— and returns everything valid before that point, discarding the bad
record and everything after it. This is a deliberate, conservative
recovery policy: a corrupted record's own length field cannot be
trusted to "skip past" it and resume from a later, possibly-genuine
record, because there is no way to know whether anything after a
corruption point represents a write that actually completed. Real
systems (e.g. a database's redo log) generally make the identical
choice for the identical reason.

`08-storage/kv/kv_store.cpp`'s `KVStore` is a durable key/value map:
every `put`/`remove` appends a record to the WAL *before* updating the
in-memory map, and the map is rebuilt from scratch by replaying every
recovered record on construction — so a `KVStore` reopened after a
clean shutdown, an unclean one, or one with a torn trailing write all
converge to a well-defined, tested state. Record encoding reuses
`07-distributed-systems/rpc/serialization.hpp`'s `Encoder`/`Decoder`
directly, a genuine cross-layer integration (distributed systems' RPC
wire-format codec doubling as storage's on-disk record format) rather
than a third bespoke serializer built for no reason.

## What this is not

- **No block-device abstraction yet.** This operates directly on a
  real host file via `fstream`, not through a sector/page-level block
  device interface — that abstraction (needed for `22-os`'s eventual
  filesystem work) is a distinct, still-unbuilt layer underneath this.
- **No compaction.** The WAL grows forever; `KVStore` never rewrites it
  into a smaller snapshot plus a fresh log (a real system needs this to
  bound recovery time and disk usage). `WriteAheadLog::clear()` exists
  for testing/future use but `KVStore` never calls it.
- **No transactions, concurrency control, or MVCC.** Every `put`/
  `remove` is its own independent, immediately-durable operation —
  there is no multi-operation atomic transaction, no isolation between
  concurrent readers/writers (the whole thing is single-threaded), and
  no versioning.
- **No indexes or query engine.** Lookup is a single in-memory hash map
  keyed by exact string match; there is no range scan, no secondary
  index, no query planner.
- **No replication.** Nothing here talks to `07-distributed-systems`'
  RPC/Raft, despite the serialization-layer reuse — a replicated KV
  store built on Raft's (still log-replication-free) leader election
  is a natural, still-unbuilt next integration.
- **Not integrated with `22-os`'s VFS or `13-developer-ecosystem`'s
  OGGit** yet, despite both being natural eventual consumers.

## Tested invariants

`08-storage/tests/storage_test.cpp` (23 hosted assertions): CRC-32
matches the standard's published check value, detects a single changed
byte, and is 0 for empty input; a WAL survives append/recover round
trips, multiple separate open/close sessions, and returns empty for a
freshly created (never written) file; **recovery correctly stops at a
simulated torn trailing record** (a length field written with nothing
following it — the literal shape of a crash mid-write) and returns
exactly the valid records before it; **recovery correctly stops at a
corrupted record's checksum mismatch** (a single flipped byte inside an
otherwise well-formed record, produced by directly modifying bytes on
disk exactly as `13-developer-ecosystem`'s OGGit object-store
corruption test does), discarding it and everything after rather than
trusting or skipping past it; `clear()` truncates correctly; the KV
store round-trips put/get and delete, survives a simulated full-process
restart with the exact correct surviving keys (including a put-then-
deleted key correctly staying deleted, proving delete records replay
too), accumulates correctly across five separate restart sessions,
reflects only the latest value after repeated overwrites of the same
key, and — the key crash-safety proof for the KV layer specifically,
not just the WAL underneath it — recovers exactly the two writes
committed before a simulated crash with a torn in-flight third write
contributing nothing, not a partial key, not a crash.

## Consequences

Every claim about storage elsewhere in this repository must describe
this layer using the scope recorded here: a real, crash-tested WAL and
KV store, not a block-device abstraction, not transactions/MVCC/
indexes/a query engine, not replication, not compaction. This ADR is
the single source of truth for that distinction until a future ADR
(covering a block device, transactions, or replication) extends it.

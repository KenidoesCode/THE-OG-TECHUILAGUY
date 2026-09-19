# ADR 0021: Techuilaguy Blockchain L1 — accounts, transactions, single-node chain

**Status:** Accepted. This is the FOUNDATION vertical slice of a new
domain (not in the original PRD.md layer numbering — added per
explicit project direction; tracked honestly in `PROJECT_STATE.md`
rather than silently folded into an existing layer number). It
establishes the smallest real account/transaction/block/persistence
loop. Multi-node P2P, consensus, a VM/contracts layer, and L2 are all
explicitly future work, not attempted here.

## Context

The project's standing rule is "smallest real implementation that
establishes the architectural foundation," applied here to a
blockchain L1: an account model, a deterministically-serializable
transaction, a real (not simulated) state-transition function, a block
structure that commits to that state, and real on-disk persistence —
enough to genuinely create accounts, submit transactions, produce
blocks, restart the process, and observe the exact same chain state
back. Nothing about multi-node behavior, consensus, or a VM is
attempted; those require this foundation to exist first.

## Account model

An `Address` (`23-blockchain/l1/types.hpp`) is a 20-byte value. This
slice does **not** derive addresses from a real public key — there is
no asymmetric-key/signature integration yet (see "What this does not
support"). `deriveAddress(label)` (SHA-256 of a caller-supplied label,
truncated to 20 bytes) exists purely so tests and demos have a
deterministic, reproducible way to get *an* address; it is explicitly
not a security mechanism.

An account's state (`AccountState`) is just `{ balance: uint64,
nonce: uint64 }`. There is no code/contract-storage field — this is a
plain-payment ledger, not the VM's account model (a future ADR's
concern).

## Transaction model and canonical serialization

`Transaction { from, to: Address; amount, nonce: uint64 }`
(`transaction.hpp/cpp`). `serializeTransaction`/`parseTransaction`
give it a single canonical byte encoding, built on
`07-distributed-systems/rpc/serialization.hpp`'s existing
`Encoder`/`Decoder` (a real cross-layer integration, not a third
bespoke codec) — every field in a fixed order, length-prefixed where
variable, so two transactions with identical field values always
serialize to identical bytes.

`transactionHash(tx)` is `SHA-256(serializeTransaction(tx))`
(`10-cryptography/hashing/sha256.hpp` — the same from-scratch,
standard-verified SHA-256 already used by OGGit's object store), giving
every transaction a content-addressed id, the same principle ADR 0007
already established for OGGit's objects.

## Deterministic state transition

`Ledger::apply(tx)` (`ledger.hpp/cpp`) is the one real state-transition
function:

1. Look up the sender's current `AccountState` (a never-seen address
   reads as `{balance: 0, nonce: 0}`, not an error — this is a normal,
   valid state, the same "empty repository" stance ADR 0014 already
   takes for refs).
2. Reject with `TxResult::InvalidNonce` if `tx.nonce != sender.nonce`
   — this is the actual replay/double-spend defense: a transaction can
   only ever be applied once, because applying it advances the
   sender's nonce, and re-submitting the identical transaction again
   will always fail this check against the now-advanced nonce.
3. Reject with `TxResult::InsufficientBalance` if `sender.balance <
   tx.amount`.
4. Otherwise, apply atomically: `sender.balance -= amount`,
   `sender.nonce += 1`, `recipient.balance += amount` (creating the
   recipient's account implicitly at `{0, 0}` first if it didn't
   exist). No partial application on rejection — steps 2/3 are checked
   before any mutation happens.

`Ledger::computeStateRoot()` hashes every account's serialized
`(address, balance, nonce)`, sorted by address (`std::map`'s natural
order — content-addressing needs a canonical order, the same principle
ADR 0007's `serializeTree` already established). This is a **whole-
state hash, not a Merkle tree** — see "What this does not support."

## Block structure

`BlockHeader { height: uint64; previousHash, stateRoot, txRoot:
Hash32 }`, `Block { header; transactions: vector<Transaction> }`
(`block.hpp/cpp`). `txRoot` is `SHA-256` over the concatenated
serialized transactions (in block order) — again a whole-content hash,
not a Merkle tree. A block's own identity, `blockHash(header)`, is
`SHA-256(serializeBlockHeader(header))` — chaining blocks the same way
every real blockchain does (each header commits to the previous
block's hash), just without a full Merkle-proof structure over the
transaction list.

The genesis block (height 0) has an all-zero `previousHash`, no
transactions, and `stateRoot` of the empty ledger.

## Single-node chain and persistence

`Chain` (`chain.hpp/cpp`) is deliberately a **single-node**,
**whole-state-snapshot** chain:

- `submitTransaction(tx)` validates and applies it to the in-memory
  `Ledger` immediately (so multiple pending transactions before the
  next block correctly build on each other, matching how transactions
  within a real block apply sequentially) and queues it as pending. A
  rejected transaction (bad nonce/insufficient balance) mutates
  nothing and is never queued.
- `produceBlock()` snapshots every currently-pending transaction into
  a new `Block`, computes its header (including the *post*-application
  `stateRoot`), persists it, and clears the pending queue. Producing a
  block with zero pending transactions is allowed (an empty block),
  matching real chains.
- Persistence is real, not simulated: `Chain` is built directly on
  `08-storage/kv/kv_store.hpp`'s `KVStore` (itself WAL-backed — a real
  cross-layer integration with Layer 8, not a bespoke file format).
  Every produced block is persisted under its own key
  (`block:<height>`); the **entire** ledger is re-serialized as one
  snapshot blob and persisted after every block (key
  `state_snapshot`), alongside the current height and tip hash. On
  reopening a `Chain` at an existing `storagePath`, the constructor
  loads the persisted snapshot/height/tip directly — no replay of
  historical blocks is needed to reconstruct current state, since the
  snapshot already *is* the current state (see "What this does not
  support" for the scalability trade-off this implies).
- `getBlock(height)` reads a specific historical block back from the
  `KVStore` — a genuine query path, not just a write path.

## Validation tested

Malformed transaction bytes (`parseTransaction` on truncated/garbage
input) are rejected without crashing — the same bounds-checked
`Decoder` discipline ADR 0008 already established. A double-spend
attempt (replaying an already-applied transaction unchanged) is
rejected by the nonce check, directly tested. Insufficient-balance and
wrong-nonce transactions are both rejected without mutating any
account. A block's `previousHash` correctly chains to the prior
block's real hash, and the genesis block's `previousHash` is the
all-zero value, both directly tested. Restarting a `Chain` against the
same `storagePath` recovers the identical height, tip hash, and every
account balance/nonce, directly tested (a real process-restart
simulation via a second `Chain` instance over the same path, not just
an in-memory-only check).

## What this does not support

- **No cryptographic signatures.** Transactions are not signed and
  `Chain`/`Ledger` never verify who is authorized to spend from an
  address — anyone can construct a `Transaction` from any address.
  This is a plain accounting/state-transition demonstration, not a
  security-authenticated ledger. Real signature verification (and the
  key-pair/identity model it needs) is explicitly future work.
- **No Merkle trees.** `stateRoot` and `txRoot` are whole-content
  hashes (hash everything, once), not Merkle roots — there is no
  Merkle proof of individual account/transaction inclusion without
  re-hashing everything. A real Merkle Patricia trie (or similar) is
  future work, named directly in the PRD's Layer 7 distributed-systems
  section ("Merkle structures").
- **No P2P, no consensus, no multiple nodes.** This is a single
  process with a single, entirely locally-authoritative chain. There
  is no network protocol, no peer discovery, no fork choice rule, and
  no Byzantine-fault-tolerant (or any) consensus algorithm.
- **No mempool eviction/prioritization/fee market.** `pending` is a
  simple FIFO queue with no transaction fees, no priority ordering, and
  no size limit.
- **No chain reorganization.** There is exactly one chain; nothing
  here handles competing forks or a longer-chain/heavier-chain switch.
- **Snapshot-per-block persistence does not scale.** Re-serializing
  the *entire* account set on every block is fine at this slice's
  demonstration scale; a real chain would persist incremental account
  deltas, not a whole-state blob every block. This is an explicit,
  acknowledged scalability limitation, not an oversight.
- **No VM, no smart contracts, no L2.** `AccountState` has no code or
  storage field; there is nothing to execute beyond the fixed
  balance-transfer state transition above.

## Consequences

THE OG TECHUILAGUY now has a real, tested, persistent, single-node
account/transaction/block chain — genuinely usable for "create an
account, send a transaction, produce a block, restart the process,
verify the state survived," but explicitly nothing more than that yet.
The next real gaps, in roughly increasing order of size, are:
multi-node simulation + a real P2P transport (this project already has
the deterministic-network-simulation and RPC building blocks from
Layer 7 to build on), a consensus algorithm (Layer 7 already has a
real Raft implementation — leader-election-style consensus is a
plausible starting point before anything more exotic), and eventually
a VM/contracts layer and an L2 architecture, none of which are started.

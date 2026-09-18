# ADR 0007: OGGit's content-addressed object store (v1)

**Status:** Accepted. This is the foundation of OGGit (Layer 13,
Developer Ecosystem) — commits, branches, merge, and remote sync
(FR-DEV-1) all still need to be built on top of it; see "What this is
not."

## Context

FR-DEV-1 calls for OGGit with content-addressed objects, commits,
branches, merge, and remote sync. Every one of those depends on having
a real object model first — Git's own design (and the design this ADR
follows in spirit) is that a commit is just a small object referencing
a tree object, which references blob/tree objects, all addressed by
the hash of their own content. This is also this repository's first
real integration between two independently-built subsystems: OGGit's
object hashing calls `10-cryptography/hashing/sha256.cpp` directly,
not a reimplementation — exactly the "continuously integrate
subsystems" principle this project follows rather than building
disconnected pieces.

## Decision

An object is identified by the SHA-256 hash of a small header
(`"<type> <size>\0"`) followed by its raw content — the same idea
Git's own object hashing uses, so that a blob and a tree with
coincidentally identical raw bytes still hash to different ids (the
type is part of what's hashed). `ObjectStore` writes an object's
header+content verbatim to
`<root>/objects/<first-2-hex>/<remaining-62-hex>` and, on read,
independently re-hashes what it actually reads back and compares it
against the id used to look the file up — an object store's whole
premise is that content-addressing makes a stored object immutable and
self-verifying, so this project's existing "never trust stored/parsed
data without checking it" discipline applies here exactly as it does
in `22-os/elf/elf.cpp`'s untrusted-input validation.

Three object types exist: `Blob` (arbitrary raw bytes — a file's
content), `Tree` (an ordered list of named entries, each pointing at
another Blob or Tree, sorted canonically by name so two directory
snapshots with identical logical contents always serialize to
byte-identical output and therefore the same object id — the property
that makes content-addressing useful for detecting "these two trees
are the same" without walking every entry), and `Commit` (a tree id, a
list of parent commit ids — zero for a root commit, more than one for
a merge — an author string, and a message).

This uses ordinary host filesystem I/O (`std::filesystem`,
`fstream`) — unlike `22-os` or `06-networking`, OGGit is a normal
host-side developer tool with no freestanding/hardware constraint.

## What this is not

- **Not byte-for-byte Git-compatible.** No zlib compression of stored
  objects, no pack files, no Git's exact tree-entry encoding (this
  encoding is length-prefixed binary, not Git's `mode name\0<20-byte
  sha1>` format) — the *idea* (content-addressed objects, a header
  included in the hash, canonical tree ordering) follows Git's design,
  the on-disk bytes do not match Git's.
- **No commits/branches/merge/remote sync as a working system yet.**
  This ADR covers the object model only — there is no ref storage
  (branch pointers), no working-directory/index concept, no diff or
  merge algorithm, and no remote transport of any kind. A `Commit`
  object can be constructed and stored, but nothing yet builds one
  from an actual set of file changes.
- **No garbage collection.** Objects are never deleted once written.
- **SHA-256, not SHA-1** (Git's historical default) or a
  collision-resistant successor scheme — chosen because SHA-256 was
  the hash function already built and verified in this repository
  (Layer 10), which is itself the right integration decision (reuse a
  real, tested primitive rather than adding a second hash
  implementation for no reason), not a claim that this format is
  interoperable with existing Git repositories.

## Tested invariants

`13-developer-ecosystem/tests/oggit_object_store_test.cpp` (24 hosted
assertions, real disk I/O against a temporary directory): write/read
round trip; content-addressing determinism (identical content →
identical id, written twice → same file); different content produces
different ids; identical raw bytes stored as different object types
produce different ids (confirming the type is part of what's hashed);
`exists()`/`NotFound` behave correctly for written vs. never-written
objects; a corrupted on-disk object (content silently modified without
changing its filename) is detected via re-hashing on read, not
trusted; a tree's entries round-trip with their directory flags
correct and are canonically sorted regardless of insertion order (two
logically-identical trees inserted in different orders produce
byte-identical serialized output); a tree rejects content claiming
more entries than are actually present; a commit round-trips including
zero-parent (root), one-parent, and two-parent (merge) cases, and
rejects content too short to contain even a tree id; and a full,
independently-verified commit → tree → blob chain is reconstructed by
following ids alone, starting from just the commit's id.

## Consequences

Every claim about OGGit elsewhere in this repository must describe it
using the scope recorded here: a real, tested, content-addressed
object store (blob/tree/commit), not commits/branches/merge/remote
sync as a working version-control system. This ADR is the single
source of truth for that distinction until a future ADR (covering refs,
a working directory model, or merge) extends it.

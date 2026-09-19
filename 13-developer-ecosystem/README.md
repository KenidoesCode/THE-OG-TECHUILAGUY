# Layer 13 — Developer Ecosystem

**Status: FOUNDATION.** OGGit's content-addressed object store (blob/
tree/commit objects, real SHA-256-based hashing reusing
`10-cryptography/`) plus refs/HEAD/first-parent history are
implemented and tested. OGForge, OGRegistry, and OGJudge (FR-DEV-1) do
not exist yet. See
[`docs/ADR/0007-oggit-object-store.md`](../docs/ADR/0007-oggit-object-store.md)
and [`docs/ADR/0014-oggit-refs.md`](../docs/ADR/0014-oggit-refs.md).

## Implemented and tested

- `oggit/object_store.hpp`/`.cpp`: a real content-addressed object
  store — `ObjectStore::writeObject`/`readObject` against real disk
  I/O, with independent re-hashing on read (a corrupted object is
  detected, never silently trusted); `Blob`, canonically-ordered
  `Tree` (serialize/parse), and `Commit` (serialize/parse, supporting
  zero, one, or multiple parents for root/normal/merge commits).
  Object hashing calls the real SHA-256 implementation from Layer 10
  directly — a genuine cross-layer integration, not a reimplementation.
- `oggit/refs.hpp`/`.cpp`: named branches (`refs/heads/<name>`), a
  symbolic-or-detached `HEAD`, and `walkFirstParentHistory()` — all
  real disk I/O on the same repository root the object store uses.
- 24 hosted unit assertions for the object store
  (`tests/oggit_object_store_test.cpp`/`oggit_object_store_test.sh`):
  write/read round trips, content-addressing determinism, corrupted-
  object detection, canonical tree ordering, commit parent-count
  variations, and a full end-to-end commit → tree → blob
  reconstruction starting from just a commit id.
- 18 hosted unit assertions for refs (`tests/oggit_refs_test.cpp`/
  `oggit_refs_test.sh`): branch set/get/exists/delete/move; listing
  branches; symbolic vs. detached HEAD resolution (including a branch
  moving while HEAD symbolically follows it); clean failure on an
  empty repository and on HEAD naming a not-yet-existing branch; and
  first-parent history over both a linear chain and a genuine merge
  commit (proving the *other* parent's history is correctly excluded).

## Not yet implemented

- a working-directory / index model, `status`/`add`/`commit` workflow
- diff or merge algorithms (first-parent-only history is a preview of
  why real merge needs a proper ancestry DAG this doesn't build yet)
- remote synchronization of any kind
- OGForge (repositories, issues, pull requests, reviews, CI)
- OGRegistry (package publishing/resolution/versions/integrity)
- OGJudge (sandboxed deterministic evaluation)
- any integration with OGLang specifically (e.g. publishing an OGLang
  module to a registry)

## Building and testing

```sh
bash tests/oggit_object_store_test.sh   # hosted tests, real disk I/O against a temp directory
bash tests/oggit_refs_test.sh           # hosted tests, real disk I/O against a temp directory
```

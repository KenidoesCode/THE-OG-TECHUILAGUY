# Layer 13 — Developer Ecosystem

**Status: FOUNDATION.** OGGit's content-addressed object store (blob/
tree/commit objects, real SHA-256-based hashing reusing
`10-cryptography/`) is implemented and tested. OGForge, OGRegistry,
and OGJudge (FR-DEV-1) do not exist yet. See
[`docs/ADR/0007-oggit-object-store.md`](../docs/ADR/0007-oggit-object-store.md).

## Implemented and tested

- `oggit/object_store.hpp`/`.cpp`: a real content-addressed object
  store — `ObjectStore::writeObject`/`readObject` against real disk
  I/O, with independent re-hashing on read (a corrupted object is
  detected, never silently trusted); `Blob`, canonically-ordered
  `Tree` (serialize/parse), and `Commit` (serialize/parse, supporting
  zero, one, or multiple parents for root/normal/merge commits).
  Object hashing calls the real SHA-256 implementation from Layer 10
  directly — a genuine cross-layer integration, not a reimplementation.
- 24 hosted unit assertions (`tests/oggit_object_store_test.cpp`/
  `oggit_object_store_test.sh`): write/read round trips, content-
  addressing determinism, corrupted-object detection, canonical tree
  ordering, commit parent-count variations, and a full end-to-end
  commit → tree → blob reconstruction starting from just a commit id.

## Not yet implemented

- refs / branches (there is no pointer-to-a-commit concept at all yet)
- a working-directory / index model
- diff or merge algorithms
- remote synchronization of any kind
- OGForge (repositories, issues, pull requests, reviews, CI)
- OGRegistry (package publishing/resolution/versions/integrity)
- OGJudge (sandboxed deterministic evaluation)
- any integration with OGLang specifically (e.g. publishing an OGLang
  module to a registry)

## Building and testing

```sh
bash tests/oggit_object_store_test.sh   # hosted tests, real disk I/O against a temp directory
```

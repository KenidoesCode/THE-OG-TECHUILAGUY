# ADR 0017: OGGit index (staging area) and tree construction

**Status:** Accepted. Continues Layer 13 from ADR 0007 (object store)
and ADR 0014 (refs/HEAD) — the index only; diff, checkout, merge, and
remote sync remain unbuilt, exactly as ADR 0014 already scoped them.

## Context

ADR 0014 explicitly named "no index or working tree" as the next gap:
`Commit` objects could only be constructed by a caller handing over an
already-built `Tree` id directly — there was no path from "here is
some real file content I want to save" to a `Tree`, the way `git add`
followed by `git commit` provides in real Git. Without a staging area,
nothing in OGGit could plausibly be called usable version control yet,
only an object model with pointers on top of it.

## Decision

`13-developer-ecosystem/oggit/index.cpp`'s `Index` class is a flat,
disk-persisted `path -> blob ObjectId` map built directly on the
existing `ObjectStore`/`Tree` machinery from `object_store.hpp` — no
new object type, no parallel storage format for content itself.

- `addFile(store, path, content)` writes a real `Blob` through the
  existing `ObjectStore` and stages `path` to point at it, replacing
  any previous entry for the same path.
- `save()`/the constructor's implicit `load()` persist the staged
  entries as plain text (`<64-hex-blob-id> <path>` per line) — a
  simple format of this project's own, not a reimplementation of
  Git's binary index format.
- `writeTreeFromIndex(store)` is the real, non-trivial piece: it walks
  every staged path, splits it on `/`, and reconstructs the actual
  directory hierarchy as nested `Tree` objects (writing each one
  through the same `ObjectStore::writeObject`/`serializeTree` ADR 0007
  already established), returning the id of the root `Tree`. This is
  the concrete bridge from "a flat set of staged files" to the single
  `ObjectId` a `Commit::tree` points at — previously that id could
  only be produced by a caller manually building `TreeEntry` vectors
  by hand.

A file/directory path conflict (staging both `"a"` and `"a/b"` — `"a"`
used as both a file and a directory) is resolved deterministically:
the directory always wins, independent of staging order, and the
conflicting file entry is dropped from the resulting tree. This is a
real, tested decision (see `testWriteTreeFromIndexDirectoryWinsOverConflictingFile`
in the test suite below), not an unhandled edge case — the goal was to
guarantee `writeTreeFromIndex` never produces an ambiguous or
ill-formed tree, not to detect and reject the conflict as a user-facing
error (that would need surfacing through a real CLI, which doesn't
exist yet).

## What this is not

- **Not a working tree or `status`.** There is no concept of "files
  currently on disk vs. what's staged vs. what's committed," no
  `git status`-equivalent diff between working tree and index, and no
  filesystem-walking `add`-a-directory convenience — every `addFile`
  call is handed content directly by its caller.
- **Not diff or merge.** Nothing here compares two trees or two
  indexes; `writeTreeFromIndex` only ever builds a tree from scratch
  from whatever is currently staged.
- **Not checkout.** Nothing here materializes a `Tree` back onto a
  real filesystem — that direction (`Tree` → files on disk) is the
  mirror image of what this ADR builds (files on disk → `Tree`) and is
  separate, unbuilt work.
- **Not Git's on-disk index format.** The persisted text format here
  is this project's own, chosen for simplicity, and makes no claim of
  being byte-compatible with `.git/index`.
- **No file-mode/permission bits, symlinks, or submodules** — every
  staged entry is treated as a plain file (`Blob`) or a directory
  (`Tree`); there is no executable-bit or symlink concept anywhere in
  the index or in ADR 0007's `TreeEntry` it builds on.

## Tested invariants

`13-developer-ecosystem/tests/oggit_index_test.cpp` (31 hosted
assertions): staging a file makes it retrievable and writes a real
blob into the object store; staging identical content twice is
idempotent (same blob id); re-staging a path replaces rather than
duplicates its entry; removing a staged path works and reports failure
correctly when nothing was staged; path normalization collapses
leading/trailing/duplicate slashes; save-then-reload round-trips every
entry exactly (including reconstructing the correct blob id, not just
the path); `clear()` empties the index; `writeTreeFromIndex` on an
empty index produces a real, readable, zero-entry `Tree` object (not a
special-cased no-op); a single root-level file produces a
one-entry tree with the correct name/id/type; a genuinely nested
example (`src/main.og`, `src/lib/util.og`, `README.md`) produces a
real multi-level tree hierarchy, independently re-read and re-parsed
two directory levels deep to confirm every blob is reachable exactly
where it should be; the resulting root tree id is independent of
staging order (proving the "canonical sorted order" property ADR 0007
established for `serializeTree` actually holds transitively through
this construction); and the file/directory conflict case above
resolves to exactly the documented, deterministic outcome.

## Consequences

OGGit's honestly-describable capability is now: content-addressed
objects (ADR 0007) + refs/HEAD/first-parent history (ADR 0014) + a
real staging area that can turn staged files into an actual tree
hierarchy (this ADR). It is still not diff, checkout, merge, or remote
sync — those remain the next real gaps, in that rough order, since
diff and checkout are the more immediate prerequisites for a usable
local workflow, and merge/remote sync depend on machinery (a real
ancestry DAG, a transfer protocol) that doesn't exist yet.

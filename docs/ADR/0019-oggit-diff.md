# ADR 0019: OGGit diff (tree comparison)

**Status:** Accepted. Continues Layer 13 from ADR 0007 (object store),
ADR 0014 (refs/HEAD), ADR 0017 (index), and ADR 0018 (checkout) — diff
only; merge and remote sync remain unbuilt, exactly as ADR 0018
already scoped them.

## Context

With both directions of "files on disk ↔ Tree object" now real (ADR
0017's `writeTreeFromIndex`, ADR 0018's `checkoutTree`), the next
question a caller of either direction actually needs answered is "what
changed?" — between a newly-staged tree and HEAD's committed tree,
or between any two commits' trees once a caller has resolved them via
`refs.hpp`. ADR 0018's own "Consequences" section named diff as the
immediate next gap over merge, since merge (finding a proper 3-way
merge base across a full ancestry DAG) is meaningfully larger work
that diff itself doesn't require.

## Decision

`13-developer-ecosystem/oggit/diff.cpp`'s `diffTrees(store, oldTreeId,
newTreeId)` recursively walks two `Tree` objects in tandem (read
through the existing `ObjectStore`/`parseTree` from ADR 0007) and
returns every changed path as a flat, sorted `std::vector<DiffEntry>`
— file-level granularity, the same shape a caller of `git diff
--name-status` would expect, not a tree of nested directory nodes.

Three deliberate, directly-tested design choices:

- **A whole added/removed directory expands into one entry per file
  inside it**, not a single directory-level entry — proven by
  `testDiffExpandsAddedDirectoryIntoIndividualFiles`. This keeps
  `DiffEntry` uniformly file-shaped (a path plus old/new blob ids)
  rather than needing a separate directory-entry variant.
- **A file/directory type change at the same path is reported as a
  full Removed (of everything under the old side) plus a full Added
  (of everything under the new side)**, never as a `Modified` — a
  `Modified` entry's old/new ids are only ever meaningful when both
  sides are the same kind of thing.
- **An identical tree id short-circuits to an empty result without
  reading either tree at all** (`oldTreeId == newTreeId` is checked
  before any I/O) — content-addressing already guarantees identical
  ids mean identical recursive content, so there is nothing to gain by
  reading and comparing. Two *different* ids with identical recursive
  content (built independently, e.g. by two separate `Index`
  instances staging the same files) are still compared structurally
  and still correctly produce an empty result — proven separately by
  `testDiffOfTwoIdenticalContentTreesIsEmpty`, which is the real proof
  the short-circuit above isn't hiding a broken comparison.

Consistent with ADR 0014's `walkFirstParentHistory` and ADR 0017/0018's
own stance: a tree id that fails to read as a real `Tree` (missing,
corrupt, or actually a `Blob`) makes `diffTrees` return an empty
result rather than guess or crash — "diff against a broken tree" is
honestly unknown, not zero-changes-with-false-confidence, though the
two states are unfortunately indistinguishable from the return value
alone in the current API (a caller needing to tell them apart must
independently check both tree ids exist and are real trees first via
`ObjectStore`/`parseTree` before calling `diffTrees`).

## What this is not

- **Not a content/line-level diff.** `DiffEntry` only ever reports
  "this path's blob id changed from X to Y" — there is no line-based
  or byte-based diffing of a `Modified` file's two blobs, no unified
  diff format, no patch generation.
- **Not merge.** Nothing here finds a merge base, attempts a 3-way
  merge, or reports conflicts — that remains genuinely larger,
  separate work needing a full ancestry DAG traversal (ADR 0014's
  first-parent-only walk isn't sufficient for it).
- **Not a working-tree diff.** `diffTrees` only ever compares two
  already-built `Tree` objects; there is no direct "diff the index
  against the last commit" or "diff the filesystem against the index"
  convenience — a caller wanting that must build (or resolve) both
  trees first and pass their ids.
- **No rename/move detection.** A file moved from one path to another
  with identical content is reported as one Removed entry at the old
  path and one Added entry at the new path, not as a single Rename
  entry — real Git's own rename detection is a heuristic similarity
  search this project doesn't attempt.

## Tested invariants

`13-developer-ecosystem/tests/oggit_diff_test.cpp` (20 hosted
assertions): diffing a tree id against itself is empty; diffing two
independently-built trees with genuinely identical content (different
ids never even compared — same real objects, built twice) is also
empty, proving the identical-id short-circuit isn't concealing a
broken general case; a single added, removed, or modified file is
each detected exactly once with the correct status and the correct
real blob id(s); a file present unchanged in both trees never appears
in the result; changes at multiple real nesting depths (one level and
two levels deep, mixed with an unrelated modification) are all
detected with their correct full paths; adding an entire new directory
expands into one entry per file inside it rather than a single
directory-level entry; the returned entries are sorted by path
regardless of staging order; and diffing two tree ids the object store
has never seen returns an empty result rather than crashing.

## Consequences

OGGit's honestly-describable local-workflow capability is now:
content-addressed objects (ADR 0007) + refs/HEAD/first-parent history
(ADR 0014) + staging files into a tree (ADR 0017) + materializing a
tree back to disk (ADR 0018) + comparing any two trees at file
granularity (this ADR). Merge and remote sync remain the next real
gaps — merge is the more immediate one now that diff exists, since a
real 3-way merge needs exactly this kind of tree comparison as one of
its own building blocks (comparing each side against a common
ancestor), on top of the full ancestry-DAG traversal ADR 0014's
first-parent-only walk doesn't yet provide.

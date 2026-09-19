# ADR 0020: OGGit merge (ancestry DAG + three-way merge)

**Status:** Accepted. Continues Layer 13 from ADR 0007 (object store),
ADR 0014 (refs/HEAD), ADR 0017 (index), ADR 0018 (checkout), and ADR
0019 (diff) — merge only; remote sync remains unbuilt.

## Context

ADR 0019's diff gave OGGit "what changed between any two trees," and
ADR 0014 explicitly limited its own history walk to first-parent-only,
naming a real merge as needing the *full* ancestry DAG. This ADR is
that full-DAG piece: real merge requires (1) walking *every* parent of
a commit, not just `parents[0]`, to find where two branches actually
diverged, and (2) a three-way comparison (base vs. ours vs. theirs)
built directly on ADR 0019's diff machinery.

## Commit graph representation

No new storage format. A commit's parents (`Commit::parents`, ADR
0007) already form a DAG — this ADR only adds real traversal logic
over that existing structure; nothing on disk changes.

## Ancestor traversal

`allAncestors(store, commit)` in `merge.cpp` performs a real BFS over
**every** parent (not `parents[0]` only): starting from `commit`
itself (included, depth 0), it visits each parent, each grandparent
through *every* parent edge, and so on, recording each visited commit
exactly once (by hex id) with its BFS depth (shortest distance from
the start). This is the genuine DAG walk ADR 0014 deferred — a merge
commit's second, third, ... parents are followed, unlike
`walkFirstParentHistory`.

If a commit reached *during* this walk (not the starting commit
itself) fails to read as a real `Commit` (missing, corrupt, wrong
type, or fails to parse), that branch of the BFS simply stops
expanding past it — consistent with ADR 0014/0019's existing stance
that a broken chain's *further* history is honestly unknown, not
guessed at. The commits already found remain valid; only ancestors
*beyond* the broken one are unreachable. This is directly tested
(`testMergeStopsWalkingPastACorruptAncestorWithoutCrashing`).

## Merge-base discovery

Given `oursCommit` and `theirsCommit`:

1. Compute `ancestorsOfOurs = allAncestors(ours)` and
   `ancestorsOfTheirs = allAncestors(theirs)`.
2. `commonAncestors = ancestorsOfOurs ∩ ancestorsOfTheirs` (by hex id).
3. If `commonAncestors` is empty, merging fails with
   `MergeError::NoCommonAncestor` — this implementation does not
   support merging genuinely unrelated histories (real Git's
   `--allow-unrelated-histories` equivalent is out of scope).

### Multiple merge bases (criss-cross merges)

A commit `c` in `commonAncestors` is **dominated** if some *other*
common ancestor `c2` has `c` in its own ancestor set (`c` is an
ancestor of `c2`, i.e. `c2` is strictly more recent and still common).
The **undominated** common ancestors are the real lowest common
ancestors — there can genuinely be more than one (a criss-cross merge
topology, where two branches each merged the other at some point).

### Deterministic merge-base selection

When more than one undominated common ancestor exists, this
implementation picks the one with the lexicographically smallest hex
id, deterministically. This is **not** Git's recursive strategy (which
would synthesize a virtual merge commit of the multiple bases and
merge against that) — it is a simpler, explicitly documented
tie-break, directly tested
(`testMergeBaseSelectionIsDeterministicAcrossMultipleCandidates`) to
prove it never varies run to run for the same input.

## Fast-forward merge

If `theirsCommit ∈ ancestorsOfOurs` (theirs is already reachable from
ours): `MergeOutcome::AlreadyUpToDate` — ours already contains
everything theirs has.

If `oursCommit ∈ ancestorsOfTheirs` (ours is an ancestor of theirs,
i.e. theirs is strictly ahead with no divergent work on ours' side):
`MergeOutcome::FastForward`, with `resultTree` set to theirs' own tree
directly (no new merge commit's content needs computing — this is
exactly "move the branch pointer forward," the same case real Git
fast-forwards).

Both checks are done before any three-way comparison — they're cheap
membership tests against the ancestor sets already computed for
merge-base discovery.

## Already-up-to-date case

Handled by the fast-forward check above (`theirsCommit ∈
ancestorsOfOurs`, which includes the trivial `oursCommit ==
theirsCommit` case since a commit is its own ancestor at depth 0).

## Three-way merge

Once a single merge-base commit is chosen, its tree, ours' tree, and
theirs' tree are each flattened to a `path -> blob ObjectId` map (file
leaves only — the same file-level granularity ADR 0019's diff already
uses). For every path appearing in any of the three maps, each side's
change relative to base is classified as **none** (identical to base,
or absent from both base and this side), **added**, **modified**, or
**removed**, then combined:

| Ours \ Theirs | none | added | modified | removed |
|---|---|---|---|---|
| **none** | keep base | take theirs | take theirs | remove |
| **added** | take ours | *see Added files* | — (path can't be "modified" on theirs if absent from base) | — |
| **modified** | take ours | — | *see Modified files* | *see Modified files (delete/modify)* |
| **removed** | remove | — | *see Modified files (delete/modify)* | remove (no conflict) |

("—" cells are unreachable: a path can't be simultaneously absent
from base *and* modified/removed, by definition of those categories.)

### Added files

Both sides adding the same new path: if the resulting blob id is
identical, no conflict (both independently added the identical
content). If the blob ids differ, this is an **Add/Add conflict**.

### Deleted files

Both sides removing the same path: no conflict, the path is removed
from the result.

### Modified files

Both sides modifying the same path relative to base: if the resulting
blob ids are identical, no conflict (both independently made the
identical change). If they differ, this is a **Modify/Modify
conflict**.

### Rename limitations

**Not supported.** Exactly as ADR 0019 already scoped: a file moved
from one path to another is seen as an unrelated removal at the old
path and an unrelated addition at the new path on whichever side moved
it — this can produce a spurious Add/Add or Delete/Modify-shaped
situation Git's rename detection would avoid. This implementation does
not attempt any content-similarity search.

### File/file conflicts

Covered above: Add/Add and Modify/Modify are both file/file conflicts
(the same path, two different pieces of file content).

### File/directory conflicts

Because merge operates on the same flat file-leaf representation as
ADR 0019's diff, a "path used as a file on one side and as a directory
on the other" conflict is detected the same way diff's
type-change case is reasoned about: if one side's flat map has an
exact-path entry `p` (a file) and the *other* side's flat map has any
entry whose path starts with `p + "/"` (meaning that side is using `p`
as a directory), this is a **File/Directory conflict** at path `p`,
reported once (not once per file inside the directory on the other
side).

### Directory/file conflicts

The mirror image of the above (ours treats `p` as a directory, theirs
has `p` as a file) is the identical conflict kind, from the other
side — reported the same way, at the same path.

## Conflict representation

```cpp
enum class MergeConflictKind {
    AddAdd,
    ModifyModify,
    ModifyDelete,   // ours modified, theirs deleted (or vice versa — see oursId/theirsId)
    FileDirectory,
};

struct MergeConflict {
    std::string path;
    MergeConflictKind kind;
    ObjectId oursId;    // zero ObjectId if ours has no file at this exact path
    ObjectId theirsId;  // zero ObjectId if theirs has no file at this exact path
};
```

A zero `ObjectId` (all 32 bytes `0x00`) is used as "not applicable"
rather than an `std::optional`, since it can never collide with a real
content-addressed id (that would require a SHA-256 preimage of an
all-zero digest) — the same convention ADR 0019's `DiffEntry` already
established for its Added/Removed sides.

## Deterministic conflict behavior

Conflict detection only ever compares content-addressed blob ids for
equality and walks sorted (`std::map`-ordered) path sets — there is no
hashing of file *content* beyond what `ObjectStore` already did when
the blobs were written, no randomness, and no dependence on traversal
order. The same three commits produce the identical `MergeResult`
(same outcome, same merge base, same conflict list in the same order)
on every run — directly tested
(`testMergeConflictDetectionIsDeterministic`).

## Malformed/corrupt object handling

- `oursCommit` or `theirsCommit` themselves failing to read as a real
  `Commit` → `MergeError::OursCommitUnreadable` /
  `TheirsCommitUnreadable`, checked before any traversal begins.
- A commit encountered mid-BFS (not a tip) being corrupt → silently
  stops that branch of the walk (see "Ancestor traversal" above), which
  can indirectly cause `NoCommonAncestor` if it hides the actual
  common ancestor — an honest "merge could not be completed with the
  information available," not a crash or a wrong answer presented with
  false confidence.
- A tree referenced by the chosen merge-base/ours/theirs commit
  failing to read or parse → `MergeError::CorruptTree`.

## Atomicity / failure behavior

`mergeCommits` **never writes anything to the object store** — it only
reads existing objects and, on a successful non-conflicting merge,
writes exactly one new `Tree` object (via the same tree-building logic
ADR 0017 established) representing the merge result. It never creates
a `Commit` object, never touches `RefStore`, and never partially
applies a conflicted merge. A caller that wants an actual merge commit
must take `MergeResult::resultTree`, build a `Commit` with `parents =
{oursCommit, theirsCommit}` themselves (via ADR 0007's existing
`Commit`/`serializeCommit`), and write it — this ADR provides the tree
computation, not repository mutation.

## What this implementation does NOT support

- Recursive/virtual-merge-base strategy for criss-cross merges (a
  deterministic single-candidate pick instead — see above).
- Rename/move detection of any kind.
- Merging genuinely unrelated histories (no common ancestor at all).
- Any conflict *resolution* — conflicts are reported, never guessed at
  or auto-resolved beyond the identical-content cases above.
- Line-level or content-level conflict markers — a conflict is
  reported at file granularity (this path, these two blob ids), not as
  a generated `<<<<<<<`-style merged file.
- Actually writing a merge `Commit` or moving any `RefStore` branch —
  by design (see "Atomicity" above), left to a caller/future CLI.
- Octopus merges (more than two parents at once) — only pairwise
  (ours, theirs) merges.

## Tested invariants

`13-developer-ecosystem/tests/oggit_merge_test.cpp` (36 hosted
assertions): linear history
fast-forward and its mirror (already-up-to-date); divergent branches
with a genuine common ancestor found by real multi-parent BFS (not
first-parent); nested-directory changes on independent paths merging
cleanly with no conflicts; the same file modified differently on both
sides producing a `ModifyModify` conflict with the correct base/ours/
theirs blob ids; both sides adding the same path with different
content producing `AddAdd`; one side modifying a path the other
deleted producing `ModifyDelete` with the correct populated/zero ids
on each side; a file/directory conflict detected via the flat-path
convention above; a corrupt/missing commit reached mid-ancestor-walk
not crashing and not fabricating a merge base beyond what's still
reachable; passing a non-Commit object (a `Tree` or `Blob` id) as
either tip failing cleanly with the specific unreadable-commit error;
and merge-base selection plus conflict detection being provably
deterministic across repeated calls with identical input.

## Consequences

OGGit's honestly-describable capability is now: content-addressed
objects (ADR 0007) + refs/HEAD/first-parent history (ADR 0014) +
staging (ADR 0017) + checkout (ADR 0018) + diff (ADR 0019) + a real,
full-DAG-based three-way merge computation (this ADR) that stops short
of writing the resulting commit or touching refs. The next real gap is
either wiring this into an actual `RefStore`-mutating merge workflow
(create the merge commit, advance the branch) or remote sync — neither
exists yet.

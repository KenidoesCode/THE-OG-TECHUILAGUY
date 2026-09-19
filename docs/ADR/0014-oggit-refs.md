# ADR 0014: OGGit refs, HEAD, and first-parent history

**Status:** Accepted. Continues Layer 13 directly from ADR 0007's
object store — branches/HEAD only; the index, working tree, diff,
checkout, and merge (all named in FR-DEV-1) remain unbuilt.

## Context

ADR 0007 built the object model (blob/tree/commit) but explicitly
deferred "refs/branches" as the next step — an object store with no
way to name a "current" commit or move a named pointer forward isn't
usable as a version-control system yet, even a minimal one.

## Decision

`13-developer-ecosystem/oggit/refs.cpp`'s `RefStore` operates on the
same repository root an `ObjectStore` uses, adding a `refs/heads/<name>`
file per branch (containing the branch's commit as a 64-character hex
id) and a `HEAD` file that is either symbolic (`"ref: <branchname>"`,
Git's own convention, followed in spirit) or detached (a bare commit
hex id). `resolveHead()` follows a symbolic HEAD to its branch's
current commit; a branch that moves is automatically reflected the
next time HEAD is resolved, without HEAD itself needing to change —
proven directly by `testMovingBranchMovesWhatHeadResolvesTo`, not just
assumed from the design.

`walkFirstParentHistory()` walks a commit's ancestry by always
following `parents[0]` — the same simplification Git's own
`git log --first-parent` makes explicit rather than hides, chosen
because a merge commit's *full* ancestry (all branches, correctly
deduplicated where they reconverge) needs a proper DAG traversal this
ADR doesn't attempt yet.

## What this is not

- **Not an index or working tree.** There is no staging area, no
  concept of "files on disk vs. what's committed," and no `status`/
  `add`/`commit` workflow — `Commit` objects are still constructed
  directly by a caller (as the tests do), not derived from real file
  changes.
- **Not diff, checkout, or merge.** Nothing here compares two trees,
  materializes a tree onto a real filesystem, or combines two
  divergent histories. `walkFirstParentHistory`'s explicit
  first-parent-only limitation is a preview of why real merge support
  is a separate, larger piece of work (it needs the *full* ancestry
  DAG to find a proper merge base).
- **Not remote synchronization.** No fetch, push, or object
  negotiation with another repository.
- **No ref locking/atomicity guarantees beyond the underlying
  filesystem's own write semantics** — concurrent writers moving the
  same branch simultaneously could race; this matters far less for a
  single local repository than it would for a shared/remote one, which
  doesn't exist yet anyway.

## Tested invariants

`13-developer-ecosystem/tests/oggit_refs_test.cpp` (18 hosted
assertions): branch set/get round trip, existence/deletion, moving an
existing branch to a new target; listing all created branches;
symbolic HEAD correctly reporting its branch name and resolving to
that branch's commit; detached HEAD correctly resolving to its own
commit and correctly failing `currentBranch()`; a branch moving while
HEAD symbolically points at it is automatically reflected by
`resolveHead()` without rewriting HEAD; `resolveHead()` failing
cleanly (not crashing, not returning a bogus id) both on a completely
fresh repository and on the common real case of HEAD naming a branch
that doesn't exist yet (before a repository's first commit);
first-parent history walking a linear chain correctly, newest-first;
first-parent history on a genuine merge commit following only
`parents[0]`, proven by constructing a real diverged-then-merged
three-commit-plus-merge scenario and checking the *other* branch's
commit is absent from the result; and history walking failing cleanly
(empty result, no crash) when the starting commit doesn't exist in the
object store.

## Consequences

Every claim about OGGit's version-control capability elsewhere in
this repository must describe it using the scope recorded here:
objects + refs/HEAD + first-parent history, not an index, not diff/
checkout/merge, not remote sync. This ADR is the single source of
truth for that distinction until a future ADR (covering the index,
diff, or merge) extends it.

# ADR 0018: OGGit checkout (tree materialization)

**Status:** Accepted. Continues Layer 13 from ADR 0007 (object store),
ADR 0014 (refs/HEAD), and ADR 0017 (index) — checkout only; diff,
merge, and remote sync remain unbuilt, exactly as ADR 0017 already
scoped them.

## Context

ADR 0017 built `writeTreeFromIndex`: real files on disk → a real,
hierarchical `Tree` object. That direction alone is only half a local
workflow — there was still no way to go the other way, from a `Tree`
object (whether just-built, or resolved from a `Commit` via
`refs.hpp`) back onto a real filesystem. ADR 0017's own "Consequences"
section named this as the more immediate of the two remaining
prerequisites (over diff), since checking out a tree is what makes an
existing commit's content usable at all — diff at least has plausible
value operating purely between two tree objects, but checkout is what
a caller actually needs to get real files back.

## Decision

`13-developer-ecosystem/oggit/checkout.cpp`'s `checkoutTree(store,
treeId, destinationDirectory)` recursively walks a `Tree` object
(read through the existing `ObjectStore`/`parseTree` from ADR 0007)
and materializes it onto disk: every file entry is written via
`std::ofstream` in binary mode (so byte-for-byte content, not just
text, round-trips exactly — proven directly by
`testFullStageCheckoutRoundTripPreservesEveryByte` using a real 256-
byte buffer including a `0x00` byte), and every directory entry
recurses into a subdirectory created with
`std::filesystem::create_directories`.

Two deliberate behavioral choices, both directly tested rather than
merely asserted in a comment:

- **Materialization, not reset.** A file already present at the
  destination but not named anywhere in the tree is left completely
  alone — `checkoutTree` never deletes or lists the destination's
  existing contents, it only ever creates/overwrites paths the tree
  actually names. This matches real Git's own default `checkout`
  behavior (untracked files survive) rather than a hard reset.
- **No transactional rollback.** If a failure (a missing/corrupt
  object, a filesystem error) occurs partway through a multi-file
  tree, whatever was already written before the failure stays on
  disk — `checkoutTree` returns the specific `CheckoutError`
  immediately rather than attempting to undo partial progress. A
  caller needing an all-or-nothing checkout is responsible for
  checking out into an empty/fresh destination.

## What this is not

- **Not diff, merge, or a working-tree "status" concept.** Nothing
  here compares the destination's existing content against the tree
  being checked out — files are simply (over)written unconditionally.
- **Not clean/reset.** As noted above, checkout never removes
  anything from the destination; there is no equivalent of `git
  clean`, `git reset --hard`, or `git checkout --force`'s
  untracked-file-clobbering variants.
- **No file-mode/permission bits, symlinks, or executable-bit
  preservation** — this is the direct mirror of ADR 0017's own
  `TreeEntry` limitation (file vs. directory only), so there is
  nothing to preserve on the way back out either.
- **No repository-level `checkout <branch>` command.** This is the
  low-level tree-to-filesystem primitive a future CLI/branch-switching
  command would be built on top of (resolving a branch to a commit to
  a tree via `refs.hpp`, then calling this) — that higher-level
  command doesn't exist yet.

## Tested invariants

`13-developer-ecosystem/tests/oggit_checkout_test.cpp` (23 hosted
assertions): checking out an empty tree creates only the destination
directory and nothing inside it; a single root-level file round-trips
its exact content; a genuinely nested three-file, two-directory-deep
hierarchy (reused from ADR 0017's own test fixture) round-trips
completely, independently verified at every level; checking out over
an existing file with different content fully replaces it (not
appends); checking out alongside an unrelated, untracked file leaves
that file's content completely untouched; checking out an unknown
tree id fails cleanly with `TreeNotFound` rather than crashing;
checking out a `Blob` id as if it were a `Tree` fails with `NotATree`;
a tree that references a blob id which was never actually written
(hand-constructed to simulate a corrupt/incomplete repository, not
reachable through the normal index path) fails with `ObjectReadError`
rather than crashing or silently skipping the missing content; and a
full stage → `writeTreeFromIndex` → `checkoutTree` → re-read-from-disk
round trip preserves binary content byte-for-byte, including files
that were never routed through `Index` at all being unaffected.

## Consequences

OGGit's honestly-describable local-workflow capability is now:
content-addressed objects (ADR 0007) + refs/HEAD/first-parent history
(ADR 0014) + staging real files into a real tree (ADR 0017) + real
tree materialization back onto disk (this ADR). The two directions
(index and checkout) are proven to be genuine inverses of each other
by the round-trip tests above, not just independently plausible in
isolation. Diff, merge, and remote sync remain the next real gaps —
diff is now the more immediate one, since it's the natural next step
once both "files → tree" and "tree → files" exist and a caller wants
to know what changed between two of either.

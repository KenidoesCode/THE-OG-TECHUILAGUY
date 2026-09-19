#pragma once

#include "object_store.hpp"

#include <string>
#include <vector>

// OGGit merge: a real, full-ancestry-DAG-based three-way merge
// computation — the next dependency named in
// docs/ADR/0019-oggit-diff.md's "Consequences" section, now that both
// checkout (ADR 0018) and diff (ADR 0019) exist. Unlike
// refs.hpp's walkFirstParentHistory (ADR 0014), merge-base discovery
// here walks EVERY parent of a commit, not just parents[0] — a real
// merge cannot be computed from first-parent-only history. See
// docs/ADR/0020-oggit-merge.md for the full design, including exactly
// how multiple candidate merge bases, fast-forwards, conflicts, and
// file/directory conflicts are each handled.

namespace oggit {

enum class MergeOutcome {
    AlreadyUpToDate,  // theirsCommit is already reachable from oursCommit
    FastForward,      // oursCommit is an ancestor of theirsCommit — just advance
    Merged,           // a real three-way merge produced a conflict-free result tree
    Conflict,         // one or more conflicts prevented an automatic merge
};

enum class MergeError {
    None,
    OursCommitUnreadable,    // oursCommit doesn't resolve to a real Commit object
    TheirsCommitUnreadable,  // theirsCommit doesn't resolve to a real Commit object
    NoCommonAncestor,        // the two histories share no common ancestor at all
    CorruptTree,             // a tree needed for the merge failed to read/parse
};

enum class MergeConflictKind {
    AddAdd,        // both sides added this path with different content
    ModifyModify,  // both sides changed this path (from a common base) to different content
    ModifyDelete,  // one side changed the path, the other deleted it
    FileDirectory, // one side has a file at this exact path, the other uses it as a directory
};

struct MergeConflict {
    std::string path;
    MergeConflictKind kind;

    // A zero ObjectId (all 32 bytes 0x00 — never a real content id)
    // means "this side has no file at this exact path" (e.g. deleted,
    // or the path is used as a directory on this side).
    ObjectId oursId;
    ObjectId theirsId;
};

struct MergeResult {
    MergeOutcome outcome = MergeOutcome::Conflict;
    MergeError error = MergeError::None;

    // The chosen merge-base commit id. Unset (zero) if `error` is
    // OursCommitUnreadable/TheirsCommitUnreadable/NoCommonAncestor.
    ObjectId mergeBase;

    // Valid only when outcome is FastForward (theirs' own tree,
    // unchanged) or Merged (a newly-built Tree object, already
    // written to `store`). Unset (zero) for AlreadyUpToDate or
    // Conflict.
    ObjectId resultTree;

    // Populated only when outcome is Conflict, sorted by path.
    std::vector<MergeConflict> conflicts;
};

// Computes (and, on success, writes) the three-way merge of
// `theirsCommit` into `oursCommit`. Never creates a Commit object,
// never touches any RefStore/branch — see docs/ADR/0020-oggit-merge.md's
// "Atomicity / failure behavior" for exactly why and what a caller
// building an actual merge commit needs to do with the result.
MergeResult mergeCommits(
    ObjectStore& store, const ObjectId& oursCommit, const ObjectId& theirsCommit
);

}  // namespace oggit

#pragma once

#include "object_store.hpp"

#include <string>
#include <vector>

// OGGit diff: compares two Tree objects and reports what changed
// between them, by full repository-relative path. This is the next
// named gap from docs/ADR/0018-oggit-checkout.md's "Consequences"
// section — with both "files -> tree" (ADR 0017) and "tree -> files"
// (ADR 0018) in place, comparing two trees (e.g. the working tree just
// staged vs. HEAD's committed tree, or any two commits' trees) is the
// natural next question a caller wants answered. See
// docs/ADR/0019-oggit-diff.md.

namespace oggit {

enum class DiffStatus {
    Added,      // path exists in newTree but not oldTree
    Removed,    // path exists in oldTree but not newTree
    Modified,   // path exists in both, pointing at a different blob
};

struct DiffEntry {
    std::string path;
    DiffStatus status;

    // Populated for Removed and Modified; left as a zero id (all
    // bytes 0x00 — never a real content-addressed id, since that
    // would require finding a SHA-256 preimage of an all-zero output)
    // for Added.
    ObjectId oldId;

    // Populated for Added and Modified; left as a zero id for Removed.
    ObjectId newId;
};

// Recursively compares two Tree objects (read through `store`) and
// returns every changed path, sorted by path. A path that is a file
// in one tree and a directory in the other is reported as a single
// Removed + a single Added entry at that same path (a type change is
// treated as "the old thing is gone, a new and different thing is
// there now," not as a Modified with any recursive structure), rather
// than as a Modified entry pointing at objects of two different
// types.
//
// Returns an empty vector, without throwing, if either tree id fails
// to read as a real Tree object (missing, corrupt, or actually a Blob)
// — a diff against a broken tree is honestly "unknown," not a guess,
// the same stance ADR 0014's walkFirstParentHistory already takes for
// a broken commit chain. Diffing a tree against itself (same id, or
// two different ids with identical recursive content) always returns
// an empty vector.
std::vector<DiffEntry> diffTrees(
    ObjectStore& store, const ObjectId& oldTreeId, const ObjectId& newTreeId
);

}  // namespace oggit

#pragma once

#include "object_store.hpp"

#include <string>

// OGGit checkout: the mirror image of index.hpp's writeTreeFromIndex.
// Where the index turns "files on disk" into a Tree object, checkout
// turns a Tree object back into real files on disk — the other half
// of a usable local workflow (`git add` + `git checkout`), and the
// remaining named prerequisite from docs/ADR/0017-oggit-index.md's
// "Consequences" section. See docs/ADR/0018-oggit-checkout.md.

namespace oggit {

enum class CheckoutError {
    None,
    TreeNotFound,     // treeId doesn't resolve to any object in the store
    NotATree,         // treeId resolves to an object, but it isn't a Tree
    ObjectReadError,   // a Tree or Blob referenced (possibly transitively) failed to read (missing/corrupt)
    IoError,          // a real filesystem operation (mkdir, file write) failed
};

// Recursively materializes the Tree identified by `treeId` onto disk
// at `destinationDirectory`, creating any missing directories
// (including `destinationDirectory` itself). Every file and directory
// named in the tree is created or overwritten with the tree's exact
// content; anything already present at `destinationDirectory` that
// is NOT named anywhere in the tree is left untouched — this is a
// materialization, not a clean/prune/reset operation (real Git's
// `checkout` has the same "leaves untracked files alone" behavior by
// default).
//
// On any failure partway through (a corrupt/missing object reached
// while recursing, or a filesystem error), the function stops and
// returns the specific error immediately — files already written
// before the failure are NOT rolled back. This mirrors
// writeTreeFromIndex's own "no partial-state pretending" stance: a
// caller that needs an all-or-nothing checkout must check the
// destination is empty (or otherwise recoverable) before calling
// this, since no transactional guarantee is made here.
CheckoutError checkoutTree(
    ObjectStore& store, const ObjectId& treeId, const std::string& destinationDirectory
);

}  // namespace oggit

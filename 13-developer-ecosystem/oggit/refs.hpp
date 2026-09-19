#pragma once

#include "object_store.hpp"

#include <string>
#include <vector>

// OGGit refs: named, mutable pointers to a commit (branches) plus
// HEAD (either symbolic — pointing at a branch name — or detached —
// pointing directly at a commit). Built on the same on-disk root as
// ObjectStore (../oggit/object_store.hpp), under a separate `refs/`
// subdirectory and a `HEAD` file, mirroring Git's own on-disk layout
// in spirit without claiming byte-for-byte format compatibility (see
// docs/ADR/0014-oggit-refs.md).

namespace oggit {

class RefStore {
public:
    // `rootDirectory` should be the same root an ObjectStore for this
    // repository was constructed with — refs and objects live under
    // the same repository root, in their own subdirectories.
    explicit RefStore(const std::string& rootDirectory);

    // Creates the branch if it doesn't exist, or moves it if it does.
    bool setBranch(const std::string& name, const ObjectId& commit);
    bool getBranch(const std::string& name, ObjectId& out) const;
    bool branchExists(const std::string& name) const;
    bool deleteBranch(const std::string& name);

    // Names only, not sorted in any particular guaranteed order beyond
    // whatever the filesystem directory iteration happens to return —
    // a caller that needs a stable display order should sort the
    // result itself.
    std::vector<std::string> listBranches() const;

    // HEAD is symbolic by default: it names a branch, and that
    // branch's own commit is what HEAD "points at." A repository with
    // no commits yet has HEAD symbolically pointing at a branch name
    // (conventionally "main") that itself doesn't exist as a ref yet
    // — this is a normal, valid state (an empty repository before its
    // first commit), not an error.
    bool setHeadToBranch(const std::string& branchName);

    // Detached HEAD: points directly at a specific commit, not at a
    // branch — the same concept Git uses for "checked out a specific
    // commit rather than the tip of any branch."
    bool setHeadDetached(const ObjectId& commit);

    bool isHeadDetached() const;

    // Resolves HEAD to the commit it currently identifies (following
    // the branch pointer if HEAD is symbolic). Returns false if HEAD
    // has never been set, or is symbolic and names a branch that
    // doesn't exist yet (the "empty repository, no commits" case) —
    // both are real, expected states a caller must handle, not
    // exceptional failures.
    bool resolveHead(ObjectId& out) const;

    // If HEAD is symbolic, returns the branch name it names (whether
    // or not that branch currently exists as an actual ref). Returns
    // false if HEAD is detached or has never been set.
    bool currentBranch(std::string& out) const;

private:
    std::string rootDirectory;

    std::string branchPath(const std::string& name) const;
    std::string headPath() const;
};

// Walks first-parent history starting from `startCommit` — i.e. for a
// merge commit (more than one parent), only ever follows parents[0],
// the same simplification Git's own `git log --first-parent` makes
// explicit rather than hidden. Returns the commits in order, starting
// with `startCommit` itself, until a commit with no parents (a root
// commit) is reached. Returns an empty vector, without throwing, if
// any commit along the way fails to read from `store` (a corrupt or
// missing object) — history for a partially-corrupt repository is
// honestly "unknown," not a guess at how far it got.
std::vector<ObjectId> walkFirstParentHistory(
    ObjectStore& store, const ObjectId& startCommit
);

}  // namespace oggit

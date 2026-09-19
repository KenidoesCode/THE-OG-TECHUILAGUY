// Real assertion-based tests for OGGit refs/HEAD/history
// (oggit/refs.cpp), built directly on the object store from ADR 0007.
// Real disk I/O against a temporary directory.

#include "../oggit/object_store.hpp"
#include "../oggit/refs.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

std::string tempDir(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("oggit_refs_test_" + name);
    std::filesystem::remove_all(path);
    return path.string();
}

oggit::ObjectId makeCommit(
    oggit::ObjectStore& store,
    const std::vector<oggit::ObjectId>& parents,
    const std::string& message
) {
    oggit::ObjectId dummyTree;
    oggit::parseObjectId(std::string(64, 'a'), dummyTree);

    oggit::Commit commit{dummyTree, parents, "author", message};
    return store.writeObject(oggit::ObjectType::Commit, oggit::serializeCommit(commit));
}

}  // namespace

void testSetAndGetBranch() {
    std::string dir = tempDir("branch_basic");
    oggit::ObjectStore store(dir);
    oggit::RefStore refs(dir);

    oggit::ObjectId commit = makeCommit(store, {}, "first commit");
    refs.setBranch("main", commit);

    oggit::ObjectId resolved;
    bool ok = refs.getBranch("main", resolved);

    check(ok && resolved == commit, "refs: setBranch/getBranch round-trips correctly");

    std::filesystem::remove_all(dir);
}

void testBranchExistsAndDelete() {
    std::string dir = tempDir("branch_lifecycle");
    oggit::ObjectStore store(dir);
    oggit::RefStore refs(dir);

    check(!refs.branchExists("feature"), "refs: a never-created branch does not exist");

    oggit::ObjectId commit = makeCommit(store, {}, "on feature branch");
    refs.setBranch("feature", commit);
    check(refs.branchExists("feature"), "refs: an existing branch is reported as existing");

    refs.deleteBranch("feature");
    check(!refs.branchExists("feature"), "refs: a deleted branch no longer exists");

    std::filesystem::remove_all(dir);
}

void testMovingBranchUpdatesItsTarget() {
    std::string dir = tempDir("branch_move");
    oggit::ObjectStore store(dir);
    oggit::RefStore refs(dir);

    oggit::ObjectId first = makeCommit(store, {}, "first");
    refs.setBranch("main", first);

    oggit::ObjectId second = makeCommit(store, {first}, "second");
    refs.setBranch("main", second);

    oggit::ObjectId resolved;
    refs.getBranch("main", resolved);

    check(resolved == second,
          "refs: setting an already-existing branch moves it to the new target");

    std::filesystem::remove_all(dir);
}

void testListBranches() {
    std::string dir = tempDir("branch_list");
    oggit::ObjectStore store(dir);
    oggit::RefStore refs(dir);

    oggit::ObjectId commit = makeCommit(store, {}, "shared commit");
    refs.setBranch("main", commit);
    refs.setBranch("develop", commit);
    refs.setBranch("feature-x", commit);

    auto branches = refs.listBranches();
    std::sort(branches.begin(), branches.end());

    check(branches.size() == 3 &&
          branches[0] == "develop" && branches[1] == "feature-x" && branches[2] == "main",
          "refs: listBranches() returns all created branches by name");

    std::filesystem::remove_all(dir);
}

void testHeadSymbolicToBranch() {
    std::string dir = tempDir("head_symbolic");
    oggit::ObjectStore store(dir);
    oggit::RefStore refs(dir);

    oggit::ObjectId commit = makeCommit(store, {}, "on main");
    refs.setBranch("main", commit);
    refs.setHeadToBranch("main");

    check(!refs.isHeadDetached(), "refs: HEAD is not detached after setHeadToBranch");

    std::string branchName;
    bool ok = refs.currentBranch(branchName);
    check(ok && branchName == "main", "refs: currentBranch() reports the branch HEAD points at");

    oggit::ObjectId resolved;
    bool resolvedOk = refs.resolveHead(resolved);
    check(resolvedOk && resolved == commit,
          "refs: resolveHead() follows the symbolic HEAD to the branch's actual commit");

    std::filesystem::remove_all(dir);
}

void testHeadDetached() {
    std::string dir = tempDir("head_detached");
    oggit::ObjectStore store(dir);
    oggit::RefStore refs(dir);

    oggit::ObjectId commit = makeCommit(store, {}, "detached target");
    refs.setHeadDetached(commit);

    check(refs.isHeadDetached(), "refs: HEAD is detached after setHeadDetached");

    std::string branchName;
    check(!refs.currentBranch(branchName),
          "refs: currentBranch() correctly fails when HEAD is detached");

    oggit::ObjectId resolved;
    bool ok = refs.resolveHead(resolved);
    check(ok && resolved == commit,
          "refs: resolveHead() returns the exact commit a detached HEAD points at");

    std::filesystem::remove_all(dir);
}

void testMovingBranchMovesWhatHeadResolvesTo() {
    std::string dir = tempDir("head_follows_branch");
    oggit::ObjectStore store(dir);
    oggit::RefStore refs(dir);

    oggit::ObjectId first = makeCommit(store, {}, "first");
    refs.setBranch("main", first);
    refs.setHeadToBranch("main");

    oggit::ObjectId second = makeCommit(store, {first}, "second");
    refs.setBranch("main", second);

    oggit::ObjectId resolved;
    refs.resolveHead(resolved);

    check(resolved == second,
          "refs: HEAD (symbolic) automatically reflects a branch's new "
          "target after the branch moves, without HEAD itself being rewritten");

    std::filesystem::remove_all(dir);
}

void testResolveHeadFailsOnEmptyRepository() {
    std::string dir = tempDir("empty_repo");
    oggit::RefStore refs(dir);  // no commits, no branches, HEAD never set

    oggit::ObjectId resolved;
    bool ok = refs.resolveHead(resolved);

    check(!ok, "refs: resolveHead() fails cleanly (no crash) on a "
               "fresh repository with no commits and HEAD never set");

    std::filesystem::remove_all(dir);
}

void testResolveHeadFailsWhenBranchDoesNotExistYet() {
    std::string dir = tempDir("head_points_nowhere");
    oggit::RefStore refs(dir);

    // A very common real state: HEAD symbolically points at "main"
    // before any commit has ever been made, so the "main" ref doesn't
    // exist as a file yet — this must be a clean failure, not a crash
    // or a bogus zero-valued ObjectId.
    refs.setHeadToBranch("main");

    oggit::ObjectId resolved;
    bool ok = refs.resolveHead(resolved);

    check(!ok, "refs: resolveHead() fails cleanly when HEAD names a "
               "branch that doesn't exist yet (a fresh repo before its first commit)");

    std::filesystem::remove_all(dir);
}

void testFirstParentHistoryWalksLinearChain() {
    std::string dir = tempDir("history_linear");
    oggit::ObjectStore store(dir);

    oggit::ObjectId c1 = makeCommit(store, {}, "root");
    oggit::ObjectId c2 = makeCommit(store, {c1}, "second");
    oggit::ObjectId c3 = makeCommit(store, {c2}, "third");

    auto history = oggit::walkFirstParentHistory(store, c3);

    check(history.size() == 3 &&
          history[0] == c3 && history[1] == c2 && history[2] == c1,
          "refs: walkFirstParentHistory returns commits newest-first "
          "down to the root commit for a linear chain");

    std::filesystem::remove_all(dir);
}

void testFirstParentHistoryOnMergeCommitFollowsOnlyFirstParent() {
    std::string dir = tempDir("history_merge");
    oggit::ObjectStore store(dir);

    oggit::ObjectId base = makeCommit(store, {}, "base");
    oggit::ObjectId mainBranch = makeCommit(store, {base}, "main work");
    oggit::ObjectId featureBranch = makeCommit(store, {base}, "feature work");
    oggit::ObjectId merge = makeCommit(store, {mainBranch, featureBranch}, "merge");

    auto history = oggit::walkFirstParentHistory(store, merge);

    check(history.size() == 3 &&
          history[0] == merge && history[1] == mainBranch && history[2] == base,
          "refs: walkFirstParentHistory on a merge commit follows only "
          "parents[0] (mainBranch), never featureBranch, matching Git's "
          "own --first-parent convention");

    std::filesystem::remove_all(dir);
}

void testFirstParentHistoryFailsCleanlyOnMissingCommit() {
    std::string dir = tempDir("history_missing");
    oggit::ObjectStore store(dir);

    oggit::ObjectId neverWritten;
    oggit::parseObjectId(std::string(64, 'f'), neverWritten);

    auto history = oggit::walkFirstParentHistory(store, neverWritten);

    check(history.empty(),
          "refs: walkFirstParentHistory returns empty (not a crash) "
          "when the starting commit doesn't exist in the object store");

    std::filesystem::remove_all(dir);
}

int main() {
    testSetAndGetBranch();
    testBranchExistsAndDelete();
    testMovingBranchUpdatesItsTarget();
    testListBranches();
    testHeadSymbolicToBranch();
    testHeadDetached();
    testMovingBranchMovesWhatHeadResolvesTo();
    testResolveHeadFailsOnEmptyRepository();
    testResolveHeadFailsWhenBranchDoesNotExistYet();
    testFirstParentHistoryWalksLinearChain();
    testFirstParentHistoryOnMergeCommitFollowsOnlyFirstParent();
    testFirstParentHistoryFailsCleanlyOnMissingCommit();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

// Real assertion-based tests for OGGit's merge (oggit/merge.cpp) —
// exercised through a real ObjectStore, real Index-built trees, and
// real hand-built Commit objects (there is no higher-level "commit"
// workflow yet — see docs/ADR/0020-oggit-merge.md's "Atomicity"
// section), all against real disk I/O in a temporary directory.

#include "../oggit/index.hpp"
#include "../oggit/merge.hpp"
#include "../oggit/object_store.hpp"

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

std::vector<uint8_t> toBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::string tempDir(const std::string& suffix) {
    auto path = std::filesystem::temp_directory_path() /
                ("oggit_merge_test_" + suffix + "_fixed");
    std::filesystem::remove_all(path);
    return path.string();
}

oggit::ObjectId makeCommit(oggit::ObjectStore& store, const oggit::ObjectId& tree,
                            const std::vector<oggit::ObjectId>& parents, const std::string& message) {
    oggit::Commit commit;
    commit.tree = tree;
    commit.parents = parents;
    commit.author = "test";
    commit.message = message;
    return store.writeObject(oggit::ObjectType::Commit, oggit::serializeCommit(commit));
}

// Builds a tree directly from a flat set of (path, content) pairs via
// a throwaway Index — the same real path both application code and
// the index/checkout/diff test suites use, not a hand-rolled shortcut.
oggit::ObjectId treeFrom(oggit::ObjectStore& store, const std::string& indexPath,
                          const std::vector<std::pair<std::string, std::string>>& files) {
    oggit::Index index(indexPath);
    for (const auto& [path, content] : files) {
        index.addFile(store, path, toBytes(content));
    }
    return index.writeTreeFromIndex(store);
}

const oggit::MergeConflict* findConflict(const std::vector<oggit::MergeConflict>& conflicts,
                                          const std::string& path) {
    for (const auto& c : conflicts) {
        if (c.path == path) return &c;
    }
    return nullptr;
}

}  // namespace

void testFastForward() {
    std::string dir = tempDir("fastforward");
    oggit::ObjectStore store(dir);

    oggit::ObjectId rootTree = treeFrom(store, dir + "/i1", {{"a.txt", "a"}});
    oggit::ObjectId root = makeCommit(store, rootTree, {}, "root");

    oggit::ObjectId aheadTree = treeFrom(store, dir + "/i2", {{"a.txt", "a"}, {"b.txt", "b"}});
    oggit::ObjectId ahead = makeCommit(store, aheadTree, {root}, "ahead");

    oggit::MergeResult result = oggit::mergeCommits(store, root, ahead);
    check(result.error == oggit::MergeError::None, "merge: fast-forward case reports no error");
    check(result.outcome == oggit::MergeOutcome::FastForward, "merge: merging a strict descendant into its ancestor is a fast-forward");
    check(result.resultTree == aheadTree, "merge: a fast-forward's result tree is exactly the descendant's own tree");

    std::filesystem::remove_all(dir);
}

void testAlreadyUpToDate() {
    std::string dir = tempDir("uptodate");
    oggit::ObjectStore store(dir);

    oggit::ObjectId rootTree = treeFrom(store, dir + "/i1", {{"a.txt", "a"}});
    oggit::ObjectId root = makeCommit(store, rootTree, {}, "root");
    oggit::ObjectId aheadTree = treeFrom(store, dir + "/i2", {{"a.txt", "a"}, {"b.txt", "b"}});
    oggit::ObjectId ahead = makeCommit(store, aheadTree, {root}, "ahead");

    oggit::MergeResult result = oggit::mergeCommits(store, ahead, root);
    check(result.outcome == oggit::MergeOutcome::AlreadyUpToDate,
          "merge: merging an ancestor into its own descendant reports already-up-to-date");

    oggit::MergeResult selfResult = oggit::mergeCommits(store, ahead, ahead);
    check(selfResult.outcome == oggit::MergeOutcome::AlreadyUpToDate,
          "merge: merging a commit into itself reports already-up-to-date");

    std::filesystem::remove_all(dir);
}

void testDivergentBranchesWithIndependentChangesMergeCleanly() {
    std::string dir = tempDir("divergent_clean");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {{"shared.txt", "base"}});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");

    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"shared.txt", "base"}, {"ours_only.txt", "ours"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");

    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {{"shared.txt", "base"}, {"theirs_only.txt", "theirs"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.error == oggit::MergeError::None, "merge: divergent branches with independent changes report no error");
    check(result.outcome == oggit::MergeOutcome::Merged, "merge: divergent branches with independent changes merge cleanly (no conflicts)");
    check(result.mergeBase == base, "merge: the real common ancestor is correctly identified as the merge base");

    oggit::ObjectType type;
    std::vector<uint8_t> content;
    store.readObject(result.resultTree, type, content);
    std::vector<oggit::TreeEntry> entries;
    oggit::parseTree(content, entries);
    check(entries.size() == 3, "merge: the merged tree contains the shared file plus both sides' independent additions");

    std::filesystem::remove_all(dir);
}

void testMergeRecursesIntoNestedDirectories() {
    std::string dir = tempDir("nested");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {{"src/main.og", "base main"}});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");

    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"src/main.og", "base main"}, {"src/lib/a.og", "ours a"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");

    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {{"src/main.og", "base main"}, {"src/lib/b.og", "theirs b"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Merged, "merge: independent nested-directory additions merge cleanly");

    std::vector<std::string> paths;
    oggit::ObjectType type;
    std::vector<uint8_t> content;
    store.readObject(result.resultTree, type, content);
    std::vector<oggit::TreeEntry> rootEntries;
    oggit::parseTree(content, rootEntries);
    check(rootEntries.size() == 1 && rootEntries[0].name == "src" && rootEntries[0].isDirectory,
          "merge: the merged tree's real nested 'src' directory is correctly reconstructed");

    if (!rootEntries.empty() && rootEntries[0].isDirectory) {
        std::vector<uint8_t> srcContent;
        store.readObject(rootEntries[0].id, type, srcContent);
        std::vector<oggit::TreeEntry> srcEntries;
        oggit::parseTree(srcContent, srcEntries);
        bool hasLib = false;
        for (const auto& e : srcEntries) if (e.name == "lib") hasLib = true;
        check(srcEntries.size() == 2 && hasLib, "merge: 'src/lib' is correctly reconstructed from both sides' independent nested additions");
    }

    std::filesystem::remove_all(dir);
}

void testModifyModifyConflict() {
    std::string dir = tempDir("modifymodify");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {{"a.txt", "base content"}});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");

    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"a.txt", "ours content"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");
    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {{"a.txt", "theirs content"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Conflict, "merge: both sides modifying the same file differently produces a conflict");
    const oggit::MergeConflict* conflict = findConflict(result.conflicts, "a.txt");
    check(conflict != nullptr && conflict->kind == oggit::MergeConflictKind::ModifyModify,
          "merge: the conflict is correctly classified as ModifyModify");

    oggit::ObjectId oursBlob, theirsBlob;
    oggit::Index(dir + "/iours").getStagedBlob("a.txt", oursBlob);
    if (conflict != nullptr) {
        check(!(conflict->oursId == conflict->theirsId), "merge: a ModifyModify conflict's ours/theirs ids are genuinely different");
    }

    std::filesystem::remove_all(dir);
}

void testAddAddConflict() {
    std::string dir = tempDir("addadd");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");

    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"new.txt", "ours version"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");
    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {{"new.txt", "theirs version"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Conflict, "merge: both sides independently adding the same new path with different content produces a conflict");
    const oggit::MergeConflict* conflict = findConflict(result.conflicts, "new.txt");
    check(conflict != nullptr && conflict->kind == oggit::MergeConflictKind::AddAdd,
          "merge: the conflict is correctly classified as AddAdd");

    std::filesystem::remove_all(dir);
}

void testAddAddWithIdenticalContentIsNotAConflict() {
    std::string dir = tempDir("addadd_identical");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");

    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"new.txt", "same content"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");
    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {{"new.txt", "same content"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Merged,
          "merge: both sides independently adding the identical new file (same content) is not a conflict");

    std::filesystem::remove_all(dir);
}

void testModifyDeleteConflict() {
    std::string dir = tempDir("modifydelete");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {{"a.txt", "base content"}});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");

    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"a.txt", "ours modified it"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");
    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Conflict, "merge: one side modifying a file the other side deleted produces a conflict");
    const oggit::MergeConflict* conflict = findConflict(result.conflicts, "a.txt");
    check(conflict != nullptr && conflict->kind == oggit::MergeConflictKind::ModifyDelete,
          "merge: the conflict is correctly classified as ModifyDelete");
    check(conflict != nullptr && !(conflict->oursId == oggit::ObjectId{}),
          "merge: the ModifyDelete conflict's ours side (the one that kept/modified the file) has a real, non-zero blob id");

    std::filesystem::remove_all(dir);
}

void testBothSidesDeletingSameFileIsNotAConflict() {
    std::string dir = tempDir("bothdelete");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {{"a.txt", "will be deleted"}, {"b.txt", "stays"}});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");
    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"b.txt", "stays"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");
    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {{"b.txt", "stays"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Merged, "merge: both sides independently deleting the same file is not a conflict");

    std::filesystem::remove_all(dir);
}

void testFileDirectoryConflict() {
    std::string dir = tempDir("filedir");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {{"unrelated.txt", "u"}});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");

    // Ours keeps "config" as a plain file (added fresh, relative to base).
    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"unrelated.txt", "u"}, {"config", "a plain file"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");

    // Theirs independently turns "config" into a directory of files.
    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs",
                                           {{"unrelated.txt", "u"}, {"config/a.txt", "a"}, {"config/b.txt", "b"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Conflict, "merge: a file staged where the other side has a directory produces a conflict");
    const oggit::MergeConflict* conflict = findConflict(result.conflicts, "config");
    check(conflict != nullptr && conflict->kind == oggit::MergeConflictKind::FileDirectory,
          "merge: the conflict is correctly classified as FileDirectory at the exact colliding path");
    check(conflict != nullptr && !(conflict->oursId == oggit::ObjectId{}) && conflict->theirsId == oggit::ObjectId{},
          "merge: the FileDirectory conflict correctly reports which side has the real file (ours) vs. the directory (theirs, reported as absent-as-a-file)");

    std::filesystem::remove_all(dir);
}

void testOursCommitUnreadableWrongType() {
    std::string dir = tempDir("wrongtype_ours");
    oggit::ObjectStore store(dir);
    oggit::ObjectId blobId = store.writeObject(oggit::ObjectType::Blob, toBytes("just a blob"));
    oggit::ObjectId treeTree = treeFrom(store, dir + "/i", {{"a.txt", "a"}});
    oggit::ObjectId realCommit = makeCommit(store, treeTree, {}, "real");

    oggit::MergeResult result = oggit::mergeCommits(store, blobId, realCommit);
    check(result.error == oggit::MergeError::OursCommitUnreadable,
          "merge: passing a Blob id (not a Commit) as ours fails cleanly with OursCommitUnreadable, not a crash");

    std::filesystem::remove_all(dir);
}

void testTheirsCommitMissingObject() {
    std::string dir = tempDir("missing_theirs");
    oggit::ObjectStore store(dir);
    oggit::ObjectId treeTree = treeFrom(store, dir + "/i", {{"a.txt", "a"}});
    oggit::ObjectId realCommit = makeCommit(store, treeTree, {}, "real");

    oggit::ObjectId neverWritten;
    oggit::parseObjectId(std::string(64, 'c'), neverWritten);

    oggit::MergeResult result = oggit::mergeCommits(store, realCommit, neverWritten);
    check(result.error == oggit::MergeError::TheirsCommitUnreadable,
          "merge: an id the object store has never seen fails cleanly with TheirsCommitUnreadable, not a crash");

    std::filesystem::remove_all(dir);
}

void testMalformedCommitContent() {
    std::string dir = tempDir("malformed");
    oggit::ObjectStore store(dir);
    // A real object of type Commit whose bytes don't actually parse as
    // one — a genuinely corrupt/malformed commit, not just a missing one.
    oggit::ObjectId malformed = store.writeObject(oggit::ObjectType::Commit, toBytes("not a real serialized commit"));
    oggit::ObjectId treeTree = treeFrom(store, dir + "/i", {{"a.txt", "a"}});
    oggit::ObjectId realCommit = makeCommit(store, treeTree, {}, "real");

    oggit::MergeResult result = oggit::mergeCommits(store, malformed, realCommit);
    check(result.error == oggit::MergeError::OursCommitUnreadable,
          "merge: a real object of type Commit with malformed content fails cleanly, not a crash");

    std::filesystem::remove_all(dir);
}

void testCorruptAncestorMidWalkDoesNotCrashOrFabricateAMergeBase() {
    std::string dir = tempDir("corrupt_ancestor");
    oggit::ObjectStore store(dir);

    oggit::ObjectId realTree = treeFrom(store, dir + "/i", {{"a.txt", "a"}});

    // A commit that will be OURS' parent, but whose own content is
    // corrupt — real ancestry beyond it (a hypothetical shared base)
    // becomes genuinely unreachable, exactly the scenario
    // docs/ADR/0020-oggit-merge.md's "Ancestor traversal" describes.
    oggit::ObjectId brokenParent = store.writeObject(oggit::ObjectType::Commit, toBytes("corrupt"));
    oggit::ObjectId ours = makeCommit(store, realTree, {brokenParent}, "ours");

    oggit::ObjectId base = makeCommit(store, realTree, {}, "unrelated real base");
    oggit::ObjectId theirs = makeCommit(store, realTree, {base}, "theirs");

    oggit::MergeResult result = oggit::mergeCommits(store, ours, theirs);
    check(result.outcome == oggit::MergeOutcome::Conflict || result.error != oggit::MergeError::None,
          "merge: a corrupt ancestor mid-walk never produces a silently-successful merge");
    check(result.error == oggit::MergeError::NoCommonAncestor,
          "merge: a corrupt ancestor that hides the real common ancestor honestly reports NoCommonAncestor rather than crashing or guessing");

    std::filesystem::remove_all(dir);
}

void testMergeIsDeterministicAcrossRepeatedCalls() {
    std::string dir = tempDir("deterministic");
    oggit::ObjectStore store(dir);

    oggit::ObjectId baseTree = treeFrom(store, dir + "/ibase", {{"shared.txt", "base"}});
    oggit::ObjectId base = makeCommit(store, baseTree, {}, "base");
    oggit::ObjectId oursTree = treeFrom(store, dir + "/iours", {{"shared.txt", "ours"}});
    oggit::ObjectId ours = makeCommit(store, oursTree, {base}, "ours");
    oggit::ObjectId theirsTree = treeFrom(store, dir + "/itheirs", {{"shared.txt", "theirs"}});
    oggit::ObjectId theirs = makeCommit(store, theirsTree, {base}, "theirs");

    oggit::MergeResult first = oggit::mergeCommits(store, ours, theirs);
    oggit::MergeResult second = oggit::mergeCommits(store, ours, theirs);

    check(first.outcome == second.outcome, "merge: repeated calls with identical input produce the identical outcome");
    check(first.mergeBase == second.mergeBase, "merge: repeated calls with identical input produce the identical merge base");
    check(first.conflicts.size() == second.conflicts.size() &&
              !first.conflicts.empty() &&
              first.conflicts[0].path == second.conflicts[0].path &&
              first.conflicts[0].kind == second.conflicts[0].kind,
          "merge: repeated calls with identical input produce the identical conflict list");

    std::filesystem::remove_all(dir);
}

void testMergeBaseSelectionIsDeterministicWithMultipleLowestCommonAncestors() {
    std::string dir = tempDir("crisscross");
    oggit::ObjectStore store(dir);

    oggit::ObjectId sharedTree = treeFrom(store, dir + "/i", {{"a.txt", "a"}});
    oggit::ObjectId root = makeCommit(store, sharedTree, {}, "root");
    oggit::ObjectId branchA = makeCommit(store, sharedTree, {root}, "branch A");
    oggit::ObjectId branchB = makeCommit(store, sharedTree, {root}, "branch B");

    // Two independent merges of A and B, with different parent order
    // (and therefore genuinely different commit ids), each becoming
    // one arm of a criss-cross: neither A nor B is an ancestor of the
    // other, so both remain "lowest common ancestor" candidates for
    // any pair of tips descending separately from crossX and crossY.
    oggit::ObjectId crossX = makeCommit(store, sharedTree, {branchA, branchB}, "cross X");
    oggit::ObjectId crossY = makeCommit(store, sharedTree, {branchB, branchA}, "cross Y");

    oggit::ObjectId ours = makeCommit(store, sharedTree, {crossX}, "ours tip");
    oggit::ObjectId theirs = makeCommit(store, sharedTree, {crossY}, "theirs tip");

    oggit::MergeResult first = oggit::mergeCommits(store, ours, theirs);
    oggit::MergeResult second = oggit::mergeCommits(store, ours, theirs);

    check(first.error == oggit::MergeError::None, "merge: a criss-cross topology with multiple lowest common ancestors still resolves without error");
    check(first.mergeBase == branchA || first.mergeBase == branchB,
          "merge: the chosen merge base is genuinely one of the two lowest common ancestors, not root/crossX/crossY");
    check(first.mergeBase == second.mergeBase,
          "merge: the deterministic tie-break picks the identical merge base across repeated calls on the same criss-cross input");

    std::filesystem::remove_all(dir);
}

int main() {
    testFastForward();
    testAlreadyUpToDate();
    testDivergentBranchesWithIndependentChangesMergeCleanly();
    testMergeRecursesIntoNestedDirectories();
    testModifyModifyConflict();
    testAddAddConflict();
    testAddAddWithIdenticalContentIsNotAConflict();
    testModifyDeleteConflict();
    testBothSidesDeletingSameFileIsNotAConflict();
    testFileDirectoryConflict();
    testOursCommitUnreadableWrongType();
    testTheirsCommitMissingObject();
    testMalformedCommitContent();
    testCorruptAncestorMidWalkDoesNotCrashOrFabricateAMergeBase();
    testMergeIsDeterministicAcrossRepeatedCalls();
    testMergeBaseSelectionIsDeterministicWithMultipleLowestCommonAncestors();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

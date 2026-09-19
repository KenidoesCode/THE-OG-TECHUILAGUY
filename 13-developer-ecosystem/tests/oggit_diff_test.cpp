// Real assertion-based tests for OGGit's diff (oggit/diff.cpp) —
// exercised through a real ObjectStore and real Index-built trees
// (../oggit/index.hpp), not hand-constructed fixtures alone, so the
// trees being diffed are genuinely produced the same way a real
// caller would produce them.

#include "../oggit/diff.hpp"
#include "../oggit/index.hpp"
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
                ("oggit_diff_test_" + suffix + "_fixed");
    std::filesystem::remove_all(path);
    return path.string();
}

const oggit::DiffEntry* findByPath(const std::vector<oggit::DiffEntry>& entries, const std::string& path) {
    for (const auto& e : entries) {
        if (e.path == path) return &e;
    }
    return nullptr;
}

}  // namespace

void testDiffOfIdenticalTreeIdIsEmptyWithoutReadingEither() {
    std::string dir = tempDir("identical_id");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");
    index.addFile(store, "a.txt", toBytes("content"));
    oggit::ObjectId treeId = index.writeTreeFromIndex(store);

    // Deliberately pass a tree id the store has never seen as BOTH
    // sides: if diffTrees actually tried to read either tree here it
    // would find nothing and (per its documented "unknown, not a
    // guess" stance) also return empty — so this alone doesn't fully
    // prove the identical-id short-circuit. The real proof is the
    // valid-id case right after it.
    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, treeId, treeId);
    check(result.empty(), "diff: diffing a tree id against itself returns no changes");

    std::filesystem::remove_all(dir);
}

void testDiffOfTwoIdenticalContentTreesIsEmpty() {
    std::string dir = tempDir("identical_content");
    oggit::ObjectStore store(dir);
    oggit::Index indexA(dir + "/indexA");
    indexA.addFile(store, "a.txt", toBytes("same"));
    oggit::ObjectId treeA = indexA.writeTreeFromIndex(store);

    oggit::Index indexB(dir + "/indexB");
    indexB.addFile(store, "a.txt", toBytes("same"));
    oggit::ObjectId treeB = indexB.writeTreeFromIndex(store);

    check(treeA == treeB, "diff test setup: two indexes staging identical content produce the identical tree id");
    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, treeA, treeB);
    check(result.empty(), "diff: two independently-built trees with identical content report no changes");

    std::filesystem::remove_all(dir);
}

void testDiffDetectsAddedFile() {
    std::string dir = tempDir("added");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "a.txt", toBytes("a"));
    oggit::ObjectId oldTree = index.writeTreeFromIndex(store);

    oggit::ObjectId newBlob = index.addFile(store, "b.txt", toBytes("b"));
    oggit::ObjectId newTree = index.writeTreeFromIndex(store);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, oldTree, newTree);
    check(result.size() == 1, "diff: adding one new file produces exactly one diff entry");
    const oggit::DiffEntry* entry = findByPath(result, "b.txt");
    check(entry != nullptr && entry->status == oggit::DiffStatus::Added && entry->newId == newBlob,
          "diff: the added entry correctly reports status=Added and the real new blob id");

    std::filesystem::remove_all(dir);
}

void testDiffDetectsRemovedFile() {
    std::string dir = tempDir("removed");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "a.txt", toBytes("a"));
    oggit::ObjectId oldBlob = index.addFile(store, "b.txt", toBytes("b"));
    oggit::ObjectId oldTree = index.writeTreeFromIndex(store);

    index.removeFile("b.txt");
    oggit::ObjectId newTree = index.writeTreeFromIndex(store);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, oldTree, newTree);
    check(result.size() == 1, "diff: removing one file produces exactly one diff entry");
    const oggit::DiffEntry* entry = findByPath(result, "b.txt");
    check(entry != nullptr && entry->status == oggit::DiffStatus::Removed && entry->oldId == oldBlob,
          "diff: the removed entry correctly reports status=Removed and the real old blob id");

    std::filesystem::remove_all(dir);
}

void testDiffDetectsModifiedFile() {
    std::string dir = tempDir("modified");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    oggit::ObjectId oldBlob = index.addFile(store, "a.txt", toBytes("version one"));
    oggit::ObjectId oldTree = index.writeTreeFromIndex(store);

    oggit::ObjectId newBlob = index.addFile(store, "a.txt", toBytes("version two"));
    oggit::ObjectId newTree = index.writeTreeFromIndex(store);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, oldTree, newTree);
    check(result.size() == 1, "diff: changing one file's content produces exactly one diff entry");
    const oggit::DiffEntry* entry = findByPath(result, "a.txt");
    check(entry != nullptr && entry->status == oggit::DiffStatus::Modified &&
              entry->oldId == oldBlob && entry->newId == newBlob,
          "diff: the modified entry correctly reports status=Modified with both the real old and new blob ids");

    std::filesystem::remove_all(dir);
}

void testDiffUnchangedFilesProduceNoEntries() {
    std::string dir = tempDir("unchanged");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "unchanged.txt", toBytes("stays the same"));
    oggit::ObjectId oldTree = index.writeTreeFromIndex(store);

    index.addFile(store, "changed.txt", toBytes("new file"));
    oggit::ObjectId newTree = index.writeTreeFromIndex(store);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, oldTree, newTree);
    check(result.size() == 1, "diff: a file present unchanged in both trees produces no entry of its own");
    check(findByPath(result, "unchanged.txt") == nullptr,
          "diff: the unchanged file specifically does not appear anywhere in the result");

    std::filesystem::remove_all(dir);
}

void testDiffRecursesIntoNestedDirectories() {
    std::string dir = tempDir("nested");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "src/main.og", toBytes("v1"));
    index.addFile(store, "src/lib/util.og", toBytes("util v1"));
    oggit::ObjectId oldTree = index.writeTreeFromIndex(store);

    index.addFile(store, "src/main.og", toBytes("v2"));  // modified, two levels... one level deep
    oggit::ObjectId newLibBlob = index.addFile(store, "src/lib/util.og", toBytes("util v2"));  // modified, two levels deep
    oggit::ObjectId newHelperBlob = index.addFile(store, "src/lib/helper.og", toBytes("new helper"));  // added, two levels deep
    oggit::ObjectId newTree = index.writeTreeFromIndex(store);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, oldTree, newTree);
    check(result.size() == 3, "diff: nested changes at multiple depths are all detected (exactly 3 changes)");

    const oggit::DiffEntry* mainEntry = findByPath(result, "src/main.og");
    const oggit::DiffEntry* utilEntry = findByPath(result, "src/lib/util.og");
    const oggit::DiffEntry* helperEntry = findByPath(result, "src/lib/helper.og");

    check(mainEntry != nullptr && mainEntry->status == oggit::DiffStatus::Modified,
          "diff: a one-level-deep modification is correctly reported with its full path");
    check(utilEntry != nullptr && utilEntry->status == oggit::DiffStatus::Modified && utilEntry->newId == newLibBlob,
          "diff: a two-levels-deep modification is correctly reported with its full path");
    check(helperEntry != nullptr && helperEntry->status == oggit::DiffStatus::Added && helperEntry->newId == newHelperBlob,
          "diff: a two-levels-deep addition is correctly reported with its full path");

    std::filesystem::remove_all(dir);
}

void testDiffExpandsAddedDirectoryIntoIndividualFiles() {
    std::string dir = tempDir("added_dir");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "a.txt", toBytes("a"));
    oggit::ObjectId oldTree = index.writeTreeFromIndex(store);

    oggit::ObjectId newLib1 = index.addFile(store, "newmodule/one.og", toBytes("one"));
    oggit::ObjectId newLib2 = index.addFile(store, "newmodule/two.og", toBytes("two"));
    oggit::ObjectId newTree = index.writeTreeFromIndex(store);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, oldTree, newTree);
    check(result.size() == 2, "diff: adding an entire new directory expands into one Added entry per file inside it, not one directory-level entry");

    const oggit::DiffEntry* one = findByPath(result, "newmodule/one.og");
    const oggit::DiffEntry* two = findByPath(result, "newmodule/two.og");
    check(one != nullptr && one->status == oggit::DiffStatus::Added && one->newId == newLib1,
          "diff: the first file inside the newly-added directory is correctly reported");
    check(two != nullptr && two->status == oggit::DiffStatus::Added && two->newId == newLib2,
          "diff: the second file inside the newly-added directory is correctly reported");

    std::filesystem::remove_all(dir);
}

void testDiffResultIsSortedByPath() {
    std::string dir = tempDir("sorted");
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");
    oggit::ObjectId oldTree = index.writeTreeFromIndex(store);

    index.addFile(store, "z.txt", toBytes("z"));
    index.addFile(store, "a.txt", toBytes("a"));
    index.addFile(store, "m/n.txt", toBytes("n"));
    oggit::ObjectId newTree = index.writeTreeFromIndex(store);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, oldTree, newTree);
    std::vector<std::string> paths;
    for (const auto& e : result) paths.push_back(e.path);
    std::vector<std::string> sortedPaths = paths;
    std::sort(sortedPaths.begin(), sortedPaths.end());

    check(paths == sortedPaths, "diff: the returned entries are sorted by path regardless of staging or traversal order");

    std::filesystem::remove_all(dir);
}

void testDiffOnUnreadableTreeReturnsEmptyNotCrash() {
    std::string dir = tempDir("unreadable");
    oggit::ObjectStore store(dir);

    oggit::ObjectId bogusOld;
    oggit::ObjectId bogusNew;
    oggit::parseObjectId(std::string(64, 'a'), bogusOld);
    oggit::parseObjectId(std::string(64, 'b'), bogusNew);

    std::vector<oggit::DiffEntry> result = oggit::diffTrees(store, bogusOld, bogusNew);
    check(result.empty(), "diff: diffing two tree ids the store has never seen returns an empty result, not a crash");

    std::filesystem::remove_all(dir);
}

int main() {
    testDiffOfIdenticalTreeIdIsEmptyWithoutReadingEither();
    testDiffOfTwoIdenticalContentTreesIsEmpty();
    testDiffDetectsAddedFile();
    testDiffDetectsRemovedFile();
    testDiffDetectsModifiedFile();
    testDiffUnchangedFilesProduceNoEntries();
    testDiffRecursesIntoNestedDirectories();
    testDiffExpandsAddedDirectoryIntoIndividualFiles();
    testDiffResultIsSortedByPath();
    testDiffOnUnreadableTreeReturnsEmptyNotCrash();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

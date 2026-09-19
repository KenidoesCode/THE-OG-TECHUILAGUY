// Real assertion-based tests for OGGit's index/staging area
// (oggit/index.cpp) — exercised through a real ObjectStore backed by
// real disk I/O against a temporary directory (removed at the end of
// the run), not an in-memory mock.

#include "../oggit/index.hpp"
#include "../oggit/object_store.hpp"

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

std::string tempDir() {
    auto path = std::filesystem::temp_directory_path() /
                "oggit_index_test_XXXXXX_fixed";
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path.string();
}

}  // namespace

void testAddFileStagesAndIsRetrievable() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    oggit::ObjectId id = index.addFile(store, "src/main.og", toBytes("fn main() {}"));

    oggit::ObjectId out;
    check(index.isStaged("src/main.og"), "index: a staged path reports as staged");
    check(index.getStagedBlob("src/main.og", out) && out == id,
          "index: the staged blob id matches what addFile returned");
    check(store.exists(id), "index: addFile actually wrote a real blob into the object store");

    std::filesystem::remove_all(dir);
}

void testAddFileIsIdempotentForIdenticalContent() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    oggit::ObjectId first = index.addFile(store, "a.txt", toBytes("same content"));
    oggit::ObjectId second = index.addFile(store, "a.txt", toBytes("same content"));

    check(first == second, "index: staging identical content twice produces the identical blob id");

    std::filesystem::remove_all(dir);
}

void testAddFileReplacesPreviousStagedContent() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    oggit::ObjectId oldId = index.addFile(store, "a.txt", toBytes("version one"));
    oggit::ObjectId newId = index.addFile(store, "a.txt", toBytes("version two"));

    oggit::ObjectId out;
    index.getStagedBlob("a.txt", out);
    check(!(oldId == newId), "index: different content produces a different blob id");
    check(out == newId, "index: re-staging the same path replaces its blob id rather than adding a second entry");
    check(index.size() == 1, "index: re-staging the same path does not grow the index size");

    std::filesystem::remove_all(dir);
}

void testRemoveFile() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "a.txt", toBytes("content"));
    check(index.removeFile("a.txt"), "index: removing a staged path succeeds");
    check(!index.isStaged("a.txt"), "index: a removed path no longer reports as staged");
    check(!index.removeFile("a.txt"), "index: removing an already-unstaged path returns false");

    std::filesystem::remove_all(dir);
}

void testPathNormalization() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "//a//b/", toBytes("content"));
    check(index.isStaged("a/b"), "index: leading/trailing/duplicate slashes normalize to the same canonical path");

    std::filesystem::remove_all(dir);
}

void testSaveAndReloadRoundTrip() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    std::string indexPath = dir + "/index";

    oggit::ObjectId id;
    {
        oggit::Index index(indexPath);
        id = index.addFile(store, "src/main.og", toBytes("fn main() {}"));
        index.addFile(store, "README.md", toBytes("# hi"));
        check(index.save(), "index: saving to disk succeeds");
    }
    {
        oggit::Index reloaded(indexPath);
        oggit::ObjectId out;
        check(reloaded.size() == 2, "index: a reloaded index has the same number of entries that were saved");
        check(reloaded.getStagedBlob("src/main.og", out) && out == id,
              "index: a reloaded index recovers the exact same blob id for each path");
    }

    std::filesystem::remove_all(dir);
}

void testClear() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "a.txt", toBytes("x"));
    index.addFile(store, "b.txt", toBytes("y"));
    index.clear();

    check(index.size() == 0, "index: clear() empties all staged entries");

    std::filesystem::remove_all(dir);
}

void testWriteTreeFromIndexEmptyIndexProducesEmptyTree() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    oggit::ObjectId treeId = index.writeTreeFromIndex(store);

    oggit::ObjectType type;
    std::vector<uint8_t> content;
    oggit::StoreError err = store.readObject(treeId, type, content);
    std::vector<oggit::TreeEntry> entries;
    oggit::parseTree(content, entries);

    check(err == oggit::StoreError::None, "index: writeTreeFromIndex on an empty index writes a real, readable tree object");
    check(type == oggit::ObjectType::Tree, "index: writeTreeFromIndex on an empty index writes an object of type Tree");
    check(entries.empty(), "index: writeTreeFromIndex on an empty index produces a tree with zero entries");

    std::filesystem::remove_all(dir);
}

void testWriteTreeFromIndexSingleFileAtRoot() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    oggit::ObjectId blobId = index.addFile(store, "README.md", toBytes("# hi"));
    oggit::ObjectId treeId = index.writeTreeFromIndex(store);

    oggit::ObjectType type;
    std::vector<uint8_t> content;
    store.readObject(treeId, type, content);
    std::vector<oggit::TreeEntry> entries;
    oggit::parseTree(content, entries);

    check(entries.size() == 1, "index: a single root-level staged file produces a tree with exactly one entry");
    check(entries.size() == 1 && entries[0].name == "README.md" && entries[0].id == blobId && !entries[0].isDirectory,
          "index: the root tree's single entry correctly names, points at, and types the staged file");

    std::filesystem::remove_all(dir);
}

void testWriteTreeFromIndexBuildsRealNestedHierarchy() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    oggit::ObjectId mainBlob = index.addFile(store, "src/main.og", toBytes("fn main() {}"));
    oggit::ObjectId libBlob = index.addFile(store, "src/lib/util.og", toBytes("fn util() {}"));
    oggit::ObjectId readmeBlob = index.addFile(store, "README.md", toBytes("# hi"));

    oggit::ObjectId rootTreeId = index.writeTreeFromIndex(store);

    oggit::ObjectType rootType;
    std::vector<uint8_t> rootContent;
    store.readObject(rootTreeId, rootType, rootContent);
    std::vector<oggit::TreeEntry> rootEntries;
    oggit::parseTree(rootContent, rootEntries);

    check(rootEntries.size() == 2, "index: nested staging produces a root tree with one entry per top-level name (README.md, src/)");

    const oggit::TreeEntry* srcEntry = nullptr;
    const oggit::TreeEntry* readmeEntry = nullptr;
    for (const auto& e : rootEntries) {
        if (e.name == "src") srcEntry = &e;
        if (e.name == "README.md") readmeEntry = &e;
    }

    check(srcEntry != nullptr && srcEntry->isDirectory, "index: 'src' is correctly written as a directory (Tree) entry");
    check(readmeEntry != nullptr && !readmeEntry->isDirectory && readmeEntry->id == readmeBlob,
          "index: 'README.md' is correctly written as a file (Blob) entry pointing at its real blob id");

    if (srcEntry != nullptr) {
        oggit::ObjectType srcType;
        std::vector<uint8_t> srcContent;
        store.readObject(srcEntry->id, srcType, srcContent);
        std::vector<oggit::TreeEntry> srcEntries;
        oggit::parseTree(srcContent, srcEntries);

        check(srcEntries.size() == 2, "index: the 'src' subtree has one entry per direct child (main.og, lib/)");

        const oggit::TreeEntry* mainEntry = nullptr;
        const oggit::TreeEntry* libEntry = nullptr;
        for (const auto& e : srcEntries) {
            if (e.name == "main.og") mainEntry = &e;
            if (e.name == "lib") libEntry = &e;
        }

        check(mainEntry != nullptr && !mainEntry->isDirectory && mainEntry->id == mainBlob,
              "index: 'src/main.og' resolves through the real subtree to its exact staged blob");
        check(libEntry != nullptr && libEntry->isDirectory, "index: 'src/lib' is correctly written as a nested directory");

        if (libEntry != nullptr) {
            oggit::ObjectType libType;
            std::vector<uint8_t> libContent;
            store.readObject(libEntry->id, libType, libContent);
            std::vector<oggit::TreeEntry> libEntries;
            oggit::parseTree(libContent, libEntries);

            check(libEntries.size() == 1 && libEntries[0].name == "util.og" && libEntries[0].id == libBlob,
                  "index: 'src/lib/util.og' resolves two levels deep to its exact staged blob");
        }
    }

    std::filesystem::remove_all(dir);
}

void testWriteTreeFromIndexIsDeterministic() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    index.addFile(store, "b.txt", toBytes("b"));
    index.addFile(store, "a.txt", toBytes("a"));
    oggit::ObjectId firstTree = index.writeTreeFromIndex(store);

    index.clear();
    index.addFile(store, "a.txt", toBytes("a"));
    index.addFile(store, "b.txt", toBytes("b"));
    oggit::ObjectId secondTree = index.writeTreeFromIndex(store);

    check(firstTree == secondTree, "index: the resulting tree id is independent of staging order (same content, same id)");

    std::filesystem::remove_all(dir);
}

void testWriteTreeFromIndexDirectoryWinsOverConflictingFile() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);
    oggit::Index index(dir + "/index");

    // "a" staged as a file, then "a/b" staged as a file too — "a" is
    // used as both a file and a directory. Per index.hpp's documented
    // rule, the directory wins and the file entry at "a" is dropped.
    index.addFile(store, "a", toBytes("this file entry should be dropped"));
    oggit::ObjectId nestedBlob = index.addFile(store, "a/b", toBytes("nested file"));

    oggit::ObjectId rootTreeId = index.writeTreeFromIndex(store);
    oggit::ObjectType rootType;
    std::vector<uint8_t> rootContent;
    store.readObject(rootTreeId, rootType, rootContent);
    std::vector<oggit::TreeEntry> rootEntries;
    oggit::parseTree(rootContent, rootEntries);

    check(rootEntries.size() == 1, "index: a file/directory path conflict resolves to exactly one root entry, not two");
    check(rootEntries.size() == 1 && rootEntries[0].name == "a" && rootEntries[0].isDirectory,
          "index: on a file/directory conflict, the directory deterministically wins over the file");

    if (rootEntries.size() == 1 && rootEntries[0].isDirectory) {
        oggit::ObjectType subType;
        std::vector<uint8_t> subContent;
        store.readObject(rootEntries[0].id, subType, subContent);
        std::vector<oggit::TreeEntry> subEntries;
        oggit::parseTree(subContent, subEntries);
        check(subEntries.size() == 1 && subEntries[0].name == "b" && subEntries[0].id == nestedBlob,
              "index: the winning directory still contains the real nested file that required it to become a directory");
    }

    std::filesystem::remove_all(dir);
}

int main() {
    testAddFileStagesAndIsRetrievable();
    testAddFileIsIdempotentForIdenticalContent();
    testAddFileReplacesPreviousStagedContent();
    testRemoveFile();
    testPathNormalization();
    testSaveAndReloadRoundTrip();
    testClear();
    testWriteTreeFromIndexEmptyIndexProducesEmptyTree();
    testWriteTreeFromIndexSingleFileAtRoot();
    testWriteTreeFromIndexBuildsRealNestedHierarchy();
    testWriteTreeFromIndexIsDeterministic();
    testWriteTreeFromIndexDirectoryWinsOverConflictingFile();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

// Real assertion-based tests for OGGit's content-addressed object
// store (oggit/object_store.cpp) — a genuine integration point with
// the SHA-256 implementation built in 10-cryptography/, exercised
// through real disk I/O against a temporary directory (removed at the
// end of the run), not an in-memory mock of the filesystem.

#include "../oggit/object_store.hpp"

#include <filesystem>
#include <fstream>
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
                "oggit_test_XXXXXX_fixed";
    std::filesystem::remove_all(path);
    return path.string();
}

}  // namespace

void testWriteThenReadRoundTrip() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    std::vector<uint8_t> content = toBytes("hello, oggit");
    oggit::ObjectId id = store.writeObject(oggit::ObjectType::Blob, content);

    oggit::ObjectType typeOut;
    std::vector<uint8_t> contentOut;
    oggit::StoreError err = store.readObject(id, typeOut, contentOut);

    check(err == oggit::StoreError::None, "object store: writing then reading a blob succeeds");
    check(typeOut == oggit::ObjectType::Blob, "object store: the read-back type matches what was written");
    check(contentOut == content, "object store: the read-back content matches exactly what was written");

    std::filesystem::remove_all(dir);
}

void testContentAddressingIsDeterministic() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    oggit::ObjectId id1 = store.writeObject(oggit::ObjectType::Blob, toBytes("same content"));
    oggit::ObjectId id2 = store.writeObject(oggit::ObjectType::Blob, toBytes("same content"));

    check(id1 == id2,
          "object store: writing identical content twice produces the identical object id");

    std::filesystem::remove_all(dir);
}

void testDifferentContentProducesDifferentIds() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    oggit::ObjectId id1 = store.writeObject(oggit::ObjectType::Blob, toBytes("content A"));
    oggit::ObjectId id2 = store.writeObject(oggit::ObjectType::Blob, toBytes("content B"));

    check(!(id1 == id2), "object store: different content produces different object ids");

    std::filesystem::remove_all(dir);
}

void testSameBytesDifferentTypeProducesDifferentIds() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    std::vector<uint8_t> bytes = toBytes("ambiguous payload");
    oggit::ObjectId blobId = store.writeObject(oggit::ObjectType::Blob, bytes);
    oggit::ObjectId treeId = store.writeObject(oggit::ObjectType::Tree, bytes);

    check(!(blobId == treeId),
          "object store: identical raw bytes stored as different object types "
          "(blob vs tree) produce different ids — the type is part of what's hashed");

    std::filesystem::remove_all(dir);
}

void testExistsReflectsWrittenObjects() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    oggit::ObjectId written = store.writeObject(oggit::ObjectType::Blob, toBytes("present"));
    check(store.exists(written), "object store: exists() is true for a written object");

    oggit::ObjectId neverWritten;
    oggit::parseObjectId(std::string(64, '0'), neverWritten);
    check(!store.exists(neverWritten), "object store: exists() is false for an object never written");

    std::filesystem::remove_all(dir);
}

void testReadingUnknownObjectReturnsNotFound() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    oggit::ObjectId unknown;
    oggit::parseObjectId(std::string(64, 'a'), unknown);

    oggit::ObjectType typeOut;
    std::vector<uint8_t> contentOut;
    oggit::StoreError err = store.readObject(unknown, typeOut, contentOut);

    check(err == oggit::StoreError::NotFound,
          "object store: reading an object that was never written returns NotFound");

    std::filesystem::remove_all(dir);
}

void testDetectsCorruptedObjectOnDisk() {
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    oggit::ObjectId id = store.writeObject(oggit::ObjectType::Blob, toBytes("original content"));

    // Simulate on-disk corruption or bit rot by directly overwriting
    // the stored file's content byte, without updating its filename
    // (the object id) — a real content-addressed store must detect
    // this by re-hashing on read, not merely trust the filename.
    std::string hex = id.toHex();
    std::filesystem::path objectPath =
        std::filesystem::path(dir) / "objects" / hex.substr(0, 2) / hex.substr(2);

    {
        std::fstream f(objectPath, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(-1, std::ios::end);
        f.put('X');
    }

    oggit::ObjectType typeOut;
    std::vector<uint8_t> contentOut;
    oggit::StoreError err = store.readObject(id, typeOut, contentOut);

    check(err == oggit::StoreError::CorruptObject,
          "object store: a corrupted on-disk object (content no longer matches its "
          "own content-addressed id) is detected on read, not silently trusted");

    std::filesystem::remove_all(dir);
}

void testTreeSerializeParseRoundTrip() {
    oggit::ObjectId blobId;
    oggit::parseObjectId(std::string(64, 'a'), blobId);
    oggit::ObjectId subtreeId;
    oggit::parseObjectId(std::string(64, 'b'), subtreeId);

    std::vector<oggit::TreeEntry> entries = {
        {"zebra.txt", blobId, false},
        {"apple.txt", blobId, false},
        {"subdir", subtreeId, true},
    };

    std::vector<uint8_t> serialized = oggit::serializeTree(entries);

    std::vector<oggit::TreeEntry> parsed;
    bool ok = oggit::parseTree(serialized, parsed);

    check(ok, "tree: serialize then parse round-trips successfully");
    check(parsed.size() == 3, "tree: all three entries survive the round trip");
    check(parsed[0].name == "apple.txt" && parsed[1].name == "subdir" &&
          parsed[2].name == "zebra.txt",
          "tree: entries are canonically sorted by name regardless of insertion order — "
          "the property that makes two logically-identical trees hash identically");
    check(parsed[1].isDirectory && !parsed[0].isDirectory,
          "tree: the isDirectory flag round-trips correctly per entry");
}

void testTreeCanonicalOrderProducesSameHash() {
    oggit::ObjectId blobId;
    oggit::parseObjectId(std::string(64, 'c'), blobId);

    std::vector<oggit::TreeEntry> orderA = {
        {"a.txt", blobId, false}, {"b.txt", blobId, false},
    };
    std::vector<oggit::TreeEntry> orderB = {
        {"b.txt", blobId, false}, {"a.txt", blobId, false},
    };

    auto bytesA = oggit::serializeTree(orderA);
    auto bytesB = oggit::serializeTree(orderB);

    check(bytesA == bytesB,
          "tree: two logically identical trees inserted in different orders "
          "serialize to byte-identical output (and therefore the same object id)");
}

void testTreeRejectsTruncatedContent() {
    std::vector<uint8_t> truncated = {0, 0, 0, 5};  // claims 5 entries, has none
    std::vector<oggit::TreeEntry> parsed;
    bool ok = oggit::parseTree(truncated, parsed);
    check(!ok, "tree: rejects content claiming more entries than actually present");
}

void testCommitSerializeParseRoundTrip() {
    oggit::ObjectId treeId;
    oggit::parseObjectId(std::string(64, '1'), treeId);
    oggit::ObjectId parentId;
    oggit::parseObjectId(std::string(64, '2'), parentId);

    oggit::Commit commit{
        treeId, {parentId}, "Ada Lovelace <ada@example.com>", "Initial commit"
    };

    std::vector<uint8_t> serialized = oggit::serializeCommit(commit);

    oggit::Commit parsed;
    bool ok = oggit::parseCommit(serialized, parsed);

    check(ok, "commit: serialize then parse round-trips successfully");
    check(parsed.tree == commit.tree, "commit: tree id round-trips correctly");
    check(parsed.parents.size() == 1 && parsed.parents[0] == parentId,
          "commit: parent list round-trips correctly");
    check(parsed.author == commit.author && parsed.message == commit.message,
          "commit: author and message strings round-trip correctly");
}

void testCommitSupportsMultipleParentsForMerges() {
    oggit::ObjectId treeId;
    oggit::parseObjectId(std::string(64, '3'), treeId);
    oggit::ObjectId parent1, parent2;
    oggit::parseObjectId(std::string(64, '4'), parent1);
    oggit::parseObjectId(std::string(64, '5'), parent2);

    oggit::Commit merge{treeId, {parent1, parent2}, "author", "Merge branch"};
    auto serialized = oggit::serializeCommit(merge);

    oggit::Commit parsed;
    bool ok = oggit::parseCommit(serialized, parsed);

    check(ok && parsed.parents.size() == 2,
          "commit: a merge commit's two parents both round-trip correctly");
}

void testCommitWithNoParentsRoundTrips() {
    oggit::ObjectId treeId;
    oggit::parseObjectId(std::string(64, '6'), treeId);

    oggit::Commit root{treeId, {}, "author", "Root commit"};
    auto serialized = oggit::serializeCommit(root);

    oggit::Commit parsed;
    bool ok = oggit::parseCommit(serialized, parsed);

    check(ok && parsed.parents.empty(),
          "commit: a root commit (no parents) round-trips with an empty parent list");
}

void testCommitRejectsTruncatedContent() {
    std::vector<uint8_t> tooShort(10, 0);  // shorter than a bare 32-byte tree id
    oggit::Commit parsed;
    bool ok = oggit::parseCommit(tooShort, parsed);
    check(!ok, "commit: rejects content too short to even contain a tree id");
}

void testFullTreeAndCommitStoredThroughObjectStore() {
    // An actual end-to-end exercise of the whole object model together:
    // write a blob, build a tree referencing it, write the tree, build
    // a commit referencing the tree, write the commit, then read
    // everything back through the store.
    std::string dir = tempDir();
    oggit::ObjectStore store(dir);

    oggit::ObjectId blobId = store.writeObject(
        oggit::ObjectType::Blob, toBytes("int main() { return 0; }")
    );

    std::vector<oggit::TreeEntry> entries = {{"main.cpp", blobId, false}};
    oggit::ObjectId treeId = store.writeObject(
        oggit::ObjectType::Tree, oggit::serializeTree(entries)
    );

    oggit::Commit commit{treeId, {}, "author", "Add main.cpp"};
    oggit::ObjectId commitId = store.writeObject(
        oggit::ObjectType::Commit, oggit::serializeCommit(commit)
    );

    oggit::ObjectType readType;
    std::vector<uint8_t> readContent;
    bool commitOk =
        store.readObject(commitId, readType, readContent) == oggit::StoreError::None &&
        readType == oggit::ObjectType::Commit;

    oggit::Commit readCommit;
    bool commitParsed = commitOk && oggit::parseCommit(readContent, readCommit);

    bool treeOk = commitParsed &&
        store.readObject(readCommit.tree, readType, readContent) == oggit::StoreError::None &&
        readType == oggit::ObjectType::Tree;

    std::vector<oggit::TreeEntry> readEntries;
    bool treeParsed = treeOk && oggit::parseTree(readContent, readEntries);

    bool blobOk = treeParsed && readEntries.size() == 1 &&
        store.readObject(readEntries[0].id, readType, readContent) == oggit::StoreError::None &&
        readType == oggit::ObjectType::Blob &&
        readContent == toBytes("int main() { return 0; }");

    check(blobOk,
          "object store: a full commit -> tree -> blob chain, each written "
          "independently, is fully reconstructible by following ids from the commit");

    std::filesystem::remove_all(dir);
}

int main() {
    testWriteThenReadRoundTrip();
    testContentAddressingIsDeterministic();
    testDifferentContentProducesDifferentIds();
    testSameBytesDifferentTypeProducesDifferentIds();
    testExistsReflectsWrittenObjects();
    testReadingUnknownObjectReturnsNotFound();
    testDetectsCorruptedObjectOnDisk();
    testTreeSerializeParseRoundTrip();
    testTreeCanonicalOrderProducesSameHash();
    testTreeRejectsTruncatedContent();
    testCommitSerializeParseRoundTrip();
    testCommitSupportsMultipleParentsForMerges();
    testCommitWithNoParentsRoundTrips();
    testCommitRejectsTruncatedContent();
    testFullTreeAndCommitStoredThroughObjectStore();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

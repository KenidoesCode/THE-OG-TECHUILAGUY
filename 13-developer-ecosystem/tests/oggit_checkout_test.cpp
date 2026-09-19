// Real assertion-based tests for OGGit's checkout (oggit/checkout.cpp)
// — exercised through a real ObjectStore and real filesystem I/O
// against temporary directories (removed at the end of the run), not
// an in-memory mock. Several tests deliberately round-trip through
// ../oggit/index.hpp's Index (stage real files -> writeTreeFromIndex
// -> checkoutTree -> re-read from disk) to prove the two halves of
// OGGit's local workflow (files -> tree, tree -> files) are actually
// inverses of each other, not just independently plausible.

#include "../oggit/checkout.hpp"
#include "../oggit/index.hpp"
#include "../oggit/object_store.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

std::string readFileContent(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

std::string tempDir(const std::string& suffix) {
    auto path = std::filesystem::temp_directory_path() /
                ("oggit_checkout_test_" + suffix + "_fixed");
    std::filesystem::remove_all(path);
    return path.string();
}

}  // namespace

void testCheckoutEmptyTreeCreatesOnlyTheDirectory() {
    std::string repoDir = tempDir("repo_empty");
    std::string destDir = tempDir("dest_empty");
    oggit::ObjectStore store(repoDir);
    oggit::Index index(repoDir + "/index");

    oggit::ObjectId treeId = index.writeTreeFromIndex(store);
    oggit::CheckoutError err = oggit::checkoutTree(store, treeId, destDir);

    check(err == oggit::CheckoutError::None, "checkout: checking out an empty tree succeeds");
    check(std::filesystem::exists(destDir) && std::filesystem::is_directory(destDir),
          "checkout: checking out an empty tree creates the destination directory");
    check(std::filesystem::is_empty(destDir), "checkout: checking out an empty tree leaves the destination empty");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testCheckoutSingleFileAtRoot() {
    std::string repoDir = tempDir("repo_single");
    std::string destDir = tempDir("dest_single");
    oggit::ObjectStore store(repoDir);
    oggit::Index index(repoDir + "/index");

    index.addFile(store, "README.md", toBytes("# hello"));
    oggit::ObjectId treeId = index.writeTreeFromIndex(store);
    oggit::CheckoutError err = oggit::checkoutTree(store, treeId, destDir);

    check(err == oggit::CheckoutError::None, "checkout: checking out a single-file tree succeeds");
    check(std::filesystem::exists(std::filesystem::path(destDir) / "README.md"),
          "checkout: the staged file exists at the destination after checkout");
    check(readFileContent(std::filesystem::path(destDir) / "README.md") == "# hello",
          "checkout: the checked-out file's content matches exactly what was staged");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testCheckoutRoundTripsRealNestedHierarchy() {
    std::string repoDir = tempDir("repo_nested");
    std::string destDir = tempDir("dest_nested");
    oggit::ObjectStore store(repoDir);
    oggit::Index index(repoDir + "/index");

    index.addFile(store, "README.md", toBytes("# hi"));
    index.addFile(store, "src/main.og", toBytes("fn main() {}"));
    index.addFile(store, "src/lib/util.og", toBytes("fn util() {}"));

    oggit::ObjectId treeId = index.writeTreeFromIndex(store);
    oggit::CheckoutError err = oggit::checkoutTree(store, treeId, destDir);

    check(err == oggit::CheckoutError::None, "checkout: checking out a nested tree succeeds");

    std::filesystem::path dest(destDir);
    check(std::filesystem::is_directory(dest / "src"), "checkout: an intermediate directory ('src') is really created as a directory");
    check(std::filesystem::is_directory(dest / "src" / "lib"), "checkout: a doubly-nested directory ('src/lib') is really created");
    check(readFileContent(dest / "README.md") == "# hi", "checkout: a root-level file round-trips exactly");
    check(readFileContent(dest / "src" / "main.og") == "fn main() {}", "checkout: a one-level-deep file round-trips exactly");
    check(readFileContent(dest / "src" / "lib" / "util.og") == "fn util() {}", "checkout: a two-levels-deep file round-trips exactly");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testCheckoutOverwritesExistingFileContent() {
    std::string repoDir = tempDir("repo_overwrite");
    std::string destDir = tempDir("dest_overwrite");
    std::filesystem::create_directories(destDir);
    {
        std::ofstream pre(std::filesystem::path(destDir) / "a.txt");
        pre << "stale content that should be replaced";
    }

    oggit::ObjectStore store(repoDir);
    oggit::Index index(repoDir + "/index");
    index.addFile(store, "a.txt", toBytes("fresh content"));
    oggit::ObjectId treeId = index.writeTreeFromIndex(store);

    oggit::CheckoutError err = oggit::checkoutTree(store, treeId, destDir);

    check(err == oggit::CheckoutError::None, "checkout: checking out over an existing file succeeds");
    check(readFileContent(std::filesystem::path(destDir) / "a.txt") == "fresh content",
          "checkout: an existing file's stale content is fully overwritten, not appended to or left stale");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testCheckoutLeavesUntrackedFilesAlone() {
    std::string repoDir = tempDir("repo_untracked");
    std::string destDir = tempDir("dest_untracked");
    oggit::ObjectStore store(repoDir);
    oggit::Index index(repoDir + "/index");
    index.addFile(store, "tracked.txt", toBytes("tracked"));
    oggit::ObjectId treeId = index.writeTreeFromIndex(store);

    std::filesystem::create_directories(destDir);
    {
        std::ofstream untracked(std::filesystem::path(destDir) / "untracked.txt");
        untracked << "should survive checkout untouched";
    }

    oggit::CheckoutError err = oggit::checkoutTree(store, treeId, destDir);

    check(err == oggit::CheckoutError::None, "checkout: checking out alongside an untracked file succeeds");
    check(std::filesystem::exists(std::filesystem::path(destDir) / "untracked.txt"),
          "checkout: a file not present in the tree is left alone, not deleted (this is materialization, not a clean/reset)");
    check(readFileContent(std::filesystem::path(destDir) / "untracked.txt") == "should survive checkout untouched",
          "checkout: the untracked file's content is untouched");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testCheckoutUnknownTreeIdFailsCleanly() {
    std::string repoDir = tempDir("repo_unknown");
    std::string destDir = tempDir("dest_unknown");
    oggit::ObjectStore store(repoDir);

    oggit::ObjectId bogus;
    bool parsed = oggit::parseObjectId(std::string(64, 'a'), bogus);
    check(parsed, "checkout test setup: a syntactically valid 64-hex-char id parses");

    oggit::CheckoutError err = oggit::checkoutTree(store, bogus, destDir);
    check(err == oggit::CheckoutError::TreeNotFound,
          "checkout: checking out a tree id the store has never seen fails with TreeNotFound, not a crash or silent success");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testCheckoutRejectsNonTreeObject() {
    std::string repoDir = tempDir("repo_nontree");
    std::string destDir = tempDir("dest_nontree");
    oggit::ObjectStore store(repoDir);

    oggit::ObjectId blobId = store.writeObject(oggit::ObjectType::Blob, toBytes("just a blob, not a tree"));
    oggit::CheckoutError err = oggit::checkoutTree(store, blobId, destDir);

    check(err == oggit::CheckoutError::NotATree,
          "checkout: attempting to check out a Blob id as if it were a Tree fails with NotATree, not a crash or garbage output");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testCheckoutDetectsMissingReferencedBlob() {
    std::string repoDir = tempDir("repo_missingblob");
    std::string destDir = tempDir("dest_missingblob");
    oggit::ObjectStore store(repoDir);

    // Hand-build a Tree that references a blob id that was never
    // actually written to the store, simulating a corrupt/incomplete
    // repository rather than the normal index-driven path.
    oggit::ObjectId neverWrittenBlob;
    oggit::parseObjectId(std::string(64, 'b'), neverWrittenBlob);
    std::vector<oggit::TreeEntry> entries = {
        oggit::TreeEntry{"ghost.txt", neverWrittenBlob, false}
    };
    oggit::ObjectId treeId = store.writeObject(oggit::ObjectType::Tree, oggit::serializeTree(entries));

    oggit::CheckoutError err = oggit::checkoutTree(store, treeId, destDir);
    check(err == oggit::CheckoutError::ObjectReadError,
          "checkout: a tree referencing a blob that was never written fails with ObjectReadError, not a crash");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

void testFullStageCheckoutRoundTripPreservesEveryByte() {
    std::string repoDir = tempDir("repo_fullroundtrip");
    std::string destDir = tempDir("dest_fullroundtrip");
    oggit::ObjectStore store(repoDir);
    oggit::Index index(repoDir + "/index");

    std::vector<uint8_t> binaryish;
    for (int i = 0; i < 256; ++i) binaryish.push_back(static_cast<uint8_t>(i));

    index.addFile(store, "docs/notes.txt", toBytes("some notes\nwith multiple\nlines"));
    index.addFile(store, "assets/data.bin", binaryish);
    oggit::ObjectId treeId = index.writeTreeFromIndex(store);

    oggit::CheckoutError err = oggit::checkoutTree(store, treeId, destDir);
    check(err == oggit::CheckoutError::None, "checkout: full stage->tree->checkout round trip succeeds");

    std::ifstream binIn(std::filesystem::path(destDir) / "assets" / "data.bin", std::ios::binary);
    std::vector<uint8_t> readBack((std::istreambuf_iterator<char>(binIn)), std::istreambuf_iterator<char>());
    check(readBack == binaryish, "checkout: binary content (including a real 0x00 byte) round-trips byte-for-byte, not just text");

    std::filesystem::remove_all(repoDir);
    std::filesystem::remove_all(destDir);
}

int main() {
    testCheckoutEmptyTreeCreatesOnlyTheDirectory();
    testCheckoutSingleFileAtRoot();
    testCheckoutRoundTripsRealNestedHierarchy();
    testCheckoutOverwritesExistingFileContent();
    testCheckoutLeavesUntrackedFilesAlone();
    testCheckoutUnknownTreeIdFailsCleanly();
    testCheckoutRejectsNonTreeObject();
    testCheckoutDetectsMissingReferencedBlob();
    testFullStageCheckoutRoundTripPreservesEveryByte();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

// Real assertion-based tests for OGForge's server foundation
// (forge/forge_server.cpp) — real OGGit repositories on real disk, not
// mocks. See docs/ADR/0023-ogforge-server-foundation.md.

#include "../forge/forge_server.hpp"
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

std::string tempDir(const std::string& suffix) {
    auto path = std::filesystem::temp_directory_path() / ("forge_test_" + suffix + "_fixed");
    std::filesystem::remove_all(path);
    return path.string();
}

}  // namespace

void testRepositoryCreationRequiresAuthentication() {
    std::string root = tempDir("create");
    forge::ForgeServer server(root);
    server.registerUser("alice", "correct-horse");

    forge::ForgeError wrongPassword = server.createRepository("alice", "wrong-password", "myrepo");
    check(wrongPassword == forge::ForgeError::AuthenticationFailed,
          "forge: creating a repository with the wrong password fails with AuthenticationFailed");
    check(!server.repositoryExists("myrepo"), "forge: a failed authentication creates no repository");

    forge::ForgeError ok = server.createRepository("alice", "correct-horse", "myrepo");
    check(ok == forge::ForgeError::None, "forge: creating a repository with correct credentials succeeds");
    check(server.repositoryExists("myrepo"), "forge: a successfully created repository is really visible on disk");

    std::filesystem::remove_all(root);
}

void testRepositoryAlreadyExists() {
    std::string root = tempDir("exists");
    forge::ForgeServer server(root);
    server.registerUser("alice", "pw");
    server.createRepository("alice", "pw", "repo1");

    forge::ForgeError result = server.createRepository("alice", "pw", "repo1");
    check(result == forge::ForgeError::RepositoryAlreadyExists,
          "forge: creating a repository that already exists fails with RepositoryAlreadyExists");

    std::filesystem::remove_all(root);
}

void testMalformedRequestsAreRejected() {
    std::string root = tempDir("malformed");
    forge::ForgeServer server(root);

    check(server.registerUser("", "pw") == forge::ForgeError::InvalidRequest,
          "forge: registering a user with an empty username is rejected as InvalidRequest");
    check(server.registerUser("bob", "pw") == forge::ForgeError::None, "forge: registering a real user succeeds");
    check(server.registerUser("bob", "pw2") == forge::ForgeError::UserAlreadyExists,
          "forge: registering an already-existing username is rejected as UserAlreadyExists");

    check(server.createRepository("bob", "pw", "") == forge::ForgeError::InvalidRequest,
          "forge: creating a repository with an empty name is rejected as InvalidRequest");

    std::filesystem::remove_all(root);
}

void testMissingRepositoryOperationsFailCleanly() {
    std::string root = tempDir("missing");
    forge::ForgeServer server(root);
    server.registerUser("alice", "pw");

    std::vector<std::string> branches;
    check(server.listBranches("does_not_exist", branches) == forge::ForgeError::RepositoryNotFound,
          "forge: listing branches on a nonexistent repository fails cleanly with RepositoryNotFound, not a crash");

    oggit::ObjectId someId;
    oggit::ObjectType type;
    std::vector<uint8_t> content;
    check(server.getObject("does_not_exist", someId, type, content) == forge::ForgeError::RepositoryNotFound,
          "forge: reading an object from a nonexistent repository fails cleanly with RepositoryNotFound");

    oggit::ObjectId pushedId;
    check(server.pushObject("alice", "pw", "does_not_exist", oggit::ObjectType::Blob, toBytes("x"), pushedId) ==
              forge::ForgeError::RepositoryNotFound,
          "forge: pushing to a nonexistent repository fails cleanly with RepositoryNotFound");

    std::filesystem::remove_all(root);
}

void testPushAndRetrieveRealObject() {
    std::string root = tempDir("push");
    forge::ForgeServer server(root);
    server.registerUser("alice", "pw");
    server.createRepository("alice", "pw", "repo1");

    oggit::ObjectId blobId;
    forge::ForgeError pushResult =
        server.pushObject("alice", "pw", "repo1", oggit::ObjectType::Blob, toBytes("hello, forge"), blobId);
    check(pushResult == forge::ForgeError::None, "forge: pushing a real blob to an existing repository succeeds");

    oggit::ObjectType typeOut;
    std::vector<uint8_t> contentOut;
    forge::ForgeError getResult = server.getObject("repo1", blobId, typeOut, contentOut);
    check(getResult == forge::ForgeError::None, "forge: retrieving a just-pushed object succeeds");
    check(typeOut == oggit::ObjectType::Blob && contentOut == toBytes("hello, forge"),
          "forge: the retrieved object's type and content exactly match what was pushed");

    check(server.pushObject("alice", "wrong", "repo1", oggit::ObjectType::Blob, toBytes("x"), blobId) ==
              forge::ForgeError::AuthenticationFailed,
          "forge: pushing with the wrong password fails with AuthenticationFailed, not silently succeeding");

    std::filesystem::remove_all(root);
}

void testSetBranchRequiresTheCommitToAlreadyExist() {
    std::string root = tempDir("branch");
    forge::ForgeServer server(root);
    server.registerUser("alice", "pw");
    server.createRepository("alice", "pw", "repo1");

    oggit::ObjectId neverPushed;
    oggit::parseObjectId(std::string(64, 'a'), neverPushed);
    forge::ForgeError result = server.setBranch("alice", "pw", "repo1", "main", neverPushed);
    check(result == forge::ForgeError::ObjectNotFound,
          "forge: moving a branch to a commit that was never pushed fails with ObjectNotFound");

    oggit::ObjectId realCommitId;
    server.pushObject("alice", "pw", "repo1", oggit::ObjectType::Commit, toBytes("fake commit bytes"), realCommitId);
    forge::ForgeError ok = server.setBranch("alice", "pw", "repo1", "main", realCommitId);
    check(ok == forge::ForgeError::None, "forge: moving a branch to a real, previously-pushed object succeeds");

    std::vector<std::string> branches;
    server.listBranches("repo1", branches);
    check(!branches.empty() && branches[0] == "main", "forge: a moved branch is really visible via listBranches");

    std::filesystem::remove_all(root);
}

void testCorruptObjectIsReportedAsObjectNotFound() {
    std::string root = tempDir("corrupt");
    forge::ForgeServer server(root);
    server.registerUser("alice", "pw");
    server.createRepository("alice", "pw", "repo1");

    oggit::ObjectId blobId;
    server.pushObject("alice", "pw", "repo1", oggit::ObjectType::Blob, toBytes("original content"), blobId);

    // Directly corrupt the object's on-disk bytes — the same
    // technique OGGit's own object-store test already uses to prove
    // the re-hash-on-read integrity check (ADR 0007).
    std::string hex = blobId.toHex();
    std::filesystem::path objectPath = std::filesystem::path(root) / "repos" / "repo1" / "objects" /
                                        hex.substr(0, 2) / hex.substr(2);
    {
        std::ofstream corrupt(objectPath, std::ios::trunc | std::ios::binary);
        corrupt << "tampered bytes that will not hash back to the original id";
    }

    oggit::ObjectType typeOut;
    std::vector<uint8_t> contentOut;
    forge::ForgeError result = server.getObject("repo1", blobId, typeOut, contentOut);
    check(result == forge::ForgeError::ObjectNotFound,
          "forge: a corrupted on-disk object is reported as ObjectNotFound, never returned as if it were valid");

    std::filesystem::remove_all(root);
}

void testRestartRecoversUsersAndRepositoryContent() {
    std::string root = tempDir("restart");
    oggit::ObjectId blobId;

    {
        forge::ForgeServer server(root);
        server.registerUser("alice", "pw");
        server.createRepository("alice", "pw", "repo1");
        server.pushObject("alice", "pw", "repo1", oggit::ObjectType::Blob, toBytes("persisted content"), blobId);
        server.setBranch("alice", "pw", "repo1", "main", blobId);
    }

    {
        // A second, independent ForgeServer over the SAME root — a
        // real process-restart simulation, not an in-memory check.
        forge::ForgeServer reopened(root);
        check(reopened.authenticate("alice", "pw"), "forge: a registered user's credentials survive a restart");
        check(reopened.repositoryExists("repo1"), "forge: a created repository survives a restart");

        oggit::ObjectType typeOut;
        std::vector<uint8_t> contentOut;
        forge::ForgeError result = reopened.getObject("repo1", blobId, typeOut, contentOut);
        check(result == forge::ForgeError::None && contentOut == toBytes("persisted content"),
              "forge: a previously-pushed object's exact content survives a restart");

        std::vector<std::string> branches;
        reopened.listBranches("repo1", branches);
        check(!branches.empty() && branches[0] == "main", "forge: a previously-set branch survives a restart");
    }

    std::filesystem::remove_all(root);
}

int main() {
    testRepositoryCreationRequiresAuthentication();
    testRepositoryAlreadyExists();
    testMalformedRequestsAreRejected();
    testMissingRepositoryOperationsFailCleanly();
    testPushAndRetrieveRealObject();
    testSetBranchRequiresTheCommitToAlreadyExist();
    testCorruptObjectIsReportedAsObjectNotFound();
    testRestartRecoversUsersAndRepositoryContent();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

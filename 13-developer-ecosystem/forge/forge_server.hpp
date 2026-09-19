#pragma once

#include "../oggit/object_store.hpp"

#include <map>
#include <string>
#include <vector>

// OGForge's server-logic foundation, built directly on the existing,
// real OGGit implementation — not a new repository storage format,
// not a mock. See docs/ADR/0023-ogforge-server-foundation.md for the
// full design and explicit scope (no network transport yet, no real
// password security, no issues/PRs/CI/registry).

namespace forge {

enum class ForgeError {
    None,
    InvalidRequest,          // empty username/repoName, or similar malformed input
    UserAlreadyExists,
    AuthenticationFailed,
    RepositoryAlreadyExists,
    RepositoryNotFound,
    ObjectNotFound,
};

class ForgeServer {
public:
    // `forgeRoot` is created (including repos/ and users.db) if it
    // doesn't already exist. Existing users and repositories at this
    // root are loaded/made available immediately.
    explicit ForgeServer(const std::string& forgeRoot);

    ForgeError registerUser(const std::string& username, const std::string& password);

    // Not constant-time — see the ADR's "Authentication" section for
    // exactly why this is not real production-grade auth.
    bool authenticate(const std::string& username, const std::string& password) const;

    ForgeError createRepository(
        const std::string& username, const std::string& password, const std::string& repoName
    );

    bool repositoryExists(const std::string& repoName) const;

    // Writes a real OGGit object (blob/tree/commit) into repoName's
    // object store and returns its real content-addressed id via
    // `idOut`.
    ForgeError pushObject(
        const std::string& username, const std::string& password, const std::string& repoName,
        oggit::ObjectType type, const std::vector<uint8_t>& content, oggit::ObjectId& idOut
    );

    // Moves a branch — requires `commit` to already exist as a real
    // object in repoName's store (a branch can never point at
    // something that was never pushed).
    ForgeError setBranch(
        const std::string& username, const std::string& password, const std::string& repoName,
        const std::string& branchName, const oggit::ObjectId& commit
    );

    // Public read paths — no authentication required (repository
    // browsing is public by default, matching real code forges).
    ForgeError listBranches(const std::string& repoName, std::vector<std::string>& out) const;
    ForgeError getObject(
        const std::string& repoName, const oggit::ObjectId& id,
        oggit::ObjectType& typeOut, std::vector<uint8_t>& contentOut
    ) const;

private:
    std::string forgeRoot;
    std::map<std::string, std::vector<uint8_t>> userPasswordHashes;

    std::string userDbPath() const;
    std::string repoPath(const std::string& repoName) const;
    void loadUsers();
    void saveUsers() const;
};

}  // namespace forge

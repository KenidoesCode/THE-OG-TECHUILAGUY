#include "forge_server.hpp"

#include "../../07-distributed-systems/rpc/serialization.hpp"
#include "../../10-cryptography/hashing/sha256.hpp"
#include "../oggit/refs.hpp"

#include <filesystem>
#include <fstream>

namespace forge {

namespace {

std::vector<uint8_t> hashPassword(const std::string& password) {
    std::vector<uint8_t> digest(32);
    crypto::sha256(reinterpret_cast<const uint8_t*>(password.data()), password.size(), digest.data());
    return digest;
}

}  // namespace

ForgeServer::ForgeServer(const std::string& forgeRoot) : forgeRoot(forgeRoot) {
    std::filesystem::create_directories(std::filesystem::path(forgeRoot) / "repos");
    loadUsers();
}

std::string ForgeServer::userDbPath() const {
    return (std::filesystem::path(forgeRoot) / "users.db").string();
}

std::string ForgeServer::repoPath(const std::string& repoName) const {
    return (std::filesystem::path(forgeRoot) / "repos" / repoName).string();
}

void ForgeServer::loadUsers() {
    std::ifstream in(userDbPath(), std::ios::binary);
    if (!in.is_open()) return;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return;

    dist::Decoder decoder(bytes);
    uint32_t count = 0;
    if (!decoder.readU32(count)) return;
    for (uint32_t i = 0; i < count; ++i) {
        std::string username;
        std::vector<uint8_t> hash;
        if (!decoder.readString(username)) return;
        if (!decoder.readBytes(hash)) return;
        userPasswordHashes[username] = hash;
    }
}

void ForgeServer::saveUsers() const {
    dist::Encoder encoder;
    encoder.writeU32(static_cast<uint32_t>(userPasswordHashes.size()));
    for (const auto& [username, hash] : userPasswordHashes) {
        encoder.writeString(username);
        encoder.writeBytes(hash);
    }
    std::ofstream out(userDbPath(), std::ios::binary | std::ios::trunc);
    const auto& data = encoder.data();
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

ForgeError ForgeServer::registerUser(const std::string& username, const std::string& password) {
    if (username.empty()) return ForgeError::InvalidRequest;
    if (userPasswordHashes.count(username) > 0) return ForgeError::UserAlreadyExists;

    userPasswordHashes[username] = hashPassword(password);
    saveUsers();
    return ForgeError::None;
}

bool ForgeServer::authenticate(const std::string& username, const std::string& password) const {
    auto it = userPasswordHashes.find(username);
    if (it == userPasswordHashes.end()) return false;
    return it->second == hashPassword(password);
}

ForgeError ForgeServer::createRepository(
    const std::string& username, const std::string& password, const std::string& repoName
) {
    if (repoName.empty()) return ForgeError::InvalidRequest;
    if (!authenticate(username, password)) return ForgeError::AuthenticationFailed;
    if (repositoryExists(repoName)) return ForgeError::RepositoryAlreadyExists;

    // Constructing ObjectStore/RefStore over the path creates the
    // real on-disk repository structure (objects/, refs/heads/).
    oggit::ObjectStore store(repoPath(repoName));
    oggit::RefStore refs(repoPath(repoName));
    (void)store;
    (void)refs;
    return ForgeError::None;
}

bool ForgeServer::repositoryExists(const std::string& repoName) const {
    return std::filesystem::exists(repoPath(repoName));
}

ForgeError ForgeServer::pushObject(
    const std::string& username, const std::string& password, const std::string& repoName,
    oggit::ObjectType type, const std::vector<uint8_t>& content, oggit::ObjectId& idOut
) {
    if (!authenticate(username, password)) return ForgeError::AuthenticationFailed;
    if (!repositoryExists(repoName)) return ForgeError::RepositoryNotFound;

    oggit::ObjectStore store(repoPath(repoName));
    idOut = store.writeObject(type, content);
    return ForgeError::None;
}

ForgeError ForgeServer::setBranch(
    const std::string& username, const std::string& password, const std::string& repoName,
    const std::string& branchName, const oggit::ObjectId& commit
) {
    if (branchName.empty()) return ForgeError::InvalidRequest;
    if (!authenticate(username, password)) return ForgeError::AuthenticationFailed;
    if (!repositoryExists(repoName)) return ForgeError::RepositoryNotFound;

    oggit::ObjectStore store(repoPath(repoName));
    if (!store.exists(commit)) return ForgeError::ObjectNotFound;

    oggit::RefStore refs(repoPath(repoName));
    refs.setBranch(branchName, commit);
    return ForgeError::None;
}

ForgeError ForgeServer::listBranches(const std::string& repoName, std::vector<std::string>& out) const {
    if (!repositoryExists(repoName)) return ForgeError::RepositoryNotFound;
    oggit::RefStore refs(repoPath(repoName));
    out = refs.listBranches();
    return ForgeError::None;
}

ForgeError ForgeServer::getObject(
    const std::string& repoName, const oggit::ObjectId& id,
    oggit::ObjectType& typeOut, std::vector<uint8_t>& contentOut
) const {
    if (!repositoryExists(repoName)) return ForgeError::RepositoryNotFound;

    oggit::ObjectStore store(repoPath(repoName));
    oggit::StoreError err = store.readObject(id, typeOut, contentOut);
    if (err != oggit::StoreError::None) return ForgeError::ObjectNotFound;
    return ForgeError::None;
}

}  // namespace forge

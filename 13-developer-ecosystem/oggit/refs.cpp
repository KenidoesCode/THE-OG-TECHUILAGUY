#include "refs.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace oggit {

namespace {

constexpr char SYMBOLIC_PREFIX[] = "ref: ";

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool readWholeFile(const std::string& path, std::string& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = trim(buffer.str());
    return true;
}

bool writeWholeFile(const std::string& path, const std::string& content) {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path()
    );
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << content << "\n";
    return out.good();
}

}  // namespace

RefStore::RefStore(const std::string& rootDirectory) : rootDirectory(rootDirectory) {
    std::filesystem::create_directories(
        std::filesystem::path(rootDirectory) / "refs" / "heads"
    );
}

std::string RefStore::branchPath(const std::string& name) const {
    return (std::filesystem::path(rootDirectory) / "refs" / "heads" / name).string();
}

std::string RefStore::headPath() const {
    return (std::filesystem::path(rootDirectory) / "HEAD").string();
}

bool RefStore::setBranch(const std::string& name, const ObjectId& commit) {
    return writeWholeFile(branchPath(name), commit.toHex());
}

bool RefStore::getBranch(const std::string& name, ObjectId& out) const {
    std::string content;
    if (!readWholeFile(branchPath(name), content)) return false;
    return parseObjectId(content, out);
}

bool RefStore::branchExists(const std::string& name) const {
    return std::filesystem::exists(branchPath(name));
}

bool RefStore::deleteBranch(const std::string& name) {
    std::error_code ec;
    return std::filesystem::remove(branchPath(name), ec);
}

std::vector<std::string> RefStore::listBranches() const {
    std::vector<std::string> names;
    std::filesystem::path headsDir =
        std::filesystem::path(rootDirectory) / "refs" / "heads";

    if (!std::filesystem::exists(headsDir)) return names;

    for (const auto& entry : std::filesystem::directory_iterator(headsDir)) {
        if (entry.is_regular_file()) {
            names.push_back(entry.path().filename().string());
        }
    }
    return names;
}

bool RefStore::setHeadToBranch(const std::string& branchName) {
    return writeWholeFile(headPath(), std::string(SYMBOLIC_PREFIX) + branchName);
}

bool RefStore::setHeadDetached(const ObjectId& commit) {
    return writeWholeFile(headPath(), commit.toHex());
}

bool RefStore::isHeadDetached() const {
    std::string content;
    if (!readWholeFile(headPath(), content)) return false;
    return content.rfind(SYMBOLIC_PREFIX, 0) != 0;  // doesn't start with "ref: "
}

bool RefStore::currentBranch(std::string& out) const {
    std::string content;
    if (!readWholeFile(headPath(), content)) return false;
    if (content.rfind(SYMBOLIC_PREFIX, 0) != 0) return false;  // detached
    out = content.substr(std::string(SYMBOLIC_PREFIX).size());
    return true;
}

bool RefStore::resolveHead(ObjectId& out) const {
    std::string content;
    if (!readWholeFile(headPath(), content)) return false;

    if (content.rfind(SYMBOLIC_PREFIX, 0) == 0) {
        std::string branchName = content.substr(std::string(SYMBOLIC_PREFIX).size());
        return getBranch(branchName, out);
    }

    return parseObjectId(content, out);
}

std::vector<ObjectId> walkFirstParentHistory(
    ObjectStore& store, const ObjectId& startCommit
) {
    std::vector<ObjectId> history;
    ObjectId current = startCommit;

    while (true) {
        ObjectType type;
        std::vector<uint8_t> content;
        if (store.readObject(current, type, content) != StoreError::None ||
            type != ObjectType::Commit) {
            return {};
        }

        Commit commit;
        if (!parseCommit(content, commit)) {
            return {};
        }

        history.push_back(current);

        if (commit.parents.empty()) {
            break;
        }
        current = commit.parents[0];
    }

    return history;
}

}  // namespace oggit

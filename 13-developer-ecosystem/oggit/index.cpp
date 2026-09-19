#include "index.hpp"

#include "tree_builder.hpp"

#include <fstream>
#include <map>
#include <sstream>

namespace oggit {

namespace {

// Splits a path on '/', dropping empty segments (so "a//b/", "/a/b",
// and "a/b" all normalize to the same two segments ["a", "b"]).
std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        segments.push_back(current);
    }
    return segments;
}

std::string joinPath(const std::vector<std::string>& segments) {
    std::string out;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i != 0) out.push_back('/');
        out += segments[i];
    }
    return out;
}

}  // namespace

Index::Index(std::string indexPath) : indexPath(std::move(indexPath)) {
    load();
}

void Index::load() {
    std::ifstream in(indexPath);
    if (!in.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string hex;
        std::string path;
        if (!(iss >> hex)) continue;
        // The rest of the line (after exactly one separating space) is
        // the path, preserved verbatim so a path containing spaces
        // round-trips correctly.
        if (iss.peek() == ' ') iss.get();
        std::getline(iss, path);
        if (path.empty()) continue;
        ObjectId id;
        if (!parseObjectId(hex, id)) continue;  // skip malformed line
        staged[path] = id;
    }
}

ObjectId Index::addFile(ObjectStore& store, const std::string& path,
                         const std::vector<uint8_t>& content) {
    ObjectId id = store.writeObject(ObjectType::Blob, content);
    std::string normalized = joinPath(splitPath(path));
    staged[normalized] = id;
    return id;
}

bool Index::removeFile(const std::string& path) {
    std::string normalized = joinPath(splitPath(path));
    return staged.erase(normalized) > 0;
}

bool Index::isStaged(const std::string& path) const {
    std::string normalized = joinPath(splitPath(path));
    return staged.count(normalized) > 0;
}

bool Index::getStagedBlob(const std::string& path, ObjectId& out) const {
    std::string normalized = joinPath(splitPath(path));
    auto it = staged.find(normalized);
    if (it == staged.end()) return false;
    out = it->second;
    return true;
}

std::vector<std::pair<std::string, ObjectId>> Index::entries() const {
    std::vector<std::pair<std::string, ObjectId>> out;
    out.reserve(staged.size());
    for (const auto& [path, id] : staged) {
        out.emplace_back(path, id);
    }
    return out;
}

size_t Index::size() const {
    return staged.size();
}

void Index::clear() {
    staged.clear();
}

bool Index::save() const {
    std::ofstream out(indexPath, std::ios::trunc);
    if (!out.is_open()) return false;
    for (const auto& [path, id] : staged) {
        out << id.toHex() << ' ' << path << '\n';
    }
    return static_cast<bool>(out);
}

ObjectId Index::writeTreeFromIndex(ObjectStore& store) const {
    return buildTreeFromPaths(store, staged);
}

}  // namespace oggit

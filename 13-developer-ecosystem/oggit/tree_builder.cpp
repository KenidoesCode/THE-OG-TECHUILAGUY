#include "tree_builder.hpp"

namespace oggit {

namespace {

// A directory-shaped node built while reconstructing hierarchy from a
// flat (path -> blob) map. Not part of the public API: purely an
// intermediate step of buildTreeFromPaths.
struct TrieNode {
    std::map<std::string, ObjectId> files;
    std::map<std::string, TrieNode> dirs;
};

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

void insertPath(TrieNode& root, const std::vector<std::string>& segments, size_t index,
                 const ObjectId& blob) {
    if (index + 1 == segments.size()) {
        // A directory of the same name already exists here: the
        // directory wins (see index.hpp's documented conflict rule) —
        // silently drop this file rather than creating an ambiguous
        // tree with both a file and a directory of the same name.
        if (root.dirs.count(segments[index]) == 0) {
            root.files[segments[index]] = blob;
        }
        return;
    }
    const std::string& dirName = segments[index];
    root.files.erase(dirName);  // a file previously staged here loses to the directory
    insertPath(root.dirs[dirName], segments, index + 1, blob);
}

ObjectId writeTrieAsTree(const TrieNode& node, ObjectStore& store) {
    std::vector<TreeEntry> entries;
    entries.reserve(node.files.size() + node.dirs.size());
    for (const auto& [name, blob] : node.files) {
        entries.push_back(TreeEntry{name, blob, false});
    }
    for (const auto& [name, child] : node.dirs) {
        ObjectId childTreeId = writeTrieAsTree(child, store);
        entries.push_back(TreeEntry{name, childTreeId, true});
    }
    std::vector<uint8_t> serialized = serializeTree(entries);
    return store.writeObject(ObjectType::Tree, serialized);
}

}  // namespace

ObjectId buildTreeFromPaths(ObjectStore& store, const std::map<std::string, ObjectId>& files) {
    TrieNode root;
    for (const auto& [path, id] : files) {
        std::vector<std::string> segments = splitPath(path);
        if (segments.empty()) continue;  // defensively ignore a fully-empty path
        insertPath(root, segments, 0, id);
    }
    return writeTrieAsTree(root, store);
}

}  // namespace oggit

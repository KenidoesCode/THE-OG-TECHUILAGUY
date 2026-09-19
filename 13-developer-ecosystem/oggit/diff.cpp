#include "diff.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace oggit {

namespace {

bool loadTree(ObjectStore& store, const ObjectId& id, std::vector<TreeEntry>& out) {
    ObjectType type;
    std::vector<uint8_t> content;
    if (store.readObject(id, type, content) != StoreError::None) return false;
    if (type != ObjectType::Tree) return false;
    return parseTree(content, out);
}

void emitAllAdded(ObjectStore& store, const TreeEntry& entry, const std::string& path,
                   std::vector<DiffEntry>& out) {
    if (!entry.isDirectory) {
        out.push_back(DiffEntry{path, DiffStatus::Added, ObjectId{}, entry.id});
        return;
    }
    std::vector<TreeEntry> children;
    if (!loadTree(store, entry.id, children)) return;  // corrupt subtree: nothing further to report
    for (const auto& child : children) {
        emitAllAdded(store, child, path + "/" + child.name, out);
    }
}

void emitAllRemoved(ObjectStore& store, const TreeEntry& entry, const std::string& path,
                     std::vector<DiffEntry>& out) {
    if (!entry.isDirectory) {
        out.push_back(DiffEntry{path, DiffStatus::Removed, entry.id, ObjectId{}});
        return;
    }
    std::vector<TreeEntry> children;
    if (!loadTree(store, entry.id, children)) return;
    for (const auto& child : children) {
        emitAllRemoved(store, child, path + "/" + child.name, out);
    }
}

void diffRecursive(ObjectStore& store, const ObjectId& oldTreeId, const ObjectId& newTreeId,
                    const std::string& prefix, std::vector<DiffEntry>& out) {
    std::vector<TreeEntry> oldEntries;
    std::vector<TreeEntry> newEntries;
    if (!loadTree(store, oldTreeId, oldEntries)) return;
    if (!loadTree(store, newTreeId, newEntries)) return;

    std::map<std::string, TreeEntry> oldByName;
    std::map<std::string, TreeEntry> newByName;
    for (const auto& e : oldEntries) oldByName[e.name] = e;
    for (const auto& e : newEntries) newByName[e.name] = e;

    std::set<std::string> allNames;
    for (const auto& [name, unused] : oldByName) allNames.insert(name);
    for (const auto& [name, unused] : newByName) allNames.insert(name);
    (void)0;  // silence unused-structured-binding warnings on some compilers

    for (const auto& name : allNames) {
        std::string path = prefix.empty() ? name : prefix + "/" + name;
        auto oldIt = oldByName.find(name);
        auto newIt = newByName.find(name);
        bool inOld = oldIt != oldByName.end();
        bool inNew = newIt != newByName.end();

        if (inOld && !inNew) {
            emitAllRemoved(store, oldIt->second, path, out);
        } else if (!inOld && inNew) {
            emitAllAdded(store, newIt->second, path, out);
        } else {
            const TreeEntry& oldEntry = oldIt->second;
            const TreeEntry& newEntry = newIt->second;
            if (oldEntry.isDirectory != newEntry.isDirectory) {
                emitAllRemoved(store, oldEntry, path, out);
                emitAllAdded(store, newEntry, path, out);
            } else if (oldEntry.isDirectory) {
                if (!(oldEntry.id == newEntry.id)) {
                    diffRecursive(store, oldEntry.id, newEntry.id, path, out);
                }
            } else if (!(oldEntry.id == newEntry.id)) {
                out.push_back(DiffEntry{path, DiffStatus::Modified, oldEntry.id, newEntry.id});
            }
        }
    }
}

}  // namespace

std::vector<DiffEntry> diffTrees(
    ObjectStore& store, const ObjectId& oldTreeId, const ObjectId& newTreeId
) {
    std::vector<DiffEntry> out;
    if (oldTreeId == newTreeId) {
        return out;  // identical tree id => provably identical content, no need to even read either
    }
    diffRecursive(store, oldTreeId, newTreeId, "", out);
    std::sort(out.begin(), out.end(), [](const DiffEntry& a, const DiffEntry& b) {
        return a.path < b.path;
    });
    return out;
}

}  // namespace oggit

#include "merge.hpp"

#include "tree_builder.hpp"

#include <algorithm>
#include <map>
#include <queue>
#include <set>

namespace oggit {

namespace {

bool readCommitObject(ObjectStore& store, const ObjectId& id, Commit& out) {
    ObjectType type;
    std::vector<uint8_t> content;
    if (store.readObject(id, type, content) != StoreError::None || type != ObjectType::Commit) {
        return false;
    }
    return parseCommit(content, out);
}

// Real BFS over EVERY parent (not parents[0] only) — the actual DAG
// traversal a merge needs, unlike refs.hpp's first-parent-only
// walkFirstParentHistory. Returns every reachable ancestor (including
// `start` itself at depth 0) mapped to its shortest BFS distance. If a
// commit reached mid-walk fails to read as a real Commit, that branch
// simply stops expanding past it (see docs/ADR/0020-oggit-merge.md's
// "Ancestor traversal" section) — the commits already found remain
// valid.
std::map<std::string, int> allAncestors(ObjectStore& store, const ObjectId& start) {
    std::map<std::string, int> depth;
    std::queue<ObjectId> queue;
    depth[start.toHex()] = 0;
    queue.push(start);

    while (!queue.empty()) {
        ObjectId current = queue.front();
        queue.pop();
        int currentDepth = depth[current.toHex()];

        Commit commit;
        if (!readCommitObject(store, current, commit)) {
            continue;  // corrupt/missing/wrong-type: stop expanding past it
        }
        for (const auto& parent : commit.parents) {
            std::string hex = parent.toHex();
            if (depth.count(hex) == 0) {
                depth[hex] = currentDepth + 1;
                queue.push(parent);
            }
        }
    }
    return depth;
}

bool flattenTree(ObjectStore& store, const ObjectId& treeId, const std::string& prefix,
                  std::map<std::string, ObjectId>& out) {
    ObjectType type;
    std::vector<uint8_t> content;
    if (store.readObject(treeId, type, content) != StoreError::None || type != ObjectType::Tree) {
        return false;
    }
    std::vector<TreeEntry> entries;
    if (!parseTree(content, entries)) {
        return false;
    }
    for (const auto& entry : entries) {
        std::string path = prefix.empty() ? entry.name : prefix + "/" + entry.name;
        if (entry.isDirectory) {
            if (!flattenTree(store, entry.id, path, out)) return false;
        } else {
            out[path] = entry.id;
        }
    }
    return true;
}

// True if `otherFiles` has any exact file leaf whose path starts with
// "path/" — i.e. `otherFiles`' side uses `path` as a directory.
bool otherSideUsesAsDirectory(const std::map<std::string, ObjectId>& otherFiles,
                               const std::string& path) {
    std::string prefix = path + "/";
    auto it = otherFiles.lower_bound(prefix);
    return it != otherFiles.end() && it->first.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

MergeResult mergeCommits(
    ObjectStore& store, const ObjectId& oursCommit, const ObjectId& theirsCommit
) {
    MergeResult result;

    Commit oursCommitObj;
    if (!readCommitObject(store, oursCommit, oursCommitObj)) {
        result.error = MergeError::OursCommitUnreadable;
        return result;
    }
    Commit theirsCommitObj;
    if (!readCommitObject(store, theirsCommit, theirsCommitObj)) {
        result.error = MergeError::TheirsCommitUnreadable;
        return result;
    }

    std::map<std::string, int> ancestorsOfOurs = allAncestors(store, oursCommit);
    std::map<std::string, int> ancestorsOfTheirs = allAncestors(store, theirsCommit);

    if (ancestorsOfOurs.count(theirsCommit.toHex()) > 0) {
        result.outcome = MergeOutcome::AlreadyUpToDate;
        result.mergeBase = theirsCommit;
        return result;
    }
    if (ancestorsOfTheirs.count(oursCommit.toHex()) > 0) {
        result.outcome = MergeOutcome::FastForward;
        result.mergeBase = oursCommit;
        result.resultTree = theirsCommitObj.tree;
        return result;
    }

    std::vector<std::string> common;
    for (const auto& [hex, unused] : ancestorsOfOurs) {
        if (ancestorsOfTheirs.count(hex) > 0) common.push_back(hex);
    }
    if (common.empty()) {
        result.error = MergeError::NoCommonAncestor;
        return result;
    }

    // Filter to the undominated (lowest) common ancestors: a candidate
    // is dominated if it appears in some OTHER candidate's own
    // ancestor set (that other candidate is strictly more recent and
    // still common).
    std::map<std::string, std::map<std::string, int>> candidateAncestors;
    for (const auto& hex : common) {
        ObjectId id;
        parseObjectId(hex, id);
        candidateAncestors[hex] = allAncestors(store, id);
    }
    std::vector<std::string> undominated;
    for (const auto& candidate : common) {
        bool dominated = false;
        for (const auto& other : common) {
            if (other == candidate) continue;
            if (candidateAncestors[other].count(candidate) > 0) {
                dominated = true;
                break;
            }
        }
        if (!dominated) undominated.push_back(candidate);
    }

    // Deterministic tie-break: lexicographically smallest hex id among
    // the (possibly several, in a criss-cross topology) undominated
    // candidates. See docs/ADR/0020-oggit-merge.md's "Deterministic
    // merge-base selection".
    std::sort(undominated.begin(), undominated.end());
    ObjectId mergeBaseId;
    parseObjectId(undominated.front(), mergeBaseId);
    result.mergeBase = mergeBaseId;

    Commit baseCommitObj;
    if (!readCommitObject(store, mergeBaseId, baseCommitObj)) {
        result.error = MergeError::CorruptTree;
        return result;
    }

    std::map<std::string, ObjectId> baseFiles;
    std::map<std::string, ObjectId> oursFiles;
    std::map<std::string, ObjectId> theirsFiles;
    if (!flattenTree(store, baseCommitObj.tree, "", baseFiles) ||
        !flattenTree(store, oursCommitObj.tree, "", oursFiles) ||
        !flattenTree(store, theirsCommitObj.tree, "", theirsFiles)) {
        result.error = MergeError::CorruptTree;
        return result;
    }

    std::set<std::string> allPaths;
    for (const auto& [path, unused] : baseFiles) allPaths.insert(path);
    for (const auto& [path, unused] : oursFiles) allPaths.insert(path);
    for (const auto& [path, unused] : theirsFiles) allPaths.insert(path);

    std::map<std::string, ObjectId> mergedFiles;
    std::vector<MergeConflict> conflicts;

    for (const auto& path : allPaths) {
        bool oursHasFile = oursFiles.count(path) > 0;
        bool theirsHasFile = theirsFiles.count(path) > 0;

        // File/directory conflict takes priority over ordinary
        // add/modify/delete classification: silently letting
        // buildTreeFromPaths's "directory wins" rule resolve this
        // would drop one side's real content without ever reporting
        // it.
        if (oursHasFile && otherSideUsesAsDirectory(theirsFiles, path)) {
            conflicts.push_back(MergeConflict{path, MergeConflictKind::FileDirectory, oursFiles[path], ObjectId{}});
            continue;
        }
        if (theirsHasFile && otherSideUsesAsDirectory(oursFiles, path)) {
            conflicts.push_back(MergeConflict{path, MergeConflictKind::FileDirectory, ObjectId{}, theirsFiles[path]});
            continue;
        }

        bool inBase = baseFiles.count(path) > 0;
        bool inOurs = oursHasFile;
        bool inTheirs = theirsHasFile;
        ObjectId baseId = inBase ? baseFiles[path] : ObjectId{};
        ObjectId oursId = inOurs ? oursFiles[path] : ObjectId{};
        ObjectId theirsId = inTheirs ? theirsFiles[path] : ObjectId{};

        bool oursChanged = (inOurs != inBase) || (inOurs && inBase && !(oursId == baseId));
        bool theirsChanged = (inTheirs != inBase) || (inTheirs && inBase && !(theirsId == baseId));

        if (!oursChanged && !theirsChanged) {
            if (inBase) mergedFiles[path] = baseId;
            continue;
        }
        if (oursChanged && !theirsChanged) {
            if (inOurs) mergedFiles[path] = oursId;
            continue;
        }
        if (!oursChanged && theirsChanged) {
            if (inTheirs) mergedFiles[path] = theirsId;
            continue;
        }

        // Both sides changed this path relative to base.
        if (inOurs && inTheirs) {
            if (oursId == theirsId) {
                mergedFiles[path] = oursId;  // both independently arrived at the same content
            } else if (!inBase) {
                conflicts.push_back(MergeConflict{path, MergeConflictKind::AddAdd, oursId, theirsId});
            } else {
                conflicts.push_back(MergeConflict{path, MergeConflictKind::ModifyModify, oursId, theirsId});
            }
        } else if (inOurs && !inTheirs) {
            conflicts.push_back(MergeConflict{path, MergeConflictKind::ModifyDelete, oursId, ObjectId{}});
        } else if (!inOurs && inTheirs) {
            conflicts.push_back(MergeConflict{path, MergeConflictKind::ModifyDelete, ObjectId{}, theirsId});
        }
        // else: both absent (both sides independently deleted it) — nothing to merge, no conflict.
    }

    if (!conflicts.empty()) {
        std::sort(conflicts.begin(), conflicts.end(),
                  [](const MergeConflict& a, const MergeConflict& b) { return a.path < b.path; });
        result.outcome = MergeOutcome::Conflict;
        result.conflicts = std::move(conflicts);
        return result;
    }

    result.outcome = MergeOutcome::Merged;
    result.resultTree = buildTreeFromPaths(store, mergedFiles);
    return result;
}

}  // namespace oggit

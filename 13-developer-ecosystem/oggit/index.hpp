#pragma once

#include "object_store.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

// OGGit's index (staging area): the missing bridge between "I have
// some file content" and "a Commit points at a Tree." Real Git
// separates these two steps (`git add` then `git commit`) for a
// reason — the index is what lets you stage exactly the changes you
// want into the next commit, independent of what's currently on disk.
// This is a real, disk-persisted staging area built directly on
// ../oggit's existing ObjectStore/Tree machinery (object_store.hpp),
// not a new parallel storage format — see
// docs/ADR/0017-oggit-index.md.

namespace oggit {

class Index {
public:
    // `indexPath` is the file this index is persisted to. Loads
    // existing entries from it if it already exists (a corrupt or
    // malformed line is skipped, not fatal — the index is a cache
    // that can always be rebuilt by re-staging); starts empty
    // otherwise.
    explicit Index(std::string indexPath);

    // Hashes `content` as a Blob via `store` (idempotent — identical
    // content always produces the identical blob id) and stages
    // `path` to point at that blob, replacing any existing staged
    // entry for the same path. Mirrors `git add`. `path` is a
    // forward-slash-separated repository-relative path (e.g.
    // "src/main.og"); leading/trailing/duplicate slashes are
    // normalized away.
    ObjectId addFile(ObjectStore& store, const std::string& path,
                      const std::vector<uint8_t>& content);

    // Removes `path` from the index. Returns false if it wasn't
    // staged (nothing is modified in that case).
    bool removeFile(const std::string& path);

    bool isStaged(const std::string& path) const;
    bool getStagedBlob(const std::string& path, ObjectId& out) const;

    // All staged (path, blob id) pairs, sorted by path (std::map's
    // natural order) — deterministic and safe to iterate for display
    // or for building a tree.
    std::vector<std::pair<std::string, ObjectId>> entries() const;

    size_t size() const;

    void clear();

    // Persists the current entries to indexPath as plain text, one
    // entry per line: "<64-hex-blob-id> <path>\n". This is
    // deliberately our own simple format, not Git's binary index
    // format — nothing here claims on-disk compatibility with real
    // Git (see docs/ADR/0017-oggit-index.md).
    bool save() const;

    // Builds real, hierarchical Tree objects from every currently
    // staged path: each path is split on '/' to reconstruct the
    // directory structure, an intermediate Tree object is written for
    // every directory level (deepest first, so a parent tree can
    // embed its children's already-computed ids), and the id of the
    // root Tree is returned. An empty index produces the id of an
    // empty Tree (zero entries). This is the actual mechanism that
    // turns "a flat set of staged files" into the single ObjectId a
    // Commit (see object_store.hpp's Commit::tree) points at.
    //
    // Path/type conflicts (staging a file at "a" AND a file at
    // "a/b" — "a" used as both a file and a directory) are resolved
    // deterministically: the directory always wins, regardless of
    // staging order, and the conflicting file entry at the shorter
    // path ("a") is dropped from the resulting tree. This is a known,
    // tested limitation, not silently undefined behavior — see the
    // test suite's conflicting-path case for the exact resolution.
    ObjectId writeTreeFromIndex(ObjectStore& store) const;

private:
    std::string indexPath;
    std::map<std::string, ObjectId> staged;

    void load();
};

}  // namespace oggit

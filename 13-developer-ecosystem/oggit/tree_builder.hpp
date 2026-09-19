#pragma once

#include "object_store.hpp"

#include <map>
#include <string>

// Shared tree-construction primitive: turns a flat `path -> blob
// ObjectId` map into a real, hierarchical Tree object (writing every
// intermediate directory level through ObjectStore, deepest first),
// returning the root Tree's id. Originally written for index.hpp's
// writeTreeFromIndex (ADR 0017) and reused as-is by merge.hpp's
// three-way merge (ADR 0020), since both need the identical
// files-on-paths -> real-Tree-hierarchy construction.

namespace oggit {

// See index.hpp's writeTreeFromIndex for the exact documented
// semantics (empty input -> empty tree; a file/directory path
// conflict resolves with the directory always winning, independent of
// map iteration order).
ObjectId buildTreeFromPaths(ObjectStore& store, const std::map<std::string, ObjectId>& files);

}  // namespace oggit

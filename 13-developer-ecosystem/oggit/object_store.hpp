#pragma once

#include <cstdint>
#include <string>
#include <vector>

// OGGit's content-addressed object store — the foundation everything
// else in OGGit (commits, branches, merge, remote sync) is built on.
// An object's identity is the SHA-256 hash (see
// 10-cryptography/hashing/sha256.hpp — a real integration point, not
// a reimplementation) of a small header ("<type> <size>\0") followed
// by its raw content, closely following the same idea real Git uses
// (a header + content hash) without claiming on-disk byte-for-byte
// compatibility with Git's own object format (no zlib compression
// here — see docs/ADR/0007-oggit-object-store.md for the exact scope
// and why).
//
// This is a real, disk-backed, host-side developer tool — unlike
// 22-os or 06-networking, there is no freestanding/hardware
// constraint here, so it uses ordinary filesystem I/O.

namespace oggit {

enum class ObjectType {
    Blob,
    Tree,
    Commit,
};

std::string objectTypeName(ObjectType type);

// A 32-byte SHA-256 digest identifying an object, plus its lowercase
// hex string form (the form used for on-disk paths and for embedding
// one object's id inside another, e.g. a tree entry's blob id or a
// commit's tree id).
struct ObjectId {
    uint8_t bytes[32];

    std::string toHex() const;
    bool operator==(const ObjectId& other) const;
};

// Parses a 64-character lowercase hex string back into an ObjectId.
// Returns false (leaving `out` unspecified) if `hex` is not exactly
// 64 valid lowercase hex characters — never partially parses.
bool parseObjectId(const std::string& hex, ObjectId& out);

enum class StoreError {
    None,
    NotFound,
    CorruptObject,     // stored bytes' hash no longer matches its filename
    MalformedHeader,
    IoError,
};

class ObjectStore {
public:
    // `rootDirectory` is created (including any missing parent
    // directories) if it doesn't already exist.
    explicit ObjectStore(const std::string& rootDirectory);

    // Computes the object's id from (type, content) and writes it to
    // disk under rootDirectory/objects/<first-2-hex>/<remaining-62-hex>
    // — writing is idempotent: writing identical content twice
    // produces the identical id and simply overwrites the same file
    // with the same bytes.
    ObjectId writeObject(ObjectType type, const std::vector<uint8_t>& content);

    // Reads an object back and independently re-hashes the bytes read
    // from disk, returning StoreError::CorruptObject if the content on
    // disk no longer hashes to the id used to look it up — objects are
    // supposed to be immutable and content-addressed, so this
    // mismatch is a real, meaningful integrity violation, not merely
    // one of many equally-valid ways of failing.
    StoreError readObject(
        const ObjectId& id,
        ObjectType& typeOut,
        std::vector<uint8_t>& contentOut
    ) const;

    bool exists(const ObjectId& id) const;

private:
    std::string rootDirectory;

    std::string pathFor(const ObjectId& id) const;
};

// --- Tree objects: an ordered list of named entries, each pointing at
// another object (a Blob for a file, or a Tree for a subdirectory). ---

struct TreeEntry {
    std::string name;
    ObjectId id;
    bool isDirectory;  // true => id refers to a Tree, false => a Blob
};

// Serializes entries in a fixed canonical order (sorted by name) so
// two trees with the same logical contents always produce identical
// bytes, and therefore the identical object id — the property that
// makes content-addressing meaningful for detecting "these two
// directory snapshots are identical" without walking every entry.
std::vector<uint8_t> serializeTree(const std::vector<TreeEntry>& entries);

// Returns false on any malformed encoding (truncated entry, an
// ObjectId field that isn't valid hex, a length field pointing past
// the buffer) rather than reading past the buffer or silently
// producing a truncated entry list.
bool parseTree(const std::vector<uint8_t>& content, std::vector<TreeEntry>& out);

// --- Commit objects ---

struct Commit {
    ObjectId tree;
    std::vector<ObjectId> parents;  // empty for the first commit; more
                                     // than one entry represents a
                                     // merge commit
    std::string author;
    std::string message;
};

std::vector<uint8_t> serializeCommit(const Commit& commit);
bool parseCommit(const std::vector<uint8_t>& content, Commit& out);

}  // namespace oggit

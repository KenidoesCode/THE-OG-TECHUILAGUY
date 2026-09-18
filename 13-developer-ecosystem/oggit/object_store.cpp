#include "object_store.hpp"

#include "../../10-cryptography/hashing/sha256.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace oggit {

namespace {

const char HEX_DIGITS[] = "0123456789abcdef";

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

bool readU32(const std::vector<uint8_t>& buf, size_t offset, uint32_t& out) {
    if (offset + 4 > buf.size()) return false;
    out = (static_cast<uint32_t>(buf[offset]) << 24) |
          (static_cast<uint32_t>(buf[offset + 1]) << 16) |
          (static_cast<uint32_t>(buf[offset + 2]) << 8) |
          static_cast<uint32_t>(buf[offset + 3]);
    return true;
}

void appendBytes(std::vector<uint8_t>& out, const std::string& s) {
    appendU32(out, static_cast<uint32_t>(s.size()));
    for (char c : s) out.push_back(static_cast<uint8_t>(c));
}

bool readBytes(
    const std::vector<uint8_t>& buf, size_t& offset, std::string& out
) {
    uint32_t length;
    if (!readU32(buf, offset, length)) return false;
    offset += 4;
    if (offset + length > buf.size()) return false;
    out.assign(buf.begin() + static_cast<long>(offset),
               buf.begin() + static_cast<long>(offset + length));
    offset += length;
    return true;
}

void appendObjectId(std::vector<uint8_t>& out, const ObjectId& id) {
    for (uint8_t b : id.bytes) out.push_back(b);
}

bool readObjectId(
    const std::vector<uint8_t>& buf, size_t& offset, ObjectId& out
) {
    if (offset + 32 > buf.size()) return false;
    for (int i = 0; i < 32; ++i) out.bytes[i] = buf[offset + static_cast<size_t>(i)];
    offset += 32;
    return true;
}

}  // namespace

std::string objectTypeName(ObjectType type) {
    switch (type) {
        case ObjectType::Blob: return "blob";
        case ObjectType::Tree: return "tree";
        case ObjectType::Commit: return "commit";
    }
    return "unknown";
}

std::string ObjectId::toHex() const {
    std::string hex;
    hex.reserve(64);
    for (uint8_t b : bytes) {
        hex += HEX_DIGITS[(b >> 4) & 0xF];
        hex += HEX_DIGITS[b & 0xF];
    }
    return hex;
}

bool ObjectId::operator==(const ObjectId& other) const {
    for (int i = 0; i < 32; ++i) {
        if (bytes[i] != other.bytes[i]) return false;
    }
    return true;
}

bool parseObjectId(const std::string& hex, ObjectId& out) {
    if (hex.size() != 64) return false;

    for (int i = 0; i < 32; ++i) {
        int hi = hexValue(hex[static_cast<size_t>(i) * 2]);
        int lo = hexValue(hex[static_cast<size_t>(i) * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return true;
}

namespace {

ObjectId hashObject(ObjectType type, const std::vector<uint8_t>& content) {
    // Header: "<type> <decimal size>\0" followed by the raw content —
    // the same idea Git's own object hashing uses (hash the header
    // plus content, not the content alone), which is what makes a
    // blob and a tree with coincidentally identical bytes still hash
    // to different ids.
    std::string header = objectTypeName(type) + " " +
                          std::to_string(content.size()) + '\0';

    std::vector<uint8_t> hashInput;
    hashInput.reserve(header.size() + content.size());
    for (char c : header) hashInput.push_back(static_cast<uint8_t>(c));
    hashInput.insert(hashInput.end(), content.begin(), content.end());

    ObjectId id;
    crypto::sha256(hashInput.data(), hashInput.size(), id.bytes);
    return id;
}

}  // namespace

ObjectStore::ObjectStore(const std::string& rootDirectory)
    : rootDirectory(rootDirectory) {
    std::filesystem::create_directories(
        std::filesystem::path(rootDirectory) / "objects"
    );
}

std::string ObjectStore::pathFor(const ObjectId& id) const {
    std::string hex = id.toHex();
    std::filesystem::path path =
        std::filesystem::path(rootDirectory) / "objects" /
        hex.substr(0, 2) / hex.substr(2);
    return path.string();
}

ObjectId ObjectStore::writeObject(
    ObjectType type, const std::vector<uint8_t>& content
) {
    ObjectId id = hashObject(type, content);

    std::filesystem::path path(pathFor(id));
    std::filesystem::create_directories(path.parent_path());

    // Stored on disk exactly as "<type> <size>\0<content>" — the same
    // bytes that were hashed — so readObject() can independently
    // re-derive and verify the id from what it actually reads back,
    // rather than trusting a filename alone.
    std::string header = objectTypeName(type) + " " +
                          std::to_string(content.size()) + '\0';

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(header.data(), static_cast<std::streamsize>(header.size()));
    out.write(
        reinterpret_cast<const char*>(content.data()),
        static_cast<std::streamsize>(content.size())
    );

    return id;
}

StoreError ObjectStore::readObject(
    const ObjectId& id,
    ObjectType& typeOut,
    std::vector<uint8_t>& contentOut
) const {
    std::filesystem::path path(pathFor(id));

    if (!std::filesystem::exists(path)) {
        return StoreError::NotFound;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return StoreError::IoError;
    }

    std::vector<uint8_t> raw(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );

    // Parse the "<type> <size>\0" header back out.
    size_t spacePos = raw.size();
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == ' ') { spacePos = i; break; }
    }
    if (spacePos == raw.size()) return StoreError::MalformedHeader;

    std::string typeName(raw.begin(), raw.begin() + static_cast<long>(spacePos));

    size_t nulPos = raw.size();
    for (size_t i = spacePos + 1; i < raw.size(); ++i) {
        if (raw[i] == '\0') { nulPos = i; break; }
    }
    if (nulPos == raw.size()) return StoreError::MalformedHeader;

    std::string sizeStr(
        raw.begin() + static_cast<long>(spacePos) + 1,
        raw.begin() + static_cast<long>(nulPos)
    );

    ObjectType type;
    if (typeName == "blob") type = ObjectType::Blob;
    else if (typeName == "tree") type = ObjectType::Tree;
    else if (typeName == "commit") type = ObjectType::Commit;
    else return StoreError::MalformedHeader;

    std::vector<uint8_t> content(
        raw.begin() + static_cast<long>(nulPos) + 1, raw.end()
    );

    size_t claimedSize;
    try {
        claimedSize = static_cast<size_t>(std::stoull(sizeStr));
    } catch (...) {
        return StoreError::MalformedHeader;
    }
    if (claimedSize != content.size()) {
        return StoreError::MalformedHeader;
    }

    // Independently re-hash what was actually read and compare
    // against the id used to locate this file — the on-disk content
    // is untrusted until this check passes, exactly the same
    // "validate before trusting" discipline applied throughout this
    // project's other parsers (see 22-os/elf/elf.cpp).
    ObjectId recomputed = hashObject(type, content);
    if (!(recomputed == id)) {
        return StoreError::CorruptObject;
    }

    typeOut = type;
    contentOut = std::move(content);
    return StoreError::None;
}

bool ObjectStore::exists(const ObjectId& id) const {
    return std::filesystem::exists(std::filesystem::path(pathFor(id)));
}

std::vector<uint8_t> serializeTree(const std::vector<TreeEntry>& entries) {
    std::vector<TreeEntry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const TreeEntry& a, const TreeEntry& b) {
        return a.name < b.name;
    });

    std::vector<uint8_t> out;
    appendU32(out, static_cast<uint32_t>(sorted.size()));
    for (const TreeEntry& entry : sorted) {
        appendBytes(out, entry.name);
        out.push_back(entry.isDirectory ? 1 : 0);
        appendObjectId(out, entry.id);
    }
    return out;
}

bool parseTree(const std::vector<uint8_t>& content, std::vector<TreeEntry>& out) {
    size_t offset = 0;
    uint32_t count;
    if (!readU32(content, offset, count)) return false;
    offset += 4;

    out.clear();
    out.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        TreeEntry entry;
        if (!readBytes(content, offset, entry.name)) return false;

        if (offset >= content.size()) return false;
        entry.isDirectory = content[offset] != 0;
        offset += 1;

        if (!readObjectId(content, offset, entry.id)) return false;

        out.push_back(entry);
    }

    return offset == content.size();
}

std::vector<uint8_t> serializeCommit(const Commit& commit) {
    std::vector<uint8_t> out;
    appendObjectId(out, commit.tree);
    appendU32(out, static_cast<uint32_t>(commit.parents.size()));
    for (const ObjectId& parent : commit.parents) {
        appendObjectId(out, parent);
    }
    appendBytes(out, commit.author);
    appendBytes(out, commit.message);
    return out;
}

bool parseCommit(const std::vector<uint8_t>& content, Commit& out) {
    size_t offset = 0;

    if (!readObjectId(content, offset, out.tree)) return false;

    uint32_t parentCount;
    if (!readU32(content, offset, parentCount)) return false;
    offset += 4;

    out.parents.clear();
    for (uint32_t i = 0; i < parentCount; ++i) {
        ObjectId parent;
        if (!readObjectId(content, offset, parent)) return false;
        out.parents.push_back(parent);
    }

    if (!readBytes(content, offset, out.author)) return false;
    if (!readBytes(content, offset, out.message)) return false;

    return offset == content.size();
}

}  // namespace oggit

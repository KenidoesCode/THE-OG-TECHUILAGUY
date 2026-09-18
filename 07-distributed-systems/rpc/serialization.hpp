#pragma once

#include <cstdint>
#include <string>
#include <vector>

// A small, deterministic binary serialization format for RPC message
// framing (Layer 7, FR-DIST-1). Every multi-byte integer is written
// big-endian; every length-prefixed field (strings, byte blobs) is
// prefixed with a 4-byte length so a decoder never has to guess where
// a field ends. Deliberately schema-free at this layer — a caller
// encodes/decodes fields in a fixed, known order (see rpc.hpp's
// RpcRequest/RpcResponse for the actual wire schema) rather than this
// layer describing a schema itself; see
// docs/ADR/0008-distributed-rpc.md for why a full self-describing
// schema/versioning system is out of scope for v1.
//
// Every Decoder read is bounds-checked against the buffer it was
// constructed with — reading past the end never occurs; it fails and
// leaves `ok()` false instead, mirroring the "never trust
// input, always bounds-check before reading" discipline this project
// already applies in 22-os/elf/elf.cpp and 06-networking/protocols.

namespace dist {

class Encoder {
public:
    void writeU8(uint8_t value);
    void writeU32(uint32_t value);
    void writeU64(uint64_t value);
    void writeBytes(const std::vector<uint8_t>& bytes);
    void writeString(const std::string& s);

    const std::vector<uint8_t>& data() const { return buffer; }

private:
    std::vector<uint8_t> buffer;
};

class Decoder {
public:
    Decoder(const uint8_t* data, size_t length);
    explicit Decoder(const std::vector<uint8_t>& data);

    // Every read function returns false (and leaves `out` untouched,
    // and stops advancing) the moment there isn't enough remaining
    // buffer to satisfy the read — a decoder that has failed once
    // stays failed; callers should stop decoding rather than keep
    // calling more read functions on a Decoder that already returned
    // false, though doing so is still safe (every read after a
    // failure also just returns false).
    bool readU8(uint8_t& out);
    bool readU32(uint32_t& out);
    bool readU64(uint64_t& out);
    bool readBytes(std::vector<uint8_t>& out);
    bool readString(std::string& out);

    // True if every byte of the original buffer has been consumed —
    // useful for rejecting a message with unexpected trailing bytes
    // after its last defined field.
    bool atEnd() const;

private:
    const uint8_t* data;
    size_t length;
    size_t offset = 0;
    bool failed = false;
};

}  // namespace dist

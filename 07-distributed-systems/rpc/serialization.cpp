#include "serialization.hpp"

namespace dist {

void Encoder::writeU8(uint8_t value) {
    buffer.push_back(value);
}

void Encoder::writeU32(uint32_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

void Encoder::writeU64(uint64_t value) {
    for (int i = 7; i >= 0; --i) {
        buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void Encoder::writeBytes(const std::vector<uint8_t>& bytes) {
    writeU32(static_cast<uint32_t>(bytes.size()));
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

void Encoder::writeString(const std::string& s) {
    writeU32(static_cast<uint32_t>(s.size()));
    for (char c : s) buffer.push_back(static_cast<uint8_t>(c));
}

Decoder::Decoder(const uint8_t* data, size_t length)
    : data(data), length(length) {}

Decoder::Decoder(const std::vector<uint8_t>& data)
    : data(data.data()), length(data.size()) {}

bool Decoder::readU8(uint8_t& out) {
    if (failed || offset + 1 > length) {
        failed = true;
        return false;
    }
    out = data[offset];
    offset += 1;
    return true;
}

bool Decoder::readU32(uint32_t& out) {
    if (failed || offset + 4 > length) {
        failed = true;
        return false;
    }
    out = (static_cast<uint32_t>(data[offset]) << 24) |
          (static_cast<uint32_t>(data[offset + 1]) << 16) |
          (static_cast<uint32_t>(data[offset + 2]) << 8) |
          static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    return true;
}

bool Decoder::readU64(uint64_t& out) {
    if (failed || offset + 8 > length) {
        failed = true;
        return false;
    }
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out = (out << 8) | static_cast<uint64_t>(data[offset + static_cast<size_t>(i)]);
    }
    offset += 8;
    return true;
}

bool Decoder::readBytes(std::vector<uint8_t>& out) {
    uint32_t len;
    if (!readU32(len)) return false;

    // The claimed length must fit within what actually remains in the
    // buffer — never trusted on its own, the same discipline applied
    // to every other length-prefixed field this project parses (IPv4
    // totalLength, UDP length, ELF segment sizes).
    if (failed || offset + len > length) {
        failed = true;
        return false;
    }

    out.assign(data + offset, data + offset + len);
    offset += len;
    return true;
}

bool Decoder::readString(std::string& out) {
    std::vector<uint8_t> bytes;
    if (!readBytes(bytes)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

bool Decoder::atEnd() const {
    return !failed && offset == length;
}

}  // namespace dist

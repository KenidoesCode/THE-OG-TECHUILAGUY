#include "types.hpp"

#include "../../10-cryptography/hashing/sha256.hpp"

namespace l1 {

namespace {

const char HEX_DIGITS[] = "0123456789abcdef";

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

template <size_t N>
std::string toHexImpl(const std::array<uint8_t, N>& bytes) {
    std::string out;
    out.reserve(N * 2);
    for (uint8_t b : bytes) {
        out.push_back(HEX_DIGITS[b >> 4]);
        out.push_back(HEX_DIGITS[b & 0x0F]);
    }
    return out;
}

template <size_t N>
bool parseHexImpl(const std::string& hex, std::array<uint8_t, N>& out) {
    if (hex.size() != N * 2) return false;
    for (size_t i = 0; i < N; ++i) {
        int hi = hexValue(hex[i * 2]);
        int lo = hexValue(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

}  // namespace

std::string toHex(const Address& addr) { return toHexImpl(addr); }
std::string toHex(const Hash32& hash) { return toHexImpl(hash); }

bool parseAddress(const std::string& hex, Address& out) { return parseHexImpl(hex, out); }
bool parseHash32(const std::string& hex, Hash32& out) { return parseHexImpl(hex, out); }

Address deriveAddress(const std::string& label) {
    uint8_t digest[32];
    crypto::sha256(reinterpret_cast<const uint8_t*>(label.data()), label.size(), digest);
    Address addr;
    for (size_t i = 0; i < addr.size(); ++i) addr[i] = digest[i];
    return addr;
}

}  // namespace l1

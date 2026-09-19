#pragma once

#include <array>
#include <cstdint>
#include <string>

// Shared primitive types for the Techuilaguy L1 blockchain slice. See
// docs/ADR/0021-techuilaguy-blockchain-l1.md for the full design and
// explicit scope (no real key-pair/signature crypto is integrated
// yet — see this header's own deriveAddress for exactly what that
// means).

namespace l1 {

// A 20-byte account identifier. NOT derived from a real public key in
// this slice — there is no asymmetric-key integration yet.
using Address = std::array<uint8_t, 20>;

// A 32-byte content hash (transaction id, state root, tx root, block
// hash — all SHA-256 digests from 10-cryptography/hashing/sha256.hpp).
using Hash32 = std::array<uint8_t, 32>;

constexpr Address ZERO_ADDRESS{};
constexpr Hash32 ZERO_HASH32{};

std::string toHex(const Address& addr);
std::string toHex(const Hash32& hash);

// Parses exactly 40 (Address) or 64 (Hash32) lowercase hex characters.
// Returns false, leaving `out` untouched, on any malformed input.
bool parseAddress(const std::string& hex, Address& out);
bool parseHash32(const std::string& hex, Hash32& out);

// Deterministically derives an Address from an arbitrary label (the
// first 20 bytes of SHA-256(label)) — purely a convenience for tests
// and demos to get *a* reproducible address, not a security
// mechanism: it proves nothing about who controls the resulting
// address, since there is no signature scheme yet to make that
// meaningful.
Address deriveAddress(const std::string& label);

}  // namespace l1

#pragma once

#include "../../06-networking/protocols/protocols.hpp"

#include <vector>

// A real, simulated Ethernet frame — built directly on
// 06-networking/protocols/protocols.hpp's actual wire-format codec
// (serializeEthernetHeader/parseEthernetHeader), not a
// simulator-specific reimplementation. See
// docs/ADR/0022-techuilaguy-netlab-foundation.md.

namespace netlab {

constexpr net::MacAddress BROADCAST_MAC{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

bool isBroadcast(const net::MacAddress& mac);

struct Packet {
    std::vector<uint8_t> bytes;  // a real, complete Ethernet frame (header + payload)
};

// Builds a real Ethernet frame (header via serializeEthernetHeader,
// followed by `payload` verbatim) as a Packet ready to hand to
// Topology::sendFrame.
Packet buildEthernetFrame(
    const net::MacAddress& source, const net::MacAddress& destination,
    uint16_t etherType, const std::vector<uint8_t>& payload
);

// Parses `packet`'s Ethernet header back out. Returns false on a
// malformed/truncated frame — never crashes, never partially parses.
bool parseFrameHeader(const Packet& packet, net::EthernetHeader& out);

}  // namespace netlab

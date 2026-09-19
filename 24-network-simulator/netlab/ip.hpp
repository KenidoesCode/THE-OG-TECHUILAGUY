#pragma once

#include "topology.hpp"

#include <string>
#include <vector>

// Real IPv4 + ARP + single-hop routing simulation on top of ADR
// 0022's L2 Topology — see
// docs/ADR/0028-netlab-ip-arp-routing-mission.md for the full design.
// Every frame here is built through 06-networking's existing protocol
// codecs; nothing reimplements ARP/IPv4/ICMP wire formats.

namespace netlab {

// Convenience: packs four octets into the host-byte-order uint32_t
// form 06-networking's codecs use.
constexpr uint32_t makeIpv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

struct SimulationEvent {
    std::string description;
    std::vector<uint8_t> frameBytes;  // empty for a pure narrative event (e.g. a failure reason)
};

struct PingResult {
    bool delivered = false;
    std::string failureReason;  // set only when delivered == false
    std::vector<SimulationEvent> timeline;
};

// Simulates one ICMP echo (ping) from fromHostId to destIp, including
// real ARP resolution and, if needed, one real router hop. See the
// ADR for the exact step sequence and every documented limitation
// (single-hop routing only, no ARP cache persisted between calls, no
// DHCP/DNS/TCP/UDP).
PingResult sendIcmpEcho(Topology& topology, const std::string& fromHostId, uint32_t destIp);

}  // namespace netlab

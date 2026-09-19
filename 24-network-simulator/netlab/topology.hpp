#pragma once

#include "node.hpp"
#include "packet.hpp"

#include <map>
#include <string>
#include <vector>

// A graph of NetLab nodes connected by undirected links, and the real
// (simplified) L2 frame-delivery simulation over it. See
// docs/ADR/0022-techuilaguy-netlab-foundation.md for the original
// Host/Switch-only algorithm and its explicit, tested limitations (no
// spanning-tree protocol — cyclic topologies terminate but aren't
// "correctly" simulated), and
// docs/ADR/0028-netlab-ip-arp-routing-mission.md for the Router
// extension (a router is an L2 broadcast-domain boundary, not a
// flooding device).

namespace netlab {

struct RouterDelivery {
    std::string routerId;
    size_t interfaceIndex;
};

struct DeliveryResult {
    // Host node ids that accepted the frame (matched their own MAC,
    // or the frame's destination was the broadcast address), in the
    // order the simulated BFS reached them.
    std::vector<std::string> receivedByHosts;

    // Router (id, interface index) pairs that accepted the frame on
    // the specific interface it arrived facing.
    std::vector<RouterDelivery> receivedByRouterInterfaces;
};

class Topology {
public:
    // Adds `node`. Returns false (adds nothing) if a node with this id
    // already exists.
    bool addNode(const Node& node);

    // Adds an undirected link between two existing node ids. Returns
    // false (adds nothing) if either id doesn't exist or the link
    // already exists between them.
    bool addLink(const std::string& a, const std::string& b);

    bool hasNode(const std::string& id) const;
    const Node* getNode(const std::string& id) const;

    // Configures one interface on an existing Router node, facing an
    // existing, already-linked neighbor. Returns false if `routerId`
    // isn't a Router, `neighborId` isn't actually linked to it, or an
    // interface already exists for that neighbor.
    bool addRouterInterface(
        const std::string& routerId, const std::string& neighborId,
        const net::MacAddress& mac, uint32_t ipAddress, uint32_t subnetMask
    );

    // Delivers `packet` (a real Ethernet frame) starting from every
    // neighbor of `originId` (correct for a Host, which has exactly
    // one link in every topology this project builds). Returns an
    // empty result (no crash) if `originId` doesn't exist, isn't a
    // Host or Router, or `packet` fails to parse as a valid Ethernet
    // frame.
    DeliveryResult sendFrame(const std::string& originId, const Packet& packet);

    // The router-as-originator counterpart: starts delivery from
    // exactly `egressNeighborId` (which must be a real neighbor of
    // `originRouterId`) rather than flooding every neighbor — used
    // when a router itself transmits (an egress-side ARP request, or
    // a forwarded IP packet), so it never also re-transmits back
    // toward the interface the original packet arrived on.
    DeliveryResult sendFrameFromInterface(
        const std::string& originRouterId, const std::string& egressNeighborId, const Packet& packet
    );

private:
    std::map<std::string, Node> nodes;
    std::map<std::string, std::vector<std::string>> adjacency;

    // Per-switch: destination MAC (hex string) -> the neighbor node id
    // a frame from that MAC most recently arrived from.
    std::map<std::string, std::map<std::string, std::string>> switchMacTables;

    DeliveryResult deliverFrom(
        const std::string& originId, const std::vector<std::string>& startNeighbors, const Packet& packet
    );
};

}  // namespace netlab

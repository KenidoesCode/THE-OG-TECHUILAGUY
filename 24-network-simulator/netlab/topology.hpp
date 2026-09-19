#pragma once

#include "node.hpp"
#include "packet.hpp"

#include <map>
#include <string>
#include <vector>

// A graph of NetLab nodes connected by undirected links, and the real
// (simplified) L2 frame-delivery simulation over it. See
// docs/ADR/0022-techuilaguy-netlab-foundation.md for the full
// algorithm and its explicit, tested limitations (no spanning-tree
// protocol — cyclic topologies terminate but aren't "correctly"
// simulated).

namespace netlab {

struct DeliveryResult {
    // Host node ids that accepted the frame (matched their own MAC,
    // or the frame's destination was the broadcast address), in the
    // order the simulated BFS reached them.
    std::vector<std::string> receivedByHosts;
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

    // Delivers `packet` (a real Ethernet frame) starting from
    // `originHostId`. Returns an empty result (no crash) if
    // `originHostId` doesn't exist, isn't a Host, or `packet` fails to
    // parse as a valid Ethernet frame.
    DeliveryResult sendFrame(const std::string& originHostId, const Packet& packet);

private:
    std::map<std::string, Node> nodes;
    std::map<std::string, std::vector<std::string>> adjacency;

    // Per-switch: destination MAC (hex string) -> the neighbor node id
    // a frame from that MAC most recently arrived from.
    std::map<std::string, std::map<std::string, std::string>> switchMacTables;
};

}  // namespace netlab

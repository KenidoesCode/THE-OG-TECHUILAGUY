#include "topology.hpp"

#include <queue>
#include <set>

namespace netlab {

namespace {

const char HEX_DIGITS[] = "0123456789abcdef";

std::string macToHex(const net::MacAddress& mac) {
    std::string out;
    out.reserve(12);
    for (uint8_t b : mac.bytes) {
        out.push_back(HEX_DIGITS[b >> 4]);
        out.push_back(HEX_DIGITS[b & 0x0F]);
    }
    return out;
}

}  // namespace

bool Topology::addNode(const Node& node) {
    if (nodes.count(node.id) > 0) return false;
    nodes[node.id] = node;
    adjacency[node.id];  // ensure an (initially empty) adjacency entry exists
    return true;
}

bool Topology::addLink(const std::string& a, const std::string& b) {
    if (nodes.count(a) == 0 || nodes.count(b) == 0) return false;
    auto& neighborsOfA = adjacency[a];
    for (const auto& existing : neighborsOfA) {
        if (existing == b) return false;  // link already exists
    }
    adjacency[a].push_back(b);
    adjacency[b].push_back(a);
    return true;
}

bool Topology::hasNode(const std::string& id) const {
    return nodes.count(id) > 0;
}

DeliveryResult Topology::sendFrame(const std::string& originHostId, const Packet& packet) {
    DeliveryResult result;

    auto originIt = nodes.find(originHostId);
    if (originIt == nodes.end() || originIt->second.kind != NodeKind::Host) {
        return result;
    }

    net::EthernetHeader header;
    if (!parseFrameHeader(packet, header)) {
        return result;
    }
    std::string destMacHex = macToHex(header.destination);
    std::string srcMacHex = macToHex(header.source);
    bool broadcast = isBroadcast(header.destination);

    std::set<std::string> visited;
    visited.insert(originHostId);

    std::queue<std::pair<std::string, std::string>> queue;  // (arrivedFrom, current)
    for (const auto& neighbor : adjacency[originHostId]) {
        queue.push({originHostId, neighbor});
    }

    while (!queue.empty()) {
        auto [arrivedFrom, current] = queue.front();
        queue.pop();
        if (visited.count(current) > 0) continue;
        visited.insert(current);

        const Node& node = nodes.at(current);

        if (node.kind == NodeKind::Host) {
            if (broadcast || macToHex(node.mac) == destMacHex) {
                result.receivedByHosts.push_back(current);
            }
            continue;  // hosts never forward
        }

        // Switch: learn the source MAC against the neighbor this
        // frame arrived from, then decide forwarding.
        switchMacTables[current][srcMacHex] = arrivedFrom;

        std::vector<std::string> targets;
        if (!broadcast) {
            auto tableIt = switchMacTables[current].find(destMacHex);
            if (tableIt != switchMacTables[current].end() && tableIt->second != arrivedFrom) {
                targets.push_back(tableIt->second);
            }
        }
        if (targets.empty()) {
            // Unknown destination (or broadcast): flood every other port.
            for (const auto& neighbor : adjacency[current]) {
                if (neighbor != arrivedFrom) targets.push_back(neighbor);
            }
        }

        for (const auto& target : targets) {
            if (visited.count(target) == 0) {
                queue.push({current, target});
            }
        }
    }

    return result;
}

}  // namespace netlab

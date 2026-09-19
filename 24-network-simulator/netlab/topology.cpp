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

const Node* Topology::getNode(const std::string& id) const {
    auto it = nodes.find(id);
    return it == nodes.end() ? nullptr : &it->second;
}

bool Topology::addRouterInterface(
    const std::string& routerId, const std::string& neighborId,
    const net::MacAddress& mac, uint32_t ipAddress, uint32_t subnetMask
) {
    auto it = nodes.find(routerId);
    if (it == nodes.end() || it->second.kind != NodeKind::Router) return false;

    bool linked = false;
    for (const auto& n : adjacency[routerId]) {
        if (n == neighborId) {
            linked = true;
            break;
        }
    }
    if (!linked) return false;

    for (const auto& existing : it->second.routerInterfaces) {
        if (existing.neighborNodeId == neighborId) return false;  // already configured
    }

    it->second.routerInterfaces.push_back(RouterInterface{neighborId, mac, ipAddress, subnetMask});
    return true;
}

DeliveryResult Topology::deliverFrom(
    const std::string& originId, const std::vector<std::string>& startNeighbors, const Packet& packet
) {
    DeliveryResult result;

    net::EthernetHeader header;
    if (!parseFrameHeader(packet, header)) {
        return result;
    }
    std::string destMacHex = macToHex(header.destination);
    std::string srcMacHex = macToHex(header.source);
    bool broadcast = isBroadcast(header.destination);

    std::set<std::string> visited;
    visited.insert(originId);

    std::queue<std::pair<std::string, std::string>> queue;  // (arrivedFrom, current)
    for (const auto& neighbor : startNeighbors) {
        queue.push({originId, neighbor});
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

        if (node.kind == NodeKind::Router) {
            // A router is an L2 broadcast-domain boundary: it never
            // floods a frame to another link, it only ever "receives"
            // on the specific interface facing where the frame
            // arrived — see docs/ADR/0028-netlab-ip-arp-routing-mission.md.
            for (size_t i = 0; i < node.routerInterfaces.size(); ++i) {
                if (node.routerInterfaces[i].neighborNodeId == arrivedFrom) {
                    if (broadcast || macToHex(node.routerInterfaces[i].mac) == destMacHex) {
                        result.receivedByRouterInterfaces.push_back(RouterDelivery{current, i});
                    }
                    break;
                }
            }
            continue;
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

DeliveryResult Topology::sendFrame(const std::string& originId, const Packet& packet) {
    auto originIt = nodes.find(originId);
    if (originIt == nodes.end() ||
        (originIt->second.kind != NodeKind::Host && originIt->second.kind != NodeKind::Router)) {
        return DeliveryResult{};
    }
    return deliverFrom(originId, adjacency[originId], packet);
}

DeliveryResult Topology::sendFrameFromInterface(
    const std::string& originRouterId, const std::string& egressNeighborId, const Packet& packet
) {
    auto originIt = nodes.find(originRouterId);
    if (originIt == nodes.end() || originIt->second.kind != NodeKind::Router) {
        return DeliveryResult{};
    }
    bool linked = false;
    for (const auto& n : adjacency[originRouterId]) {
        if (n == egressNeighborId) {
            linked = true;
            break;
        }
    }
    if (!linked) return DeliveryResult{};

    return deliverFrom(originRouterId, {egressNeighborId}, packet);
}

}  // namespace netlab

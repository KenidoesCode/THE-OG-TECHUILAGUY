#include "ip.hpp"

#include <optional>

namespace netlab {

namespace {

std::string ipToString(uint32_t ip) {
    return std::to_string((ip >> 24) & 0xFF) + "." + std::to_string((ip >> 16) & 0xFF) + "." +
           std::to_string((ip >> 8) & 0xFF) + "." + std::to_string(ip & 0xFF);
}

// Identifies who is originating a frame transmission — either a Host
// (uses Topology::sendFrame, which floods its one link) or a Router
// speaking out one specific interface (uses
// Topology::sendFrameFromInterface).
struct Originator {
    bool isRouter = false;
    std::string nodeId;
    std::string egressNeighborId;  // only meaningful if isRouter
    net::MacAddress mac{};
    uint32_t ip = 0;
};

DeliveryResult sendFromOriginator(Topology& topology, const Originator& o, const netlab::Packet& packet) {
    if (o.isRouter) return topology.sendFrameFromInterface(o.nodeId, o.egressNeighborId, packet);
    return topology.sendFrame(o.nodeId, packet);
}

bool deliveryReachedOriginator(const DeliveryResult& dr, const Originator& o) {
    if (!o.isRouter) {
        for (const auto& hostId : dr.receivedByHosts) {
            if (hostId == o.nodeId) return true;
        }
        return false;
    }
    for (const auto& rd : dr.receivedByRouterInterfaces) {
        if (rd.routerId == o.nodeId) return true;
    }
    return false;
}

// Real ARP resolution: broadcasts a real ARP request from `requester`,
// finds whichever host or router interface on the reachable L2
// segment owns `targetIp`, and sends a real unicast ARP reply back.
// Returns the resolved MAC, or std::nullopt (with a reason appended to
// `timeline`) on failure.
std::optional<net::MacAddress> resolveArp(
    Topology& topology, const Originator& requester, uint32_t targetIp,
    std::vector<SimulationEvent>& timeline
) {
    net::ArpPacket request;
    request.operation = net::ArpOperation::Request;
    request.senderMac = requester.mac;
    request.senderIp = requester.ip;
    request.targetMac = net::MacAddress{{0, 0, 0, 0, 0, 0}};
    request.targetIp = targetIp;

    uint8_t arpBytes[net::ARP_PACKET_SIZE];
    net::serializeArpPacket(request, arpBytes, sizeof(arpBytes));
    std::vector<uint8_t> arpPayload(arpBytes, arpBytes + sizeof(arpBytes));
    Packet requestFrame = buildEthernetFrame(requester.mac, BROADCAST_MAC, net::ETHERTYPE_ARP, arpPayload);

    DeliveryResult requestDelivery = sendFromOriginator(topology, requester, requestFrame);
    timeline.push_back(SimulationEvent{
        "ARP request broadcast by " + requester.nodeId + " for " + ipToString(targetIp), requestFrame.bytes
    });

    Originator responder;
    net::MacAddress responderMac{};
    bool found = false;

    for (const auto& hostId : requestDelivery.receivedByHosts) {
        const Node* node = topology.getNode(hostId);
        if (node != nullptr && node->ipAddress == targetIp) {
            responder = Originator{false, hostId, "", node->mac, node->ipAddress};
            responderMac = node->mac;
            found = true;
            break;
        }
    }
    if (!found) {
        for (const auto& rd : requestDelivery.receivedByRouterInterfaces) {
            const Node* router = topology.getNode(rd.routerId);
            if (router == nullptr || rd.interfaceIndex >= router->routerInterfaces.size()) continue;
            const RouterInterface& iface = router->routerInterfaces[rd.interfaceIndex];
            if (iface.ipAddress == targetIp) {
                responder = Originator{true, rd.routerId, iface.neighborNodeId, iface.mac, iface.ipAddress};
                responderMac = iface.mac;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        timeline.push_back(SimulationEvent{
            "ARP resolution failed: no device on the reachable segment owns " + ipToString(targetIp), {}
        });
        return std::nullopt;
    }

    net::ArpPacket reply;
    reply.operation = net::ArpOperation::Reply;
    reply.senderMac = responderMac;
    reply.senderIp = targetIp;
    reply.targetMac = requester.mac;
    reply.targetIp = requester.ip;

    uint8_t replyBytes[net::ARP_PACKET_SIZE];
    net::serializeArpPacket(reply, replyBytes, sizeof(replyBytes));
    std::vector<uint8_t> replyPayload(replyBytes, replyBytes + sizeof(replyBytes));
    Packet replyFrame = buildEthernetFrame(responderMac, requester.mac, net::ETHERTYPE_ARP, replyPayload);

    DeliveryResult replyDelivery = sendFromOriginator(topology, responder, replyFrame);
    timeline.push_back(SimulationEvent{
        "ARP reply sent by " + responder.nodeId + " for " + ipToString(targetIp), replyFrame.bytes
    });

    if (!deliveryReachedOriginator(replyDelivery, requester)) {
        timeline.push_back(SimulationEvent{"ARP reply did not reach " + requester.nodeId, {}});
        return std::nullopt;
    }

    return responderMac;
}

std::vector<uint8_t> buildIcmpEchoIpPacket(uint32_t sourceIp, uint32_t destIp) {
    net::IcmpEchoMessage echo{net::ICMP_TYPE_ECHO_REQUEST, 0, 1, 1};
    uint8_t icmpBytes[net::ICMP_ECHO_HEADER_SIZE];
    uint32_t icmpLen = net::serializeIcmpEcho(echo, nullptr, 0, icmpBytes, sizeof(icmpBytes));

    net::Ipv4Header ipHeader;
    ipHeader.ttl = 64;
    ipHeader.protocol = net::IPV4_PROTO_ICMP;
    ipHeader.identification = 1;
    ipHeader.flagsAndFragmentOffset = 0;
    ipHeader.totalLength = static_cast<uint16_t>(net::IPV4_MIN_HEADER_SIZE + icmpLen);
    ipHeader.sourceIp = sourceIp;
    ipHeader.destIp = destIp;

    std::vector<uint8_t> packetBytes(net::IPV4_MIN_HEADER_SIZE + icmpLen);
    net::serializeIpv4Header(ipHeader, packetBytes.data(), static_cast<uint32_t>(packetBytes.size()));
    for (uint32_t i = 0; i < icmpLen; ++i) {
        packetBytes[net::IPV4_MIN_HEADER_SIZE + i] = icmpBytes[i];
    }
    return packetBytes;
}

}  // namespace

PingResult sendIcmpEcho(Topology& topology, const std::string& fromHostId, uint32_t destIp) {
    PingResult result;

    const Node* fromNode = topology.getNode(fromHostId);
    if (fromNode == nullptr || fromNode->kind != NodeKind::Host) {
        result.failureReason = "Source '" + fromHostId + "' is not a configured host";
        return result;
    }
    if (fromNode->ipAddress == 0) {
        result.failureReason = "Source host has no IPv4 address configured";
        return result;
    }

    bool sameSubnet = (fromNode->ipAddress & fromNode->subnetMask) == (destIp & fromNode->subnetMask);
    uint32_t nextHopIp = sameSubnet ? destIp : fromNode->defaultGatewayIp;
    if (!sameSubnet && nextHopIp == 0) {
        result.failureReason = "Destination is off-subnet and no default gateway is configured";
        return result;
    }

    Originator requester{false, fromHostId, "", fromNode->mac, fromNode->ipAddress};
    std::optional<net::MacAddress> nextHopMac = resolveArp(topology, requester, nextHopIp, result.timeline);
    if (!nextHopMac.has_value()) {
        result.failureReason = "ARP resolution failed for next hop " + ipToString(nextHopIp);
        return result;
    }

    std::vector<uint8_t> ipPacketBytes = buildIcmpEchoIpPacket(fromNode->ipAddress, destIp);
    Packet ipFrame = buildEthernetFrame(fromNode->mac, *nextHopMac, net::ETHERTYPE_IPV4, ipPacketBytes);
    DeliveryResult delivery = topology.sendFrame(fromHostId, ipFrame);
    result.timeline.push_back(SimulationEvent{
        "IPv4 ICMP echo request sent from " + fromHostId + " to " + ipToString(destIp) +
            " via next hop " + ipToString(nextHopIp),
        ipFrame.bytes
    });

    for (const auto& hostId : delivery.receivedByHosts) {
        const Node* node = topology.getNode(hostId);
        if (node != nullptr && node->ipAddress == destIp) {
            result.delivered = true;
            result.timeline.push_back(SimulationEvent{"Delivered to destination host " + hostId, {}});
            return result;
        }
    }

    for (const auto& rd : delivery.receivedByRouterInterfaces) {
        const Node* router = topology.getNode(rd.routerId);
        if (router == nullptr) continue;
        result.timeline.push_back(SimulationEvent{"Packet reached router " + rd.routerId, {}});

        for (size_t i = 0; i < router->routerInterfaces.size(); ++i) {
            if (i == rd.interfaceIndex) continue;
            const RouterInterface& iface = router->routerInterfaces[i];
            if ((destIp & iface.subnetMask) != (iface.ipAddress & iface.subnetMask)) continue;

            Originator routerOriginator{true, rd.routerId, iface.neighborNodeId, iface.mac, iface.ipAddress};
            std::optional<net::MacAddress> destMac = resolveArp(topology, routerOriginator, destIp, result.timeline);
            if (!destMac.has_value()) {
                result.failureReason = "Router " + rd.routerId + " could not resolve ARP for destination " + ipToString(destIp);
                return result;
            }

            Packet forwarded = buildEthernetFrame(iface.mac, *destMac, net::ETHERTYPE_IPV4, ipPacketBytes);
            DeliveryResult forwardedDelivery = topology.sendFrameFromInterface(rd.routerId, iface.neighborNodeId, forwarded);
            result.timeline.push_back(SimulationEvent{
                "Router " + rd.routerId + " forwarded the packet toward " + ipToString(destIp), forwarded.bytes
            });

            for (const auto& hostId2 : forwardedDelivery.receivedByHosts) {
                const Node* node2 = topology.getNode(hostId2);
                if (node2 != nullptr && node2->ipAddress == destIp) {
                    result.delivered = true;
                    result.timeline.push_back(SimulationEvent{"Delivered to destination host " + hostId2, {}});
                    return result;
                }
            }
            result.failureReason = "Router " + rd.routerId + " forwarded the packet, but it was not delivered to " + ipToString(destIp);
            return result;
        }

        result.failureReason = "Router " + rd.routerId + " has no route to " + ipToString(destIp);
        return result;
    }

    result.failureReason = "Packet was not delivered to any host or router on the reachable segment";
    return result;
}

}  // namespace netlab

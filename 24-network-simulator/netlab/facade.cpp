#include "facade.hpp"

namespace netlab {

namespace {

const uint32_t SUBNET_MASK_24 = makeIpv4(255, 255, 255, 0);

net::MacAddress mac(uint8_t last) {
    return net::MacAddress{{0x02, 0x00, 0x00, 0x00, 0x00, last}};
}

}  // namespace

NetLabFacade NetLabFacade::buildReferenceScenario() {
    NetLabFacade facade;
    Topology& topo = facade.topo;

    uint32_t gatewayA = makeIpv4(10, 0, 1, 1);
    uint32_t gatewayB = makeIpv4(10, 0, 2, 1);

    Node pc1{"PC1", NodeKind::Host, mac(1)};
    pc1.ipAddress = makeIpv4(10, 0, 1, 10);
    pc1.subnetMask = SUBNET_MASK_24;
    pc1.defaultGatewayIp = gatewayA;

    Node pc2{"PC2", NodeKind::Host, mac(2)};
    pc2.ipAddress = makeIpv4(10, 0, 2, 10);
    pc2.subnetMask = SUBNET_MASK_24;
    pc2.defaultGatewayIp = gatewayB;

    topo.addNode(pc1);
    topo.addNode(pc2);
    topo.addNode(Node{"SW1", NodeKind::Switch, {}});
    topo.addNode(Node{"SW2", NodeKind::Switch, {}});
    topo.addNode(Node{"R1", NodeKind::Router, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("SW1", "R1");
    topo.addLink("R1", "SW2");
    topo.addLink("SW2", "PC2");
    topo.addRouterInterface("R1", "SW1", mac(101), gatewayA, SUBNET_MASK_24);
    topo.addRouterInterface("R1", "SW2", mac(102), gatewayB, SUBNET_MASK_24);

    return facade;
}

bool NetLabFacade::runPing(const std::string& fromHostId, uint32_t destIp) {
    lastResult = sendIcmpEcho(topo, fromHostId, destIp);
    session.emplace(lastResult.timeline);
    return lastResult.delivered;
}

size_t NetLabFacade::eventCount() const {
    return session.has_value() ? session->eventCount() : 0;
}

size_t NetLabFacade::currentIndex() const {
    return session.has_value() ? session->currentIndex() : 0;
}

bool NetLabFacade::atStart() const {
    return !session.has_value() || session->atStart();
}

bool NetLabFacade::atEnd() const {
    return !session.has_value() || session->atEnd();
}

bool NetLabFacade::stepForward() {
    return session.has_value() && session->stepForward() != nullptr;
}

bool NetLabFacade::stepBackward() {
    return session.has_value() && session->stepBackward() != nullptr;
}

void NetLabFacade::resetSession() {
    if (session.has_value()) session->reset();
}

void NetLabFacade::jumpTo(size_t index) {
    if (session.has_value()) session->jumpTo(index);
}

std::string NetLabFacade::currentEventDescription() const {
    if (!session.has_value() || session->currentIndex() == 0) return "";
    return session->eventsSoFar().back().description;
}

DecodedFrame NetLabFacade::currentEventDecoded() const {
    if (!session.has_value() || session->currentIndex() == 0) return DecodedFrame{};
    return decodeFrame(session->eventsSoFar().back().frameBytes);
}

MissionResult NetLabFacade::runConnectTwoNetworksMission(const std::string& hostAId, const std::string& hostBId) {
    return evaluateConnectTwoNetworksMission(topo, hostAId, hostBId);
}

}  // namespace netlab

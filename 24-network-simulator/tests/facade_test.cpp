// Real assertion-based tests for NetLab's browser-foundation facade
// (netlab/facade.cpp) — proves the facade is a faithful composition of
// already-tested pieces (ADR 0022/0028/0029), not a new source of
// behavior. See docs/ADR/0030-netlab-browser-foundation-facade.md.

#include "../netlab/facade.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

net::MacAddress mac(uint8_t last) {
    return net::MacAddress{{0x02, 0x00, 0x00, 0x00, 0x00, last}};
}

const uint32_t SUBNET_MASK_24 = netlab::makeIpv4(255, 255, 255, 0);

// Builds the identical reference topology directly (not through the
// facade) so the facade's behavior can be independently cross-checked
// against calling the underlying primitives by hand.
std::vector<netlab::SimulationEvent> runReferenceScenarioDirectly() {
    netlab::Topology topo;
    uint32_t gatewayA = netlab::makeIpv4(10, 0, 1, 1);
    uint32_t gatewayB = netlab::makeIpv4(10, 0, 2, 1);
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 2, 10);

    netlab::Node pc1{"PC1", netlab::NodeKind::Host, mac(1)};
    pc1.ipAddress = netlab::makeIpv4(10, 0, 1, 10);
    pc1.subnetMask = SUBNET_MASK_24;
    pc1.defaultGatewayIp = gatewayA;
    netlab::Node pc2{"PC2", netlab::NodeKind::Host, mac(2)};
    pc2.ipAddress = pc2Ip;
    pc2.subnetMask = SUBNET_MASK_24;
    pc2.defaultGatewayIp = gatewayB;

    topo.addNode(pc1);
    topo.addNode(pc2);
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"SW2", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"R1", netlab::NodeKind::Router, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("SW1", "R1");
    topo.addLink("R1", "SW2");
    topo.addLink("SW2", "PC2");
    topo.addRouterInterface("R1", "SW1", mac(101), gatewayA, SUBNET_MASK_24);
    topo.addRouterInterface("R1", "SW2", mac(102), gatewayB, SUBNET_MASK_24);

    return netlab::sendIcmpEcho(topo, "PC1", pc2Ip).timeline;
}

}  // namespace

void testBuildReferenceScenarioAndRunPingSucceeds() {
    netlab::NetLabFacade facade = netlab::NetLabFacade::buildReferenceScenario();
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 2, 10);

    bool delivered = facade.runPing("PC1", pc2Ip);
    check(delivered, "facade: buildReferenceScenario() + runPing() reproduces the project's own verified PC1->Switch->Router->Switch->PC2 scenario");
    check(facade.lastRunDelivered(), "facade: lastRunDelivered() correctly reports the real outcome");
}

void testFacadeSessionMatchesDirectlyBuiltSession() {
    netlab::NetLabFacade facade = netlab::NetLabFacade::buildReferenceScenario();
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 2, 10);
    facade.runPing("PC1", pc2Ip);

    std::vector<netlab::SimulationEvent> directTimeline = runReferenceScenarioDirectly();
    check(facade.eventCount() == directTimeline.size(),
          "facade: the facade's session has exactly as many events as directly calling sendIcmpEcho on an identical topology produces");

    std::vector<std::string> facadeDescriptions;
    while (facade.stepForward()) {
        facadeDescriptions.push_back(facade.currentEventDescription());
    }
    check(facadeDescriptions.size() == directTimeline.size(), "facade test setup: stepping fully through the facade session visits every event");

    bool identical = facadeDescriptions.size() == directTimeline.size();
    for (size_t i = 0; identical && i < facadeDescriptions.size(); ++i) {
        if (facadeDescriptions[i] != directTimeline[i].description) identical = false;
    }
    check(identical, "facade: stepping through the facade's session reproduces the EXACT same event order/descriptions as building a SimulationSession directly (the facade doesn't silently reorder or alter anything)");

    check(facade.atEnd(), "facade: after stepping through every event, the facade reports atEnd()");
    facade.resetSession();
    check(facade.atStart(), "facade: resetSession() returns the facade's session to the start");
}

void testCurrentEventDecodedReflectsTheRealSimulatedPacket() {
    netlab::NetLabFacade facade = netlab::NetLabFacade::buildReferenceScenario();
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 2, 10);
    facade.runPing("PC1", pc2Ip);

    bool foundIcmpEvent = false;
    while (facade.stepForward()) {
        if (facade.currentEventDescription().find("IPv4 ICMP echo request sent") != std::string::npos) {
            foundIcmpEvent = true;
            netlab::DecodedFrame decoded = facade.currentEventDecoded();
            check(decoded.parsedSuccessfully, "facade: currentEventDecoded() at the real ICMP echo event decodes successfully");
            check(decoded.summary.find("10.0.2.10") != std::string::npos,
                  "facade: the decoded packet's destination IP matches the address the facade's own runPing() call actually used (rule 3: no fabricated visual state)");
            break;
        }
    }
    check(foundIcmpEvent, "facade test setup: the real ICMP echo event was found while stepping through the facade's session");
}

void testCurrentEventDecodedIsEmptyBeforeAnyStep() {
    netlab::NetLabFacade facade = netlab::NetLabFacade::buildReferenceScenario();
    facade.runPing("PC1", netlab::makeIpv4(10, 0, 2, 10));

    check(facade.currentEventDescription().empty(), "facade: before any stepForward(), currentEventDescription() is empty (nothing revealed yet)");
    netlab::DecodedFrame decoded = facade.currentEventDecoded();
    check(!decoded.parsedSuccessfully, "facade: before any stepForward(), currentEventDecoded() reports no successfully-decoded frame, not fabricated data");
}

void testRunConnectTwoNetworksMissionThroughFacade() {
    netlab::NetLabFacade facade = netlab::NetLabFacade::buildReferenceScenario();
    netlab::MissionResult mission = facade.runConnectTwoNetworksMission("PC1", "PC2");
    check(mission.success, "facade: 'Connect Two Networks' succeeds when evaluated through the facade on the reference scenario");
}

void testTopologyAccessorAllowsDirectInspection() {
    netlab::NetLabFacade facade = netlab::NetLabFacade::buildReferenceScenario();
    const netlab::Node* pc1 = facade.topology().getNode("PC1");
    check(pc1 != nullptr && pc1->ipAddress == netlab::makeIpv4(10, 0, 1, 10),
          "facade: topology() exposes the real underlying Topology for direct inspection/configuration, not a restricted copy");
}

int main() {
    testBuildReferenceScenarioAndRunPingSucceeds();
    testFacadeSessionMatchesDirectlyBuiltSession();
    testCurrentEventDecodedReflectsTheRealSimulatedPacket();
    testCurrentEventDecodedIsEmptyBeforeAnyStep();
    testRunConnectTwoNetworksMissionThroughFacade();
    testTopologyAccessorAllowsDirectInspection();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

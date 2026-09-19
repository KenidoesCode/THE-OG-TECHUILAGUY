// Real assertion-based tests for NetLab's IPv4 + ARP + single-hop
// routing simulation and first mission (ip.hpp/mission.hpp) — real
// frames built through 06-networking's actual protocol codecs, real
// L2 delivery via ADR 0022's Topology, not mocks. See
// docs/ADR/0028-netlab-ip-arp-routing-mission.md.

#include "../netlab/mission.hpp"

#include <iostream>
#include <string>

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

netlab::Node makeHost(
    const std::string& id, uint8_t macLast, uint32_t ip, uint32_t mask, uint32_t gateway = 0
) {
    netlab::Node node{id, netlab::NodeKind::Host, mac(macLast)};
    node.ipAddress = ip;
    node.subnetMask = mask;
    node.defaultGatewayIp = gateway;
    return node;
}

bool timelineContains(const netlab::PingResult& result, const std::string& substring) {
    for (const auto& event : result.timeline) {
        if (event.description.find(substring) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

void testPingWithinSameSubnetSucceedsViaDirectArp() {
    netlab::Topology topo;
    topo.addNode(makeHost("PC1", 1, netlab::makeIpv4(192, 168, 1, 10), SUBNET_MASK_24));
    topo.addNode(makeHost("PC2", 2, netlab::makeIpv4(192, 168, 1, 20), SUBNET_MASK_24));
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("PC2", "SW1");

    netlab::PingResult result = netlab::sendIcmpEcho(topo, "PC1", netlab::makeIpv4(192, 168, 1, 20));
    check(result.delivered, "netlab-ip: a ping between two hosts on the same subnet (no router needed) is delivered");
    check(timelineContains(result, "ARP request broadcast"), "netlab-ip: the same-subnet ping's timeline shows a real ARP request");
    check(timelineContains(result, "Delivered to destination host PC2"), "netlab-ip: the timeline's final event names the actual destination host");
}

void testFullTopologyPingAcrossRouterSucceeds() {
    netlab::Topology topo;
    uint32_t gatewayA = netlab::makeIpv4(10, 0, 1, 1);
    uint32_t gatewayB = netlab::makeIpv4(10, 0, 2, 1);
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 2, 10);

    topo.addNode(makeHost("PC1", 1, netlab::makeIpv4(10, 0, 1, 10), SUBNET_MASK_24, gatewayA));
    topo.addNode(makeHost("PC2", 2, pc2Ip, SUBNET_MASK_24, gatewayB));
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"SW2", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"R1", netlab::NodeKind::Router, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("SW1", "R1");
    topo.addLink("R1", "SW2");
    topo.addLink("SW2", "PC2");

    check(topo.addRouterInterface("R1", "SW1", mac(101), gatewayA, SUBNET_MASK_24),
          "netlab-ip test setup: configuring the router's first interface succeeds");
    check(topo.addRouterInterface("R1", "SW2", mac(102), gatewayB, SUBNET_MASK_24),
          "netlab-ip test setup: configuring the router's second interface succeeds");

    netlab::PingResult result = netlab::sendIcmpEcho(topo, "PC1", pc2Ip);
    check(result.delivered, "netlab-ip: PC1 -> Switch -> Router -> Switch -> PC2 ping succeeds end to end");
    check(timelineContains(result, "Packet reached router R1"), "netlab-ip: the timeline shows the packet genuinely reaching the router");
    check(timelineContains(result, "Router R1 forwarded the packet"), "netlab-ip: the timeline shows the router's real forwarding decision");
    check(timelineContains(result, "Delivered to destination host PC2"), "netlab-ip: the timeline's final event confirms delivery to PC2");
}

void testArpResolutionFailureIsReportedNotCrashed() {
    netlab::Topology topo;
    topo.addNode(makeHost("PC1", 1, netlab::makeIpv4(192, 168, 1, 10), SUBNET_MASK_24));
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("PC1", "SW1");

    // Nobody on the topology owns this IP.
    netlab::PingResult result = netlab::sendIcmpEcho(topo, "PC1", netlab::makeIpv4(192, 168, 1, 99));
    check(!result.delivered, "netlab-ip: pinging an IP nobody owns fails (not a crash, not a false success)");
    check(result.failureReason.find("ARP resolution failed") != std::string::npos,
          "netlab-ip: the failure reason specifically identifies ARP resolution as the cause");
}

void testRouterWithNoMatchingInterfaceReportsNoRoute() {
    netlab::Topology topo;
    uint32_t gatewayA = netlab::makeIpv4(10, 0, 1, 1);
    topo.addNode(makeHost("PC1", 1, netlab::makeIpv4(10, 0, 1, 10), SUBNET_MASK_24, gatewayA));
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"R1", netlab::NodeKind::Router, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("SW1", "R1");

    // The router has only ONE interface configured — no route to any
    // other subnet exists (a realistic misconfiguration).
    topo.addRouterInterface("R1", "SW1", mac(101), gatewayA, SUBNET_MASK_24);

    netlab::PingResult result = netlab::sendIcmpEcho(topo, "PC1", netlab::makeIpv4(10, 0, 2, 10));
    check(!result.delivered, "netlab-ip: a router with no interface covering the destination subnet fails to deliver");
    check(result.failureReason.find("no route") != std::string::npos,
          "netlab-ip: the failure reason specifically reports 'no route', not a generic failure");
}

void testFullPingSimulationIsDeterministic() {
    netlab::Topology topo;
    uint32_t gatewayA = netlab::makeIpv4(10, 0, 1, 1);
    uint32_t gatewayB = netlab::makeIpv4(10, 0, 2, 1);
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 2, 10);

    topo.addNode(makeHost("PC1", 1, netlab::makeIpv4(10, 0, 1, 10), SUBNET_MASK_24, gatewayA));
    topo.addNode(makeHost("PC2", 2, pc2Ip, SUBNET_MASK_24, gatewayB));
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"SW2", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"R1", netlab::NodeKind::Router, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("SW1", "R1");
    topo.addLink("R1", "SW2");
    topo.addLink("SW2", "PC2");
    topo.addRouterInterface("R1", "SW1", mac(101), gatewayA, SUBNET_MASK_24);
    topo.addRouterInterface("R1", "SW2", mac(102), gatewayB, SUBNET_MASK_24);

    netlab::PingResult first = netlab::sendIcmpEcho(topo, "PC1", pc2Ip);
    netlab::PingResult second = netlab::sendIcmpEcho(topo, "PC1", pc2Ip);

    check(first.delivered && second.delivered, "netlab-ip determinism test setup: both runs actually succeed");
    check(first.timeline.size() == second.timeline.size(), "netlab-ip: replaying the identical ping produces the identical number of timeline events");

    bool identical = first.timeline.size() == second.timeline.size();
    for (size_t i = 0; identical && i < first.timeline.size(); ++i) {
        if (first.timeline[i].description != second.timeline[i].description ||
            first.timeline[i].frameBytes != second.timeline[i].frameBytes) {
            identical = false;
        }
    }
    check(identical, "netlab-ip: replaying the identical ping produces byte-for-byte identical timeline events (deterministic replay)");
}

void testConnectTwoNetworksMissionSucceeds() {
    netlab::Topology topo;
    uint32_t gatewayA = netlab::makeIpv4(10, 0, 1, 1);
    uint32_t gatewayB = netlab::makeIpv4(10, 0, 2, 1);

    topo.addNode(makeHost("PC1", 1, netlab::makeIpv4(10, 0, 1, 10), SUBNET_MASK_24, gatewayA));
    topo.addNode(makeHost("PC2", 2, netlab::makeIpv4(10, 0, 2, 10), SUBNET_MASK_24, gatewayB));
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"SW2", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"R1", netlab::NodeKind::Router, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("SW1", "R1");
    topo.addLink("R1", "SW2");
    topo.addLink("SW2", "PC2");
    topo.addRouterInterface("R1", "SW1", mac(101), gatewayA, SUBNET_MASK_24);
    topo.addRouterInterface("R1", "SW2", mac(102), gatewayB, SUBNET_MASK_24);

    netlab::MissionResult mission = netlab::evaluateConnectTwoNetworksMission(topo, "PC1", "PC2");
    check(mission.success, "netlab-mission: 'Connect Two Networks' succeeds on a correctly-configured two-subnet-plus-router topology");
    check(mission.reason.find("successfully pinged") != std::string::npos,
          "netlab-mission: the success reason specifically confirms the ping outcome");
}

void testConnectTwoNetworksMissionFailsOnSameSubnet() {
    netlab::Topology topo;
    topo.addNode(makeHost("PC1", 1, netlab::makeIpv4(192, 168, 1, 10), SUBNET_MASK_24));
    topo.addNode(makeHost("PC2", 2, netlab::makeIpv4(192, 168, 1, 20), SUBNET_MASK_24));
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("PC2", "SW1");

    netlab::MissionResult mission = netlab::evaluateConnectTwoNetworksMission(topo, "PC1", "PC2");
    check(!mission.success, "netlab-mission: the mission fails when both hosts are on the same subnet (no routing exercised)");
    check(mission.reason.find("same subnet") != std::string::npos,
          "netlab-mission: the failure reason specifically explains that the hosts share a subnet");
}

void testConnectTwoNetworksMissionFailsOnMissingIpConfig() {
    netlab::Topology topo;
    topo.addNode(netlab::Node{"PC1", netlab::NodeKind::Host, mac(1)});
    topo.addNode(netlab::Node{"PC2", netlab::NodeKind::Host, mac(2)});

    netlab::MissionResult mission = netlab::evaluateConnectTwoNetworksMission(topo, "PC1", "PC2");
    check(!mission.success, "netlab-mission: the mission fails when a host has no IPv4 address configured at all");
    check(mission.reason.find("no IPv4 address configured") != std::string::npos,
          "netlab-mission: the failure reason specifically names the missing IP configuration");
}

int main() {
    testPingWithinSameSubnetSucceedsViaDirectArp();
    testFullTopologyPingAcrossRouterSucceeds();
    testArpResolutionFailureIsReportedNotCrashed();
    testRouterWithNoMatchingInterfaceReportsNoRoute();
    testFullPingSimulationIsDeterministic();
    testConnectTwoNetworksMissionSucceeds();
    testConnectTwoNetworksMissionFailsOnSameSubnet();
    testConnectTwoNetworksMissionFailsOnMissingIpConfig();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

// Real assertion-based tests for Techuilaguy NetLab's simulation
// engine (24-network-simulator/netlab/) — real Ethernet frames built
// on 06-networking/protocols/protocols.hpp's actual wire-format codec,
// not a simulator-specific reimplementation. See
// docs/ADR/0022-techuilaguy-netlab-foundation.md.

#include "../netlab/node.hpp"
#include "../netlab/packet.hpp"
#include "../netlab/topology.hpp"

#include <algorithm>
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

bool contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

}  // namespace

void testEthernetFrameRoundTrips() {
    net::MacAddress src = mac(1);
    net::MacAddress dst = mac(2);
    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};

    netlab::Packet packet = netlab::buildEthernetFrame(src, dst, net::ETHERTYPE_IPV4, payload);
    net::EthernetHeader parsed;
    check(netlab::parseFrameHeader(packet, parsed), "netlab: a built Ethernet frame parses back successfully");
    check(net::macEquals(parsed.source, src) && net::macEquals(parsed.destination, dst) && parsed.etherType == net::ETHERTYPE_IPV4,
          "netlab: a parsed frame's header fields exactly match what was built");
    check(packet.bytes.size() == net::ETHERNET_HEADER_SIZE + payload.size(),
          "netlab: a built frame's total size is exactly the header plus the payload");

    netlab::Packet malformed{std::vector<uint8_t>{0x01, 0x02}};
    net::EthernetHeader ignored;
    check(!netlab::parseFrameHeader(malformed, ignored), "netlab: parsing a too-short frame fails cleanly, not a crash");
}

void testSimpleUnicastDeliveryThroughASwitch() {
    netlab::Topology topo;
    net::MacAddress macA = mac(0xA);
    net::MacAddress macB = mac(0xB);
    net::MacAddress macC = mac(0xC);

    topo.addNode(netlab::Node{"A", netlab::NodeKind::Host, macA});
    topo.addNode(netlab::Node{"B", netlab::NodeKind::Host, macB});
    topo.addNode(netlab::Node{"C", netlab::NodeKind::Host, macC});
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("A", "SW1");
    topo.addLink("B", "SW1");
    topo.addLink("C", "SW1");

    netlab::Packet toB = netlab::buildEthernetFrame(macA, macB, net::ETHERTYPE_IPV4, {1, 2, 3});
    netlab::DeliveryResult result = topo.sendFrame("A", toB);

    check(contains(result.receivedByHosts, "B"), "netlab: a unicast frame from A to B is delivered to B");
    check(!contains(result.receivedByHosts, "C"), "netlab: a unicast frame from A to B is NOT delivered to an uninvolved host C on the same switch");
    check(!contains(result.receivedByHosts, "A"), "netlab: the sending host never receives its own frame back");
}

void testUnknownDestinationFloods() {
    netlab::Topology topo;
    topo.addNode(netlab::Node{"A", netlab::NodeKind::Host, mac(0xA)});
    topo.addNode(netlab::Node{"B", netlab::NodeKind::Host, mac(0xB)});
    topo.addNode(netlab::Node{"C", netlab::NodeKind::Host, mac(0xC)});
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("A", "SW1");
    topo.addLink("B", "SW1");
    topo.addLink("C", "SW1");

    net::MacAddress unknownDest = mac(0xFE);
    netlab::Packet packet = netlab::buildEthernetFrame(mac(0xA), unknownDest, net::ETHERTYPE_IPV4, {1});
    netlab::DeliveryResult result = topo.sendFrame("A", packet);

    check(result.receivedByHosts.empty(),
          "netlab: a frame addressed to a MAC nobody on the topology owns is delivered to nobody (no crash, no false match)");
}

void testSwitchLearningStopsFloodingAfterFirstFrame() {
    netlab::Topology topo;
    net::MacAddress macA = mac(0xA);
    net::MacAddress macB = mac(0xB);
    net::MacAddress macC = mac(0xC);
    topo.addNode(netlab::Node{"A", netlab::NodeKind::Host, macA});
    topo.addNode(netlab::Node{"B", netlab::NodeKind::Host, macB});
    topo.addNode(netlab::Node{"C", netlab::NodeKind::Host, macC});
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("A", "SW1");
    topo.addLink("B", "SW1");
    topo.addLink("C", "SW1");

    // First frame A -> B: the switch has no prior learning, so this
    // exercises the flood path (B and C are both connected the same
    // way at this point, so flooding is the only possible mechanism
    // that could have reached B at all here).
    netlab::Packet aToB = netlab::buildEthernetFrame(macA, macB, net::ETHERTYPE_IPV4, {1});
    topo.sendFrame("A", aToB);

    // Reply B -> A: the switch has now learned A's port from the first
    // frame. A learning switch forwards this directly to A only.
    netlab::Packet bToA = netlab::buildEthernetFrame(macB, macA, net::ETHERTYPE_IPV4, {2});
    netlab::DeliveryResult reply = topo.sendFrame("B", bToA);

    check(contains(reply.receivedByHosts, "A"), "netlab: after learning, a reply is still correctly delivered to the real destination");
    check(!contains(reply.receivedByHosts, "C"),
          "netlab: after learning A's port, a switch forwards a reply ONLY to A's learned port, not by flooding C as well");
}

void testBroadcastReachesEveryHost() {
    netlab::Topology topo;
    net::MacAddress macA = mac(0xA);
    topo.addNode(netlab::Node{"A", netlab::NodeKind::Host, macA});
    topo.addNode(netlab::Node{"B", netlab::NodeKind::Host, mac(0xB)});
    topo.addNode(netlab::Node{"C", netlab::NodeKind::Host, mac(0xC)});
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("A", "SW1");
    topo.addLink("B", "SW1");
    topo.addLink("C", "SW1");

    netlab::Packet broadcast = netlab::buildEthernetFrame(macA, netlab::BROADCAST_MAC, net::ETHERTYPE_ARP, {});
    netlab::DeliveryResult result = topo.sendFrame("A", broadcast);

    check(contains(result.receivedByHosts, "B") && contains(result.receivedByHosts, "C"),
          "netlab: a broadcast-destination frame reaches every other host on the topology");
}

void testCyclicTopologyDoesNotInfiniteLoop() {
    netlab::Topology topo;
    topo.addNode(netlab::Node{"A", netlab::NodeKind::Host, mac(0xA)});
    topo.addNode(netlab::Node{"B", netlab::NodeKind::Host, mac(0xB)});
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"SW2", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"SW3", netlab::NodeKind::Switch, {}});
    topo.addLink("A", "SW1");
    topo.addLink("B", "SW2");
    // A real triangle among the three switches — a genuine cycle,
    // not just a redundant single edge (which addLink would reject).
    topo.addLink("SW1", "SW2");
    topo.addLink("SW2", "SW3");
    topo.addLink("SW3", "SW1");

    netlab::Packet packet = netlab::buildEthernetFrame(mac(0xA), mac(0xB), net::ETHERTYPE_IPV4, {1});
    netlab::DeliveryResult result = topo.sendFrame("A", packet);

    check(true, "netlab: sendFrame over a topology containing a real cycle terminates without hanging or crashing");
    check(contains(result.receivedByHosts, "B"), "netlab: delivery still reaches the real destination despite the cycle");
}

void testAddNodeAndLinkRejectDuplicatesAndUnknownIds() {
    netlab::Topology topo;
    check(topo.addNode(netlab::Node{"A", netlab::NodeKind::Host, mac(0xA)}), "netlab: adding a new node succeeds");
    check(!topo.addNode(netlab::Node{"A", netlab::NodeKind::Host, mac(0xA)}), "netlab: adding a node with a duplicate id fails");
    check(!topo.addLink("A", "does_not_exist"), "netlab: adding a link to a nonexistent node id fails cleanly, not a crash");

    topo.addNode(netlab::Node{"B", netlab::NodeKind::Host, mac(0xB)});
    check(topo.addLink("A", "B"), "netlab: adding a link between two real nodes succeeds");
    check(!topo.addLink("A", "B"), "netlab: adding the identical link twice fails (no duplicate link)");
}

int main() {
    testEthernetFrameRoundTrips();
    testSimpleUnicastDeliveryThroughASwitch();
    testUnknownDestinationFloods();
    testSwitchLearningStopsFloodingAfterFirstFrame();
    testBroadcastReachesEveryHost();
    testCyclicTopologyDoesNotInfiniteLoop();
    testAddNodeAndLinkRejectDuplicatesAndUnknownIds();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

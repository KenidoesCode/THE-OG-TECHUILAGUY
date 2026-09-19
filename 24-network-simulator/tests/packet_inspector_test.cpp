// Real assertion-based tests for NetLab's packet inspector
// (netlab/packet_inspector.cpp) — decodes real frames built the same
// way ADR 0028's simulation actually builds them, not hand-crafted
// test-only shapes. See
// docs/ADR/0029-netlab-packet-inspector-and-timeline-session.md.

#include "../netlab/mission.hpp"
#include "../netlab/packet_inspector.hpp"

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

const netlab::SimulationEvent* findEventContaining(const netlab::PingResult& result, const std::string& substring) {
    for (const auto& event : result.timeline) {
        if (event.description.find(substring) != std::string::npos && !event.frameBytes.empty()) {
            return &event;
        }
    }
    return nullptr;
}

}  // namespace

void testDecodeRealArpRequestAndReply() {
    netlab::Topology topo;
    netlab::Node pc1{"PC1", netlab::NodeKind::Host, mac(1)};
    pc1.ipAddress = netlab::makeIpv4(192, 168, 1, 10);
    pc1.subnetMask = netlab::makeIpv4(255, 255, 255, 0);
    netlab::Node pc2{"PC2", netlab::NodeKind::Host, mac(2)};
    pc2.ipAddress = netlab::makeIpv4(192, 168, 1, 20);
    pc2.subnetMask = netlab::makeIpv4(255, 255, 255, 0);
    topo.addNode(pc1);
    topo.addNode(pc2);
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("PC2", "SW1");

    netlab::PingResult result = netlab::sendIcmpEcho(topo, "PC1", pc2.ipAddress);
    check(result.delivered, "packet-inspector test setup: the underlying ping actually succeeds");

    const netlab::SimulationEvent* requestEvent = findEventContaining(result, "ARP request broadcast");
    check(requestEvent != nullptr, "packet-inspector test setup: a real ARP request event with frame bytes was recorded");
    if (requestEvent != nullptr) {
        netlab::DecodedFrame decoded = netlab::decodeFrame(requestEvent->frameBytes);
        check(decoded.parsedSuccessfully, "packet-inspector: a real captured ARP request frame decodes successfully");
        check(decoded.summary.find("ARP Request") != std::string::npos, "packet-inspector: the ARP request summary correctly identifies it as a Request");
        check(decoded.summary.find("192.168.1.20") != std::string::npos,
              "packet-inspector: the ARP request summary correctly names the real target IP being resolved");
    }

    const netlab::SimulationEvent* replyEvent = findEventContaining(result, "ARP reply");
    check(replyEvent != nullptr, "packet-inspector test setup: a real ARP reply event with frame bytes was recorded");
    if (replyEvent != nullptr) {
        netlab::DecodedFrame decoded = netlab::decodeFrame(replyEvent->frameBytes);
        check(decoded.parsedSuccessfully, "packet-inspector: a real captured ARP reply frame decodes successfully");
        check(decoded.summary.find("ARP Reply") != std::string::npos, "packet-inspector: the ARP reply summary correctly identifies it as a Reply");
    }
}

void testDecodeRealIcmpEchoFrameMatchesTheRealSimulation() {
    netlab::Topology topo;
    netlab::Node pc1{"PC1", netlab::NodeKind::Host, mac(1)};
    pc1.ipAddress = netlab::makeIpv4(10, 0, 1, 10);
    pc1.subnetMask = netlab::makeIpv4(255, 255, 255, 0);
    netlab::Node pc2{"PC2", netlab::NodeKind::Host, mac(2)};
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 1, 20);
    pc2.ipAddress = pc2Ip;
    pc2.subnetMask = netlab::makeIpv4(255, 255, 255, 0);
    topo.addNode(pc1);
    topo.addNode(pc2);
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("PC2", "SW1");

    netlab::PingResult result = netlab::sendIcmpEcho(topo, "PC1", pc2Ip);
    check(result.delivered, "packet-inspector test setup: the underlying ping actually succeeds");

    const netlab::SimulationEvent* icmpEvent = findEventContaining(result, "IPv4 ICMP echo request sent");
    check(icmpEvent != nullptr, "packet-inspector test setup: a real ICMP echo event with frame bytes was recorded");
    if (icmpEvent != nullptr) {
        netlab::DecodedFrame decoded = netlab::decodeFrame(icmpEvent->frameBytes);
        check(decoded.parsedSuccessfully, "packet-inspector: a real captured IPv4 ICMP echo request frame decodes successfully");
        check(decoded.summary.find("Echo Request") != std::string::npos, "packet-inspector: the summary correctly identifies an ICMP Echo Request");
        check(decoded.summary.find("10.0.1.20") != std::string::npos,
              "packet-inspector: decoding the REAL captured frame reproduces the exact destination IP the live simulation actually used (rule 3: no fabricated visual state)");

        bool hasTtlField = false;
        for (const auto& field : decoded.protocolFields) {
            if (field.name == "TTL" && field.value == "64") hasTtlField = true;
        }
        check(hasTtlField, "packet-inspector: the decoded protocol fields include the real TTL value the frame was built with");
    }
}

void testDecodeMalformedFrameFailsCleanly() {
    std::vector<uint8_t> tooShort = {0x01, 0x02, 0x03};
    netlab::DecodedFrame decoded = netlab::decodeFrame(tooShort);
    check(!decoded.parsedSuccessfully, "packet-inspector: a too-short buffer fails to decode cleanly, not a crash");
    check(decoded.summary.empty(), "packet-inspector: a failed decode produces no fabricated summary text");

    std::vector<uint8_t> unsupportedEtherType(20, 0);
    unsupportedEtherType[12] = 0x88;
    unsupportedEtherType[13] = 0xB5;  // an EtherType this slice doesn't decode
    netlab::DecodedFrame decoded2 = netlab::decodeFrame(unsupportedEtherType);
    check(!decoded2.parsedSuccessfully, "packet-inspector: an unsupported (but structurally valid) EtherType is reported as undecoded, not guessed at");
}

int main() {
    testDecodeRealArpRequestAndReply();
    testDecodeRealIcmpEchoFrameMatchesTheRealSimulation();
    testDecodeMalformedFrameFailsCleanly();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

// Real assertion-based unit tests for the Layer 6 (Networking)
// protocol codec layer (06-networking/protocols/protocols.cpp).
// Hardware-independent by design (no NIC, no OS dependency), so this
// is a normal hosted-compiler test, the same reasoning
// tests/elf_test.cpp and tests/heap_test.cpp already apply in 22-os.

#include "../protocols/protocols.hpp"

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

net::MacAddress mac(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
    return {{a, b, c, d, e, f}};
}

}  // namespace

void testEthernetRoundTrip() {
    net::EthernetHeader header{
        mac(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF),
        mac(0x02, 0x00, 0x00, 0x00, 0x00, 0x01),
        net::ETHERTYPE_ARP,
    };

    uint8_t buffer[net::ETHERNET_HEADER_SIZE];
    uint32_t written = net::serializeEthernetHeader(header, buffer, sizeof(buffer));
    check(written == net::ETHERNET_HEADER_SIZE,
          "ethernet: serializes to exactly 14 bytes");

    net::EthernetHeader parsed;
    net::ParseError err = net::parseEthernetHeader(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::None, "ethernet: round-tripped header parses successfully");
    check(net::macEquals(parsed.destination, header.destination),
          "ethernet: destination MAC round-trips correctly");
    check(net::macEquals(parsed.source, header.source),
          "ethernet: source MAC round-trips correctly");
    check(parsed.etherType == net::ETHERTYPE_ARP,
          "ethernet: etherType round-trips correctly");
}

void testEthernetRejectsTruncatedBuffer() {
    uint8_t buffer[10] = {0};
    net::EthernetHeader parsed;
    net::ParseError err = net::parseEthernetHeader(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::BufferTooSmall,
          "ethernet: rejects a buffer shorter than a full 14-byte header");
}

void testEthernetSerializeRejectsUndersizedOutput() {
    net::EthernetHeader header{
        mac(1, 2, 3, 4, 5, 6), mac(7, 8, 9, 10, 11, 12), net::ETHERTYPE_IPV4
    };
    uint8_t tooSmall[8];
    uint32_t written = net::serializeEthernetHeader(header, tooSmall, sizeof(tooSmall));
    check(written == 0,
          "ethernet: serialize into an undersized buffer writes nothing and returns 0");
}

void testArpRoundTrip() {
    net::ArpPacket packet{
        net::ArpOperation::Request,
        mac(0x02, 0, 0, 0, 0, 1),
        0xC0A80001,  // 192.168.0.1
        mac(0, 0, 0, 0, 0, 0),
        0xC0A80002,  // 192.168.0.2
    };

    uint8_t buffer[net::ARP_PACKET_SIZE];
    uint32_t written = net::serializeArpPacket(packet, buffer, sizeof(buffer));
    check(written == net::ARP_PACKET_SIZE, "arp: serializes to exactly 28 bytes");

    net::ArpPacket parsed;
    net::ParseError err = net::parseArpPacket(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::None, "arp: round-tripped packet parses successfully");
    check(parsed.operation == net::ArpOperation::Request,
          "arp: operation (request) round-trips correctly");
    check(parsed.senderIp == packet.senderIp && parsed.targetIp == packet.targetIp,
          "arp: sender/target IPs round-trip correctly");
    check(net::macEquals(parsed.senderMac, packet.senderMac),
          "arp: sender MAC round-trips correctly");
}

void testArpRejectsWrongHardwareType() {
    uint8_t buffer[net::ARP_PACKET_SIZE] = {0};
    buffer[0] = 0x00; buffer[1] = 0x06;  // htype = 6 (IEEE 802, not Ethernet=1)
    buffer[2] = 0x08; buffer[3] = 0x00;  // ptype = IPv4
    buffer[4] = 6; buffer[5] = 4;
    buffer[7] = 1;  // operation = request

    net::ArpPacket parsed;
    net::ParseError err = net::parseArpPacket(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::UnsupportedHardwareType,
          "arp: rejects a hardware type other than Ethernet");
}

void testArpRejectsWrongAddressLengths() {
    net::ArpPacket packet{
        net::ArpOperation::Reply, mac(1,2,3,4,5,6), 1, mac(6,5,4,3,2,1), 2
    };
    uint8_t buffer[net::ARP_PACKET_SIZE];
    net::serializeArpPacket(packet, buffer, sizeof(buffer));
    buffer[4] = 8;  // corrupt hardware address length (should be 6)

    net::ArpPacket parsed;
    net::ParseError err = net::parseArpPacket(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::InvalidHardwareAddressLength,
          "arp: rejects a hardware address length other than 6 (Ethernet MAC size)");
}

void testArpRejectsUnknownOperation() {
    net::ArpPacket packet{
        net::ArpOperation::Request, mac(1,2,3,4,5,6), 1, mac(6,5,4,3,2,1), 2
    };
    uint8_t buffer[net::ARP_PACKET_SIZE];
    net::serializeArpPacket(packet, buffer, sizeof(buffer));
    buffer[6] = 0; buffer[7] = 99;  // operation = 99 (neither request nor reply)

    net::ArpPacket parsed;
    net::ParseError err = net::parseArpPacket(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::UnsupportedOperation,
          "arp: rejects an operation code that is neither request (1) nor reply (2)");
}

void testArpRejectsTruncatedBuffer() {
    uint8_t buffer[20] = {0};
    net::ArpPacket parsed;
    net::ParseError err = net::parseArpPacket(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::BufferTooSmall,
          "arp: rejects a buffer shorter than a full 28-byte packet");
}

void testIpv4RoundTrip() {
    // totalLength matches the 20-byte buffer exactly (no payload
    // appended) since this test only exercises the header round trip.
    net::Ipv4Header header{
        64, net::IPV4_PROTO_UDP, 0x1234, 0,
        static_cast<uint16_t>(net::IPV4_MIN_HEADER_SIZE),
        0x0A000001, 0x0A000002,
    };

    uint8_t buffer[net::IPV4_MIN_HEADER_SIZE];
    uint32_t written = net::serializeIpv4Header(header, buffer, sizeof(buffer));
    check(written == net::IPV4_MIN_HEADER_SIZE, "ipv4: serializes to exactly 20 bytes");

    net::Ipv4Header parsed;
    net::ParseError err = net::parseIpv4Header(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::None,
          "ipv4: a correctly-checksummed round-tripped header parses successfully");
    check(parsed.ttl == 64 && parsed.protocol == net::IPV4_PROTO_UDP,
          "ipv4: ttl and protocol round-trip correctly");
    check(parsed.sourceIp == header.sourceIp && parsed.destIp == header.destIp,
          "ipv4: source/dest IPs round-trip correctly");
    check(parsed.totalLength == header.totalLength,
          "ipv4: totalLength round-trips correctly");
}

void testIpv4RejectsWrongVersion() {
    net::Ipv4Header header{
        64, net::IPV4_PROTO_ICMP, 0, 0,
        net::IPV4_MIN_HEADER_SIZE, 0x0A000001, 0x0A000002
    };
    uint8_t buffer[net::IPV4_MIN_HEADER_SIZE];
    net::serializeIpv4Header(header, buffer, sizeof(buffer));
    buffer[0] = (6 << 4) | 5;  // version = 6 instead of 4

    net::Ipv4Header parsed;
    net::ParseError err = net::parseIpv4Header(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::UnsupportedVersion,
          "ipv4: rejects a version field other than 4");
}

void testIpv4RejectsNonZeroOptionsIhl() {
    net::Ipv4Header header{
        64, net::IPV4_PROTO_ICMP, 0, 0,
        net::IPV4_MIN_HEADER_SIZE, 0x0A000001, 0x0A000002
    };
    uint8_t buffer[net::IPV4_MIN_HEADER_SIZE];
    net::serializeIpv4Header(header, buffer, sizeof(buffer));
    buffer[0] = (net::IPV4_VERSION << 4) | 6;  // IHL = 6 (implies options)

    net::Ipv4Header parsed;
    net::ParseError err = net::parseIpv4Header(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::HeaderLengthInvalid,
          "ipv4: rejects an IHL other than 5 (v1 supports no options)");
}

void testIpv4RejectsTotalLengthExceedingBuffer() {
    net::Ipv4Header header{
        64, net::IPV4_PROTO_ICMP, 0, 0,
        net::IPV4_MIN_HEADER_SIZE, 0x0A000001, 0x0A000002
    };
    uint8_t buffer[net::IPV4_MIN_HEADER_SIZE];
    net::serializeIpv4Header(header, buffer, sizeof(buffer));
    // Corrupt totalLength to claim far more than the actual buffer —
    // the checksum will also now mismatch, but the length check must
    // fire (whichever fires first is fine; the packet must be
    // rejected either way, never trusted for a larger read).
    buffer[2] = 0xFF; buffer[3] = 0xFF;

    net::Ipv4Header parsed;
    net::ParseError err = net::parseIpv4Header(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::TotalLengthInvalid ||
          err == net::ParseError::ChecksumMismatch,
          "ipv4: rejects a totalLength claiming more bytes than the real buffer holds");
}

void testIpv4RejectsCorruptedChecksum() {
    net::Ipv4Header header{
        64, net::IPV4_PROTO_ICMP, 0, 0,
        net::IPV4_MIN_HEADER_SIZE, 0x0A000001, 0x0A000002
    };
    uint8_t buffer[net::IPV4_MIN_HEADER_SIZE];
    net::serializeIpv4Header(header, buffer, sizeof(buffer));
    buffer[9] = static_cast<uint8_t>(buffer[9] + 1);  // corrupt protocol byte
                                                        // without recomputing checksum

    net::Ipv4Header parsed;
    net::ParseError err = net::parseIpv4Header(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::ChecksumMismatch,
          "ipv4: rejects a header whose checksum no longer matches its (corrupted) content");
}

void testIpv4RejectsTruncatedBuffer() {
    uint8_t buffer[10] = {0};
    net::Ipv4Header parsed;
    net::ParseError err = net::parseIpv4Header(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::BufferTooSmall,
          "ipv4: rejects a buffer shorter than the 20-byte minimum header");
}

void testIcmpEchoRoundTripAndChecksum() {
    net::IcmpEchoMessage message{
        net::ICMP_TYPE_ECHO_REQUEST, 0, 0xABCD, 1
    };
    const uint8_t payload[] = {'p', 'i', 'n', 'g'};

    uint8_t buffer[net::ICMP_ECHO_HEADER_SIZE + sizeof(payload)];
    uint32_t written = net::serializeIcmpEcho(
        message, payload, sizeof(payload), buffer, sizeof(buffer)
    );
    check(written == sizeof(buffer), "icmp: serializes header+payload to the expected length");

    check(net::verifyIcmpChecksum(buffer, written),
          "icmp: a freshly serialized echo message's checksum verifies");

    net::IcmpEchoMessage parsed;
    net::ParseError err = net::parseIcmpEcho(buffer, written, parsed);
    check(err == net::ParseError::None, "icmp: round-tripped message parses successfully");
    check(parsed.type == net::ICMP_TYPE_ECHO_REQUEST && parsed.identifier == 0xABCD,
          "icmp: type and identifier round-trip correctly");
}

void testIcmpChecksumDetectsCorruption() {
    net::IcmpEchoMessage message{net::ICMP_TYPE_ECHO_REQUEST, 0, 1, 1};
    const uint8_t payload[] = {'x'};
    uint8_t buffer[net::ICMP_ECHO_HEADER_SIZE + 1];
    net::serializeIcmpEcho(message, payload, 1, buffer, sizeof(buffer));

    buffer[sizeof(buffer) - 1] = 'y';  // corrupt the payload after checksumming

    check(!net::verifyIcmpChecksum(buffer, sizeof(buffer)),
          "icmp: a corrupted payload is detected by the checksum verification");
}

void testIcmpRejectsTruncatedBuffer() {
    uint8_t buffer[4] = {0};
    net::IcmpEchoMessage parsed;
    net::ParseError err = net::parseIcmpEcho(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::BufferTooSmall,
          "icmp: rejects a buffer shorter than the 8-byte echo header");
}

void testUdpRoundTripAndChecksum() {
    net::UdpHeader header{53, 12345, 0};
    const uint8_t payload[] = {'d', 'n', 's'};
    uint32_t sourceIp = 0x0A000001;
    uint32_t destIp = 0x0A000002;

    uint8_t buffer[net::UDP_HEADER_SIZE + sizeof(payload)];
    uint32_t written = net::serializeUdpDatagramIpv4(
        header, payload, sizeof(payload), sourceIp, destIp, buffer, sizeof(buffer)
    );
    check(written == sizeof(buffer), "udp: serializes header+payload to the expected length");

    check(net::verifyUdpChecksumIpv4(sourceIp, destIp, buffer, written),
          "udp: a freshly serialized datagram's checksum verifies against the correct pseudo-header");

    check(!net::verifyUdpChecksumIpv4(sourceIp, destIp + 1, buffer, written),
          "udp: the same datagram fails checksum verification against a *different* "
          "destination IP (the pseudo-header genuinely participates in the checksum)");

    net::UdpHeader parsed;
    net::ParseError err = net::parseUdpHeader(buffer, written, parsed);
    check(err == net::ParseError::None, "udp: round-tripped header parses successfully");
    check(parsed.sourcePort == 53 && parsed.destPort == 12345,
          "udp: source/dest ports round-trip correctly");
}

void testUdpZeroChecksumIsValidPerRfc768() {
    uint8_t buffer[net::UDP_HEADER_SIZE] = {0};
    buffer[4] = 0; buffer[5] = net::UDP_HEADER_SIZE;  // length field
    // checksum field (bytes 6-7) left as zero.

    check(net::verifyUdpChecksumIpv4(1, 2, buffer, sizeof(buffer)),
          "udp: a checksum of exactly zero is treated as \"none computed\" and accepted, per RFC 768");
}

void testUdpRejectsTruncatedBuffer() {
    uint8_t buffer[4] = {0};
    net::UdpHeader parsed;
    net::ParseError err = net::parseUdpHeader(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::BufferTooSmall,
          "udp: rejects a buffer shorter than the 8-byte header");
}

void testUdpRejectsLengthExceedingBuffer() {
    net::UdpHeader header{1, 2, 0};
    uint8_t buffer[net::UDP_HEADER_SIZE];
    net::serializeUdpDatagramIpv4(header, nullptr, 0, 1, 2, buffer, sizeof(buffer));
    buffer[4] = 0xFF; buffer[5] = 0xFF;  // corrupt length to claim far more than the buffer

    net::UdpHeader parsed;
    net::ParseError err = net::parseUdpHeader(buffer, sizeof(buffer), parsed);
    check(err == net::ParseError::TotalLengthInvalid,
          "udp: rejects a length field claiming more bytes than the real buffer holds");
}

int main() {
    testEthernetRoundTrip();
    testEthernetRejectsTruncatedBuffer();
    testEthernetSerializeRejectsUndersizedOutput();
    testArpRoundTrip();
    testArpRejectsWrongHardwareType();
    testArpRejectsWrongAddressLengths();
    testArpRejectsUnknownOperation();
    testArpRejectsTruncatedBuffer();
    testIpv4RoundTrip();
    testIpv4RejectsWrongVersion();
    testIpv4RejectsNonZeroOptionsIhl();
    testIpv4RejectsTotalLengthExceedingBuffer();
    testIpv4RejectsCorruptedChecksum();
    testIpv4RejectsTruncatedBuffer();
    testIcmpEchoRoundTripAndChecksum();
    testIcmpChecksumDetectsCorruption();
    testIcmpRejectsTruncatedBuffer();
    testUdpRoundTripAndChecksum();
    testUdpZeroChecksumIsValidPerRfc768();
    testUdpRejectsTruncatedBuffer();
    testUdpRejectsLengthExceedingBuffer();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

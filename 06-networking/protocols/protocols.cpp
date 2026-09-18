#include "protocols.hpp"

namespace net {

namespace {

uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(p[0]) << 8) | static_cast<uint32_t>(p[1])
    );
}

void writeU16(uint8_t* p, uint16_t value) {
    p[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>(value & 0xFF);
}

uint32_t readU32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

void writeU32(uint8_t* p, uint32_t value) {
    p[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(value & 0xFF);
}

void readMac(const uint8_t* p, MacAddress& out) {
    for (int i = 0; i < 6; ++i) out.bytes[i] = p[i];
}

void writeMac(uint8_t* p, const MacAddress& mac) {
    for (int i = 0; i < 6; ++i) p[i] = mac.bytes[i];
}

}  // namespace

bool macEquals(const MacAddress& a, const MacAddress& b) {
    for (int i = 0; i < 6; ++i) {
        if (a.bytes[i] != b.bytes[i]) return false;
    }
    return true;
}

uint16_t internetChecksum(const uint8_t* data, uint32_t len) {
    uint32_t sum = 0;
    uint32_t i = 0;

    while (i + 1 < len) {
        sum += (static_cast<uint32_t>(data[i]) << 8) |
               static_cast<uint32_t>(data[i + 1]);
        i += 2;
    }

    if (i < len) {
        // Odd trailing byte: treated as the high byte of a final
        // 16-bit word with an implicit zero low byte, per RFC 1071.
        sum += static_cast<uint32_t>(data[i]) << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum & 0xFFFF);
}

ParseError parseEthernetHeader(
    const uint8_t* buffer, uint32_t length, EthernetHeader& out
) {
    if (length < ETHERNET_HEADER_SIZE) {
        return ParseError::BufferTooSmall;
    }

    readMac(buffer, out.destination);
    readMac(buffer + 6, out.source);
    out.etherType = readU16(buffer + 12);

    return ParseError::None;
}

uint32_t serializeEthernetHeader(
    const EthernetHeader& header, uint8_t* out, uint32_t outCapacity
) {
    if (outCapacity < ETHERNET_HEADER_SIZE) {
        return 0;
    }

    writeMac(out, header.destination);
    writeMac(out + 6, header.source);
    writeU16(out + 12, header.etherType);

    return ETHERNET_HEADER_SIZE;
}

ParseError parseArpPacket(
    const uint8_t* buffer, uint32_t length, ArpPacket& out
) {
    if (length < ARP_PACKET_SIZE) {
        return ParseError::BufferTooSmall;
    }

    uint16_t hardwareType = readU16(buffer + 0);
    uint16_t protocolType = readU16(buffer + 2);
    uint8_t hardwareAddrLen = buffer[4];
    uint8_t protocolAddrLen = buffer[5];
    uint16_t operation = readU16(buffer + 6);

    if (hardwareType != ARP_HTYPE_ETHERNET) {
        return ParseError::UnsupportedHardwareType;
    }

    if (protocolType != ETHERTYPE_IPV4) {
        // v1 only supports ARP for IPv4 resolution, the overwhelming
        // common case and the only one this stack's IPv4 layer needs.
        return ParseError::UnsupportedHardwareType;
    }

    if (hardwareAddrLen != 6) {
        return ParseError::InvalidHardwareAddressLength;
    }

    if (protocolAddrLen != 4) {
        return ParseError::InvalidProtocolAddressLength;
    }

    if (operation != static_cast<uint16_t>(ArpOperation::Request) &&
        operation != static_cast<uint16_t>(ArpOperation::Reply)) {
        return ParseError::UnsupportedOperation;
    }

    out.operation = static_cast<ArpOperation>(operation);
    readMac(buffer + 8, out.senderMac);
    out.senderIp = readU32(buffer + 14);
    readMac(buffer + 18, out.targetMac);
    out.targetIp = readU32(buffer + 24);

    return ParseError::None;
}

uint32_t serializeArpPacket(
    const ArpPacket& packet, uint8_t* out, uint32_t outCapacity
) {
    if (outCapacity < ARP_PACKET_SIZE) {
        return 0;
    }

    writeU16(out + 0, ARP_HTYPE_ETHERNET);
    writeU16(out + 2, ETHERTYPE_IPV4);
    out[4] = 6;  // hardware address length
    out[5] = 4;  // protocol address length
    writeU16(out + 6, static_cast<uint16_t>(packet.operation));
    writeMac(out + 8, packet.senderMac);
    writeU32(out + 14, packet.senderIp);
    writeMac(out + 18, packet.targetMac);
    writeU32(out + 24, packet.targetIp);

    return ARP_PACKET_SIZE;
}

ParseError parseIpv4Header(
    const uint8_t* buffer, uint32_t length, Ipv4Header& out
) {
    if (length < IPV4_MIN_HEADER_SIZE) {
        return ParseError::BufferTooSmall;
    }

    uint8_t versionAndIhl = buffer[0];
    uint8_t version = static_cast<uint8_t>(versionAndIhl >> 4);
    uint8_t ihl = static_cast<uint8_t>(versionAndIhl & 0x0F);

    if (version != IPV4_VERSION) {
        return ParseError::UnsupportedVersion;
    }

    if (ihl != 5) {
        // IHL is in 32-bit words; 5 words = 20 bytes = no options.
        // Rejected rather than silently mis-parsed: skipping past
        // options this code never reads would require trusting ihl
        // for an offset calculation this parser doesn't validate
        // further.
        return ParseError::HeaderLengthInvalid;
    }

    uint16_t totalLength = readU16(buffer + 2);

    if (totalLength < IPV4_MIN_HEADER_SIZE ||
        static_cast<uint32_t>(totalLength) > length) {
        // The packet must never claim to be longer than the buffer
        // that actually holds it — trusting totalLength over the real
        // buffer length here would let a crafted header cause a
        // caller to read past the end of its own buffer.
        return ParseError::TotalLengthInvalid;
    }

    // Verified by summing the real header bytes with the transmitted
    // checksum field included (not zeroed) and confirming the result
    // is one of one's-complement arithmetic's two representations of
    // zero: plain 0x0000, or 0xFFFF (since a one's-complement sum is
    // only defined modulo 0xFFFF, not 0x10000 — the classic "two
    // zeros" of sign-and-magnitude-style arithmetic). Checking only
    // for 0x0000 rejects perfectly valid packets whose fold happens
    // to land on the other representation.
    uint16_t verifySum = internetChecksum(buffer, IPV4_MIN_HEADER_SIZE);
    if (verifySum != 0 && verifySum != 0xFFFF) {
        return ParseError::ChecksumMismatch;
    }

    out.ttl = buffer[8];
    out.protocol = buffer[9];
    out.identification = readU16(buffer + 4);
    out.flagsAndFragmentOffset = readU16(buffer + 6);
    out.totalLength = totalLength;
    out.sourceIp = readU32(buffer + 12);
    out.destIp = readU32(buffer + 16);

    return ParseError::None;
}

uint32_t serializeIpv4Header(
    const Ipv4Header& header, uint8_t* out, uint32_t outCapacity
) {
    if (outCapacity < IPV4_MIN_HEADER_SIZE) {
        return 0;
    }

    out[0] = static_cast<uint8_t>((IPV4_VERSION << 4) | 5);  // IHL=5
    out[1] = 0;  // DSCP/ECN, unused
    writeU16(out + 2, header.totalLength);
    writeU16(out + 4, header.identification);
    writeU16(out + 6, header.flagsAndFragmentOffset);
    out[8] = header.ttl;
    out[9] = header.protocol;
    writeU16(out + 10, 0);  // checksum placeholder, filled below
    writeU32(out + 12, header.sourceIp);
    writeU32(out + 16, header.destIp);

    uint16_t checksum = internetChecksum(out, IPV4_MIN_HEADER_SIZE);
    writeU16(out + 10, checksum);

    return IPV4_MIN_HEADER_SIZE;
}

ParseError parseIcmpEcho(
    const uint8_t* buffer, uint32_t length, IcmpEchoMessage& out
) {
    if (length < ICMP_ECHO_HEADER_SIZE) {
        return ParseError::BufferTooSmall;
    }

    out.type = buffer[0];
    out.code = buffer[1];
    out.identifier = readU16(buffer + 4);
    out.sequenceNumber = readU16(buffer + 6);

    return ParseError::None;
}

bool verifyIcmpChecksum(const uint8_t* buffer, uint32_t length) {
    if (length < ICMP_ECHO_HEADER_SIZE) {
        return false;
    }

    uint16_t sum = internetChecksum(buffer, length);
    return sum == 0 || sum == 0xFFFF;
}

uint32_t serializeIcmpEcho(
    const IcmpEchoMessage& message,
    const uint8_t* payload,
    uint32_t payloadLength,
    uint8_t* out,
    uint32_t outCapacity
) {
    uint32_t totalLength = ICMP_ECHO_HEADER_SIZE + payloadLength;

    if (outCapacity < totalLength) {
        return 0;
    }

    out[0] = message.type;
    out[1] = message.code;
    writeU16(out + 2, 0);  // checksum placeholder
    writeU16(out + 4, message.identifier);
    writeU16(out + 6, message.sequenceNumber);

    for (uint32_t i = 0; i < payloadLength; ++i) {
        out[ICMP_ECHO_HEADER_SIZE + i] = payload[i];
    }

    uint16_t checksum = internetChecksum(out, totalLength);
    writeU16(out + 2, checksum);

    return totalLength;
}

ParseError parseUdpHeader(
    const uint8_t* buffer, uint32_t length, UdpHeader& out
) {
    if (length < UDP_HEADER_SIZE) {
        return ParseError::BufferTooSmall;
    }

    uint16_t udpLength = readU16(buffer + 4);

    if (udpLength < UDP_HEADER_SIZE || udpLength > length) {
        return ParseError::TotalLengthInvalid;
    }

    out.sourcePort = readU16(buffer + 0);
    out.destPort = readU16(buffer + 2);
    out.length = udpLength;

    return ParseError::None;
}

namespace {

// Computes the checksum over an IPv4 pseudo-header followed by the
// real UDP datagram bytes, without ever allocating a combined buffer:
// the running sum is accumulated across both pieces using the same
// carry-folding rule internetChecksum uses internally, then finished
// off (an odd total length across the two pieces combined is handled
// by internetChecksum's own byte-at-a-time fallback applied to just
// the datagram, since the 12-byte pseudo-header is always even).
uint16_t udpChecksumWithPseudoHeader(
    uint32_t sourceIp,
    uint32_t destIp,
    const uint8_t* udpDatagram,
    uint32_t udpDatagramLength
) {
    uint8_t pseudoHeader[12];
    writeU32(pseudoHeader + 0, sourceIp);
    writeU32(pseudoHeader + 4, destIp);
    pseudoHeader[8] = 0;
    pseudoHeader[9] = IPV4_PROTO_UDP;
    writeU16(pseudoHeader + 10, static_cast<uint16_t>(udpDatagramLength));

    uint32_t sum = 0;
    for (uint32_t i = 0; i < 12; i += 2) {
        sum += (static_cast<uint32_t>(pseudoHeader[i]) << 8) |
               static_cast<uint32_t>(pseudoHeader[i + 1]);
    }

    uint32_t i = 0;
    while (i + 1 < udpDatagramLength) {
        sum += (static_cast<uint32_t>(udpDatagram[i]) << 8) |
               static_cast<uint32_t>(udpDatagram[i + 1]);
        i += 2;
    }
    if (i < udpDatagramLength) {
        sum += static_cast<uint32_t>(udpDatagram[i]) << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum & 0xFFFF);
}

}  // namespace

bool verifyUdpChecksumIpv4(
    uint32_t sourceIp,
    uint32_t destIp,
    const uint8_t* udpDatagram,
    uint32_t udpDatagramLength
) {
    if (udpDatagramLength < UDP_HEADER_SIZE) {
        return false;
    }

    uint16_t claimedChecksum = readU16(udpDatagram + 6);

    if (claimedChecksum == 0) {
        // RFC 768: a sender that computed no checksum transmits zero,
        // and a receiver must treat that as valid, not as a mismatch.
        return true;
    }

    uint16_t computed = udpChecksumWithPseudoHeader(
        sourceIp, destIp, udpDatagram, udpDatagramLength
    );

    return computed == 0 || computed == 0xFFFF;
}

uint32_t serializeUdpDatagramIpv4(
    const UdpHeader& header,
    const uint8_t* payload,
    uint32_t payloadLength,
    uint32_t sourceIp,
    uint32_t destIp,
    uint8_t* out,
    uint32_t outCapacity
) {
    uint32_t totalLength = UDP_HEADER_SIZE + payloadLength;

    if (outCapacity < totalLength) {
        return 0;
    }

    writeU16(out + 0, header.sourcePort);
    writeU16(out + 2, header.destPort);
    writeU16(out + 4, static_cast<uint16_t>(totalLength));
    writeU16(out + 6, 0);  // checksum placeholder

    for (uint32_t i = 0; i < payloadLength; ++i) {
        out[UDP_HEADER_SIZE + i] = payload[i];
    }

    uint16_t checksum =
        udpChecksumWithPseudoHeader(sourceIp, destIp, out, totalLength);

    // A computed checksum of exactly 0 is transmitted as all-ones
    // (0xFFFF) instead, per RFC 768 — plain zero is reserved to mean
    // "no checksum," which this function never intends.
    if (checksum == 0) {
        checksum = 0xFFFF;
    }

    writeU16(out + 6, checksum);

    return totalLength;
}

}  // namespace net

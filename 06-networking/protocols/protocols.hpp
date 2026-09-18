#pragma once

#include <stdint.h>

// Layer 6 (Networking) foundation: real wire-format codecs for
// Ethernet, ARP, IPv4, ICMP, and UDP — parsing, serialization, and
// checksum computation/verification against the actual RFCs (RFC 826
// for ARP, RFC 791 for IPv4, RFC 792 for ICMP, RFC 768 for UDP), not
// a made-up replacement protocol.
//
// This is honestly scoped as the *wire-format layer only*: there is
// no network interface card driver anywhere in 22-os yet, so nothing
// here transmits or receives a real frame — these functions parse and
// build the bytes a real driver would eventually hand to/from
// hardware. Deliberately hardware-independent (pure arithmetic over
// byte buffers, no allocation, no OS dependency), the same reasoning
// elf.cpp and heap.cpp's core logic already apply, so it's
// exhaustively unit-testable with a hosted compiler. See
// docs/ADR/0005-networking-protocol-layer.md for the full design and
// exact scope (no TCP yet, no fragmentation/reassembly, no IPv4
// options, no routing).

namespace net {

// --- Ethernet (IEEE 802.3) ---

constexpr uint32_t ETHERNET_HEADER_SIZE = 14;
constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
constexpr uint16_t ETHERTYPE_ARP = 0x0806;

struct MacAddress {
    uint8_t bytes[6];
};

bool macEquals(const MacAddress& a, const MacAddress& b);

struct EthernetHeader {
    MacAddress destination;
    MacAddress source;
    uint16_t etherType;
};

// --- ARP (RFC 826), fixed for Ethernet (hlen=6) + IPv4 (plen=4) ---

constexpr uint32_t ARP_PACKET_SIZE = 28;
constexpr uint16_t ARP_HTYPE_ETHERNET = 1;

enum class ArpOperation : uint16_t {
    Request = 1,
    Reply = 2,
};

struct ArpPacket {
    ArpOperation operation;
    MacAddress senderMac;
    uint32_t senderIp;  // host byte order
    MacAddress targetMac;
    uint32_t targetIp;  // host byte order
};

// --- IPv4 (RFC 791) — header only; v1 supports no options (IHL must
// be exactly 5 32-bit words = 20 bytes) and no fragmentation handling
// beyond reading the flags/offset field through unchanged. ---

constexpr uint32_t IPV4_MIN_HEADER_SIZE = 20;
constexpr uint8_t IPV4_VERSION = 4;
constexpr uint8_t IPV4_PROTO_ICMP = 1;
constexpr uint8_t IPV4_PROTO_UDP = 17;

struct Ipv4Header {
    uint8_t ttl;
    uint8_t protocol;
    uint16_t identification;
    uint16_t flagsAndFragmentOffset;
    uint16_t totalLength;  // includes the 20-byte header itself
    uint32_t sourceIp;     // host byte order
    uint32_t destIp;       // host byte order
};

// --- ICMP echo request/reply (RFC 792) ---

constexpr uint32_t ICMP_ECHO_HEADER_SIZE = 8;
constexpr uint8_t ICMP_TYPE_ECHO_REPLY = 0;
constexpr uint8_t ICMP_TYPE_ECHO_REQUEST = 8;

struct IcmpEchoMessage {
    uint8_t type;
    uint8_t code;
    uint16_t identifier;
    uint16_t sequenceNumber;
};

// --- UDP (RFC 768) ---

constexpr uint32_t UDP_HEADER_SIZE = 8;

struct UdpHeader {
    uint16_t sourcePort;
    uint16_t destPort;
    uint16_t length;  // header + payload, in bytes
};

// --- Errors shared by every parse function below. Not every value
// applies to every protocol; each parse function documents which
// subset it can return. ---

enum class ParseError {
    None,
    BufferTooSmall,
    UnsupportedVersion,
    HeaderLengthInvalid,
    TotalLengthInvalid,
    UnsupportedHardwareType,
    InvalidHardwareAddressLength,
    InvalidProtocolAddressLength,
    UnsupportedOperation,
    ChecksumMismatch,
};

// RFC 1071 Internet checksum (one's-complement sum of 16-bit words,
// folding carries back in, then one's-complemented). Used identically
// by IPv4, ICMP, and UDP (the latter over a pseudo-header + segment).
// `len` may be odd; a trailing single byte is treated as the high
// byte of a final 16-bit word with an implicit zero low byte, per the
// RFC.
uint16_t internetChecksum(const uint8_t* data, uint32_t len);

ParseError parseEthernetHeader(
    const uint8_t* buffer, uint32_t length, EthernetHeader& out
);

// Returns the number of bytes written, or 0 if outCapacity is too
// small (never partially writes).
uint32_t serializeEthernetHeader(
    const EthernetHeader& header, uint8_t* out, uint32_t outCapacity
);

ParseError parseArpPacket(
    const uint8_t* buffer, uint32_t length, ArpPacket& out
);

uint32_t serializeArpPacket(
    const ArpPacket& packet, uint8_t* out, uint32_t outCapacity
);

// Validates the version, header length (must be exactly
// IPV4_MIN_HEADER_SIZE — no options support), that totalLength is
// both >= the header size and <= the actual buffer length (never
// trusting the file/packet-claimed length over the real buffer), and
// the header checksum.
ParseError parseIpv4Header(
    const uint8_t* buffer, uint32_t length, Ipv4Header& out
);

// Serializes a 20-byte IPv4 header and computes and writes its own
// correct checksum (the caller never supplies one) — makes it
// impossible to accidentally serialize a header with a stale or wrong
// checksum.
uint32_t serializeIpv4Header(
    const Ipv4Header& header, uint8_t* out, uint32_t outCapacity
);

ParseError parseIcmpEcho(
    const uint8_t* buffer, uint32_t length, IcmpEchoMessage& out
);

// Verifies the ICMP checksum over the whole ICMP message (header +
// payload) independently of parseIcmpEcho, which does not itself
// check it — callers that care about integrity call this explicitly,
// mirroring the fact that ICMP's checksum covers the payload too,
// which parseIcmpEcho (header-only) never sees.
bool verifyIcmpChecksum(const uint8_t* buffer, uint32_t length);

uint32_t serializeIcmpEcho(
    const IcmpEchoMessage& message,
    const uint8_t* payload,
    uint32_t payloadLength,
    uint8_t* out,
    uint32_t outCapacity
);

ParseError parseUdpHeader(
    const uint8_t* buffer, uint32_t length, UdpHeader& out
);

// UDP's checksum covers an IPv4 pseudo-header (source/dest IP,
// protocol, UDP length) in addition to the UDP header+payload itself
// — per RFC 768, a checksum of exactly 0 means "no checksum computed"
// and is always considered valid, not a failure.
bool verifyUdpChecksumIpv4(
    uint32_t sourceIp,
    uint32_t destIp,
    const uint8_t* udpDatagram,
    uint32_t udpDatagramLength
);

uint32_t serializeUdpDatagramIpv4(
    const UdpHeader& header,
    const uint8_t* payload,
    uint32_t payloadLength,
    uint32_t sourceIp,
    uint32_t destIp,
    uint8_t* out,
    uint32_t outCapacity
);

}  // namespace net

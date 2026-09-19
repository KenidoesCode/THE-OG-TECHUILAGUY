#include "packet.hpp"

namespace netlab {

bool isBroadcast(const net::MacAddress& mac) {
    return net::macEquals(mac, BROADCAST_MAC);
}

Packet buildEthernetFrame(
    const net::MacAddress& source, const net::MacAddress& destination,
    uint16_t etherType, const std::vector<uint8_t>& payload
) {
    net::EthernetHeader header;
    header.source = source;
    header.destination = destination;
    header.etherType = etherType;

    std::vector<uint8_t> bytes(net::ETHERNET_HEADER_SIZE + payload.size());
    uint32_t written = net::serializeEthernetHeader(header, bytes.data(), static_cast<uint32_t>(bytes.size()));
    // serializeEthernetHeader never partially writes; a real header
    // always fits since `bytes` was sized to hold it plus the payload.
    for (size_t i = 0; i < payload.size(); ++i) {
        bytes[written + i] = payload[i];
    }

    return Packet{std::move(bytes)};
}

bool parseFrameHeader(const Packet& packet, net::EthernetHeader& out) {
    return net::parseEthernetHeader(packet.bytes.data(), static_cast<uint32_t>(packet.bytes.size()), out)
           == net::ParseError::None;
}

}  // namespace netlab

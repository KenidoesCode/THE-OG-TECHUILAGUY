#include "packet_inspector.hpp"

#include "../../06-networking/protocols/protocols.hpp"

namespace netlab {

namespace {

const char HEX_DIGITS[] = "0123456789abcdef";

std::string macToString(const net::MacAddress& mac) {
    std::string out;
    for (size_t i = 0; i < 6; ++i) {
        if (i != 0) out.push_back(':');
        out.push_back(HEX_DIGITS[mac.bytes[i] >> 4]);
        out.push_back(HEX_DIGITS[mac.bytes[i] & 0x0F]);
    }
    return out;
}

std::string ipToString(uint32_t ip) {
    return std::to_string((ip >> 24) & 0xFF) + "." + std::to_string((ip >> 16) & 0xFF) + "." +
           std::to_string((ip >> 8) & 0xFF) + "." + std::to_string(ip & 0xFF);
}

void decodeArp(const uint8_t* payload, uint32_t length, DecodedFrame& out) {
    net::ArpPacket arp;
    if (net::parseArpPacket(payload, length, arp) != net::ParseError::None) {
        out.parsedSuccessfully = false;
        return;
    }

    bool isRequest = arp.operation == net::ArpOperation::Request;
    out.protocolFields = {
        {"Operation", isRequest ? "Request" : "Reply"},
        {"Sender MAC", macToString(arp.senderMac)},
        {"Sender IP", ipToString(arp.senderIp)},
        {"Target MAC", macToString(arp.targetMac)},
        {"Target IP", ipToString(arp.targetIp)},
    };

    if (isRequest) {
        out.summary = "ARP Request: who has " + ipToString(arp.targetIp) + "? tell " + ipToString(arp.senderIp);
    } else {
        out.summary = "ARP Reply: " + ipToString(arp.senderIp) + " is at " + macToString(arp.senderMac);
    }
    out.parsedSuccessfully = true;
}

void decodeIpv4(const uint8_t* payload, uint32_t length, DecodedFrame& out) {
    net::Ipv4Header ip;
    if (net::parseIpv4Header(payload, length, ip) != net::ParseError::None) {
        out.parsedSuccessfully = false;
        return;
    }

    out.protocolFields = {
        {"Source IP", ipToString(ip.sourceIp)},
        {"Destination IP", ipToString(ip.destIp)},
        {"TTL", std::to_string(ip.ttl)},
        {"Protocol", std::to_string(ip.protocol)},
        {"Total Length", std::to_string(ip.totalLength)},
    };

    if (ip.protocol == net::IPV4_PROTO_ICMP && length > net::IPV4_MIN_HEADER_SIZE) {
        net::IcmpEchoMessage icmp;
        uint32_t icmpLength = length - net::IPV4_MIN_HEADER_SIZE;
        if (net::parseIcmpEcho(payload + net::IPV4_MIN_HEADER_SIZE, icmpLength, icmp) == net::ParseError::None) {
            bool isRequest = icmp.type == net::ICMP_TYPE_ECHO_REQUEST;
            out.protocolFields.push_back({"ICMP Type", isRequest ? "Echo Request" : "Echo Reply"});
            out.protocolFields.push_back({"ICMP Identifier", std::to_string(icmp.identifier)});
            out.protocolFields.push_back({"ICMP Sequence", std::to_string(icmp.sequenceNumber)});

            out.summary = std::string("IPv4 ICMP ") + (isRequest ? "Echo Request" : "Echo Reply") + ": " +
                          ipToString(ip.sourceIp) + " -> " + ipToString(ip.destIp) + ", ttl=" + std::to_string(ip.ttl);
            out.parsedSuccessfully = true;
            return;
        }
    }

    out.summary = "IPv4 packet (protocol " + std::to_string(ip.protocol) + "): " + ipToString(ip.sourceIp) +
                  " -> " + ipToString(ip.destIp);
    out.parsedSuccessfully = true;
}

}  // namespace

DecodedFrame decodeFrame(const std::vector<uint8_t>& frameBytes) {
    DecodedFrame out;

    net::EthernetHeader ethernet;
    if (net::parseEthernetHeader(frameBytes.data(), static_cast<uint32_t>(frameBytes.size()), ethernet) !=
        net::ParseError::None) {
        return out;
    }
    out.ethernetFields = {
        {"Source MAC", macToString(ethernet.source)},
        {"Destination MAC", macToString(ethernet.destination)},
        {"EtherType", "0x" + std::to_string(ethernet.etherType)},
    };

    const uint8_t* payload = frameBytes.data() + net::ETHERNET_HEADER_SIZE;
    uint32_t payloadLength = static_cast<uint32_t>(frameBytes.size()) - net::ETHERNET_HEADER_SIZE;

    if (ethernet.etherType == net::ETHERTYPE_ARP) {
        decodeArp(payload, payloadLength, out);
    } else if (ethernet.etherType == net::ETHERTYPE_IPV4) {
        decodeIpv4(payload, payloadLength, out);
    } else {
        out.parsedSuccessfully = false;
    }

    return out;
}

}  // namespace netlab

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Real packet decoding for NetLab's timeline — parses a captured
// frame's actual bytes using ONLY 06-networking's existing protocol
// codecs (never reimplements a header layout). UI-independent: this
// is the data a future timeline/inspector UI would render, not the
// UI itself. See
// docs/ADR/0029-netlab-packet-inspector-and-timeline-session.md.

namespace netlab {

struct DecodedField {
    std::string name;
    std::string value;
};

struct DecodedFrame {
    bool parsedSuccessfully = false;
    std::string summary;  // one-line human-readable description
    std::vector<DecodedField> ethernetFields;
    std::vector<DecodedField> protocolFields;  // ARP, or IPv4(+ICMP), fields — empty if undecodable/unsupported
};

// Decodes a real captured frame. Returns
// DecodedFrame{parsedSuccessfully: false} (with empty summary/fields)
// on any malformed/truncated input or an EtherType this slice doesn't
// decode yet — never crashes, never fabricates field values.
DecodedFrame decodeFrame(const std::vector<uint8_t>& frameBytes);

}  // namespace netlab

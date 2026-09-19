#pragma once

#include "../../06-networking/protocols/protocols.hpp"

#include <string>
#include <vector>

// A NetLab topology node — Host (has a MAC + optional IPv4 config,
// never forwards), Switch (forwards, with real MAC learning — see
// topology.hpp), or Router (an L2 broadcast-domain boundary that
// performs real single-hop L3 forwarding between its interfaces' own
// subnets — see docs/ADR/0028-netlab-ip-arp-routing-mission.md). See
// also docs/ADR/0022-techuilaguy-netlab-foundation.md for the
// original L2-only foundation.

namespace netlab {

enum class NodeKind {
    Host,
    Switch,
    Router,
};

// One of a Router's interfaces — identified by which neighbor (link)
// it faces, matching how a real router's interfaces correspond to
// physical links.
struct RouterInterface {
    std::string neighborNodeId;
    net::MacAddress mac{};
    uint32_t ipAddress = 0;
    uint32_t subnetMask = 0;
};

struct Node {
    std::string id;
    NodeKind kind;

    // Meaningful for Host nodes only.
    net::MacAddress mac{};
    uint32_t ipAddress = 0;
    uint32_t subnetMask = 0;
    uint32_t defaultGatewayIp = 0;

    // Meaningful for Router nodes only.
    std::vector<RouterInterface> routerInterfaces;
};

}  // namespace netlab

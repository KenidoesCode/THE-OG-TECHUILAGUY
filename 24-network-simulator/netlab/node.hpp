#pragma once

#include "../../06-networking/protocols/protocols.hpp"

#include <string>

// A NetLab topology node — either a Host (has a MAC address, never
// forwards) or a Switch (forwards, with real MAC learning — see
// topology.hpp). See docs/ADR/0022-techuilaguy-netlab-foundation.md.

namespace netlab {

enum class NodeKind {
    Host,
    Switch,
};

struct Node {
    std::string id;
    NodeKind kind;
    net::MacAddress mac{};  // meaningful for Host nodes; ignored for Switch nodes
};

}  // namespace netlab

#include "mission.hpp"

namespace netlab {

MissionResult evaluateConnectTwoNetworksMission(
    Topology& topology, const std::string& hostAId, const std::string& hostBId
) {
    MissionResult result;

    const Node* hostA = topology.getNode(hostAId);
    const Node* hostB = topology.getNode(hostBId);

    if (hostA == nullptr || hostA->kind != NodeKind::Host) {
        result.reason = "Host '" + hostAId + "' does not exist";
        return result;
    }
    if (hostB == nullptr || hostB->kind != NodeKind::Host) {
        result.reason = "Host '" + hostBId + "' does not exist";
        return result;
    }
    if (hostA->ipAddress == 0) {
        result.reason = "'" + hostAId + "' has no IPv4 address configured";
        return result;
    }
    if (hostB->ipAddress == 0) {
        result.reason = "'" + hostBId + "' has no IPv4 address configured";
        return result;
    }
    if ((hostA->ipAddress & hostA->subnetMask) == (hostB->ipAddress & hostB->subnetMask)) {
        result.reason = "'" + hostAId + "' and '" + hostBId +
                         "' are on the same subnet — this mission requires two DIFFERENT networks joined by a router";
        return result;
    }

    result.pingResult = sendIcmpEcho(topology, hostAId, hostB->ipAddress);
    if (!result.pingResult.delivered) {
        result.reason = "Ping from '" + hostAId + "' to '" + hostBId + "' failed: " + result.pingResult.failureReason;
        return result;
    }

    result.success = true;
    result.reason = "'" + hostAId + "' successfully pinged '" + hostBId + "' across the router";
    return result;
}

}  // namespace netlab

#include "rpc.hpp"

namespace dist {

std::vector<uint8_t> serializeRequest(const RpcRequest& request) {
    Encoder enc;
    enc.writeU64(request.id);
    enc.writeString(request.method);
    enc.writeBytes(request.payload);
    return enc.data();
}

bool parseRequest(const std::vector<uint8_t>& bytes, RpcRequest& out) {
    Decoder dec(bytes);
    if (!dec.readU64(out.id)) return false;
    if (!dec.readString(out.method)) return false;
    if (!dec.readBytes(out.payload)) return false;
    return dec.atEnd();
}

std::vector<uint8_t> serializeResponse(const RpcResponse& response) {
    Encoder enc;
    enc.writeU64(response.id);
    enc.writeU8(response.ok ? 1 : 0);
    enc.writeBytes(response.payload);
    enc.writeString(response.errorMessage);
    return enc.data();
}

bool parseResponse(const std::vector<uint8_t>& bytes, RpcResponse& out) {
    Decoder dec(bytes);
    if (!dec.readU64(out.id)) return false;

    uint8_t okByte;
    if (!dec.readU8(okByte)) return false;
    out.ok = okByte != 0;

    if (!dec.readBytes(out.payload)) return false;
    if (!dec.readString(out.errorMessage)) return false;
    return dec.atEnd();
}

// --------------------------------------------------------------
// SimulatedNetwork
// --------------------------------------------------------------

SimulatedNetwork::SimulatedNetwork(uint64_t seed, FaultConfig faults)
    : rngState(seed == 0 ? 0x9E3779B97F4A7C15ULL : seed), faults(faults) {}

uint64_t SimulatedNetwork::nextRandom() {
    // xorshift64*
    rngState ^= rngState >> 12;
    rngState ^= rngState << 25;
    rngState ^= rngState >> 27;
    return rngState * 0x2545F4914F6CDD1DULL;
}

double SimulatedNetwork::nextUnitDouble() {
    // Top 53 bits give a value uniformly distributed in [0, 1).
    return static_cast<double>(nextRandom() >> 11) *
           (1.0 / 9007199254740992.0);  // 2^53
}

bool SimulatedNetwork::isPartitioned(NodeId a, NodeId b) const {
    for (const auto& pair : partitions) {
        if ((pair.first == a && pair.second == b) ||
            (pair.first == b && pair.second == a)) {
            return true;
        }
    }
    return false;
}

void SimulatedNetwork::partition(NodeId a, NodeId b) {
    if (!isPartitioned(a, b)) {
        partitions.push_back({a, b});
    }
}

void SimulatedNetwork::healPartition(NodeId a, NodeId b) {
    for (size_t i = 0; i < partitions.size(); ++i) {
        if ((partitions[i].first == a && partitions[i].second == b) ||
            (partitions[i].first == b && partitions[i].second == a)) {
            partitions.erase(partitions.begin() + static_cast<long>(i));
            return;
        }
    }
}

void SimulatedNetwork::send(NodeId from, NodeId to, std::vector<uint8_t> message) {
    if (isPartitioned(from, to)) {
        // Unconditionally dropped — a partition is not merely "high
        // packet loss," it is a hard, deterministic block, matching
        // the real-world failure mode it simulates.
        return;
    }

    if (nextUnitDouble() < faults.dropProbability) {
        return;
    }

    uint32_t extraDelay = 0;
    if (faults.maxExtraDelayTicks > 0) {
        extraDelay = static_cast<uint32_t>(
            nextRandom() % (static_cast<uint64_t>(faults.maxExtraDelayTicks) + 1)
        );
    }

    inFlight.push_back({from, to, message, tickCounter + 1 + extraDelay});

    if (nextUnitDouble() < faults.duplicateProbability) {
        // A duplicate is scheduled independently (its own possible
        // extra delay) so a duplicated message doesn't necessarily
        // arrive adjacent to the original — genuine reordering, not
        // just genuine duplication.
        uint32_t dupDelay = 0;
        if (faults.maxExtraDelayTicks > 0) {
            dupDelay = static_cast<uint32_t>(
                nextRandom() % (static_cast<uint64_t>(faults.maxExtraDelayTicks) + 1)
            );
        }
        inFlight.push_back({from, to, message, tickCounter + 1 + dupDelay});
    }
}

void SimulatedNetwork::tick() {
    ++tickCounter;

    std::vector<InFlightMessage> stillInFlight;
    stillInFlight.reserve(inFlight.size());

    for (auto& msg : inFlight) {
        if (msg.deliverAtTick <= tickCounter) {
            inboxes[msg.to].push_back({msg.from, std::move(msg.bytes)});
        } else {
            stillInFlight.push_back(std::move(msg));
        }
    }

    inFlight = std::move(stillInFlight);
}

std::vector<std::pair<NodeId, std::vector<uint8_t>>> SimulatedNetwork::receiveAll(NodeId node) {
    auto it = inboxes.find(node);
    if (it == inboxes.end()) {
        return {};
    }
    auto result = std::move(it->second);
    it->second.clear();
    return result;
}

// --------------------------------------------------------------
// RpcServer
// --------------------------------------------------------------

void RpcServer::registerMethod(const std::string& name, MethodHandler handler) {
    handlers[name] = std::move(handler);
}

std::vector<uint8_t> RpcServer::handleRequest(const std::vector<uint8_t>& requestBytes) {
    RpcRequest request;
    if (!parseRequest(requestBytes, request)) {
        RpcResponse response{0, false, {}, "Malformed request"};
        return serializeResponse(response);
    }

    auto it = handlers.find(request.method);
    if (it == handlers.end()) {
        RpcResponse response{
            request.id, false, {}, "Unknown method: " + request.method
        };
        return serializeResponse(response);
    }

    std::vector<uint8_t> resultPayload = it->second(request.payload);
    RpcResponse response{request.id, true, resultPayload, ""};
    return serializeResponse(response);
}

void pumpServer(SimulatedNetwork& network, NodeId serverNode, RpcServer& server) {
    for (auto& [from, bytes] : network.receiveAll(serverNode)) {
        std::vector<uint8_t> response = server.handleRequest(bytes);
        network.send(serverNode, from, response);
    }
}

// --------------------------------------------------------------
// RpcClient
// --------------------------------------------------------------

RpcClient::RpcClient(
    NodeId self, NodeId serverNode, SimulatedNetwork& network, RpcServer& server
)
    : self(self), serverNode(serverNode), network(network), server(server) {}

CallResult RpcClient::call(
    const std::string& method,
    const std::vector<uint8_t>& payload,
    uint32_t maxTicks
) {
    RequestId id = nextRequestId++;
    RpcRequest request{id, method, payload};
    network.send(self, serverNode, serializeRequest(request));

    for (uint32_t i = 0; i < maxTicks; ++i) {
        network.tick();
        pumpServer(network, serverNode, server);

        for (auto& [from, bytes] : network.receiveAll(self)) {
            (void)from;
            RpcResponse response;
            if (!parseResponse(bytes, response)) {
                continue;  // a malformed/foreign message; not this call's answer
            }
            if (response.id != id) {
                continue;  // a stale or unrelated response; discard, keep waiting
            }

            if (response.ok) {
                return {CallOutcome::Success, response.payload, ""};
            }
            return {CallOutcome::Error, {}, response.errorMessage};
        }
    }

    return {CallOutcome::TimedOut, {}, ""};
}

}  // namespace dist

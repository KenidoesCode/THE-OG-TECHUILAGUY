#pragma once

#include "serialization.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// A real RPC request/response model plus a deterministic in-process
// network simulator with fault injection (FR-DIST-1: "RPC + ... with
// deterministic simulation and fault injection (partitions, node
// loss, message loss/reorder/duplication, recovery)"). There is no
// real socket/NIC-backed transport yet (Layer 6 has no driver — see
// 06-networking/README.md) — see docs/ADR/0008-distributed-rpc.md for
// the full scope and exactly what a future real-transport
// implementation would need to add.

namespace dist {

using NodeId = uint32_t;
using RequestId = uint64_t;

struct RpcRequest {
    RequestId id;
    std::string method;
    std::vector<uint8_t> payload;
};

struct RpcResponse {
    RequestId id;
    bool ok;
    std::vector<uint8_t> payload;    // meaningful only if ok
    std::string errorMessage;         // meaningful only if !ok
};

std::vector<uint8_t> serializeRequest(const RpcRequest& request);
bool parseRequest(const std::vector<uint8_t>& bytes, RpcRequest& out);

std::vector<uint8_t> serializeResponse(const RpcResponse& response);
bool parseResponse(const std::vector<uint8_t>& bytes, RpcResponse& out);

// --------------------------------------------------------------
// Deterministic network simulation.
//
// Every random decision (drop, duplicate, reorder delay) is drawn
// from a single seeded PRNG owned by the SimulatedNetwork — running
// the identical sequence of send()/tick() calls against two
// SimulatedNetwork instances constructed with the same seed produces
// byte-identical fault decisions and delivery order every time. This
// is what "deterministic simulation" means here: not that faults
// don't happen, but that a failure is exactly reproducible from its
// seed, which is what makes debugging a distributed-systems bug
// tractable at all.
// --------------------------------------------------------------

struct FaultConfig {
    double dropProbability = 0.0;       // [0,1]: message never delivered
    double duplicateProbability = 0.0;  // [0,1]: message delivered twice
    uint32_t maxExtraDelayTicks = 0;    // message delivered 0..N ticks late
};

class SimulatedNetwork {
public:
    explicit SimulatedNetwork(uint64_t seed, FaultConfig faults = {});

    // Enqueues `message` for delivery from `from` to `to`, subject to
    // the configured fault injection and any active partition between
    // the two nodes (a partitioned pair drops every message between
    // them unconditionally, regardless of dropProbability, until
    // healed).
    void send(NodeId from, NodeId to, std::vector<uint8_t> message);

    // Advances simulated time by one tick: delivers every message
    // whose scheduled delivery tick has arrived, appending it to that
    // destination node's inbox. Determinism means the exact same
    // sequence of send()/tick() calls with the same seed always
    // produces the same set of drop/duplicate/delay decisions.
    void tick();

    // Drains and returns every message currently sitting in `node`'s
    // inbox, each paired with the NodeId that sent it (so a server can
    // address its response back to the right sender), in delivery
    // order — which, under reordering, is not necessarily send order.
    std::vector<std::pair<NodeId, std::vector<uint8_t>>> receiveAll(NodeId node);

    // Blocks all message delivery between these two nodes in both
    // directions until healPartition() is called for the same pair —
    // the concrete simulation of a network partition.
    void partition(NodeId a, NodeId b);
    void healPartition(NodeId a, NodeId b);
    bool isPartitioned(NodeId a, NodeId b) const;

    uint64_t currentTick() const { return tickCounter; }

private:
    struct InFlightMessage {
        NodeId from;
        NodeId to;
        std::vector<uint8_t> bytes;
        uint64_t deliverAtTick;
    };

    uint64_t rngState;
    FaultConfig faults;
    uint64_t tickCounter = 0;

    std::vector<InFlightMessage> inFlight;
    std::unordered_map<NodeId, std::vector<std::pair<NodeId, std::vector<uint8_t>>>> inboxes;
    std::vector<std::pair<NodeId, NodeId>> partitions;

    // xorshift64* — a small, fully deterministic PRNG (not
    // cryptographically secure, and not intended to be; determinism
    // and speed are what a fault-injection simulator needs, not
    // unpredictability).
    uint64_t nextRandom();
    double nextUnitDouble();  // [0, 1)
};

// --------------------------------------------------------------
// A minimal RPC server: registers named method handlers and dispatches
// an incoming request to one, returning the response bytes to send
// back. An unrecognized method name produces a real error response
// (ok=false, a descriptive errorMessage) rather than crashing or
// silently dropping the request.
// --------------------------------------------------------------

using MethodHandler = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

class RpcServer {
public:
    void registerMethod(const std::string& name, MethodHandler handler);

    // Parses `requestBytes` as an RpcRequest and returns the
    // serialized RpcResponse bytes. A malformed request (fails to
    // parse) produces an error response with request id 0 (there is
    // no other id to attribute the error to) rather than being
    // silently ignored.
    std::vector<uint8_t> handleRequest(const std::vector<uint8_t>& requestBytes);

private:
    std::unordered_map<std::string, MethodHandler> handlers;
};

// Drains every message waiting in `serverNode`'s inbox, hands each to
// `server` for handling, and sends the resulting response back to
// whichever node actually sent that message — the piece that makes a
// server "live" in the simulation. A test drives a simulated RPC
// exchange by calling this once per tick for every server node,
// interleaved with SimulatedNetwork::tick() and RpcClient::call()
// (which does this automatically for its own single target server —
// see RpcClient's constructor).
void pumpServer(SimulatedNetwork& network, NodeId serverNode, RpcServer& server);

// --------------------------------------------------------------
// A minimal RPC client driving calls over a SimulatedNetwork. Because
// the network is a deterministic step-simulation (not a real
// wall-clock transport), a "timeout" here is expressed in ticks: the
// caller drives the simulation forward (see the tests for the exact
// pattern) and call() reports whether a matching response arrived
// within the given tick budget.
// --------------------------------------------------------------

enum class CallOutcome {
    Success,
    Error,     // a well-formed error response was received
    TimedOut,  // no response (matching or otherwise) arrived in time
};

struct CallResult {
    CallOutcome outcome;
    std::vector<uint8_t> payload;  // meaningful only if outcome == Success
    std::string errorMessage;       // meaningful only if outcome == Error
};

class RpcClient {
public:
    // `server` is the RpcServer instance actually running on
    // `serverNode` — in this single-process simulation there is no
    // real inter-process boundary, so call() can directly pump it
    // each tick rather than requiring the test to interleave a
    // separate pumpServer() call for the common single-server case. A
    // multi-node test (several servers) drives pumpServer() itself
    // for the other nodes instead of using this convenience.
    RpcClient(
        NodeId self,
        NodeId serverNode,
        SimulatedNetwork& network,
        RpcServer& server
    );

    // Sends the request immediately, then advances the network up to
    // `maxTicks` ticks — pumping the target server after every tick,
    // then checking this client's inbox — until a response with the
    // matching request id arrives or the tick budget is exhausted.
    // Any response with a *different* id that happens to arrive (e.g.
    // a stale duplicate from an earlier call) is discarded, not
    // mistaken for this call's answer — the concrete reason request
    // ids exist at all.
    CallResult call(
        const std::string& method,
        const std::vector<uint8_t>& payload,
        uint32_t maxTicks
    );

private:
    NodeId self;
    NodeId serverNode;
    SimulatedNetwork& network;
    RpcServer& server;
    RequestId nextRequestId = 1;
};

}  // namespace dist

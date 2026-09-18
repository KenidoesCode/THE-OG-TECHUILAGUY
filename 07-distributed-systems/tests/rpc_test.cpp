// Real assertion-based tests for the Layer 7 (Distributed Systems)
// RPC + deterministic network simulation foundation (rpc/serialization.cpp,
// rpc/rpc.cpp). Hosted, no hardware/OS dependency.

#include "../rpc/rpc.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

std::vector<uint8_t> toBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::string toString(const std::vector<uint8_t>& b) {
    return std::string(b.begin(), b.end());
}

}  // namespace

// --- Serialization ---

void testEncoderDecoderRoundTrip() {
    dist::Encoder enc;
    enc.writeU8(0xAB);
    enc.writeU32(0xDEADBEEF);
    enc.writeU64(0x0123456789ABCDEFULL);
    enc.writeBytes({1, 2, 3, 4, 5});
    enc.writeString("hello");

    dist::Decoder dec(enc.data());

    uint8_t u8;
    uint32_t u32;
    uint64_t u64;
    std::vector<uint8_t> bytes;
    std::string str;

    bool ok = dec.readU8(u8) && dec.readU32(u32) && dec.readU64(u64) &&
              dec.readBytes(bytes) && dec.readString(str);

    check(ok, "serialization: all fields decode successfully");
    check(u8 == 0xAB, "serialization: u8 round-trips correctly");
    check(u32 == 0xDEADBEEF, "serialization: u32 round-trips correctly");
    check(u64 == 0x0123456789ABCDEFULL, "serialization: u64 round-trips correctly");
    check(bytes.size() == 5 && bytes[4] == 5, "serialization: byte blob round-trips correctly");
    check(str == "hello", "serialization: string round-trips correctly");
    check(dec.atEnd(), "serialization: decoder correctly reports the buffer fully consumed");
}

void testDecoderRejectsTruncatedU32() {
    std::vector<uint8_t> tooShort = {1, 2};
    dist::Decoder dec(tooShort);
    uint32_t value;
    check(!dec.readU32(value), "serialization: rejects a u32 read from a 2-byte buffer");
}

void testDecoderRejectsBytesLengthExceedingBuffer() {
    dist::Encoder enc;
    enc.writeU32(1000);  // claims 1000 bytes follow
    enc.writeU8('x');    // only 1 actually does

    dist::Decoder dec(enc.data());
    std::vector<uint8_t> out;
    check(!dec.readBytes(out),
          "serialization: rejects a length-prefixed field claiming more "
          "bytes than actually remain in the buffer");
}

void testDecoderStaysFailedAfterFirstFailure() {
    std::vector<uint8_t> empty;
    dist::Decoder dec(empty);
    uint8_t a, b;
    bool firstOk = dec.readU8(a);
    bool secondOk = dec.readU8(b);
    check(!firstOk && !secondOk,
          "serialization: a decoder that has already failed continues "
          "returning false rather than reading garbage");
}

void testAtEndDetectsTrailingBytes() {
    dist::Encoder enc;
    enc.writeU8(1);
    std::vector<uint8_t> withExtra = enc.data();
    withExtra.push_back(0xFF);  // unexpected trailing byte

    dist::Decoder dec(withExtra);
    uint8_t value;
    dec.readU8(value);
    check(!dec.atEnd(),
          "serialization: atEnd() correctly detects unconsumed trailing bytes");
}

// --- RPC message framing ---

void testRequestSerializeParseRoundTrip() {
    dist::RpcRequest request{42, "echo", toBytes("payload data")};
    auto bytes = dist::serializeRequest(request);

    dist::RpcRequest parsed;
    bool ok = dist::parseRequest(bytes, parsed);

    check(ok, "rpc: request serialize/parse round trip succeeds");
    check(parsed.id == 42, "rpc: request id round-trips correctly");
    check(parsed.method == "echo", "rpc: request method name round-trips correctly");
    check(toString(parsed.payload) == "payload data",
          "rpc: request payload round-trips correctly");
}

void testResponseSerializeParseRoundTrip() {
    dist::RpcResponse response{42, true, toBytes("result"), ""};
    auto bytes = dist::serializeResponse(response);

    dist::RpcResponse parsed;
    bool ok = dist::parseResponse(bytes, parsed);

    check(ok, "rpc: response serialize/parse round trip succeeds");
    check(parsed.ok, "rpc: response ok flag round-trips correctly");
    check(toString(parsed.payload) == "result", "rpc: response payload round-trips correctly");
}

void testErrorResponseRoundTrip() {
    dist::RpcResponse response{7, false, {}, "something went wrong"};
    auto bytes = dist::serializeResponse(response);

    dist::RpcResponse parsed;
    bool ok = dist::parseResponse(bytes, parsed);

    check(ok && !parsed.ok && parsed.errorMessage == "something went wrong",
          "rpc: an error response's ok=false flag and error message both round-trip correctly");
}

void testRequestRejectsTruncatedBytes() {
    dist::RpcRequest request{1, "method", {}};
    auto bytes = dist::serializeRequest(request);
    bytes.resize(bytes.size() - 2);  // truncate the last field

    dist::RpcRequest parsed;
    check(!dist::parseRequest(bytes, parsed),
          "rpc: rejects a request message truncated mid-field");
}

void testRequestRejectsTrailingGarbage() {
    dist::RpcRequest request{1, "method", {}};
    auto bytes = dist::serializeRequest(request);
    bytes.push_back(0xFF);  // unexpected trailing byte after the last field

    dist::RpcRequest parsed;
    check(!dist::parseRequest(bytes, parsed),
          "rpc: rejects a request message with unexpected trailing bytes");
}

// --- End-to-end RPC over the deterministic simulated network ---

void registerEchoAndUpperMethods(dist::RpcServer& server) {
    server.registerMethod("echo", [](const std::vector<uint8_t>& payload) {
        return payload;
    });
    server.registerMethod("upper", [](const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> result = payload;
        for (auto& b : result) {
            if (b >= 'a' && b <= 'z') b = static_cast<uint8_t>(b - 'a' + 'A');
        }
        return result;
    });
}

void testSuccessfulRpcCallNoFaults() {
    dist::SimulatedNetwork network(1234);
    dist::RpcServer server;
    registerEchoAndUpperMethods(server);

    dist::RpcClient client(/*self=*/1, /*serverNode=*/2, network, server);
    auto result = client.call("upper", toBytes("hello"), /*maxTicks=*/10);

    check(result.outcome == dist::CallOutcome::Success,
          "rpc e2e: a call with no fault injection succeeds within the tick budget");
    check(toString(result.payload) == "HELLO",
          "rpc e2e: the server's handler result is delivered back to the client correctly");
}

void testUnknownMethodReturnsErrorNotCrash() {
    dist::SimulatedNetwork network(1234);
    dist::RpcServer server;
    registerEchoAndUpperMethods(server);

    dist::RpcClient client(1, 2, network, server);
    auto result = client.call("does_not_exist", {}, 10);

    check(result.outcome == dist::CallOutcome::Error,
          "rpc e2e: calling an unregistered method produces a real error "
          "response, not a crash or a silent timeout");
    check(result.errorMessage.find("does_not_exist") != std::string::npos,
          "rpc e2e: the error message names the unknown method");
}

void testMultipleIndependentCallsEachGetCorrectResponse() {
    dist::SimulatedNetwork network(5555);
    dist::RpcServer server;
    registerEchoAndUpperMethods(server);

    dist::RpcClient client(1, 2, network, server);

    auto r1 = client.call("echo", toBytes("first"), 10);
    auto r2 = client.call("upper", toBytes("second"), 10);
    auto r3 = client.call("echo", toBytes("third"), 10);

    check(r1.outcome == dist::CallOutcome::Success && toString(r1.payload) == "first",
          "rpc e2e: first call gets its own correct response");
    check(r2.outcome == dist::CallOutcome::Success && toString(r2.payload) == "SECOND",
          "rpc e2e: second call (different method) gets its own correct response");
    check(r3.outcome == dist::CallOutcome::Success && toString(r3.payload) == "third",
          "rpc e2e: third call is not confused with the first two despite "
          "sharing the same client/server pair");
}

// --- Fault injection ---

void testMessageDropCausesTimeout() {
    dist::FaultConfig faults;
    faults.dropProbability = 1.0;  // every message is dropped, unconditionally

    dist::SimulatedNetwork network(42, faults);
    dist::RpcServer server;
    registerEchoAndUpperMethods(server);

    dist::RpcClient client(1, 2, network, server);
    auto result = client.call("echo", toBytes("lost"), 10);

    check(result.outcome == dist::CallOutcome::TimedOut,
          "rpc fault injection: a 100% message drop rate causes the client "
          "to time out rather than hang forever or crash");
}

void testPartitionBlocksDeliveryUntilHealed() {
    dist::SimulatedNetwork network(99);
    dist::RpcServer server;
    registerEchoAndUpperMethods(server);

    network.partition(1, 2);

    dist::RpcClient client(1, 2, network, server);
    auto duringPartition = client.call("echo", toBytes("blocked"), 10);

    check(duringPartition.outcome == dist::CallOutcome::TimedOut,
          "rpc fault injection: a call across a partitioned link times out, "
          "even with zero random drop probability configured");

    network.healPartition(1, 2);
    auto afterHealing = client.call("echo", toBytes("unblocked"), 10);

    check(afterHealing.outcome == dist::CallOutcome::Success &&
          toString(afterHealing.payload) == "unblocked",
          "rpc fault injection: the identical call succeeds immediately "
          "after the partition is healed");
}

void testDuplicationDoesNotBreakClientMatching() {
    dist::FaultConfig faults;
    faults.duplicateProbability = 1.0;  // every message is duplicated

    dist::SimulatedNetwork network(777, faults);
    dist::RpcServer server;
    registerEchoAndUpperMethods(server);

    dist::RpcClient client(1, 2, network, server);
    auto result = client.call("echo", toBytes("dup"), 20);

    // The client must still resolve the call correctly (it discards a
    // duplicate/foreign response by id rather than getting confused by
    // receiving more than one message) — a genuine, documented gap is
    // that the SERVER may execute the request handler twice (v1 has no
    // request-deduplication cache), recorded explicitly in
    // docs/ADR/0008-distributed-rpc.md rather than silently assumed
    // away.
    check(result.outcome == dist::CallOutcome::Success &&
          toString(result.payload) == "dup",
          "rpc fault injection: with every message duplicated, the client "
          "still correctly resolves its call by matching request ids");
}

void testReorderingStillMatchesResponsesByRequestId() {
    dist::FaultConfig faults;
    faults.maxExtraDelayTicks = 5;  // messages can arrive noticeably out of order

    dist::SimulatedNetwork network(31337, faults);
    dist::RpcServer server;
    registerEchoAndUpperMethods(server);

    dist::RpcClient client(1, 2, network, server);

    // Each call resolves fully (including its own request/response
    // round trip) before the next begins, so this specifically tests
    // that reordering *within* one call's own request/response pair
    // doesn't break correctness — concurrent in-flight calls with
    // reordering across each other would need a more advanced client,
    // an explicitly documented v1 limit.
    auto r1 = client.call("echo", toBytes("alpha"), 30);
    auto r2 = client.call("upper", toBytes("beta"), 30);

    check(r1.outcome == dist::CallOutcome::Success && toString(r1.payload) == "alpha",
          "rpc fault injection: under reordering/delay, a call still "
          "resolves to its own correct response");
    check(r2.outcome == dist::CallOutcome::Success && toString(r2.payload) == "BETA",
          "rpc fault injection: a second call after the first also "
          "resolves correctly despite reordering delay");
}

// --- Determinism ---

void testSameSeedProducesIdenticalFaultDecisions() {
    dist::FaultConfig faults;
    faults.dropProbability = 0.5;
    faults.duplicateProbability = 0.3;
    faults.maxExtraDelayTicks = 3;

    auto runScenario = [&](uint64_t seed) {
        dist::SimulatedNetwork network(seed, faults);
        dist::RpcServer server;
        registerEchoAndUpperMethods(server);
        dist::RpcClient client(1, 2, network, server);

        std::vector<dist::CallOutcome> outcomes;
        for (int i = 0; i < 8; ++i) {
            outcomes.push_back(client.call("echo", toBytes("msg"), 15).outcome);
        }
        return outcomes;
    };

    auto run1 = runScenario(2024);
    auto run2 = runScenario(2024);
    auto run3 = runScenario(999);

    check(run1 == run2,
          "determinism: two simulations constructed with the identical "
          "seed and fault config produce an identical sequence of call "
          "outcomes (success/error/timeout), byte-for-byte reproducible");

    check(run1 != run3,
          "determinism: a different seed produces a genuinely different "
          "fault pattern (confirms the seed actually drives the outcome, "
          "rather than the test being trivially deterministic regardless)");
}

int main() {
    testEncoderDecoderRoundTrip();
    testDecoderRejectsTruncatedU32();
    testDecoderRejectsBytesLengthExceedingBuffer();
    testDecoderStaysFailedAfterFirstFailure();
    testAtEndDetectsTrailingBytes();
    testRequestSerializeParseRoundTrip();
    testResponseSerializeParseRoundTrip();
    testErrorResponseRoundTrip();
    testRequestRejectsTruncatedBytes();
    testRequestRejectsTrailingGarbage();
    testSuccessfulRpcCallNoFaults();
    testUnknownMethodReturnsErrorNotCrash();
    testMultipleIndependentCallsEachGetCorrectResponse();
    testMessageDropCausesTimeout();
    testPartitionBlocksDeliveryUntilHealed();
    testDuplicationDoesNotBreakClientMatching();
    testReorderingStillMatchesResponsesByRequestId();
    testSameSeedProducesIdenticalFaultDecisions();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

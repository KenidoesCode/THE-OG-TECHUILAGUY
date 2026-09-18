// Real assertion-based tests for authenticated RPC message envelopes
// (rpc/auth.cpp) — a genuine integration of 10-cryptography's
// HMAC-SHA256 into Layer 7. Hosted, no hardware dependency.

#include "../rpc/auth.hpp"
#include "../rpc/rpc.hpp"
#include "../../10-cryptography/mac/hmac_sha256.hpp"

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

const std::vector<uint8_t> KEY = toBytes("a shared symmetric RPC key");
const std::vector<uint8_t> WRONG_KEY = toBytes("a completely different key");

}  // namespace

void testAuthenticateThenVerifyRoundTrip() {
    auto message = toBytes("this is an RPC message");
    auto authenticated = dist::authenticateMessage(message, KEY.data(), KEY.size());

    check(authenticated.size() == message.size() + crypto::HMAC_SHA256_OUTPUT_SIZE,
          "auth: an authenticated message is exactly the original length "
          "plus one 32-byte HMAC-SHA256 tag");

    std::vector<uint8_t> extracted;
    bool ok = dist::verifyAndExtractMessage(authenticated, KEY.data(), KEY.size(), extracted);

    check(ok, "auth: verification succeeds with the correct key");
    check(extracted == message,
          "auth: the extracted message exactly matches the original before authentication");
}

void testVerificationFailsWithWrongKey() {
    auto message = toBytes("secret instructions");
    auto authenticated = dist::authenticateMessage(message, KEY.data(), KEY.size());

    std::vector<uint8_t> extracted;
    bool ok = dist::verifyAndExtractMessage(
        authenticated, WRONG_KEY.data(), WRONG_KEY.size(), extracted
    );

    check(!ok, "auth: verification fails when the wrong key is used");
}

void testVerificationFailsForTamperedMessage() {
    auto message = toBytes("transfer 100 credits to account A");
    auto authenticated = dist::authenticateMessage(message, KEY.data(), KEY.size());

    // Simulate an attacker (without the key) modifying the message
    // content in transit — flip one byte within the message portion,
    // leaving the (now-stale) tag untouched.
    authenticated[5] = static_cast<uint8_t>(authenticated[5] ^ 0xFF);

    std::vector<uint8_t> extracted;
    bool ok = dist::verifyAndExtractMessage(authenticated, KEY.data(), KEY.size(), extracted);

    check(!ok,
          "auth: a single tampered byte in the message is detected — "
          "the tag no longer matches the (modified) content");
}

void testVerificationFailsForTamperedTag() {
    auto message = toBytes("some message");
    auto authenticated = dist::authenticateMessage(message, KEY.data(), KEY.size());

    // Tamper with the tag itself instead of the message — an attacker
    // trying to forge a different tag for the original message
    // without knowing the key.
    authenticated.back() = static_cast<uint8_t>(authenticated.back() ^ 0xFF);

    std::vector<uint8_t> extracted;
    bool ok = dist::verifyAndExtractMessage(authenticated, KEY.data(), KEY.size(), extracted);

    check(!ok, "auth: a tampered tag (message left intact) is also detected");
}

void testVerificationFailsForTruncatedInput() {
    std::vector<uint8_t> tooShort(10, 0);  // shorter than even one HMAC tag (32 bytes)

    std::vector<uint8_t> extracted;
    bool ok = dist::verifyAndExtractMessage(tooShort, KEY.data(), KEY.size(), extracted);

    check(!ok,
          "auth: input too short to even contain a tag is rejected, "
          "not treated as an empty message with a missing/zero tag");
}

void testEndToEndAuthenticatedRpcRequestRoundTrip() {
    // A realistic pipeline: build a real RpcRequest, serialize it
    // (rpc.hpp), authenticate the serialized bytes (auth.hpp), then —
    // on the "receiving" side — verify+extract before handing the
    // recovered bytes back to parseRequest. This is what wiring
    // authentication into the RPC layer's actual message flow would
    // look like, exercised end to end even though RpcClient/RpcServer
    // don't call this by default yet (see docs/ADR/0012-authenticated-rpc.md).
    dist::RpcRequest original{99, "transfer", toBytes("100 credits")};
    auto serialized = dist::serializeRequest(original);
    auto onWire = dist::authenticateMessage(serialized, KEY.data(), KEY.size());

    std::vector<uint8_t> recoveredSerialized;
    bool authOk = dist::verifyAndExtractMessage(
        onWire, KEY.data(), KEY.size(), recoveredSerialized
    );
    check(authOk, "auth e2e: an authenticated, serialized RpcRequest verifies correctly");

    dist::RpcRequest recovered;
    bool parseOk = dist::parseRequest(recoveredSerialized, recovered);
    check(parseOk && recovered.id == 99 && recovered.method == "transfer",
          "auth e2e: after successful authentication, the underlying "
          "RpcRequest still parses correctly and matches the original");
}

void testEndToEndRejectsForgedRequestWithoutKey() {
    // An attacker without the key cannot forge a valid authenticated
    // request even if they know the exact wire format (rpc.hpp's
    // serialization is not itself secret) — they can construct a
    // syntactically perfect RpcRequest, but they cannot produce a tag
    // that verifies against a key they don't have.
    dist::RpcRequest forged{1, "transfer", toBytes("1000000 credits")};
    auto serialized = dist::serializeRequest(forged);

    // The attacker guesses/fabricates 32 arbitrary tag bytes, since
    // they cannot compute the real HMAC without the key.
    std::vector<uint8_t> fakeAuthenticated = serialized;
    for (int i = 0; i < 32; ++i) fakeAuthenticated.push_back(static_cast<uint8_t>(i));

    std::vector<uint8_t> extracted;
    bool ok = dist::verifyAndExtractMessage(
        fakeAuthenticated, KEY.data(), KEY.size(), extracted
    );

    check(!ok,
          "auth e2e: a syntactically valid but unauthenticated (forged) "
          "request is rejected — knowing the wire format alone is not "
          "enough to forge a valid message without the shared key");
}

int main() {
    testAuthenticateThenVerifyRoundTrip();
    testVerificationFailsWithWrongKey();
    testVerificationFailsForTamperedMessage();
    testVerificationFailsForTamperedTag();
    testVerificationFailsForTruncatedInput();
    testEndToEndAuthenticatedRpcRequestRoundTrip();
    testEndToEndRejectsForgedRequestWithoutKey();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

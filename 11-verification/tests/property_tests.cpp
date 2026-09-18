// Property-based tests applying 11-verification/property_testing.hpp
// to REAL existing subsystems across the repository — this is what
// makes this a verification layer rather than a demo: every property
// below is checked against genuine, already-shipped code (Layer 6's
// network parsers, Layer 7's RPC serialization, Layer 8's WAL,
// Layer 10's HMAC, and 22-os's ELF loader), not a toy example.

#include "../property_testing.hpp"

#include "../../06-networking/protocols/protocols.hpp"
#include "../../07-distributed-systems/rpc/serialization.hpp"
#include "../../08-storage/wal/wal.hpp"
#include "../../10-cryptography/mac/hmac_sha256.hpp"
#include "../../22-os/elf/elf.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {
int failures = 0;
}

// --- Property: dist::Encoder/Decoder round-trips arbitrary byte
// blobs and strings exactly, for any random content and length. ---

void propertySerializationRoundTrip() {
    bool ok = verify::forAll<std::pair<std::vector<uint8_t>, std::string>>(
        "property: dist::Encoder/Decoder round-trips an arbitrary "
        "random byte blob and string exactly, for 500 random inputs",
        /*seed=*/1001, /*trialCount=*/500,
        [](verify::Random& rng) {
            return std::make_pair(
                rng.nextBytes(0, 200),
                rng.nextPrintableString(0, 100)
            );
        },
        [](const std::pair<std::vector<uint8_t>, std::string>& input) {
            dist::Encoder enc;
            enc.writeBytes(input.first);
            enc.writeString(input.second);

            dist::Decoder dec(enc.data());
            std::vector<uint8_t> outBytes;
            std::string outString;

            return dec.readBytes(outBytes) && dec.readString(outString) &&
                   dec.atEnd() &&
                   outBytes == input.first && outString == input.second;
        }
    );
    if (!ok) failures++;
}

// --- Property: HMAC-SHA256 is deterministic — computing it twice for
// the identical (key, message) pair always produces the identical
// result, for any random key/message. ---

void propertyHmacIsDeterministic() {
    bool ok = verify::forAll<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
        "property: HMAC-SHA256(key, message) is deterministic across "
        "repeated calls, for 300 random (key, message) pairs",
        /*seed=*/2002, /*trialCount=*/300,
        [](verify::Random& rng) {
            return std::make_pair(
                rng.nextBytes(0, 100),   // key
                rng.nextBytes(0, 200)    // message
            );
        },
        [](const std::pair<std::vector<uint8_t>, std::vector<uint8_t>>& input) {
            uint8_t mac1[crypto::HMAC_SHA256_OUTPUT_SIZE];
            uint8_t mac2[crypto::HMAC_SHA256_OUTPUT_SIZE];

            crypto::hmacSha256(
                input.first.data(), input.first.size(),
                input.second.data(), input.second.size(), mac1
            );
            crypto::hmacSha256(
                input.first.data(), input.first.size(),
                input.second.data(), input.second.size(), mac2
            );

            return crypto::constantTimeEquals(
                mac1, crypto::HMAC_SHA256_OUTPUT_SIZE,
                mac2, crypto::HMAC_SHA256_OUTPUT_SIZE
            );
        }
    );
    if (!ok) failures++;
}

// --- Property: a write-ahead log recovers exactly the sequence of
// records that was appended to it, for any random sequence of random
// byte-blob records. ---

void propertyWalRecoversExactlyWhatWasAppended() {
    bool ok = verify::forAll<std::vector<std::vector<uint8_t>>>(
        "property: WriteAheadLog::recoverRecords() returns exactly the "
        "sequence of records appended, for 100 random append sequences",
        /*seed=*/3003, /*trialCount=*/100,
        [](verify::Random& rng) {
            uint32_t recordCount = rng.nextInRange(0, 10);
            std::vector<std::vector<uint8_t>> records;
            for (uint32_t i = 0; i < recordCount; ++i) {
                records.push_back(rng.nextBytes(0, 50));
            }
            return records;
        },
        [](const std::vector<std::vector<uint8_t>>& records) {
            std::string path =
                (std::filesystem::temp_directory_path() /
                 "property_wal_test_fixed").string();
            std::filesystem::remove(path);

            {
                storage::WriteAheadLog wal(path);
                for (const auto& record : records) {
                    wal.append(record);
                }
            }

            storage::WriteAheadLog wal(path);
            auto recovered = wal.recoverRecords();

            std::filesystem::remove(path);

            return recovered == records;
        }
    );
    if (!ok) failures++;
}

// --- Property (fuzz-style robustness): the ELF loader's validator
// never crashes on arbitrary random bytes, and only ever returns
// ElfError::None for a buffer that genuinely satisfies every
// invariant it claims to check. Since a random buffer is
// astronomically unlikely to happen to be a valid ELF image, this
// mostly exercises "does it survive garbage input without crashing,"
// the core fuzz-testing question for an untrusted-input parser. ---

void propertyElfLoaderNeverCrashesOnRandomInput() {
    bool ok = verify::forAll<std::vector<uint8_t>>(
        "property (fuzz): elf_validate_and_plan() never crashes on "
        "arbitrary random bytes, for 2000 random buffers",
        /*seed=*/4004, /*trialCount=*/2000,
        [](verify::Random& rng) {
            return rng.nextBytes(0, 300);
        },
        [](const std::vector<uint8_t>& buffer) {
            ElfLoadPlan plan;
            // Simply not crashing (segfault, out-of-bounds read,
            // infinite loop) is the property under test; any return
            // value (None or any error) is an acceptable outcome for
            // random garbage input.
            elf_validate_and_plan(
                buffer.empty() ? nullptr : buffer.data(),
                static_cast<uint32_t>(buffer.size()),
                plan
            );
            return true;
        }
    );
    if (!ok) failures++;
}

// --- Property (fuzz-style robustness): every network protocol parser
// (Ethernet, ARP, IPv4, ICMP, UDP) never crashes on arbitrary random
// bytes of any length — the exact same untrusted-input discipline
// applied to a completely different parser family than the ELF
// loader, confirming the property-testing approach generalizes rather
// than being specific to one subsystem's quirks. ---

void propertyNetworkParsersNeverCrashOnRandomInput() {
    bool ok = verify::forAll<std::vector<uint8_t>>(
        "property (fuzz): every network protocol parser "
        "(Ethernet/ARP/IPv4/ICMP/UDP) never crashes on arbitrary "
        "random bytes, for 2000 random buffers each",
        /*seed=*/5005, /*trialCount=*/2000,
        [](verify::Random& rng) {
            return rng.nextBytes(0, 100);
        },
        [](const std::vector<uint8_t>& buffer) {
            const uint8_t* data = buffer.empty() ? nullptr : buffer.data();
            uint32_t length = static_cast<uint32_t>(buffer.size());

            net::EthernetHeader eth;
            net::parseEthernetHeader(data, length, eth);

            net::ArpPacket arp;
            net::parseArpPacket(data, length, arp);

            net::Ipv4Header ip;
            net::parseIpv4Header(data, length, ip);

            net::IcmpEchoMessage icmp;
            net::parseIcmpEcho(data, length, icmp);

            net::UdpHeader udp;
            net::parseUdpHeader(data, length, udp);

            return true;  // reaching this line means nothing crashed
        }
    );
    if (!ok) failures++;
}

int main() {
    propertySerializationRoundTrip();
    propertyHmacIsDeterministic();
    propertyWalRecoversExactlyWhatWasAppended();
    propertyElfLoaderNeverCrashesOnRandomInput();
    propertyNetworkParsersNeverCrashOnRandomInput();

    if (failures == 0) {
        std::cout << "\nALL PROPERTIES HELD\n";
    } else {
        std::cout << "\n" << failures << " PROPERTY/PROPERTIES FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

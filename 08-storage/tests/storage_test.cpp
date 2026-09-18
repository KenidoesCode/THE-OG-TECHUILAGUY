// Real assertion-based tests for Layer 8 (Storage): CRC-32, the
// write-ahead log, and the KV store built on it. Real file I/O against
// a temporary directory (removed at the end of the run), including
// genuine crash/corruption simulation by directly truncating or
// bit-flipping WAL files on disk — the same technique
// 13-developer-ecosystem's OGGit object-store test already uses for
// its own corruption test.

#include "../kv/kv_store.hpp"
#include "../wal/crc32.hpp"
#include "../wal/wal.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
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

std::string tempPath(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("storage_test_" + name);
    std::filesystem::remove(path);
    return path.string();
}

void appendRawBytes(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::app | std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

// --- CRC-32 ---

void testCrc32MatchesStandardCheckValue() {
    // The standard CRC-32 (IEEE 802.3) "check value": the CRC of the
    // nine ASCII bytes "123456789" is defined to be 0xCBF43926 —
    // exactly the same kind of known-answer verification used for
    // SHA-256 in 10-cryptography/.
    const uint8_t input[] = {'1','2','3','4','5','6','7','8','9'};
    uint32_t result = storage::crc32(input, sizeof(input));
    check(result == 0xCBF43926u,
          "crc32: matches the standard's own published check value for \"123456789\"");
}

void testCrc32DetectsSingleByteChange() {
    std::string a = "hello world";
    std::string b = "hello worle";  // last byte changed

    uint32_t crcA = storage::crc32(reinterpret_cast<const uint8_t*>(a.data()), a.size());
    uint32_t crcB = storage::crc32(reinterpret_cast<const uint8_t*>(b.data()), b.size());

    check(crcA != crcB, "crc32: a single changed byte produces a different checksum");
}

void testCrc32OfEmptyInputIsZero() {
    uint32_t result = storage::crc32(nullptr, 0);
    check(result == 0, "crc32: the checksum of zero bytes is 0 (the standard's defined identity)");
}

// --- Write-ahead log ---

void testWalAppendAndRecover() {
    std::string path = tempPath("wal_basic");

    {
        storage::WriteAheadLog wal(path);
        wal.append(toBytes("record one"));
        wal.append(toBytes("record two"));
        wal.append(toBytes("record three"));
    }

    storage::WriteAheadLog wal(path);
    auto records = wal.recoverRecords();

    check(records.size() == 3, "wal: all three appended records are recovered");
    check(std::string(records[0].begin(), records[0].end()) == "record one" &&
          std::string(records[1].begin(), records[1].end()) == "record two" &&
          std::string(records[2].begin(), records[2].end()) == "record three",
          "wal: recovered records are in append order with correct content");

    std::filesystem::remove(path);
}

void testWalRecoverFromNonexistentFileReturnsEmpty() {
    std::string path = tempPath("wal_never_created");
    std::filesystem::remove(path);

    storage::WriteAheadLog wal(path);  // constructor creates the (empty) file
    auto records = wal.recoverRecords();

    check(records.empty(), "wal: a freshly created log recovers zero records");

    std::filesystem::remove(path);
}

void testWalSurvivesReopenAcrossMultipleSessions() {
    std::string path = tempPath("wal_multisession");

    { storage::WriteAheadLog wal(path); wal.append(toBytes("first session")); }
    { storage::WriteAheadLog wal(path); wal.append(toBytes("second session")); }
    { storage::WriteAheadLog wal(path); wal.append(toBytes("third session")); }

    storage::WriteAheadLog wal(path);
    auto records = wal.recoverRecords();

    check(records.size() == 3,
          "wal: records appended across three separate open/close sessions "
          "are all still present and recoverable");

    std::filesystem::remove(path);
}

void testWalRecoveryStopsAtTruncatedTrailingRecord() {
    std::string path = tempPath("wal_truncated");

    storage::WriteAheadLog wal(path);
    wal.append(toBytes("good record one"));
    wal.append(toBytes("good record two"));

    // Simulate a crash mid-write: append a length field claiming a
    // large record, but never actually write its crc32 or payload —
    // exactly what a process crashing between writing the length
    // prefix and the rest of the record would leave behind on disk.
    appendRawBytes(path, {0x00, 0x00, 0x10, 0x00});  // length = 4096, nothing follows

    storage::WriteAheadLog wal2(path);
    auto records = wal2.recoverRecords();

    check(records.size() == 2,
          "wal: recovery returns exactly the records written before a "
          "simulated crash (a torn trailing record), not fewer and not "
          "a crash/exception from trying to read past it");
    check(std::string(records[0].begin(), records[0].end()) == "good record one" &&
          std::string(records[1].begin(), records[1].end()) == "good record two",
          "wal: the two records preceding the simulated crash have "
          "fully intact, correct content");

    std::filesystem::remove(path);
}

void testWalRecoveryStopsAtCorruptedRecordChecksum() {
    std::string path = tempPath("wal_corrupted");

    storage::WriteAheadLog wal(path);
    wal.append(toBytes("intact record"));
    wal.append(toBytes("record to corrupt"));
    wal.append(toBytes("record after corruption"));

    // Flip a byte inside the SECOND record's payload region without
    // touching its length/crc header — simulates bit rot or a
    // media-level corruption, not merely a truncated write.
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        // First record: 4(len)+4(crc)+13 bytes("intact record") = 21 bytes.
        // Second record's payload starts 4+4 bytes into its own
        // header, i.e. at offset 21 + 8 = 29.
        f.seekp(29, std::ios::beg);
        f.put('X');
    }

    storage::WriteAheadLog wal2(path);
    auto records = wal2.recoverRecords();

    check(records.size() == 1,
          "wal: recovery stops at the first record whose checksum no "
          "longer matches its (corrupted) content, discarding it and "
          "everything after it rather than trusting a corrupted middle "
          "record or skipping past it to reach later ones");
    check(std::string(records[0].begin(), records[0].end()) == "intact record",
          "wal: the one record genuinely before the corruption point is fully intact");

    std::filesystem::remove(path);
}

void testWalClearTruncatesToEmpty() {
    std::string path = tempPath("wal_clear");

    storage::WriteAheadLog wal(path);
    wal.append(toBytes("will be cleared"));
    wal.clear();

    auto records = wal.recoverRecords();
    check(records.empty(), "wal: clear() truncates the log; recovery afterward finds nothing");

    std::filesystem::remove(path);
}

// --- KV store ---

void testKVStorePutGetRoundTrip() {
    std::string path = tempPath("kv_basic");
    storage::KVStore kv(path);

    kv.put("name", toBytes("techuilaguy"));

    std::vector<uint8_t> value;
    bool found = kv.get("name", value);

    check(found && std::string(value.begin(), value.end()) == "techuilaguy",
          "kv store: put() then get() round-trips the value correctly");

    std::filesystem::remove(path);
}

void testKVStoreDeleteRemovesKey() {
    std::string path = tempPath("kv_delete");
    storage::KVStore kv(path);

    kv.put("temp", toBytes("value"));
    check(kv.contains("temp"), "kv store: key exists after put()");

    kv.remove("temp");
    check(!kv.contains("temp"), "kv store: key no longer exists after remove()");

    std::filesystem::remove(path);
}

void testKVStoreSurvivesRestartWithFullState() {
    std::string path = tempPath("kv_restart");

    {
        storage::KVStore kv(path);
        kv.put("a", toBytes("1"));
        kv.put("b", toBytes("2"));
        kv.put("c", toBytes("3"));
        kv.remove("b");
    }

    // A fresh KVStore instance over the same WAL path — simulating a
    // full process restart — must recover the exact same logical
    // state by replaying the log, not an in-memory object that
    // happened to survive.
    storage::KVStore kv(path);

    std::vector<uint8_t> value;
    check(kv.get("a", value) && std::string(value.begin(), value.end()) == "1",
          "kv store: key 'a' survives a simulated restart with the correct value");
    check(!kv.contains("b"),
          "kv store: key 'b' (put then deleted before restart) correctly "
          "stays deleted after restart — the delete record is replayed too");
    check(kv.get("c", value) && std::string(value.begin(), value.end()) == "3",
          "kv store: key 'c' survives a simulated restart with the correct value");
    check(kv.size() == 2, "kv store: size() after restart reflects exactly the surviving keys");

    std::filesystem::remove(path);
}

void testKVStoreRepeatedRestartsAccumulateCorrectly() {
    std::string path = tempPath("kv_repeated_restart");

    for (int session = 0; session < 5; ++session) {
        storage::KVStore kv(path);
        kv.put("counter_session_" + std::to_string(session), toBytes("present"));
    }

    storage::KVStore kv(path);
    bool allPresent = true;
    for (int session = 0; session < 5; ++session) {
        if (!kv.contains("counter_session_" + std::to_string(session))) {
            allPresent = false;
        }
    }

    check(allPresent && kv.size() == 5,
          "kv store: five separate open/write/close sessions each "
          "contribute correctly to the final recovered state after a "
          "sixth, fresh open");

    std::filesystem::remove(path);
}

void testKVStoreOverwriteReflectsLatestValue() {
    std::string path = tempPath("kv_overwrite");

    storage::KVStore kv(path);
    kv.put("key", toBytes("first"));
    kv.put("key", toBytes("second"));
    kv.put("key", toBytes("third"));

    std::vector<uint8_t> value;
    kv.get("key", value);
    check(std::string(value.begin(), value.end()) == "third",
          "kv store: multiple puts to the same key reflect only the latest value");

    std::filesystem::remove(path);
}

void testKVStoreRecoversPartialStateAfterSimulatedCrash() {
    std::string path = tempPath("kv_crash_recovery");

    {
        storage::KVStore kv(path);
        kv.put("safe1", toBytes("committed"));
        kv.put("safe2", toBytes("committed"));
    }

    // Simulate a crash while a third write was in progress: append a
    // torn record directly to the WAL file, bypassing KVStore/WAL's
    // own append() (which always writes complete records) — the same
    // technique testWalRecoveryStopsAtTruncatedTrailingRecord uses,
    // applied here to prove the KV layer built on top inherits the
    // WAL's crash-safety rather than losing it somewhere in between.
    appendRawBytes(path, {0x00, 0x00, 0x00, 0x05, 0xAB, 0xCD});  // claims a 5-byte payload, provides none

    storage::KVStore recovered(path);

    check(recovered.contains("safe1") && recovered.contains("safe2"),
          "kv store: both writes committed before a simulated crash are "
          "fully recovered");
    check(recovered.size() == 2,
          "kv store: the torn in-flight write from the simulated crash "
          "contributes nothing to recovered state — not a partial key, "
          "not a crash, just cleanly absent");

    std::filesystem::remove(path);
}

int main() {
    testCrc32MatchesStandardCheckValue();
    testCrc32DetectsSingleByteChange();
    testCrc32OfEmptyInputIsZero();
    testWalAppendAndRecover();
    testWalRecoverFromNonexistentFileReturnsEmpty();
    testWalSurvivesReopenAcrossMultipleSessions();
    testWalRecoveryStopsAtTruncatedTrailingRecord();
    testWalRecoveryStopsAtCorruptedRecordChecksum();
    testWalClearTruncatesToEmpty();
    testKVStorePutGetRoundTrip();
    testKVStoreDeleteRemovesKey();
    testKVStoreSurvivesRestartWithFullState();
    testKVStoreRepeatedRestartsAccumulateCorrectly();
    testKVStoreOverwriteReflectsLatestValue();
    testKVStoreRecoversPartialStateAfterSimulatedCrash();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}

#pragma once

#include <cstdint>
#include <cstddef>

// A real CRC-32 (IEEE 802.3 polynomial, reflected, the same variant
// used by Ethernet, gzip, and PNG) — used by the write-ahead log
// (wal.cpp) to detect a torn or corrupted record after an unclean
// shutdown. Verified against the standard's own published check value
// (see docs/ADR/0010-storage-wal.md and crc32_test.cpp): CRC32 of the
// nine ASCII bytes "123456789" must equal 0xCBF43926.

namespace storage {

uint32_t crc32(const uint8_t* data, size_t length);

}  // namespace storage

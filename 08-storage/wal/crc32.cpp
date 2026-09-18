#include "crc32.hpp"

namespace storage {

namespace {

uint32_t table[256];
bool tableInitialized = false;

void initTable() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    tableInitialized = true;
}

}  // namespace

uint32_t crc32(const uint8_t* data, size_t length) {
    if (!tableInitialized) {
        initTable();
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

}  // namespace storage

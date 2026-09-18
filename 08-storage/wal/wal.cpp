#include "wal.hpp"
#include "crc32.hpp"

#include <fstream>

namespace storage {

namespace {

void writeU32(std::ofstream& out, uint32_t value) {
    uint8_t bytes[4] = {
        static_cast<uint8_t>((value >> 24) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), 4);
}

bool readU32(std::ifstream& in, uint32_t& out) {
    uint8_t bytes[4];
    in.read(reinterpret_cast<char*>(bytes), 4);
    if (in.gcount() != 4) {
        return false;
    }
    out = (static_cast<uint32_t>(bytes[0]) << 24) |
          (static_cast<uint32_t>(bytes[1]) << 16) |
          (static_cast<uint32_t>(bytes[2]) << 8) |
          static_cast<uint32_t>(bytes[3]);
    return true;
}

}  // namespace

WriteAheadLog::WriteAheadLog(std::string path) : path(std::move(path)) {
    // Ensure the file exists (open-for-append creates it if missing,
    // then immediately closes) without disturbing any existing
    // content — a fresh WriteAheadLog over a file from a prior run
    // must see that run's records on the next recoverRecords() call.
    std::ofstream touch(this->path, std::ios::app | std::ios::binary);
}

bool WriteAheadLog::append(const std::vector<uint8_t>& payload) {
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        return false;
    }

    uint32_t crc = crc32(payload.data(), payload.size());

    writeU32(out, static_cast<uint32_t>(payload.size()));
    writeU32(out, crc);
    out.write(reinterpret_cast<const char*>(payload.data()),
              static_cast<std::streamsize>(payload.size()));

    out.flush();
    return out.good();
}

std::vector<std::vector<uint8_t>> WriteAheadLog::recoverRecords() const {
    std::vector<std::vector<uint8_t>> records;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return records;
    }

    while (true) {
        uint32_t length;
        if (!readU32(in, length)) {
            break;  // clean EOF, or a torn write mid-length-field: stop either way
        }

        uint32_t storedCrc;
        if (!readU32(in, storedCrc)) {
            break;  // torn write: length was written but crc wasn't
        }

        std::vector<uint8_t> payload(length);
        if (length > 0) {
            in.read(reinterpret_cast<char*>(payload.data()),
                    static_cast<std::streamsize>(length));
            if (static_cast<uint32_t>(in.gcount()) != length) {
                break;  // torn write: fewer payload bytes were ever written than claimed
            }
        }

        uint32_t actualCrc = crc32(payload.data(), payload.size());
        if (actualCrc != storedCrc) {
            // Corruption (bit rot, or a genuinely torn write that
            // happened to leave a full-length but garbage payload).
            // Stop here — this record and anything after it in the
            // file cannot be trusted to represent committed writes.
            break;
        }

        records.push_back(std::move(payload));
    }

    return records;
}

void WriteAheadLog::clear() {
    std::ofstream out(path, std::ios::trunc | std::ios::binary);
}

}  // namespace storage

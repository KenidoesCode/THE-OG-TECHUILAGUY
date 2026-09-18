#include "sha256.hpp"

namespace crypto {

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

uint32_t readU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

void writeU32BE(uint8_t* p, uint32_t value) {
    p[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(value & 0xFF);
}

void writeU64BE(uint8_t* p, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<uint8_t>((value >> (56 - 8 * i)) & 0xFF);
    }
}

}  // namespace

Sha256::Sha256() {
    reset();
}

void Sha256::reset() {
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;

    bufferLength = 0;
    totalLength = 0;
}

void Sha256::processBlock(const uint8_t* block) {
    uint32_t w[64];

    for (int i = 0; i < 16; ++i) {
        w[i] = readU32BE(block + i * 4);
    }

    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void Sha256::update(const uint8_t* data, size_t length) {
    totalLength += length;

    while (length > 0) {
        uint32_t toCopy = SHA256_BLOCK_SIZE - bufferLength;
        if (toCopy > length) {
            toCopy = static_cast<uint32_t>(length);
        }

        for (uint32_t i = 0; i < toCopy; ++i) {
            buffer[bufferLength + i] = data[i];
        }

        bufferLength += toCopy;
        data += toCopy;
        length -= toCopy;

        if (bufferLength == SHA256_BLOCK_SIZE) {
            processBlock(buffer);
            bufferLength = 0;
        }
    }
}

void Sha256::finish(uint8_t* digestOut) {
    uint64_t bitLength = totalLength * 8;

    uint8_t padByte = 0x80;
    update(&padByte, 1);

    uint8_t zero = 0x00;
    // Pad with zeros until exactly 8 bytes (the 64-bit length field)
    // remain to fill out a full block. update() may have already
    // flushed a block via the 0x80 byte above, so recompute how many
    // zero bytes are needed from the current buffer position.
    while (bufferLength != SHA256_BLOCK_SIZE - 8) {
        update(&zero, 1);
    }

    // Note: update() above kept incrementing totalLength, but the
    // length field appended to the message must be the length of the
    // *original* message only, not including this padding — bitLength
    // was captured before any padding was added, so that's what gets
    // written here regardless of totalLength's current value.
    uint8_t lengthBytes[8];
    writeU64BE(lengthBytes, bitLength);
    for (int i = 0; i < 8; ++i) {
        buffer[bufferLength + i] = lengthBytes[i];
    }
    bufferLength += 8;
    processBlock(buffer);
    bufferLength = 0;

    for (int i = 0; i < 8; ++i) {
        writeU32BE(digestOut + i * 4, state[i]);
    }
}

void sha256(const uint8_t* data, size_t length, uint8_t* digestOut) {
    Sha256 hasher;
    hasher.update(data, length);
    hasher.finish(digestOut);
}

}  // namespace crypto

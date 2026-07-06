#include "sha256.h"
#include <cstring>

namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

struct Ctx {
    uint32_t h[8];
    uint64_t bitlen;
    uint8_t buf[64];
    size_t buflen;
};

void init(Ctx& c) {
    c.h[0] = 0x6a09e667; c.h[1] = 0xbb67ae85; c.h[2] = 0x3c6ef372; c.h[3] = 0xa54ff53a;
    c.h[4] = 0x510e527f; c.h[5] = 0x9b05688c; c.h[6] = 0x1f83d9ab; c.h[7] = 0x5be0cd19;
    c.bitlen = 0; c.buflen = 0;
}

void transform(Ctx& c, const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
               (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3];
    uint32_t e = c.h[4], f = c.h[5], g = c.h[6], h = c.h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c.h[0] += a; c.h[1] += b; c.h[2] += cc; c.h[3] += d;
    c.h[4] += e; c.h[5] += f; c.h[6] += g; c.h[7] += h;
}

void update(Ctx& c, const uint8_t* data, size_t len) {
    c.bitlen += uint64_t(len) * 8;
    while (len > 0) {
        size_t take = 64 - c.buflen;
        if (take > len) take = len;
        memcpy(c.buf + c.buflen, data, take);
        c.buflen += take; data += take; len -= take;
        if (c.buflen == 64) { transform(c, c.buf); c.buflen = 0; }
    }
}

void final(Ctx& c, uint8_t out[32]) {
    uint64_t bl = c.bitlen;            // total message length in bits
    c.buf[c.buflen++] = 0x80;          // append the '1' bit
    if (c.buflen > 56) {               // not enough room for the length field
        while (c.buflen < 64) c.buf[c.buflen++] = 0x00;
        transform(c, c.buf);
        c.buflen = 0;
    }
    while (c.buflen < 56) c.buf[c.buflen++] = 0x00;
    for (int i = 0; i < 8; i++) c.buf[56 + i] = uint8_t(bl >> ((7 - i) * 8));
    transform(c, c.buf);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = uint8_t(c.h[i] >> 24);
        out[i * 4 + 1] = uint8_t(c.h[i] >> 16);
        out[i * 4 + 2] = uint8_t(c.h[i] >> 8);
        out[i * 4 + 3] = uint8_t(c.h[i]);
    }
}

} // namespace

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    Ctx c; init(c); update(c, data, len); final(c, out);
}

void sha256d(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint8_t tmp[32];
    sha256(data, len, tmp);
    sha256(tmp, 32, out);
}

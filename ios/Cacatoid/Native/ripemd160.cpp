#include "ripemd160.h"
#include <cstring>

namespace {

inline uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

inline uint32_t f1(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
inline uint32_t f2(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
inline uint32_t f3(uint32_t x, uint32_t y, uint32_t z) { return (x | ~y) ^ z; }
inline uint32_t f4(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
inline uint32_t f5(uint32_t x, uint32_t y, uint32_t z) { return x ^ (y | ~z); }

// Message word selection for left and right lines.
const int rL[80] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,
    3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,
    1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,
    4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13 };
const int rR[80] = {
    5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,
    6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,
    15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,
    8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,
    12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11 };
const int sL[80] = {
    11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,
    7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,
    11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,
    11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,
    9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6 };
const int sR[80] = {
    8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,
    9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,
    9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,
    15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,
    8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11 };
const uint32_t KL[5] = { 0x00000000, 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xa953fd4e };
const uint32_t KR[5] = { 0x50a28be6, 0x5c4dd124, 0x6d703ef3, 0x7a6d76e9, 0x00000000 };

inline uint32_t ff(int round, uint32_t x, uint32_t y, uint32_t z) {
    switch (round) {
        case 0: return f1(x, y, z);
        case 1: return f2(x, y, z);
        case 2: return f3(x, y, z);
        case 3: return f4(x, y, z);
        default: return f5(x, y, z);
    }
}

void transform(uint32_t h[5], const uint8_t* block) {
    uint32_t x[16];
    for (int i = 0; i < 16; i++) {
        x[i] = uint32_t(block[i * 4]) | (uint32_t(block[i * 4 + 1]) << 8) |
               (uint32_t(block[i * 4 + 2]) << 16) | (uint32_t(block[i * 4 + 3]) << 24);
    }
    uint32_t al = h[0], bl = h[1], cl = h[2], dl = h[3], el = h[4];
    uint32_t ar = h[0], br = h[1], cr = h[2], dr = h[3], er = h[4];
    for (int i = 0; i < 80; i++) {
        int round = i / 16;
        uint32_t t = rol(al + ff(round, bl, cl, dl) + x[rL[i]] + KL[round], sL[i]) + el;
        al = el; el = dl; dl = rol(cl, 10); cl = bl; bl = t;
        int rr = 4 - round;
        t = rol(ar + ff(rr, br, cr, dr) + x[rR[i]] + KR[round], sR[i]) + er;
        ar = er; er = dr; dr = rol(cr, 10); cr = br; br = t;
    }
    uint32_t tmp = h[1] + cl + dr;
    h[1] = h[2] + dl + er;
    h[2] = h[3] + el + ar;
    h[3] = h[4] + al + br;
    h[4] = h[0] + bl + cr;
    h[0] = tmp;
}

} // namespace

void ripemd160(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
    uint64_t total = len;
    while (len >= 64) { transform(h, data); data += 64; len -= 64; }

    uint8_t buf[64];
    memcpy(buf, data, len);
    buf[len] = 0x80;
    size_t pos = len + 1;
    if (pos > 56) {
        memset(buf + pos, 0, 64 - pos);
        transform(h, buf);
        pos = 0;
        memset(buf, 0, 56);
    } else {
        memset(buf + pos, 0, 56 - pos);
    }
    uint64_t bits = total * 8;
    for (int i = 0; i < 8; i++) buf[56 + i] = uint8_t(bits >> (i * 8));
    transform(h, buf);

    for (int i = 0; i < 5; i++) {
        out[i * 4] = uint8_t(h[i]);
        out[i * 4 + 1] = uint8_t(h[i] >> 8);
        out[i * 4 + 2] = uint8_t(h[i] >> 16);
        out[i * 4 + 3] = uint8_t(h[i] >> 24);
    }
}

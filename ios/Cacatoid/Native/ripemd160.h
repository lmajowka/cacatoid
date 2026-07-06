#ifndef CACATOID_RIPEMD160_H
#define CACATOID_RIPEMD160_H

#include <cstdint>
#include <cstddef>

// Computes RIPEMD-160 of `len` bytes at `data` into the 20-byte `out` buffer.
void ripemd160(const uint8_t* data, size_t len, uint8_t out[20]);

#endif // CACATOID_RIPEMD160_H

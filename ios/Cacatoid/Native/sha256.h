#ifndef CACATOID_SHA256_H
#define CACATOID_SHA256_H

#include <cstdint>
#include <cstddef>

// Computes SHA-256 of `len` bytes at `data` into the 32-byte `out` buffer.
void sha256(const uint8_t* data, size_t len, uint8_t out[32]);

// Double SHA-256 (SHA256(SHA256(data))), used for Base58Check checksums.
void sha256d(const uint8_t* data, size_t len, uint8_t out[32]);

#endif // CACATOID_SHA256_H

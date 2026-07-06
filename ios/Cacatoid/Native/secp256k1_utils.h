#ifndef CACATOID_SECP256K1_UTILS_H
#define CACATOID_SECP256K1_UTILS_H

#include <cstdint>
#include <string>

// Thin RAII-free wrapper around libsecp256k1 for the searcher's needs.
namespace ec {

// Initializes the shared secp256k1 context. Safe to call multiple times.
void init();

// Opaque handle to a public key (large enough for secp256k1_pubkey).
struct PubKey { uint8_t data[64]; };

// Derives the public key for `seckey` (32-byte big-endian). Returns false if
// the secret key is invalid (zero or >= group order).
bool pubkey_create(const uint8_t seckey[32], PubKey& out);

// In-place tweak: pubkey += 1*G. Equivalent to advancing the private key by 1.
// Returns false on the (cryptographically negligible) failure case.
bool pubkey_increment(PubKey& pub);

// Serializes `pub` in 33-byte compressed form into `out`.
void serialize_compressed(const PubKey& pub, uint8_t out[33]);

// Computes HASH160 (RIPEMD160(SHA256(x))) of the compressed pubkey into out[20].
void hash160_compressed(const PubKey& pub, uint8_t out[20]);

} // namespace ec

#endif // CACATOID_SECP256K1_UTILS_H

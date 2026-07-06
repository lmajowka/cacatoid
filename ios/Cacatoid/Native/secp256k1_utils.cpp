#include "secp256k1_utils.h"
#include "sha256.h"
#include "ripemd160.h"

#include <secp256k1.h>
#include <mutex>
#include <cstring>

namespace ec {

namespace {
secp256k1_context* g_ctx = nullptr;
std::once_flag g_once;

// Big-endian 32-byte representation of the scalar 1, used as the tweak.
const uint8_t ONE[32] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1 };
} // namespace

void init() {
    std::call_once(g_once, [] {
        g_ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    });
}

bool pubkey_create(const uint8_t seckey[32], PubKey& out) {
    secp256k1_pubkey pk;
    if (!secp256k1_ec_pubkey_create(g_ctx, &pk, seckey)) return false;
    static_assert(sizeof(pk.data) <= sizeof(out.data), "PubKey buffer too small");
    memcpy(out.data, pk.data, sizeof(pk.data));
    return true;
}

bool pubkey_increment(PubKey& pub) {
    secp256k1_pubkey pk;
    memcpy(pk.data, pub.data, sizeof(pk.data));
    if (!secp256k1_ec_pubkey_tweak_add(g_ctx, &pk, ONE)) return false;
    memcpy(pub.data, pk.data, sizeof(pk.data));
    return true;
}

void serialize_compressed(const PubKey& pub, uint8_t out[33]) {
    secp256k1_pubkey pk;
    memcpy(pk.data, pub.data, sizeof(pk.data));
    size_t len = 33;
    secp256k1_ec_pubkey_serialize(g_ctx, out, &len, &pk, SECP256K1_EC_COMPRESSED);
}

void hash160_compressed(const PubKey& pub, uint8_t out[20]) {
    uint8_t ser[33];
    serialize_compressed(pub, ser);
    uint8_t sha[32];
    sha256(ser, 33, sha);
    ripemd160(sha, 32, out);
}

} // namespace ec

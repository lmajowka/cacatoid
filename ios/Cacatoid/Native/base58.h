#ifndef CACATOID_BASE58_H
#define CACATOID_BASE58_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Plain Base58 encode of arbitrary bytes.
std::string base58_encode(const uint8_t* data, size_t len);

// Base58Check encode: appends a 4-byte double-SHA256 checksum, then Base58.
std::string base58check_encode(const uint8_t* payload, size_t len);

// Base58Check decode. Returns the payload with version byte but without the
// 4-byte checksum, or an empty vector if the checksum is invalid.
std::vector<uint8_t> base58check_decode(const std::string& str);

#endif // CACATOID_BASE58_H

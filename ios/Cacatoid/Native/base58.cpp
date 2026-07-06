#include "base58.h"
#include "sha256.h"
#include <cstring>

namespace {
const char* ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

int8_t decode_char(char c) {
    for (int i = 0; i < 58; i++) if (ALPHABET[i] == c) return int8_t(i);
    return -1;
}
} // namespace

std::string base58_encode(const uint8_t* data, size_t len) {
    // Count leading zero bytes -> leading '1's.
    size_t zeros = 0;
    while (zeros < len && data[zeros] == 0) zeros++;

    // Convert base-256 to base-58 via repeated division (big-endian digit buffer).
    std::vector<uint8_t> digits;
    digits.reserve(len * 138 / 100 + 1);
    for (size_t i = zeros; i < len; i++) {
        int carry = data[i];
        for (size_t j = 0; j < digits.size(); j++) {
            carry += int(digits[j]) << 8;
            digits[j] = uint8_t(carry % 58);
            carry /= 58;
        }
        while (carry > 0) {
            digits.push_back(uint8_t(carry % 58));
            carry /= 58;
        }
    }

    std::string out;
    out.reserve(zeros + digits.size());
    for (size_t i = 0; i < zeros; i++) out.push_back('1');
    for (size_t i = digits.size(); i-- > 0;) out.push_back(ALPHABET[digits[i]]);
    return out;
}

std::string base58check_encode(const uint8_t* payload, size_t len) {
    std::vector<uint8_t> buf(payload, payload + len);
    uint8_t check[32];
    sha256d(payload, len, check);
    buf.insert(buf.end(), check, check + 4);
    return base58_encode(buf.data(), buf.size());
}

std::vector<uint8_t> base58check_decode(const std::string& str) {
    size_t zeros = 0;
    while (zeros < str.size() && str[zeros] == '1') zeros++;

    std::vector<uint8_t> bytes; // big-endian byte buffer
    for (size_t i = zeros; i < str.size(); i++) {
        int8_t v = decode_char(str[i]);
        if (v < 0) return {};
        int carry = v;
        for (size_t j = 0; j < bytes.size(); j++) {
            carry += int(bytes[j]) * 58;
            bytes[j] = uint8_t(carry & 0xff);
            carry >>= 8;
        }
        while (carry > 0) {
            bytes.push_back(uint8_t(carry & 0xff));
            carry >>= 8;
        }
    }

    std::vector<uint8_t> result;
    result.reserve(zeros + bytes.size());
    for (size_t i = 0; i < zeros; i++) result.push_back(0);
    for (size_t i = bytes.size(); i-- > 0;) result.push_back(bytes[i]);

    if (result.size() < 4) return {};
    uint8_t check[32];
    sha256d(result.data(), result.size() - 4, check);
    if (memcmp(check, result.data() + result.size() - 4, 4) != 0) return {};
    result.resize(result.size() - 4); // strip checksum, keep version + payload
    return result;
}

#include "searcher_bridge.h"
#include "secp256k1_utils.h"
#include "base58.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

using u128 = unsigned __int128;
using Clock = std::chrono::steady_clock;

namespace {

struct Target { int puzzle; const char* addr; };
const Target TARGETS[] = {
    { 20, "1HsMJxNiV7TLxmoF6uJNkydxPFDog4NQum" },
    { 71, "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU" },
    { 72, "1JTK7s9YVYywfm5XUH7RNhHJH1LshCaRFR" },
    { 73, "12VVRNPi4SJqUTsp6FmqDqY5sGosDtysn4" },
};

const char* target_address(int puzzle) {
    for (const auto& t : TARGETS) if (t.puzzle == puzzle) return t.addr;
    return nullptr;
}

constexpr int BATCH = 1000;

std::mutex  g_mutex;
std::vector<std::thread> g_workers;
std::thread g_monitor;

std::atomic<bool>     g_running{false};
std::atomic<bool>     g_found{false};
std::atomic<uint64_t> g_total{0};

// Display key: protected by g_display_mutex so the monitor always reads a
// consistent 128-bit value (no torn read between lo and hi halves).
std::mutex g_display_mutex;
u128       g_display_key{0};

stats_cb g_on_stats = nullptr;
found_cb g_on_found = nullptr;
void*    g_cb_ctx   = nullptr;

u128 g_found_key    = 0;
int  g_active_puzzle = 0;

uint8_t g_targets[3][20];

// ── helpers ──────────────────────────────────────────────────────────────────

void u128_to_key(u128 v, uint8_t key[32]) {
    memset(key, 0, 32);
    for (int i = 0; i < 16; i++) key[31 - i] = uint8_t(v >> (i * 8));
}

static const char HEX[] = "0123456789abcdef";

std::string to_hex(const uint8_t* data, size_t len) {
    std::string s(len * 2, '0');
    for (size_t i = 0; i < len; i++) {
        s[i * 2]     = HEX[data[i] >> 4];
        s[i * 2 + 1] = HEX[data[i] & 0xf];
    }
    return s;
}

// Converts u128 directly to hex with no leading zeros.
// For puzzle-71 keys this always produces exactly 18 chars starting with 4/5/6/7.
std::string u128_to_hex(u128 v) {
    if (v == 0) return "0";
    char buf[33];
    buf[32] = '\0';
    int pos = 32;
    while (v > 0) {
        buf[--pos] = HEX[v & 0xF];
        v >>= 4;
    }
    return std::string(buf + pos, buf + 32);
}

std::string to_wif(const uint8_t key[32]) {
    uint8_t payload[34];
    payload[0] = 0x80;
    memcpy(payload + 1, key, 32);
    payload[33] = 0x01;
    return base58check_encode(payload, 34);
}

std::string to_address(const uint8_t hash160[20]) {
    uint8_t payload[21];
    payload[0] = 0x00;
    memcpy(payload + 1, hash160, 20);
    return base58check_encode(payload, 21);
}

void report_found(u128 key_value, int puzzle) {
    uint8_t key[32];
    u128_to_key(key_value, key);

    ec::PubKey pub;
    ec::pubkey_create(key, pub);
    uint8_t hash[20];
    ec::hash160_compressed(pub, hash);

    std::string priv_hex = to_hex(key, 32);
    std::string wif      = to_wif(key);
    std::string addr     = to_address(hash);

    fprintf(stderr, "MATCH puzzle %d priv=%s addr=%s\n",
            puzzle, priv_hex.c_str(), addr.c_str());

    if (g_on_found && g_cb_ctx)
        g_on_found(priv_hex.c_str(), wif.c_str(), addr.c_str(), puzzle, g_cb_ctx);
}

// ── worker ───────────────────────────────────────────────────────────────────

void worker_run(u128 lo, u128 hi, int puzzle) {
    ec::init();
    // span is always 2^(puzzle-1) — a power of 2 — so bitmask gives exact uniform dist.
    const u128 span = hi - lo + 1;
    const u128 mask = span - 1;

    // Seed from multiple independent sources so threads have distinct sequences
    // even when std::random_device is deterministic (common on jailbroken iOS).
    std::random_device rd;
    uint64_t seed = ((uint64_t)rd() << 32) ^ rd();
    seed ^= (uint64_t)Clock::now().time_since_epoch().count();
    seed ^= (uint64_t)(uintptr_t)&seed; // stack address differs per thread (ASLR)
    std::mt19937_64 rng(seed);

    uint8_t seckey[32];
    uint8_t hash[20];
    uint64_t local = 0;
    u128 cur = lo;

    while (!g_found.load(std::memory_order_relaxed) &&
            g_running.load(std::memory_order_relaxed)) {

        // key = lo + random(0, span-1)
        u128 rand128 = ((u128)rng() << 64) | (u128)rng();
        cur = lo + (rand128 & mask);

        u128_to_key(cur, seckey);
        ec::PubKey pub;
        if (!ec::pubkey_create(seckey, pub)) continue;

        ec::hash160_compressed(pub, hash);

        for (int t = 0; t < 3; t++) {
            if (memcmp(hash, g_targets[t], 20) == 0) {
                g_found_key = cur;
                bool expected = false;
                if (g_found.compare_exchange_strong(expected, true))
                    report_found(cur, puzzle);
                g_total.fetch_add(local + 1, std::memory_order_relaxed);
                return;
            }
        }

        local++;
        if (local >= (uint64_t)BATCH) {
            g_total.fetch_add(local, std::memory_order_relaxed);
            // Write the full 128-bit key atomically so the monitor never sees
            // a torn value (half from this thread, half from another).
            { std::lock_guard<std::mutex> lk(g_display_mutex); g_display_key = cur; }
            local = 0;
        }
    }
    if (local > 0) g_total.fetch_add(local, std::memory_order_relaxed);
}

// ── monitor ──────────────────────────────────────────────────────────────────

void monitor_run() {
    uint64_t last_total = 0;
    auto last_time = Clock::now();

    while (g_running.load(std::memory_order_relaxed) &&
           !g_found.load(std::memory_order_relaxed)) {

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto     now   = Clock::now();
        uint64_t total = g_total.load(std::memory_order_relaxed);
        double   secs  = std::chrono::duration<double>(now - last_time).count();
        uint64_t kps   = secs > 0 ? (uint64_t)((total - last_total) / secs) : 0;
        last_total = total;
        last_time  = now;

        u128 cur;
        { std::lock_guard<std::mutex> lk(g_display_mutex); cur = g_display_key; }

        if (g_on_stats && g_cb_ctx) {
            std::string hex = u128_to_hex(cur);
            g_on_stats(hex.c_str(), (long long)kps, (long long)total, g_cb_ctx);
        }
    }
}

// ── cleanup ──────────────────────────────────────────────────────────────────

void cleanup_locked() {
    g_running.store(false);
    for (auto& t : g_workers) if (t.joinable()) t.join();
    g_workers.clear();
    if (g_monitor.joinable()) g_monitor.join();
}

} // namespace

// ── C interface ──────────────────────────────────────────────────────────────

extern "C" void searcher_start(int puzzle, stats_cb on_stats, found_cb on_found, void* ctx) {
    std::lock_guard<std::mutex> lock(g_mutex);
    cleanup_locked();

    const char* addr = target_address(puzzle);
    if (!addr) { fprintf(stderr, "unsupported puzzle %d\n", puzzle); return; }

    g_on_stats = on_stats;
    g_on_found = on_found;
    g_cb_ctx   = ctx;

    ec::init();
    std::vector<uint8_t> decoded = base58check_decode(addr);
    if (decoded.size() != 21) {
        fprintf(stderr, "failed to decode target address for puzzle %d\n", puzzle);
        return;
    }
    for (int t = 0; t < 3; t++) memcpy(g_targets[t], decoded.data() + 1, 20);

    u128 lo = (u128)1 << (puzzle - 1);
    u128 hi = (((u128)1 << puzzle) - 1);

    g_found.store(false);
    g_total.store(0);
    { std::lock_guard<std::mutex> lk(g_display_mutex); g_display_key = lo; }
    g_active_puzzle = puzzle;
    g_running.store(true);

    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;

    fprintf(stderr, "start puzzle %d with %u threads\n", puzzle, hw);

    for (unsigned i = 0; i < hw; i++)
        g_workers.emplace_back([lo, hi, puzzle]() { worker_run(lo, hi, puzzle); });

    g_monitor = std::thread(monitor_run);
}

extern "C" void searcher_stop(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    cleanup_locked();
    fprintf(stderr, "stopped; total=%llu\n", (unsigned long long)g_total.load());
    g_on_stats = nullptr;
    g_on_found = nullptr;
    g_cb_ctx   = nullptr;
}

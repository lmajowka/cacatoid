#include <jni.h>
#include <android/log.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "secp256k1_utils.h"
#include "base58.h"

#define LOG_TAG "BtcSearcher"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using u128 = unsigned __int128;
using Clock = std::chrono::steady_clock;

namespace {

// ---- Target addresses (compressed P2PKH) keyed by puzzle number ----
// Puzzle 20 is already solved (key 0xd2c55); its tiny 2^19..2^20-1 range makes
// it a fast end-to-end test that the search actually finds a match.
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

// ---- JNI / threading state ----
JavaVM* g_vm = nullptr;
std::mutex g_mutex;
std::vector<std::thread> g_workers;
std::thread g_monitor;

std::atomic<bool> g_running{false};
std::atomic<bool> g_found{false};
std::atomic<uint64_t> g_total{0};
// Current key under test, stored as low/high 64-bit halves of a 128-bit value.
std::atomic<uint64_t> g_cur_lo{0};
std::atomic<uint64_t> g_cur_hi{0};

jobject g_listener = nullptr;        // global ref to the Kotlin SearchListener
jmethodID g_on_stats = nullptr;
jmethodID g_on_found = nullptr;

// Found result (set once, read after g_found becomes true).
u128 g_found_key = 0;
int g_active_puzzle = 0;

// ---- helpers ----

void u128_to_key(u128 v, uint8_t key[32]) {
    memset(key, 0, 32);
    for (int i = 0; i < 16; i++) key[31 - i] = uint8_t(v >> (i * 8));
}

std::string to_hex(const uint8_t* data, size_t len) {
    static const char* H = "0123456789abcdef";
    std::string s;
    s.resize(len * 2);
    for (size_t i = 0; i < len; i++) {
        s[i * 2] = H[data[i] >> 4];
        s[i * 2 + 1] = H[data[i] & 0xf];
    }
    return s;
}

std::string key_hex(u128 v) {
    uint8_t key[32];
    u128_to_key(v, key);
    return to_hex(key, 32);
}

// WIF for a compressed key: base58check(0x80 || privkey || 0x01).
std::string to_wif(const uint8_t key[32]) {
    uint8_t payload[34];
    payload[0] = 0x80;
    memcpy(payload + 1, key, 32);
    payload[33] = 0x01;
    return base58check_encode(payload, 34);
}

// P2PKH address: base58check(0x00 || hash160).
std::string to_address(const uint8_t hash160[20]) {
    uint8_t payload[21];
    payload[0] = 0x00;
    memcpy(payload + 1, hash160, 20);
    return base58check_encode(payload, 21);
}

JNIEnv* attach_env(bool& attached) {
    JNIEnv* env = nullptr;
    attached = false;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
        attached = true;
    }
    return env;
}

void report_found(u128 key_value, int puzzle) {
    uint8_t key[32];
    u128_to_key(key_value, key);

    ec::PubKey pub;
    ec::pubkey_create(key, pub);
    uint8_t hash[20];
    ec::hash160_compressed(pub, hash);

    std::string priv_hex = to_hex(key, 32);
    std::string wif = to_wif(key);
    std::string addr = to_address(hash);

    LOGI("MATCH puzzle %d priv=%s addr=%s", puzzle, priv_hex.c_str(), addr.c_str());

    bool attached = false;
    JNIEnv* env = attach_env(attached);
    if (env && g_listener) {
        jstring jp = env->NewStringUTF(priv_hex.c_str());
        jstring jw = env->NewStringUTF(wif.c_str());
        jstring ja = env->NewStringUTF(addr.c_str());
        env->CallVoidMethod(g_listener, g_on_found, jp, jw, ja, (jint)puzzle);
        env->DeleteLocalRef(jp);
        env->DeleteLocalRef(jw);
        env->DeleteLocalRef(ja);
    }
    if (attached) g_vm->DetachCurrentThread();
}

// One worker scans the sub-range [sub_start, sub_end] starting from a random
// offset and wrapping once, advancing the public key by +G each step.
void worker_run(u128 sub_start, u128 sub_end, const uint8_t targets[3][20], int puzzle) {
    ec::init();
    const u128 len = sub_end - sub_start + 1;

    std::random_device rd;
    std::mt19937_64 rng(((uint64_t)rd() << 32) ^ rd());
    u128 offset = ((u128)rng() << 64 | rng()) % len;
    u128 cur = sub_start + offset;

    uint8_t seckey[32];
    u128_to_key(cur, seckey);
    ec::PubKey pub;
    if (!ec::pubkey_create(seckey, pub)) return;

    uint8_t hash[20];
    u128 done = 0;
    uint64_t local = 0;

    while (done < len) {
        if (g_found.load(std::memory_order_relaxed) || !g_running.load(std::memory_order_relaxed))
            break;

        int b = 0;
        for (; b < BATCH && done < len; b++, done++) {
            ec::hash160_compressed(pub, hash);
            for (int t = 0; t < 3; t++) {
                if (memcmp(hash, targets[t], 20) == 0) {
                    g_found_key = cur;
                    bool expected = false;
                    if (g_found.compare_exchange_strong(expected, true)) {
                        report_found(cur, puzzle);
                    }
                    g_total.fetch_add(local + b + 1, std::memory_order_relaxed);
                    return;
                }
            }
            // advance to the next key
            if (cur == sub_end) {
                cur = sub_start;
                u128_to_key(cur, seckey);
                if (!ec::pubkey_create(seckey, pub)) return;
            } else {
                cur++;
                if (!ec::pubkey_increment(pub)) {
                    u128_to_key(cur, seckey);
                    if (!ec::pubkey_create(seckey, pub)) return;
                }
            }
        }
        local += b;
        g_total.fetch_add(b, std::memory_order_relaxed);
        g_cur_lo.store((uint64_t)cur, std::memory_order_relaxed);
        g_cur_hi.store((uint64_t)(cur >> 64), std::memory_order_relaxed);
        local = 0;
    }
}

void monitor_run() {
    bool attached = false;
    JNIEnv* env = attach_env(attached);

    uint64_t last_total = 0;
    auto last_time = Clock::now();

    while (g_running.load(std::memory_order_relaxed) && !g_found.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto now = Clock::now();
        uint64_t total = g_total.load(std::memory_order_relaxed);
        double secs = std::chrono::duration<double>(now - last_time).count();
        uint64_t kps = secs > 0 ? (uint64_t)((total - last_total) / secs) : 0;
        last_total = total;
        last_time = now;

        u128 cur = ((u128)g_cur_hi.load(std::memory_order_relaxed) << 64) |
                   g_cur_lo.load(std::memory_order_relaxed);

        if (env && g_listener) {
            std::string hex = key_hex(cur);
            jstring jhex = env->NewStringUTF(hex.c_str());
            env->CallVoidMethod(g_listener, g_on_stats, jhex, (jlong)kps, (jlong)total);
            env->DeleteLocalRef(jhex);
        }
    }
    if (attached) g_vm->DetachCurrentThread();
}

void cleanup_locked(JNIEnv* env) {
    g_running.store(false);
    for (auto& t : g_workers) if (t.joinable()) t.join();
    g_workers.clear();
    if (g_monitor.joinable()) g_monitor.join();
    if (g_listener) {
        env->DeleteGlobalRef(g_listener);
        g_listener = nullptr;
    }
}

} // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cacatoid_NativeBridge_nativeStart(JNIEnv* env, jobject /*thiz*/,
                                                   jint puzzle, jobject listener) {
    std::lock_guard<std::mutex> lock(g_mutex);
    cleanup_locked(env); // join any threads from a previous (finished) run

    const char* addr = target_address(puzzle);
    if (!addr) {
        LOGE("unsupported puzzle %d", (int)puzzle);
        return;
    }

    // Resolve listener callbacks.
    g_listener = env->NewGlobalRef(listener);
    jclass cls = env->GetObjectClass(listener);
    g_on_stats = env->GetMethodID(cls, "onStats", "(Ljava/lang/String;JJ)V");
    g_on_found = env->GetMethodID(cls, "onFound", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
    if (!g_on_stats || !g_on_found) {
        LOGE("could not resolve listener methods");
        env->DeleteGlobalRef(g_listener);
        g_listener = nullptr;
        return;
    }

    // Decode the matching target address into a 20-byte HASH160.
    ec::init();
    static uint8_t targets[3][20];
    std::vector<uint8_t> decoded = base58check_decode(addr);
    if (decoded.size() != 21) {
        LOGE("failed to decode target address for puzzle %d", (int)puzzle);
        env->DeleteGlobalRef(g_listener);
        g_listener = nullptr;
        return;
    }
    // Only puzzle `idx` is active; fill all three slots with it so the worker's
    // fixed 3-way compare loop has no spurious targets.
    for (int t = 0; t < 3; t++) memcpy(targets[t], decoded.data() + 1, 20);

    // Range: puzzle n covers [2^(n-1), 2^n - 1].
    u128 lo = (u128)1 << (puzzle - 1);
    u128 hi = (((u128)1 << puzzle) - 1);

    g_found.store(false);
    g_total.store(0);
    g_cur_lo.store((uint64_t)lo);
    g_cur_hi.store((uint64_t)(lo >> 64));
    g_active_puzzle = puzzle;
    g_running.store(true);

    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    u128 span = hi - lo + 1;
    u128 per = span / hw;
    if (per == 0) { per = span; hw = 1; }

    LOGI("start puzzle %d with %u threads", (int)puzzle, hw);

    for (unsigned i = 0; i < hw; i++) {
        u128 s = lo + (u128)i * per;
        u128 e = (i == hw - 1) ? hi : (lo + (u128)(i + 1) * per - 1);
        g_workers.emplace_back([s, e, puzzle]() {
            worker_run(s, e, targets, puzzle);
        });
    }
    g_monitor = std::thread(monitor_run);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cacatoid_NativeBridge_nativeStop(JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_mutex);
    cleanup_locked(env);
    LOGI("stopped; total checked=%llu", (unsigned long long)g_total.load());
}

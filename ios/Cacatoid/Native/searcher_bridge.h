#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*stats_cb)(const char* key_hex, long long kps, long long total, void* ctx);
typedef void (*found_cb)(const char* priv_hex, const char* wif, const char* addr, int puzzle, void* ctx);

void searcher_start(int puzzle, stats_cb on_stats, found_cb on_found, void* ctx);
void searcher_stop(void);

#ifdef __cplusplus
}
#endif

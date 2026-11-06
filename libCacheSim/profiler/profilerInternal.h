#pragma once
/**
 * Profiler functions that the MRC tool (bin/MRC) calls but that are not part
 * of the public API in include/libCacheSim. This header is not installed.
 *
 * dist.c and simulator.c include it, so the compiler checks these
 * declarations against the definitions.
 */

#include <glib.h>

#include "../dataStructure/splay.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/simulator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Defined in dist.c: record req at curr_ts and return its stack distance
 * (-1 on the first access). If last_access_ts is not NULL, it receives the
 * previous access time, or -1. */
int64_t get_stack_dist_add_req(const request_t *req, sTree **splay_tree,
                               GHashTable *hash_table, const int64_t curr_ts,
                               int64_t *last_access_ts);

/* Defined in simulator.c: like simulate_with_multi_caches, but cache i reads
 * from its own reader, readers[i], so each cache can run on a differently
 * sampled copy of the trace. */
cache_stat_t *simulate_with_multi_caches_scaling(
    reader_t **readers, cache_t *caches[], int num_of_caches,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool free_cache_when_finish);

#ifdef __cplusplus
}
#endif

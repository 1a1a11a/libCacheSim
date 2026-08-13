/**
 * @file cacheAlgoRegistry.c
 * @brief Maps eviction algorithm names to their constructors.
 *
 * Callers that only have the algorithm's name — the CLI tools and the MINISIM
 * profiler — used to find the constructor two different ways: cachesim carried
 * its own table, while the profiler went through dlsym() against the running
 * executable. The latter cannot work for a statically linked build, because the
 * constructors live in an archive member nothing references, so the linker
 * never pulls them in and the lookup fails at run time.
 *
 * Referencing the table from this translation unit is what pulls those archive
 * members in, so the lookup is a plain function call with no dynamic loading.
 */

#include <strings.h>

#include "libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *name;
  cache_t *(*init_func)(common_cache_params_t, const char *);
} cache_algo_entry_t;

/* Keep alphabetical; several names are aliases for the same constructor. */
static const cache_algo_entry_t g_cache_algos[] = {
    {"2q", TwoQ_init},
    {"arc", ARC_init},
    {"arcv0", ARCv0_init},
    {"CAR", CAR_init},
    {"cacheus", Cacheus_init},
    {"clock", Clock_init},
    {"clock2qplus", Clock2QPlus_init},
    {"clockpro", ClockPro_init},
    {"fifo", FIFO_init},
    {"fifo-merge", FIFO_Merge_init},
    {"fifo-reinsertion", Clock_init},
    {"fifomerge", FIFO_Merge_init},
    {"flashProb", flashProb_init},
    {"gdsf", GDSF_init},
    {"lhd", LHD_init},
    {"lecar", LeCaR_init},
    {"lecarv0", LeCaRv0_init},
    {"lfu", LFU_init},
    {"lfucpp", LFUCpp_init},
    {"lfuda", LFUDA_init},
    {"lirs", LIRS_init},
    {"lru", LRU_init},
    {"lru-k", LRU_K_init},
    {"lru-prob", LRU_Prob_init},
    {"nop", nop_init},
    /* plugin cache that allows user to implement custom cache */
    {"pluginCache", pluginCache_init},
    {"qdlp", QDLP_init},
    {"random", Random_init},
    {"RandomLRU", RandomLRU_init},
    {"randomTwo", RandomTwo_init},
    {"s3-fifo", S3FIFO_init},
    {"s3-fifov0", S3FIFOv0_init},
    {"s3fifo", S3FIFO_init},
    {"s3fifod", S3FIFOd_init},
    {"s3fifov0", S3FIFOv0_init},
    {"sieve", Sieve_init},
    {"size", Size_init},
    {"slru", SLRU_init},
    {"slruv0", SLRUv0_init},
    {"twoq", TwoQ_init},
    {"wtinyLFU", WTinyLFU_init},
    /* these need future information and are only valid on oracle traces, so
     * callers that know the trace type should check before using them */
    {"belady", Belady_init},
    {"beladySize", BeladySize_init},
    {"hyperbolic", Hyperbolic_init},
#ifdef ENABLE_3L_CACHE
    {"3LCache", ThreeLCache_init},
#endif
#ifdef ENABLE_GLCACHE
    {"GLCache", GLCache_init},
    {"gl-cache", GLCache_init},
#endif
#ifdef ENABLE_LRB
    {"lrb", LRB_init},
#endif
};

cache_init_func_ptr find_cache_init_func(const char *cache_algo_name) {
  if (cache_algo_name == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < sizeof(g_cache_algos) / sizeof(g_cache_algos[0]);
       i++) {
    if (strcasecmp(cache_algo_name, g_cache_algos[i].name) == 0) {
      return g_cache_algos[i].init_func;
    }
  }

  return NULL;
}

cache_t *create_cache_by_name(const char *cache_algo_name,
                              const common_cache_params_t cc_params,
                              const char *cache_specific_params) {
  cache_init_func_ptr init_func = find_cache_init_func(cache_algo_name);
  if (init_func == NULL) {
    return NULL;
  }

  return init_func(cc_params, cache_specific_params);
}

#ifdef __cplusplus
}
#endif

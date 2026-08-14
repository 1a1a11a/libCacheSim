#include <strings.h>

#include "libCacheSim/evictionAlgo.h"

// Registry for built-in eviction algorithm init functions.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *name;
  cache_init_func_ptr init_func;
} eviction_algo_entry_t;

cache_init_func_ptr get_builtin_cache_init(const char *const cache_alg_name) {
  static const eviction_algo_entry_t builtin_caches[] = {
      {"2q", TwoQ_init},
      {"arc", ARC_init},
      {"arcv0", ARCv0_init},
      {"belady", Belady_init},
      {"beladySize", BeladySize_init},
      {"CAR", CAR_init},
      {"cacheus", Cacheus_init},
      {"clock", Clock_init},
      {"clock2qplus", Clock2QPlus_init},
      {"clockpro", ClockPro_init},
      {"cr-lfu", CR_LFU_init},
      {"cr_lfu", CR_LFU_init},
      {"fifo", FIFO_init},
      {"fifo-merge", FIFO_Merge_init},
      {"fifo-reinsertion", Clock_init},
      {"fifomerge", FIFO_Merge_init},
      {"flashProb", flashProb_init},
      {"gdsf", GDSF_init},
      {"hyperbolic", Hyperbolic_init},
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
      {"lru_prob", LRU_Prob_init},
      {"lruv0", LRUv0_init},
      {"mru", MRU_init},
      {"nop", nop_init},
      {"pluginCache", pluginCache_init},
      {"qdlp", QDLP_init},
      {"random", Random_init},
      {"RandomLRU", RandomLRU_init},
      {"randomTwo", RandomTwo_init},
      {"s3-fifo", S3FIFO_init},
      {"s3fifo", S3FIFO_init},
      {"s3fifod", S3FIFOd_init},
      {"s3-fifov0", S3FIFOv0_init},
      {"s3fifov0", S3FIFOv0_init},
      {"s3lru", S3LRU_init},
      {"sieve", Sieve_init},
      {"size", Size_init},
      {"slru", SLRU_init},
      {"slruv0", SLRUv0_init},
      {"sr-lru", SR_LRU_init},
      {"sr_lru", SR_LRU_init},
      {"twoq", TwoQ_init},
      {"wtinyLFU", WTinyLFU_init},
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

  for (size_t i = 0; i < sizeof(builtin_caches) / sizeof(builtin_caches[0]);
       i++) {
    if (strcasecmp(cache_alg_name, builtin_caches[i].name) == 0) {
      return builtin_caches[i].init_func;
    }
  }

  return NULL;
}

#ifdef __cplusplus
}
#endif

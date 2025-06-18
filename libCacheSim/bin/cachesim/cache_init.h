
#include <strings.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline cache_t *create_cache(const char *trace_path, const char *eviction_algo, const uint64_t cache_size,
                                    const char *eviction_params, const bool consider_obj_metadata) {
  common_cache_params_t cc_params = {
    .cache_size = cache_size,
    .default_ttl = 86400 * 300,
    .hashpower = 24,
    .consider_obj_metadata = consider_obj_metadata,
};
  cache_t *cache;

  /* the trace provided is small */
  if (trace_path != NULL && strstr(trace_path, "data/trace.") != NULL) cc_params.hashpower -= 8;
  typedef struct {
    const char *name;
    cache_t *(*init_func)(common_cache_params_t, const char *);
  } eviction_algo_entry_t;

  static const eviction_algo_entry_t simple_algos[] = {
      {"lru", LRU_init},
      {"fifo", FIFO_init},
      {"arc", ARC_init},
      {"arcv0", ARCv0_init},
      {"lhd", LHD_init},
      {"random", Random_init},
      {"randomTwo", RandomTwo_init},
      {"lfu", LFU_init},
      {"gdsf", GDSF_init},
      {"lfuda", LFUDA_init},
      {"twoq", TwoQ_init},
      {"2q", TwoQ_init},
      {"slru", SLRU_init},
      {"slruv0", SLRUv0_init},
      {"lecar", LeCaR_init},
      {"lecarv0", LeCaRv0_init},
      {"RandomLRU", RandomLRU_init},
      {"cacheus", Cacheus_init},
      {"size", Size_init},
      {"lfucpp", LFUCpp_init},
      {"wtinyLFU", WTinyLFU_init},
      {"nop", nop_init},
      {"fifo-reinsertion", Clock_init},
      {"clock", Clock_init},
      {"second-chance", Clock_init},
      {"clockpro", ClockPro_init},
      {"lirs", LIRS_init},
      {"fifomerge", FIFO_Merge_init},
      {"fifo-merge", FIFO_Merge_init},
      {"flashProb", flashProb_init},
      {"sfifo", SFIFO_init},
      {"sfifov0", SFIFOv0_init},
      {"lru-prob", LRU_Prob_init},
      {"fifo-belady", FIFO_Belady_init},
      {"lru-belady", LRU_Belady_init},
      {"sieve-belady", Sieve_Belady_init},
      {"s3lru", S3LRU_init},
      {"s3fifo", S3FIFO_init},
      {"s3-fifo", S3FIFO_init},
      {"s3fifov0", S3FIFOv0_init},
      {"s3-fifov0", S3FIFOv0_init},
      {"s3fifod", S3FIFOd_init},
      {"qdlp", QDLP_init},
      {"CAR", CAR_init},
      {"sieve", Sieve_init},
  };

  cache_t *(*init_func)(common_cache_params_t, const char *) = NULL;
  for (size_t i = 0; i < sizeof(simple_algos) / sizeof(simple_algos[0]); ++i) {
    if (strcasecmp(eviction_algo, simple_algos[i].name) == 0) {
      init_func = simple_algos[i].init_func;
      break;
    }
  }

  // Initializing for algorithms which require special handling (not in simple_algos)
  if (init_func) {
    cache = init_func(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "hyperbolic") == 0) {
    cc_params.hashpower = MAX(cc_params.hashpower - 8, 16);
    cache = Hyperbolic_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "tinyLFU") == 0) {
    if (eviction_params == NULL) {
      cache = WTinyLFU_init(cc_params, eviction_params);
    } else {
      const char *window_size = strstr(eviction_params, "window-size=");
      if (window_size == NULL) {
        char *new_params = malloc(strlen(eviction_params) + 20);
        sprintf(new_params, "%s,window-size=0.01", eviction_params);
        cache = WTinyLFU_init(cc_params, new_params);
      } else {
        cache = WTinyLFU_init(cc_params, eviction_params);
      }
    }
  } else if (strcasecmp(eviction_algo, "belady") == 0 && strcasestr(trace_path, "lcs") == NULL) {
    if (strcasestr(trace_path, "oracleGeneral") == NULL) {
      WARN("belady is only supported for oracleGeneral and lcs trace\n");
      WARN("to convert a trace to lcs format\n");
      WARN("./bin/traceConv input_trace trace_format output_trace\n");
      WARN("./bin/traceConv ../data/cloudPhysicsIO.txt txt\n");
      exit(1);
    }
    cache = Belady_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "beladySize") == 0) {
    if (strcasestr(trace_path, "oracleGeneral") == NULL && strcasestr(trace_path, "lcs") == NULL) {
      WARN("beladySize is only supported for oracleGeneral and lcs trace\n");
      WARN("to convert a trace to lcs format\n");
      WARN("./bin/traceConv input_trace trace_format output_trace\n");
      WARN("./bin/traceConv ../data/cloudPhysicsIO.txt txt\n");
      exit(1);
    }
    cc_params.hashpower = MAX(cc_params.hashpower - 8, 16);
    cache = BeladySize_init(cc_params, eviction_params);
#ifdef ENABLE_3L_CACHE
  } else if (strcasecmp(eviction_algo, "3LCache") == 0) {
    cache = ThreeLCache_init(cc_params, eviction_params);
#endif
#ifdef ENABLE_GLCACHE
  } else if (strcasecmp(eviction_algo, "GLCache") == 0 || strcasecmp(eviction_algo, "gl-cache") == 0) {
    cache = GLCache_init(cc_params, eviction_params);
#endif
#ifdef ENABLE_LRB
  } else if (strcasecmp(eviction_algo, "lrb") == 0) {
    cache = LRB_init(cc_params, eviction_params);
#endif
#ifdef INCLUDE_PRIV
  } else if (strcasecmp(eviction_algo, "mclock") == 0) {
    cache = MClock_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lp-sfifo") == 0) {
    cache = LP_SFIFO_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lp-arc") == 0) {
    cache = LP_ARC_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lp-twoq") == 0) {
    cache = LP_TwoQ_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "qdlpv0") == 0) {
    cache = QDLPv0_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "s3fifodv2") == 0) {
    cache = S3FIFOdv2_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "myMQv1") == 0) {
    cache = myMQv1_init(cc_params, eviction_params);
#endif
  } else {
    ERROR("do not support algorithm %s\n", eviction_algo);
    abort();
  }

  return cache;
}

#ifdef __cplusplus
}
#endif

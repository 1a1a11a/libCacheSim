
#include <strings.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline cache_t *create_cache(const char *trace_path,
                                    const char *eviction_algo,
                                    const uint64_t cache_size,
                                    const char *eviction_params,
                                    const bool consider_obj_metadata) {
  common_cache_params_t cc_params = {
      .cache_size = cache_size,
      .default_ttl = 86400 * 300,
      .hashpower = 24,
      .consider_obj_metadata = consider_obj_metadata,
  };
  cache_t *cache;

  /* the trace provided is small */
  if (trace_path != NULL && strstr(trace_path, "data/trace.") != NULL)
    cc_params.hashpower -= 8;
  typedef struct {
    const char *name;
    cache_t *(*init_func)(common_cache_params_t, const char *);
  } eviction_algo_entry_t;
  static const eviction_algo_entry_t simple_algos[] = {
      {"2q", TwoQ_init},
      {"arc", ARC_init},
      {"arcv0", ARCv0_init},
      {"CAR", CAR_init},
      {"cacheus", Cacheus_init},
      {"clock", Clock_init},
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
      {"lru-prob", LRU_Prob_init},
      {"nop", nop_init},
      // plugin cache that allows user to implement custom cache
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

  cache_t *(*init_func)(common_cache_params_t, const char *) = NULL;
  for (size_t i = 0; i < sizeof(simple_algos) / sizeof(simple_algos[0]); ++i) {
    if (strcasecmp(eviction_algo, simple_algos[i].name) == 0) {
      init_func = simple_algos[i].init_func;
      break;
    }
  }

  // Initializing for algorithms which require special handling (not in
  // simple_algos)
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
        // Calculate exact size needed: original + ",window-size=0.01" + null
        // terminator
        size_t new_params_len =
            strlen(eviction_params) + strlen(",window-size=0.01") + 1;
        char *new_params = (char *)malloc(new_params_len);
        if (new_params == NULL) {
          ERROR("failed to allocate memory for new_params\n");
          abort();
        }
        snprintf(new_params, new_params_len, "%s,window-size=0.01",
                 eviction_params);
        cache = WTinyLFU_init(cc_params, new_params);
        free(new_params);  // Free the allocated memory
      } else {
        cache = WTinyLFU_init(cc_params, eviction_params);
      }
    }
  } else if (strcasecmp(eviction_algo, "belady") == 0 &&
             strcasestr(trace_path, "lcs") == NULL) {
    if (strcasestr(trace_path, "oracleGeneral") == NULL) {
      WARN("belady is only supported for oracleGeneral and lcs trace\n");
      WARN("to convert a trace to lcs format\n");
      WARN("./bin/traceConv input_trace trace_format output_trace\n");
      WARN("./bin/traceConv ../data/cloudPhysicsIO.txt txt\n");
      exit(1);
    }
    cache = Belady_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "beladySize") == 0) {
    if (strcasestr(trace_path, "oracleGeneral") == NULL &&
        strcasestr(trace_path, "lcs") == NULL) {
      WARN("beladySize is only supported for oracleGeneral and lcs trace\n");
      WARN("to convert a trace to lcs format\n");
      WARN("./bin/traceConv input_trace trace_format output_trace\n");
      WARN("./bin/traceConv ../data/cloudPhysicsIO.txt txt\n");
      exit(1);
    }
    cc_params.hashpower = MAX(cc_params.hashpower - 8, 16);
    cache = BeladySize_init(cc_params, eviction_params);
  } else {
    ERROR("do not support algorithm %s\n", eviction_algo);
    abort();
  }

  return cache;
}

#ifdef __cplusplus
}
#endif

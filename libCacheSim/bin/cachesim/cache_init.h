
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/evictionAlgo.h"

static inline cache_t *create_cache(const char *trace_path,
                                    const char *eviction_algo,
                                    const uint64_t cache_size,
                                    const char *eviction_params,
                                    const bool consider_obj_metadata) {
  common_cache_params_t cc_params = {
      .cache_size = cache_size,
      .hashpower = 24,
      .default_ttl = 86400 * 300,
      .consider_obj_metadata = consider_obj_metadata,
  };
  cache_t *cache;

  /* the trace provided is small */
  if (strcasestr(trace_path, "data/trace.") != NULL) cc_params.hashpower -= 8;

  if (strcasecmp(eviction_algo, "lru") == 0) {
    cache = LRU_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "fifo") == 0) {
    cache = FIFO_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "arc") == 0) {
    cache = ARC_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "arcv0") == 0) {
    cache = ARCv0_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lhd") == 0) {
    cache = LHD_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lfu") == 0) {
    cache = LFU_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "gdsf") == 0) {
    cache = GDSF_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lfuda") == 0) {
    cache = LFUDA_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "twoq") == 0) {
    cache = TwoQ_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "slru") == 0) {
    cache = SLRU_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "slruv0") == 0) {
    cache = SLRUv0_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "sfifo") == 0) {
    cache = SFIFO_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "sfifov0") == 0) {
    cache = SFIFOv0_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "hyperbolic") == 0) {
    cc_params.hashpower = MAX(cc_params.hashpower - 8, 16);
    cache = Hyperbolic_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lecar") == 0) {
    cache = LeCaR_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lecarv0") == 0) {
    cache = LeCaRv0_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "cacheus") == 0) {
    cache = Cacheus_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "size") == 0) {
    cache = Size_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lfucpp") == 0) {
    cache = LFUCpp_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "tinyLFU") == 0) {
    // if no window specified, we add window-size=0 to the eviction params
    if (eviction_params == NULL) {
      // eviction_params = strdup("window-size=0");
      cache = WTinyLFU_init(cc_params, "window-size=0");
    } else {
      const char *window_size = strstr(eviction_params, "window-size=");
      if (window_size == NULL) {
        char *new_params = malloc(strlen(eviction_params) + 20);
        sprintf(new_params, "%s,window-size=0", eviction_params);
        cache = WTinyLFU_init(cc_params, new_params);
      } else {
        cache = WTinyLFU_init(cc_params, eviction_params);
      }
    }
  } else if (strcasecmp(eviction_algo, "wtinyLFU") == 0) {
    cache = WTinyLFU_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "belady") == 0) {
    cache = Belady_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "beladySize") == 0) {
    cc_params.hashpower = MAX(cc_params.hashpower - 8, 16);
    cache = BeladySize_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "fifo-reinsertion") == 0 ||
             strcasecmp(eviction_algo, "clock") == 0 ||
             strcasecmp(eviction_algo, "second-chance") == 0) {
    cache = Clock_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lirs") == 0) {
    cache = LIRS_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "flashProb") == 0) {
    cache = flashProb_init(cc_params, eviction_params);
#if defined(ENABLE_GLCACHE)
  } else if (strcasecmp(eviction_algo, "GLCache") == 0 ||
             strcasecmp(eviction_algo, "gl-cache") == 0) {
    cache = GLCache_init(cc_params, eviction_params);
#endif
#ifdef ENABLE_LRB
  } else if (strcasecmp(eviction_algo, "lrb") == 0) {
    cache = LRB_init(cc_params, eviction_params);
#endif
#ifdef INCLUDE_PRIV
  } else if (strcasecmp(eviction_algo, "myclock") == 0) {
    cache = MyClock_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "mclock") == 0) {
    cache = MClock_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lp-sfifo") == 0) {
    cache = LP_SFIFO_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lp-arc") == 0) {
    cache = LP_ARC_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lp-twoq") == 0) {
    cache = LP_TwoQ_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "sfifomerge") == 0 ||
             strcasecmp(eviction_algo, "sfifo-merge") == 0) {
    cache = SFIFO_Merge_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "sfifo-reinsertion") == 0) {
    cache = SFIFO_Reinsertion_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lru-prob") == 0) {
    cache = LRU_Prob_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "qdlp") == 0) {
    cache = QDLP_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "qdlpv1") == 0) {
    cache = QDLPv1_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "qdlpv2") == 0) {
    cache = QDLPv2_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "s3fifo") == 0) {
    cache = S3FIFO_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "myMQv1") == 0) {
    cache = myMQv1_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "lru-belady") == 0) {
    if (strstr(trace_path, ".zst") != NULL) {
      ERROR("lru-belady only supports uncompressed trace files\n");
    }
    ERROR("not implemented\n");
    // reader_t *reader = clone_reader(args->reader);
    cache = LRU_init(cc_params, eviction_params);
    // cache->future_stack_dist = get_stack_dist(
    //     reader, FUTURE_STACK_DIST, &(cache->future_stack_dist_array_size));
    // assert(get_num_of_req(reader) == cache->future_stack_dist_array_size);
    // close_reader(reader);
#endif
  } else {
    ERROR("do not support algorithm %s\n", eviction_algo);
    abort();
  }

  return cache;
}
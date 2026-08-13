
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

/* log2 of the hash table size; 24 gives 16M entries */
#define DEFAULT_HASHPOWER 24

/**
 * @brief create a cache for the CLI, given the algorithm name
 *
 * @param hashpower log2 of the hash table size. This used to be adjusted by
 * sniffing the trace path for "data/trace.", a file that has not existed for a
 * long time, so the adjustment never fired. It is a --hashpower option now
 * rather than a hidden rule, because sampling-based algorithms draw candidates
 * from the hash table and so their miss ratios depend on its size — not
 * something to change silently based on where a trace happens to live.
 */
static inline cache_t *create_cache(const char *trace_path,
                                    const char *eviction_algo,
                                    const uint64_t cache_size,
                                    const char *eviction_params,
                                    const bool consider_obj_metadata,
                                    const int hashpower) {
  common_cache_params_t cc_params = {
      .cache_size = cache_size,
      .default_ttl = 86400 * 300,
      .hashpower = hashpower,
      .consider_obj_metadata = consider_obj_metadata,
  };
  cache_t *cache;

  /* The name to constructor mapping lives in the library
   * (cache/cacheAlgoRegistry.c) so that the MINISIM profiler, which only knows
   * the algorithm by name, shares one table with the CLI. The cases below need
   * more than a lookup — a smaller hash table, a default parameter, or a check
   * that the trace carries the future information the algorithm needs — so
   * they are handled here rather than in the registry. */
  if (strcasecmp(eviction_algo, "hyperbolic") == 0) {
    cc_params.hashpower = MAX(cc_params.hashpower - 8, 16);
    cache = Hyperbolic_init(cc_params, eviction_params);
  } else if (strcasecmp(eviction_algo, "tinyLFU") == 0) {
    if (eviction_params == NULL || eviction_params[0] == '\0') {
      cache = WTinyLFU_init(cc_params, NULL);
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
  } else if (strcasecmp(eviction_algo, "belady") == 0) {
    if (strcasestr(trace_path, "oracleGeneral") == NULL &&
        strcasestr(trace_path, "lcs") == NULL) {
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
    cache = create_cache_by_name(eviction_algo, cc_params, eviction_params);
    if (cache == NULL) {
      ERROR("do not support algorithm %s\n", eviction_algo);
      abort();
    }
  }

  return cache;
}

#ifdef __cplusplus
}
#endif

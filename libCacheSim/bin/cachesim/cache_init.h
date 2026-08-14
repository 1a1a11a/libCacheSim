
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
    cache_init_func_ptr init_func = get_builtin_cache_init(eviction_algo);
    if (init_func == NULL) {
      ERROR("do not support algorithm %s\n", eviction_algo);
      abort();
    }
    cache = init_func(cc_params, eviction_params);
  }

  return cache;
}

#ifdef __cplusplus
}
#endif
